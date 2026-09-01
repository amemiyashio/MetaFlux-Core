#include "metaflux/backend/cpu/placement.hpp"

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <sched.h>
#include <set>
#include <sstream>
#include <string_view>
#include <tuple>
#include <utility>

namespace metaflux::backend::cpu {
namespace {

constexpr std::uint32_t kMaximumLinuxId = 1U << 20U;
constexpr std::size_t kMaximumTopologyRootBytes = 4096U;

[[nodiscard]] bool contains_parent_reference(const std::filesystem::path& path) {
  return std::any_of(path.begin(), path.end(),
                     [](const auto& component) { return component == ".."; });
}

struct CpuRecord final {
  std::uint32_t cpu = 0;
  std::uint32_t package = 0;
  std::uint32_t core = 0;
  std::uint32_t node = 0;
  std::vector<std::uint32_t> siblings;
};

[[nodiscard]] PlacementResult failure(PlacementError error, std::string diagnostic) {
  return {.snapshot = std::nullopt, .error = error, .diagnostic = std::move(diagnostic)};
}

[[nodiscard]] std::optional<std::string> read_text(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    return std::nullopt;
  }
  std::ostringstream contents;
  contents << input.rdbuf();
  if (!input.good() && !input.eof()) {
    return std::nullopt;
  }
  return contents.str();
}

[[nodiscard]] std::string_view trim(std::string_view value) {
  while (!value.empty() && (value.front() == ' ' || value.front() == '\t' ||
                            value.front() == '\r' || value.front() == '\n')) {
    value.remove_prefix(1U);
  }
  while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' ||
                            value.back() == '\n')) {
    value.remove_suffix(1U);
  }
  return value;
}

[[nodiscard]] bool parse_id(std::string_view text, std::uint32_t& value) {
  text = trim(text);
  if (text.empty()) {
    return false;
  }
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  return result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
         value <= kMaximumLinuxId;
}

[[nodiscard]] std::optional<std::vector<std::uint32_t>> parse_list(std::string_view text) {
  std::set<std::uint32_t> values;
  text = trim(text);
  if (text.empty()) {
    return std::vector<std::uint32_t>{};
  }
  while (!text.empty()) {
    const auto comma = text.find(',');
    const auto token = trim(text.substr(0, comma));
    const auto dash = token.find('-');
    std::uint32_t first = 0;
    std::uint32_t last = 0;
    if (dash == std::string_view::npos) {
      if (!parse_id(token, first)) {
        return std::nullopt;
      }
      last = first;
    } else if (!parse_id(token.substr(0, dash), first) ||
               !parse_id(token.substr(dash + 1U), last) || first > last) {
      return std::nullopt;
    }
    for (std::uint32_t value = first;; ++value) {
      values.insert(value);
      if (value == last) {
        break;
      }
    }
    if (comma == std::string_view::npos) {
      break;
    }
    text.remove_prefix(comma + 1U);
  }
  return std::vector<std::uint32_t>(values.begin(), values.end());
}

[[nodiscard]] std::vector<std::uint32_t> set_intersection(const std::vector<std::uint32_t>& left,
                                                          const std::vector<std::uint32_t>& right) {
  std::vector<std::uint32_t> result;
  std::set_intersection(left.begin(), left.end(), right.begin(), right.end(),
                        std::back_inserter(result));
  return result;
}

[[nodiscard]] std::optional<std::vector<std::uint32_t>> process_affinity(std::string& diagnostic) {
  for (std::size_t cpu_count = CPU_SETSIZE; cpu_count <= kMaximumLinuxId; cpu_count *= 2U) {
    const auto byte_count = CPU_ALLOC_SIZE(cpu_count);
    cpu_set_t* mask = CPU_ALLOC(cpu_count);
    if (mask == nullptr) {
      diagnostic = "CPU affinity mask allocation failed";
      return std::nullopt;
    }
    CPU_ZERO_S(byte_count, mask);
    if (sched_getaffinity(0, byte_count, mask) == 0) {
      std::vector<std::uint32_t> cpus;
      for (std::size_t cpu = 0; cpu < cpu_count; ++cpu) {
        if (CPU_ISSET_S(cpu, byte_count, mask) != 0) {
          cpus.push_back(static_cast<std::uint32_t>(cpu));
        }
      }
      CPU_FREE(mask);
      return cpus;
    }
    const int error = errno;
    CPU_FREE(mask);
    if (error != EINVAL) {
      diagnostic = std::string("sched_getaffinity failed: ") + std::strerror(error);
      return std::nullopt;
    }
  }
  diagnostic = "sched_getaffinity mask exceeds the supported CPU ID bound";
  return std::nullopt;
}

