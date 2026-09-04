#include "metaflux/backend/vulkan.h"
#include "metaflux/backend/vulkan_streams.hpp"

#include "benchmark_common.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <span>

namespace {

using metaflux::backend::vulkan::Dependency;
using metaflux::backend::vulkan::OperationKind;
using metaflux::backend::vulkan::QueueSubmission;
using metaflux::backend::vulkan::QueueSubmissionLedger;
using metaflux::backend::vulkan::QueueSubmissionStatus;
using metaflux::backend::vulkan::StreamGraph;
using metaflux::backend::vulkan::StreamStatus;
using metaflux::backend::vulkan::SubmissionPlan;
using metaflux::backend::vulkan::Visibility;

bool plan_add_copy_barrier(std::uint32_t vendor_id, std::array<SubmissionPlan, 4>* plans) {
  if (plans == nullptr ||
      (vendor_id != MF_VULKAN_VENDOR_ID_AMD && vendor_id != MF_VULKAN_VENDOR_ID_NVIDIA)) {
    return false;
  }
  StreamGraph graph(21U);
  if (graph.create_stream(1U) != StreamStatus::success ||
      graph.create_stream(2U) != StreamStatus::success) {
    return false;
  }
  if (graph.submit(21U, 1U, OperationKind::copy,
                   Visibility{.stage_mask = metaflux::backend::vulkan::kStageTransfer,
                              .access_mask = metaflux::backend::vulkan::kAccessTransferWrite},
                   {}, &(*plans)[0]) != StreamStatus::success) {
    return false;
  }
  const std::array<Dependency, 1> after_copy{
      {Dependency{.stream_id = 1U, .timeline_value = (*plans)[0].sequence}}};
  if (graph.submit(21U, 1U, OperationKind::launch,
                   Visibility{.stage_mask = metaflux::backend::vulkan::kStageCompute,
                              .access_mask = metaflux::backend::vulkan::kAccessShaderRead |
                                             metaflux::backend::vulkan::kAccessShaderWrite},
                   after_copy, &(*plans)[1]) != StreamStatus::success) {
    return false;
  }
  if (graph.submit(21U, 2U, OperationKind::copy,
                   Visibility{.stage_mask = metaflux::backend::vulkan::kStageTransfer,
                              .access_mask = metaflux::backend::vulkan::kAccessTransferRead |
                                             metaflux::backend::vulkan::kAccessTransferWrite},
                   {}, &(*plans)[2]) != StreamStatus::success) {
    return false;
  }
  const std::array<Dependency, 1> barrier{
      {Dependency{.stream_id = 1U, .timeline_value = (*plans)[1].sequence}}};
  if (graph.submit(21U, 2U, OperationKind::launch,
                   Visibility{.stage_mask = metaflux::backend::vulkan::kStageCompute,
                              .access_mask = metaflux::backend::vulkan::kAccessShaderRead},
                   barrier, &(*plans)[3]) != StreamStatus::success) {
    return false;
  }
  return true;
}

bool same_plan(const SubmissionPlan& left, const SubmissionPlan& right) noexcept {
  if (left.sequence != right.sequence || left.generation != right.generation ||
      left.stream_id != right.stream_id || left.kind != right.kind ||
      left.visibility.stage_mask != right.visibility.stage_mask ||
      left.visibility.access_mask != right.visibility.access_mask ||
      left.dependency_count != right.dependency_count) {
    return false;
  }
  for (std::uint32_t index = 0U; index < left.dependency_count; ++index) {
    if (left.dependencies[index].stream_id != right.dependencies[index].stream_id ||
        left.dependencies[index].timeline_value != right.dependencies[index].timeline_value) {
      return false;
    }
  }
  return true;
}

bool dual_family_body(void*) {
  std::array<SubmissionPlan, 4> amd{};
  std::array<SubmissionPlan, 4> nvidia{};
  if (!plan_add_copy_barrier(MF_VULKAN_VENDOR_ID_AMD, &amd) ||
      !plan_add_copy_barrier(MF_VULKAN_VENDOR_ID_NVIDIA, &nvidia)) {
    return false;
  }
  for (std::size_t index = 0; index < amd.size(); ++index) {
    if (!same_plan(amd[index], nvidia[index])) {
      return false;
    }
  }
  return mf_vulkan_driver_family_from_vendor_id_v1(MF_VULKAN_VENDOR_ID_AMD) ==
             MF_VULKAN_DRIVER_FAMILY_AMD &&
         mf_vulkan_driver_family_from_vendor_id_v1(MF_VULKAN_VENDOR_ID_NVIDIA) ==
             MF_VULKAN_DRIVER_FAMILY_NVIDIA;
}

struct LedgerContext {
  QueueSubmissionLedger* ledger = nullptr;
  std::uint64_t generation = 0U;
  std::uint64_t stream_id = 0U;
};

bool ledger_enqueue_complete_body(void* context) {
  auto* state = static_cast<LedgerContext*>(context);
  QueueSubmission submission{};
  const Visibility launch_visibility{
      .stage_mask = metaflux::backend::vulkan::kStageCompute,
      .access_mask = metaflux::backend::vulkan::kAccessShaderRead |
                     metaflux::backend::vulkan::kAccessShaderWrite};
  const auto status = state->ledger->submit(state->generation, state->stream_id,
                                            OperationKind::launch, launch_visibility, {},
                                            &submission);
  if (status != QueueSubmissionStatus::success) {
    return false;
  }
  return state->ledger->complete(state->generation, submission.completion_value) ==
         QueueSubmissionStatus::success;
}

bool time_stage(const char* metric, std::uint32_t sample_index, bool (*body)(void*),
                void* context) {
  std::uint64_t start_ns = 0U;
  std::uint64_t end_ns = 0U;
  if (mf_benchmark_now_ns(&start_ns) != 0) {
    return false;
  }
  if (!body(context)) {
    return false;
  }
  if (mf_benchmark_now_ns(&end_ns) != 0 || end_ns < start_ns) {
    return false;
  }
  mf_benchmark_emit_sample(metric, sample_index, end_ns - start_ns, "ns");
  return true;
}

} // namespace

