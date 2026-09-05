#ifndef METAFLUX_BACKEND_CPU_PLACEMENT_HPP
#define METAFLUX_BACKEND_CPU_PLACEMENT_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace metaflux::backend::cpu {

enum class PlacementError : std::uint32_t {
  None,
  NoEffectiveCpu,
  NoEffectiveMemoryNode,
  ExplicitPinLost,
  InvalidTopology,
  System,
};

[[nodiscard]] std::string_view placement_error_name(PlacementError error) noexcept;

struct PlacementPaths final {
  std::filesystem::path proc_self_cgroup = "/proc/self/cgroup";
  std::filesystem::path proc_self_mountinfo = "/proc/self/mountinfo";
  std::filesystem::path sys_cpu_root = "/sys/devices/system/cpu";
  std::filesystem::path sys_node_root = "/sys/devices/system/node";
  std::optional<std::filesystem::path> cgroup_directory;
  std::optional<std::vector<std::uint32_t>> affinity_override;
};

struct PlacementPathsResult final {
  std::optional<PlacementPaths> paths;
  PlacementError error = PlacementError::None;
  std::string diagnostic;

  [[nodiscard]] bool ok() const noexcept { return paths.has_value(); }
};

struct PlacementPolicy final {
  std::optional<std::uint32_t> explicit_cpu;
  bool allow_cross_node_stealing = false;
};

struct PhysicalCorePlacement final {
  std::uint32_t package_id = 0;
  std::uint32_t core_id = 0;
  std::uint32_t numa_node = 0;
  std::vector<std::uint32_t> sysfs_siblings;
  std::vector<std::uint32_t> effective_siblings;

  bool operator==(const PhysicalCorePlacement&) const = default;
};

struct NumaWorkerPoolPlacement final {
  std::uint32_t numa_node = 0;
  std::vector<std::uint32_t> worker_cpus;

  bool operator==(const NumaWorkerPoolPlacement&) const = default;
};

struct PlacementSnapshot final {
  std::uint64_t generation = 0;
  std::vector<std::uint32_t> sched_affinity;
  std::vector<std::uint32_t> online_cpus;
  std::vector<std::uint32_t> cpuset_cpus;
  std::vector<std::uint32_t> effective_cpus;
  std::vector<std::uint32_t> online_mems;
  std::vector<std::uint32_t> cpuset_mems;
  std::vector<std::uint32_t> effective_mems;
  std::vector<PhysicalCorePlacement> physical_cores;
  std::optional<PhysicalCorePlacement> reserved_control_core;
  std::vector<NumaWorkerPoolPlacement> pools;
  std::optional<std::uint32_t> explicit_cpu;
  bool cross_node_stealing = false;
  // Revalidation inputs: last-write-time and size of every topology file the
  // full discovery consumed, captured when the snapshot was produced. Filled
  // by discover_cpu_placement; used by placement_snapshot_current.
  std::map<std::filesystem::path, std::filesystem::file_time_type> file_identities;
  std::map<std::filesystem::path, std::uintmax_t> file_sizes;

  [[nodiscard]] std::size_t worker_count() const noexcept;
};

struct PlacementResult final {
  std::optional<PlacementSnapshot> snapshot;
  PlacementError error = PlacementError::System;
  std::string diagnostic;

  [[nodiscard]] bool ok() const noexcept { return snapshot.has_value(); }
};

[[nodiscard]] PlacementResult discover_cpu_placement(const PlacementPaths& paths = {},
                                                     const PlacementPolicy& policy = {});

// Revalidation form of discovery used on kernel boundaries: cheap identity
// inputs only (sched_getaffinity plus stat metadata of the topology files the
// full discovery reads) decide whether the previous snapshot still holds. No
// snapshot is constructed and no topology file content is parsed unless the
// identity changed; callers then re-run plain discovery.
[[nodiscard]] bool placement_snapshot_current(const PlacementPaths& paths,
                                              const PlacementPolicy& policy,
                                              const PlacementSnapshot& previous);

[[nodiscard]] PlacementPathsResult
placement_paths_for_topology_root(std::optional<std::string_view> topology_root);
[[nodiscard]] PlacementPathsResult placement_paths_from_environment();

} // namespace metaflux::backend::cpu

#endif
