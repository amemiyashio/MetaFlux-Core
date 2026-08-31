#include "metaflux/backend/vulkan_streams.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace {

using metaflux::backend::vulkan::CommandResourcePool;
using metaflux::backend::vulkan::CommandResourceStatus;
using metaflux::backend::vulkan::Dependency;
using metaflux::backend::vulkan::OperationKind;
using metaflux::backend::vulkan::QueueSubmissionLedger;
using metaflux::backend::vulkan::QueueSubmissionStatus;
using metaflux::backend::vulkan::StreamGraph;
using metaflux::backend::vulkan::StreamStatus;
using metaflux::backend::vulkan::SubmissionPlan;
using metaflux::backend::vulkan::Visibility;

bool fifo_and_cross_stream_dependencies() {
  StreamGraph graph(9U);
  if (graph.create_stream(1U) != StreamStatus::success ||
      graph.create_stream(2U) != StreamStatus::success) {
    return false;
  }
  SubmissionPlan first{};
  SubmissionPlan independent{};
  if (graph.submit(9U, 1U, OperationKind::launch,
                   Visibility{.stage_mask = metaflux::backend::vulkan::kStageCompute,
                              .access_mask = metaflux::backend::vulkan::kAccessShaderWrite},
                   {}, &first) != StreamStatus::success ||
      first.sequence != 1U || first.dependency_count != 0U ||
      graph.submit(9U, 2U, OperationKind::copy,
                   Visibility{.stage_mask = metaflux::backend::vulkan::kStageTransfer,
                              .access_mask = metaflux::backend::vulkan::kAccessTransferRead |
                                             metaflux::backend::vulkan::kAccessTransferWrite},
                   {}, &independent) != StreamStatus::success ||
      independent.sequence != 2U || independent.dependency_count != 0U) {
    return false;
  }
  const std::array<Dependency, 1> wait{{Dependency{.stream_id = 2U, .timeline_value = 2U}}};
  SubmissionPlan dependent{};
  if (graph.submit(9U, 1U, OperationKind::launch,
                   Visibility{.stage_mask = metaflux::backend::vulkan::kStageCompute,
                              .access_mask = metaflux::backend::vulkan::kAccessShaderRead},
                   wait, &dependent) != StreamStatus::success ||
      dependent.sequence != 3U || dependent.dependency_count != 2U ||
      dependent.dependencies[0].stream_id != 1U || dependent.dependencies[0].timeline_value != 1U ||
      dependent.dependencies[1].stream_id != 2U || dependent.dependencies[1].timeline_value != 2U) {
    return false;
  }
  return true;
}

bool negative_paths() {
  StreamGraph graph(4U);
  SubmissionPlan plan{};
  if (graph.create_stream(1U) != StreamStatus::success ||
      graph.create_stream(1U) != StreamStatus::invalid_argument ||
      graph.submit(3U, 1U, OperationKind::copy,
                   Visibility{.stage_mask = metaflux::backend::vulkan::kStageTransfer,
                              .access_mask = metaflux::backend::vulkan::kAccessTransferRead},
                   {}, &plan) != StreamStatus::stale_generation ||
      graph.submit(4U, 99U, OperationKind::copy,
                   Visibility{.stage_mask = metaflux::backend::vulkan::kStageTransfer,
                              .access_mask = metaflux::backend::vulkan::kAccessTransferRead},
                   {}, &plan) != StreamStatus::unknown_stream ||
      graph.submit(4U, 1U, OperationKind::launch,
                   Visibility{.stage_mask = metaflux::backend::vulkan::kStageTransfer,
                              .access_mask = metaflux::backend::vulkan::kAccessTransferRead},
                   {}, &plan) != StreamStatus::invalid_visibility) {
    return false;
  }
  if (graph.submit(4U, 1U, OperationKind::copy,
                   Visibility{.stage_mask = metaflux::backend::vulkan::kStageTransfer,
                              .access_mask = metaflux::backend::vulkan::kAccessTransferRead},
                   {}, &plan) != StreamStatus::success) {
    return false;
  }
  const std::array<Dependency, 2> duplicate{{Dependency{.stream_id = 1U, .timeline_value = 1U},
                                             Dependency{.stream_id = 1U, .timeline_value = 1U}}};
  return graph.submit(4U, 1U, OperationKind::copy,
                      Visibility{.stage_mask = metaflux::backend::vulkan::kStageTransfer,
                                 .access_mask = metaflux::backend::vulkan::kAccessTransferWrite},
                      duplicate, &plan) == StreamStatus::duplicate_dependency;
}