[[nodiscard]] std::string unescape_mount_field(std::string_view value) {
  std::string result;
  result.reserve(value.size());
  for (std::size_t index = 0; index < value.size(); ++index) {
    if (value[index] == '\\' && index + 3U < value.size() && value[index + 1U] >= '0' &&
        value[index + 1U] <= '7' && value[index + 2U] >= '0' && value[index + 2U] <= '7' &&
        value[index + 3U] >= '0' && value[index + 3U] <= '7') {
      const auto decoded =
          static_cast<char>((value[index + 1U] - '0') * 64 + (value[index + 2U] - '0') * 8 +
                            (value[index + 3U] - '0'));
      result.push_back(decoded);
      index += 3U;
    } else {
      result.push_back(value[index]);
    }
  }
  return result;
}

[[nodiscard]] std::optional<std::filesystem::path>
resolve_cgroup_directory(const PlacementPaths& paths, std::string& diagnostic) {
  if (paths.cgroup_directory.has_value()) {
    return paths.cgroup_directory;
  }
  const auto cgroup_text = read_text(paths.proc_self_cgroup);
  const auto mountinfo_text = read_text(paths.proc_self_mountinfo);
  if (!cgroup_text.has_value() || !mountinfo_text.has_value()) {
    diagnostic = "cgroup v2 membership or mount information cannot be read";
    return std::nullopt;
  }

  std::optional<std::filesystem::path> membership;
  std::istringstream cgroup_lines(*cgroup_text);
  for (std::string line; std::getline(cgroup_lines, line);) {
    if (line.starts_with("0::")) {
      membership = std::filesystem::path(line.substr(3U)).lexically_normal();
      break;
    }
  }
  if (!membership.has_value() || !membership->is_absolute()) {
    diagnostic = "unified cgroup v2 membership is absent";
    return std::nullopt;
  }

  std::istringstream mount_lines(*mountinfo_text);
  for (std::string line; std::getline(mount_lines, line);) {
    std::istringstream fields_stream(line);
    std::vector<std::string> fields;
    for (std::string field; fields_stream >> field;) {
      fields.push_back(std::move(field));
    }
    const auto separator = std::find(fields.begin(), fields.end(), "-");
    if (separator == fields.end() || fields.size() < 7U ||
        static_cast<std::size_t>(separator - fields.begin()) + 1U >= fields.size() ||
        separator[1] != "cgroup2") {
      continue;
    }
    const std::filesystem::path mount_root(unescape_mount_field(fields[3]));
    const std::filesystem::path mount_point(unescape_mount_field(fields[4]));
    auto relative = membership->lexically_relative(mount_root);
    if (relative.empty() && *membership != mount_root) {
      continue;
    }
    if (!relative.empty() && *relative.begin() == "..") {
      continue;
    }
    return (mount_point / relative).lexically_normal();
  }
  diagnostic = "cgroup v2 mount cannot be resolved";
  return std::nullopt;
}

[[nodiscard]] std::optional<std::vector<std::uint32_t>>
read_list_file(const std::filesystem::path& path, std::string& diagnostic) {
  const auto text = read_text(path);
  if (!text.has_value()) {
    diagnostic = path.string() + " cannot be read";
    return std::nullopt;
  }
  auto values = parse_list(*text);
  if (!values.has_value()) {
    diagnostic = path.string() + " contains a malformed Linux list";
  }
  return values;
}

