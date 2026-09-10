# Keep one CMake directory scope for cross-component qualification: include()
# preserves existing source paths, executable locations and test working dirs.
find_package(Python3 REQUIRED COMPONENTS Interpreter)

include(contract/registration.cmake)
include(kernel/registration.cmake)
include(cmake/registration.cmake)
include(architecture/registration.cmake)
include(lifecycle/registration.cmake)
include(release/registration.cmake)
include(compatibility/registration.cmake)
include(performance/registration.cmake)