bool command_resources_recycle_only_after_completion() {
  CommandResourcePool pool(2U, 9U);
  if (pool.capacity() != 2U || pool.generation() != 9U || pool.available_count() != 2U) {
    return false;
  }
  metaflux::backend::vulkan::CommandResource first{};
  metaflux::backend::vulkan::CommandResource second{};
  if (pool.acquire(9U, 1U, &first) != CommandResourceStatus::success ||
      pool.acquire(9U, 2U, &second) != CommandResourceStatus::success ||
      pool.acquire(9U, 3U, &first) != CommandResourceStatus::exhausted || first.id == 0U ||
      second.id == 0U || first.sequence == second.sequence) {
    return false;
  }
  const auto first_copy = first;
  if (pool.submit(first_copy, 4U) != CommandResourceStatus::success ||
      pool.submit(second, 5U) != CommandResourceStatus::success || pool.in_flight_count() != 2U ||
      pool.recycle(9U, 3U) != CommandResourceStatus::success || pool.available_count() != 0U ||
      pool.recycle(9U, 4U) != CommandResourceStatus::success || pool.available_count() != 1U ||
      pool.acquire(9U, 3U, &first) != CommandResourceStatus::success || first.id == first_copy.id ||
      pool.submit(first_copy, 6U) != CommandResourceStatus::not_found ||
      pool.submit(first, 7U) != CommandResourceStatus::success) {
    return false;
  }
  if (pool.reconfigure(10U) != CommandResourceStatus::busy ||
      pool.recycle(9U, 7U) != CommandResourceStatus::success ||
      pool.reconfigure(10U) != CommandResourceStatus::success || pool.available_count() != 2U ||
      pool.in_flight_count() != 0U ||
      pool.submit(second, 1U) != CommandResourceStatus::stale_generation ||
      pool.recycle(9U, 0U) != CommandResourceStatus::stale_generation ||
      pool.reconfigure(10U) != CommandResourceStatus::stale_generation) {
    return false;
  }
  return pool.acquire(10U, 1U, &first) == CommandResourceStatus::success &&
         metaflux::backend::vulkan::command_resource_status_string(CommandResourceStatus::busy) ==
             std::string_view("busy");
}