int main(int argc, char** argv) {
  std::uint32_t warmup_count = 0U;
  std::uint32_t sample_count = 0U;
  if (argc != 3 || mf_benchmark_parse_u32(argv[1], &warmup_count) != 0 ||
      mf_benchmark_parse_u32(argv[2], &sample_count) != 0) {
    (void)std::fprintf(stderr, "usage: %s WARMUP_COUNT SAMPLE_COUNT\n", argv[0]);
    return 2;
  }

  // capacity, generation
  QueueSubmissionLedger warmup_ledger(4U, 1U);
  if (warmup_ledger.create_stream(1U) != QueueSubmissionStatus::success) {
    return 3;
  }
  LedgerContext ledger_context{.ledger = &warmup_ledger, .generation = 1U, .stream_id = 1U};
  for (std::uint32_t index = 0U; index < warmup_count; ++index) {
    if (!dual_family_body(nullptr) || !ledger_enqueue_complete_body(&ledger_context)) {
      return 4;
    }
  }

  QueueSubmissionLedger measured_ledger(8U, 2U);
  if (measured_ledger.create_stream(1U) != QueueSubmissionStatus::success) {
    return 5;
  }
  ledger_context.ledger = &measured_ledger;
  ledger_context.generation = 2U;

  for (std::uint32_t index = 0U; index < sample_count; ++index) {
    if (!time_stage("vulkan_provider_enqueue_plan_ns", index, dual_family_body, nullptr) ||
        !time_stage("vulkan_worker_dequeue_ledger_ns", index, ledger_enqueue_complete_body,
                    &ledger_context)) {
      return 6;
    }
  }

  if (!dual_family_body(nullptr)) {
    return 7;
  }

  mf_benchmark_emit_metadata_text("workload", "vulkan_stage_profile_host_independent");
  mf_benchmark_emit_metadata_u64("warmup_count", warmup_count);
  mf_benchmark_emit_metadata_u64("sample_count", sample_count);
  mf_benchmark_emit_metadata_text("mode_poll", "stream_graph_and_ledger");
  mf_benchmark_emit_metadata_text("mode_block", "host_pending_physical_queue");
  mf_benchmark_emit_metadata_text("transport_memfd", "planner_identity_shared");
  mf_benchmark_emit_metadata_text("transport_cdev", "host_pending");
  mf_benchmark_emit_metadata_text("transport_vfio_user", "host_pending");
  mf_benchmark_emit_metadata_text("memory_tier", "planner_agnostic");
  mf_benchmark_emit_metadata_text("icd_submit", "outside_client_zero_syscall_claim");
  mf_benchmark_emit_metadata_text("dual_family", "amd_nvidia_plan_identity");
  return 0;
}
