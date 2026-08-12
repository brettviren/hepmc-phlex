cmake_minimum_required(VERSION 3.25)
project(hepmc_phlex VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# WORKAROUND (remove per ddm-ovy once Framework-R-D/phlex#656 ships): phlexConfig
# does not find_dependency() its transitive deps, but phlexTargets references the
# spdlog::spdlog, TBB::tbb and fmt::fmt imported targets.  Create them BEFORE
# find_package(phlex) or its Targets file errors ("target not found").
find_package(Boost REQUIRED COMPONENTS json)
find_package(TBB REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(phlex REQUIRED)
find_package(HepMC3 REQUIRED)

add_compile_options(-Wall -Wpedantic)

# ---------------------------------------------------------------------------
# hepmc_phlex — shared library (the GenEventGun callable and the GenEvent product).
# Depends only on phlex + HepMC3 (+ Boost via phlex); no other domain packages.
# ---------------------------------------------------------------------------
add_library(hepmc_phlex SHARED
  hepmc_phlex/GenEventGun.cpp
  hepmc_phlex/GenEventSequence.cpp
)

target_include_directories(hepmc_phlex
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(hepmc_phlex
  PUBLIC
    HepMC3::HepMC3
    phlex::core
    Boost::json
)

# ---------------------------------------------------------------------------
# Phlex MODULE target (dlopen-only): the GenEventGun source/provider node.
# ---------------------------------------------------------------------------
add_library(hmp_gen_event_gun MODULE modules/gen_event_gun.cpp)
target_link_libraries(hmp_gen_event_gun PRIVATE
  hepmc_phlex
  phlex::source
)
target_compile_options(hmp_gen_event_gun PRIVATE -UNDEBUG)

# The multi-event, multi-vertex sequence provider.
add_library(hmp_gen_event_sequence MODULE modules/gen_event_sequence.cpp)
target_link_libraries(hmp_gen_event_sequence PRIVATE
  hepmc_phlex
  phlex::source
)
target_compile_options(hmp_gen_event_sequence PRIVATE -UNDEBUG)

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS hepmc_phlex
  EXPORT hepmc_phlexTargets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

install(DIRECTORY hepmc_phlex/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/hepmc_phlex"
  FILES_MATCHING PATTERN "*.hpp"
)

install(TARGETS hmp_gen_event_gun
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

install(TARGETS hmp_gen_event_sequence
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

install(EXPORT hepmc_phlexTargets
  FILE hepmc_phlexTargets.cmake
  NAMESPACE hepmc_phlex::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/hepmc_phlex"
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/hepmc_phlexConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/hepmc_phlexConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/hepmc_phlexConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/hepmc_phlex"
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/hepmc_phlexConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/hepmc_phlexConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/hepmc_phlex"
)