[[nodiscard]] std::optional<std::vector<std::uint32_t>>
read_list_file_up(const std::filesystem::path& start, const std::filesystem::path& filename,
                  const std::filesystem::path& stop_root) {
  for (auto directory = start; !directory.empty() && directory != stop_root;
       directory = directory.parent_path()) {
    const auto path = directory / filename;
    if (auto text = read_text(path); text.has_value()) {
      if (auto values = parse_list(*text); values.has_value()) {
        return values;
      }
    }
  }
  const auto path = stop_root / filename;
  if (auto text = read_text(path); text.has_value()) {
    if (auto values = parse_list(*text); values.has_value()) {
      return values;
    }
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<std::uint32_t> read_id_file(const std::filesystem::path& path,
                                                        std::string& diagnostic) {
  const auto text = read_text(path);
  std::uint32_t value = 0;
  if (!text.has_value() || !parse_id(*text, value)) {
    diagnostic = path.string() + " does not contain a valid non-negative ID";
    return std::nullopt;
  }
  return value;
}

[[nodiscard]] std::map<std::uint32_t, std::uint32_t>
cpu_to_node_map(const PlacementPaths& paths, const std::vector<std::uint32_t>& online_nodes,
                const std::vector<std::uint32_t>& online_cpus, std::string& diagnostic) {
  std::map<std::uint32_t, std::uint32_t> result;
  for (const auto node : online_nodes) {
    const auto cpus = read_list_file(
        paths.sys_node_root / ("node" + std::to_string(node)) / "cpulist", diagnostic);
    if (!cpus.has_value()) {
      return {};
    }
    for (const auto cpu : *cpus) {
      if (!result.emplace(cpu, node).second) {
        diagnostic = "one CPU appears in multiple NUMA-node cpulists";
        return {};
      }
    }
  }
  if (online_nodes.size() == 1U) {
    for (const auto cpu : online_cpus) {
      result.try_emplace(cpu, online_nodes.front());
    }
  }
  return result;
}

} // namespace

PlacementPathsResult
placement_paths_for_topology_root(std::optional<std::string_view> topology_root) {
  if (!topology_root.has_value()) {
    return {.paths = PlacementPaths{}, .error = PlacementError::None, .diagnostic = {}};
  }
  if (topology_root->empty()) {
    return {.paths = std::nullopt,
            .error = PlacementError::InvalidTopology,
            .diagnostic = "METAFLUX_CPU_TOPOLOGY_ROOT must not be empty"};
  }
  if (topology_root->size() > kMaximumTopologyRootBytes ||
      topology_root->find('\0') != std::string_view::npos) {
    return {.paths = std::nullopt,
            .error = PlacementError::InvalidTopology,
            .diagnostic =
                "METAFLUX_CPU_TOPOLOGY_ROOT must be a bounded absolute path without '..'"};
  }

  const std::filesystem::path root{std::string(*topology_root)};
  if (!root.is_absolute() || contains_parent_reference(root)) {
    return {.paths = std::nullopt,
            .error = PlacementError::InvalidTopology,
            .diagnostic =
                "METAFLUX_CPU_TOPOLOGY_ROOT must be a bounded absolute path without '..'"};
  }
  const auto normalized = root.lexically_normal();
  if (normalized == normalized.root_path()) {
    return {.paths = std::nullopt,
            .error = PlacementError::InvalidTopology,
            .diagnostic =
                "METAFLUX_CPU_TOPOLOGY_ROOT must be a bounded absolute path without '..'"};
  }

  PlacementPaths paths;
  paths.sys_cpu_root = normalized / "cpu";
  paths.sys_node_root = normalized / "node";
  paths.cgroup_directory = normalized / "cgroup";
  return {.paths = std::move(paths), .error = PlacementError::None, .diagnostic = {}};
}

PlacementPathsResult placement_paths_from_environment() {
  const char* topology_root = std::getenv("METAFLUX_CPU_TOPOLOGY_ROOT");
  return placement_paths_for_topology_root(
      topology_root == nullptr ? std::nullopt : std::optional<std::string_view>(topology_root));
}

std::string_view placement_error_name(PlacementError error) noexcept {
  switch (error) {
  case PlacementError::None:
    return "MF_CPU_PLACEMENT_SUCCESS";
  case PlacementError::NoEffectiveCpu:
    return "MF_CPU_NO_EFFECTIVE_CPU";
  case PlacementError::NoEffectiveMemoryNode:
    return "MF_CPU_NO_EFFECTIVE_MEMORY_NODE";
  case PlacementError::ExplicitPinLost:
    return "MF_CPU_EXPLICIT_PIN_LOST";
  case PlacementError::InvalidTopology:
    return "MF_CPU_INVALID_TOPOLOGY";
  case PlacementError::System:
    return "MF_CPU_PLACEMENT_SYSTEM_ERROR";
  }
  return "MF_CPU_PLACEMENT_UNKNOWN";
}

std::size_t PlacementSnapshot::worker_count() const noexcept {
  std::size_t result = 0;
  for (const auto& pool : pools) {
    result += pool.worker_cpus.size();
  }
  return result;
}

PlacementResult discover_cpu_placement(const PlacementPaths& paths, const PlacementPolicy& policy) {
  if (policy.allow_cross_node_stealing) {
    return failure(PlacementError::InvalidTopology,
                   "cross-node stealing is outside the decision-0015 default policy");
  }

  std::string diagnostic;
  auto affinity = paths.affinity_override;
  if (!affinity.has_value()) {
    affinity = process_affinity(diagnostic);
  }
  if (!affinity.has_value()) {
    return failure(PlacementError::System, std::move(diagnostic));
  }
  std::sort(affinity->begin(), affinity->end());
  affinity->erase(std::unique(affinity->begin(), affinity->end()), affinity->end());

  const auto online_cpus = read_list_file(paths.sys_cpu_root / "online", diagnostic);
  if (!online_cpus.has_value()) {
    return failure(PlacementError::System, std::move(diagnostic));
  }

  const auto cgroup_directory = resolve_cgroup_directory(paths, diagnostic);
  if (!cgroup_directory.has_value()) {
    return failure(PlacementError::System, std::move(diagnostic));
  }
  const auto cgroup2_mount = [&]() -> std::filesystem::path {
    if (paths.cgroup_directory.has_value()) {
      return *paths.cgroup_directory;
    }
    return std::filesystem::path("/");
  }();
  auto cpuset_cpus = read_list_file_up(*cgroup_directory, "cpuset.cpus.effective", cgroup2_mount);
  if (!cpuset_cpus.has_value()) {
    cpuset_cpus = *affinity;
  }

  auto effective_cpus = set_intersection(*affinity, *online_cpus);
  effective_cpus = set_intersection(effective_cpus, *cpuset_cpus);
  if (effective_cpus.empty()) {
    return failure(PlacementError::NoEffectiveCpu,
                   "sched affinity, online CPUs, and cpuset.cpus.effective have no intersection");
  }

  std::vector<std::uint32_t> online_mems;
  std::error_code node_online_error;
  const bool has_node_online =
      std::filesystem::exists(paths.sys_node_root / "online", node_online_error);
  if (node_online_error) {
    return failure(PlacementError::System,
                   "NUMA online-node state cannot be inspected: " + node_online_error.message());
  }
  if (has_node_online) {
    const auto parsed = read_list_file(paths.sys_node_root / "online", diagnostic);
    if (!parsed.has_value()) {
      return failure(PlacementError::System, std::move(diagnostic));
    }
    online_mems = *parsed;
  } else {
    online_mems = {0U};
  }
  auto cpuset_mems = read_list_file_up(*cgroup_directory, "cpuset.mems.effective", cgroup2_mount);
  if (!cpuset_mems.has_value()) {
    cpuset_mems = online_mems;
  }
  const auto effective_mems = set_intersection(online_mems, *cpuset_mems);
  if (effective_mems.empty()) {
    return failure(PlacementError::NoEffectiveMemoryNode,
                   "online NUMA nodes and cpuset.mems.effective have no intersection");
  }

  auto cpu_nodes = cpu_to_node_map(paths, online_mems, *online_cpus, diagnostic);
  if (cpu_nodes.empty()) {
    return failure(PlacementError::InvalidTopology,
                   diagnostic.empty() ? "NUMA CPU topology is empty" : std::move(diagnostic));
  }

  std::vector<CpuRecord> records;
  records.reserve(effective_cpus.size());
  for (const auto cpu : effective_cpus) {
    const auto node = cpu_nodes.find(cpu);
    if (node == cpu_nodes.end()) {
      return failure(PlacementError::InvalidTopology,
                     "effective CPU " + std::to_string(cpu) + " has no NUMA node");
    }
    const auto topology = paths.sys_cpu_root / ("cpu" + std::to_string(cpu)) / "topology";
    const auto package = read_id_file(topology / "physical_package_id", diagnostic);
    const auto core = read_id_file(topology / "core_id", diagnostic);
    const auto siblings = read_list_file(topology / "thread_siblings_list", diagnostic);
    if (!package.has_value() || !core.has_value() || !siblings.has_value()) {
      return failure(PlacementError::InvalidTopology, std::move(diagnostic));
    }
    if (!std::binary_search(siblings->begin(), siblings->end(), cpu)) {
      return failure(PlacementError::InvalidTopology,
                     "thread_siblings_list does not contain its CPU");
    }
    records.push_back({.cpu = cpu,
                       .package = *package,
                       .core = *core,
                       .node = node->second,
                       .siblings = *siblings});
  }

  std::map<std::vector<std::uint32_t>, PhysicalCorePlacement> grouped;
  for (const auto& record : records) {
    auto [entry, inserted] = grouped.try_emplace(
        record.siblings, PhysicalCorePlacement{.package_id = record.package,
                                               .core_id = record.core,
                                               .numa_node = record.node,
                                               .sysfs_siblings = record.siblings,
                                               .effective_siblings = {}});
    if (!inserted &&
        (entry->second.package_id != record.package || entry->second.core_id != record.core ||
         entry->second.numa_node != record.node)) {
      return failure(PlacementError::InvalidTopology,
                     "SMT siblings disagree on physical-core or NUMA identity");
    }
    entry->second.effective_siblings.push_back(record.cpu);
  }

  PlacementSnapshot snapshot;
  snapshot.sched_affinity = *affinity;
  snapshot.online_cpus = *online_cpus;
  snapshot.cpuset_cpus = *cpuset_cpus;
  snapshot.effective_cpus = effective_cpus;
  snapshot.online_mems = online_mems;
  snapshot.cpuset_mems = *cpuset_mems;
  snapshot.effective_mems = effective_mems;
  snapshot.explicit_cpu = policy.explicit_cpu;
  snapshot.cross_node_stealing = false;
  for (auto& [siblings, core] : grouped) {
    static_cast<void>(siblings);
    std::sort(core.effective_siblings.begin(), core.effective_siblings.end());
    snapshot.physical_cores.push_back(std::move(core));
  }
  std::sort(snapshot.physical_cores.begin(), snapshot.physical_cores.end(),
            [](const auto& left, const auto& right) {
              return std::tuple{left.effective_siblings.front(), left.package_id, left.core_id} <
                     std::tuple{right.effective_siblings.front(), right.package_id, right.core_id};
            });

  const auto memory_allowed = [&](const PhysicalCorePlacement& core) {
    return std::binary_search(effective_mems.begin(), effective_mems.end(), core.numa_node);
  };
  std::vector<PhysicalCorePlacement> schedulable_cores;
  std::copy_if(snapshot.physical_cores.begin(), snapshot.physical_cores.end(),
               std::back_inserter(schedulable_cores), memory_allowed);
  if (schedulable_cores.empty()) {
    return failure(PlacementError::NoEffectiveMemoryNode,
                   "no effective physical core belongs to an effective memory node");
  }

  std::map<std::uint32_t, std::vector<std::uint32_t>> workers_by_node;
  if (policy.explicit_cpu.has_value()) {
    if (!std::binary_search(effective_cpus.begin(), effective_cpus.end(), *policy.explicit_cpu)) {
      return failure(PlacementError::ExplicitPinLost,
                     "explicit CPU pin is outside the effective CPU set");
    }
    const auto core = std::find_if(
        schedulable_cores.begin(), schedulable_cores.end(), [&](const auto& candidate) {
          return std::binary_search(candidate.effective_siblings.begin(),
                                    candidate.effective_siblings.end(), *policy.explicit_cpu);
        });
    if (core == schedulable_cores.end()) {
      return failure(PlacementError::ExplicitPinLost,
                     "explicit CPU pin has no effective local memory node");
    }
    workers_by_node[core->numa_node].push_back(*policy.explicit_cpu);
  } else {
    std::size_t first_worker_core = 0;
    if (schedulable_cores.size() >= 4U) {
      snapshot.reserved_control_core = schedulable_cores.front();
      first_worker_core = 1U;
    }
    for (std::size_t index = first_worker_core; index < schedulable_cores.size(); ++index) {
      const auto& core = schedulable_cores[index];
      workers_by_node[core.numa_node].push_back(core.effective_siblings.front());
    }
  }

  for (auto& [node, workers] : workers_by_node) {
    snapshot.pools.push_back({.numa_node = node, .worker_cpus = std::move(workers)});
  }
  if (snapshot.worker_count() == 0U) {
    return failure(PlacementError::NoEffectiveCpu, "control-core reservation left no worker CPU");
  }
  return {.snapshot = std::move(snapshot), .error = PlacementError::None, .diagnostic = {}};
}

} // namespace metaflux::backend::cpu