bool queue_submission_ledger_is_transactional() {
  QueueSubmissionLedger ledger(1U, 7U);
  QueueSubmissionStatus status = QueueSubmissionStatus::success;
  metaflux::backend::vulkan::QueueSubmission submission{};
  const Visibility copy_visibility{.stage_mask = metaflux::backend::vulkan::kStageTransfer,
                                   .access_mask = metaflux::backend::vulkan::kAccessTransferRead |
                                                  metaflux::backend::vulkan::kAccessTransferWrite};
  const Visibility launch_visibility{.stage_mask = metaflux::backend::vulkan::kStageCompute,
                                     .access_mask = metaflux::backend::vulkan::kAccessShaderWrite};

  if (ledger.generation() != 7U || ledger.available_count() != 1U ||
      ledger.in_flight_count() != 0U || ledger.next_completion_value() != 1U ||
      ledger.create_stream(1U) != QueueSubmissionStatus::success) {
    return false;
  }

  status = ledger.submit(7U, 1U, OperationKind::copy, launch_visibility, {}, &submission);
  if (status != QueueSubmissionStatus::invalid_visibility || ledger.available_count() != 1U ||
      ledger.in_flight_count() != 0U || ledger.next_completion_value() != 1U) {
    return false;
  }

  const std::array<Dependency, 1> unknown_dependency{{
      Dependency{.stream_id = 2U, .timeline_value = 1U},
  }};
  status =
      ledger.submit(7U, 1U, OperationKind::copy, copy_visibility, unknown_dependency, &submission);
  if (status != QueueSubmissionStatus::dependency_not_ready || ledger.available_count() != 1U ||
      ledger.in_flight_count() != 0U || ledger.next_completion_value() != 1U) {
    return false;
  }

  status = ledger.submit(7U, 1U, OperationKind::copy, copy_visibility, {}, &submission);
  if (status != QueueSubmissionStatus::success || submission.plan.sequence != 1U ||
      submission.plan.generation != 7U || submission.plan.stream_id != 1U ||
      submission.resource.id == 0U || submission.resource.generation != 7U ||
      submission.resource.stream_id != 1U || submission.completion_value != 1U ||
      ledger.available_count() != 0U || ledger.in_flight_count() != 1U ||
      ledger.next_completion_value() != 2U) {
    return false;
  }

  const auto accepted = submission;
  status = ledger.submit(7U, 1U, OperationKind::copy, copy_visibility, {}, &submission);
  if (status != QueueSubmissionStatus::resource_exhausted || ledger.in_flight_count() != 1U ||
      ledger.next_completion_value() != 2U) {
    return false;
  }
  if (ledger.reconfigure(8U) != QueueSubmissionStatus::busy ||
      ledger.complete(6U, accepted.completion_value) != QueueSubmissionStatus::stale_generation ||
      ledger.complete(7U, accepted.completion_value + 1U) !=
          QueueSubmissionStatus::invalid_timeline ||
      ledger.complete(7U, accepted.completion_value) != QueueSubmissionStatus::success ||
      ledger.available_count() != 1U || ledger.in_flight_count() != 0U) {
    return false;
  }

  if (ledger.reconfigure(8U) != QueueSubmissionStatus::success || ledger.generation() != 8U ||
      ledger.next_completion_value() != 1U ||
      ledger.submit(7U, 1U, OperationKind::copy, copy_visibility, {}, &submission) !=
          QueueSubmissionStatus::stale_generation ||
      ledger.create_stream(1U) != QueueSubmissionStatus::success ||
      ledger.submit(8U, 1U, OperationKind::copy, copy_visibility, {}, &submission) !=
          QueueSubmissionStatus::success ||
      submission.plan.sequence != 1U || submission.completion_value != 1U) {
    return false;
  }

  return metaflux::backend::vulkan::queue_submission_status_string(
             QueueSubmissionStatus::resource_exhausted) == std::string_view("resource-exhausted");
}

bool command_resource_cancel_is_bounded() {
  CommandResourcePool pool(1U, 12U);
  metaflux::backend::vulkan::CommandResource resource{};
  if (pool.acquire(12U, 1U, &resource) != CommandResourceStatus::success ||
      pool.cancel(resource) != CommandResourceStatus::success || pool.available_count() != 1U ||
      pool.cancel(resource) != CommandResourceStatus::not_found ||
      pool.acquire(12U, 1U, &resource) != CommandResourceStatus::success ||
      pool.submit(resource, 1U) != CommandResourceStatus::success ||
      pool.cancel(resource) != CommandResourceStatus::busy ||
      pool.recycle(12U, 0U) != CommandResourceStatus::invalid_timeline) {
    return false;
  }
  return true;
}

} // namespace

int main() {
  const bool ok = fifo_and_cross_stream_dependencies() && negative_paths() &&
                  command_resources_recycle_only_after_completion() &&
                  command_resource_cancel_is_bounded() &&
                  queue_submission_ledger_is_transactional();
  std::printf("vulkan stream graph: %s\n", ok ? "pass" : "fail");
  return ok ? 0 : 1;
}
