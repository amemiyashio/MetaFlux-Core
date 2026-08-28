{ lib }:
let
  root = ../..;
  fileset = lib.fileset;

  select = paths: fileset.unions (map (path: fileset.maybeMissing (root + "/${path}")) paths);

  common = select [
    "CMakeLists.txt"
    "cmake"
    "contracts/CMakeLists.txt"
    "toolchains/compiler-epoch-1.json"
  ];

  runtimeFiles = fileset.union common (select [
    "contracts/plugin/backend/v1"
    "contracts/protocol/client/v1"
    "runtime"
  ]);
  providerFiles = fileset.union common (select [
    "contracts/protocol/client/v1"
    "runtime/CMakeLists.txt"
    "runtime/client"
    "plugins/CMakeLists.txt"
    "plugins/compat/CMakeLists.txt"
    "plugins/compat/cuda/CMakeLists.txt"
    "plugins/compat/cuda/abi"
    "plugins/compat/cuda/management"
  ]);
  daemonFiles = fileset.union common (select [
    "contracts/plugin/backend/v1"
    "contracts/protocol/client/v1"
    "runtime/CMakeLists.txt"
    "runtime/core"
    "compiler"
    "plugins/CMakeLists.txt"
    "plugins/backend/CMakeLists.txt"
    "plugins/backend/cpu"
    "plugins/compat/CMakeLists.txt"
    "plugins/compat/cuda/CMakeLists.txt"
    "plugins/compat/cuda/compiler"
    "services"
  ]);
  testFiles = fileset.unions [
    providerFiles
    daemonFiles
    (select [ "tests" ])
  ];
  formatFiles = fileset.union testFiles (select [
    ".clang-format"
    "flake.nix"
    "kernel"
    "nix"
    "plugins"
    "tools"
    "transports"
  ]);
  agentRecordsFiles = select [
    "README.md"
    ".claude"
    ".githooks"
    "AGENTS.md"
    "CLAUDE.md"
    "agent"
    "compiler"
    "contracts"
    "docs"
    "kernel"
    "nix"
    "packaging"
    "plugins"
    "runtime"
    "services"
    "tests"
    "toolchains"
    "tools/README.md"
    "tools/check-agent-records.py"
    "tools/check-component-graph.py"
    "tools/test-check-agent-records.py"
    "transports"
  ];

  toSource =
    selected:
    fileset.toSource {
      inherit root;
      fileset = selected;
    };
in
{
  runtime = toSource runtimeFiles;
  provider = toSource providerFiles;
  daemon = toSource daemonFiles;
  tests = toSource testFiles;
  format = toSource formatFiles;
  agentRecords = toSource agentRecordsFiles;
}
