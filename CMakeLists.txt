cmake_minimum_required(VERSION 3.24)
project(BunnyHopperOfSkyrim VERSION 1.1.0.0 LANGUAGES CXX)

if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "BunnyHopperOfSkyrim requires MSVC x64")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)

if(DEFINED BHOS_TOOLCHAIN_ROOT AND EXISTS "${BHOS_TOOLCHAIN_ROOT}")
elseif(DEFINED ENV{SKSE_TOOLCHAIN_ROOT} AND EXISTS "$ENV{SKSE_TOOLCHAIN_ROOT}")
  set(BHOS_TOOLCHAIN_ROOT "$ENV{SKSE_TOOLCHAIN_ROOT}" CACHE PATH "Shared SKSE toolchain root")
else()
  get_filename_component(BHOS_DEFAULT_TOOLCHAIN "${CMAKE_CURRENT_SOURCE_DIR}/../_toolchain" ABSOLUTE)
  set(BHOS_TOOLCHAIN_ROOT "${BHOS_DEFAULT_TOOLCHAIN}" CACHE PATH "Shared SKSE toolchain root")
endif()

if(DEFINED BHOS_COMMONLIB_ROOT AND EXISTS "${BHOS_COMMONLIB_ROOT}")
elseif(DEFINED ENV{SKSE_COMMONLIB_SOURCE_ROOT} AND EXISTS "$ENV{SKSE_COMMONLIB_SOURCE_ROOT}")
  set(BHOS_COMMONLIB_ROOT "$ENV{SKSE_COMMONLIB_SOURCE_ROOT}" CACHE PATH "CommonLibSSE-NG v16.11.1 source root")
else()
  set(BHOS_COMMONLIB_ROOT "${BHOS_TOOLCHAIN_ROOT}/CommonLibSSE-NG-v16.11.1" CACHE PATH "CommonLibSSE-NG v16.11.1 source root")
endif()

set(BHOS_DEPS_ROOT "${BHOS_TOOLCHAIN_ROOT}/skyrim_1_7_104_compat/deps-msvc1951" CACHE PATH "Prebuilt spdlog/fmt/rapidcsv prefix")
set(BHOS_VCPKG_ROOT "${BHOS_TOOLCHAIN_ROOT}/vcpkg-installed/x64-windows-static-md" CACHE PATH "vcpkg installed triplet root")
set(BHOS_JSON_INCLUDE "${BHOS_TOOLCHAIN_ROOT}/nlohmann-json-3.11.3/include" CACHE PATH "nlohmann/json include directory")
set(BHOS_SMF_INCLUDE "${BHOS_TOOLCHAIN_ROOT}/SKSE-Menu-Framework-3-API" CACHE PATH "SKSE Menu Framework API include directory")

list(PREPEND CMAKE_PREFIX_PATH "${BHOS_DEPS_ROOT}" "${BHOS_VCPKG_ROOT}")
set(RAPIDCSV_INCLUDE_DIRS "${BHOS_DEPS_ROOT}/include" CACHE PATH "" FORCE)

add_compile_options(/EHsc /utf-8)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" BHOS_PRIV_SRC)
file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}" BHOS_PRIV_BIN)
file(TO_CMAKE_PATH "${BHOS_TOOLCHAIN_ROOT}" BHOS_PRIV_TC)
get_filename_component(BHOS_PRIV_WS "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
file(TO_CMAKE_PATH "${BHOS_PRIV_WS}" BHOS_PRIV_WS)
set(BHOS_PATHMAP_ROOT "X:/workspace" CACHE STRING "Neutral deterministic debug source prefix")
add_compile_options(
  /experimental:deterministic
  "/pathmap:${BHOS_PRIV_SRC}=${BHOS_PATHMAP_ROOT}/source"
  "/pathmap:${BHOS_PRIV_BIN}=${BHOS_PATHMAP_ROOT}/build"
  "/pathmap:${BHOS_PRIV_TC}=${BHOS_PATHMAP_ROOT}/dependencies"
  "/pathmap:${BHOS_PRIV_WS}=${BHOS_PATHMAP_ROOT}"
)
if(DEFINED ENV{USERPROFILE})
  file(TO_CMAKE_PATH "$ENV{USERPROFILE}" BHOS_PRIV_USER)
  add_compile_options("/pathmap:${BHOS_PRIV_USER}=X:/user")
endif()
add_link_options(/DEBUG:FULL /Brepro /INCREMENTAL:NO)

set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(COMMONLIB_PREBUILT OFF CACHE BOOL "" FORCE)
set(SKSE_SUPPORT_XBYAK OFF CACHE BOOL "" FORCE)
set(SKSE_SUPPORT_PATCH_SAFETY ON CACHE BOOL "" FORCE)

add_subdirectory("${BHOS_COMMONLIB_ROOT}" "${CMAKE_BINARY_DIR}/commonlib" EXCLUDE_FROM_ALL)

file(GLOB BHOS_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB BHOS_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

add_library(BunnyHopperOfSkyrim SHARED
  ${BHOS_SOURCES}
  ${BHOS_HEADERS}
  version.rc
)

target_link_libraries(BunnyHopperOfSkyrim PRIVATE CommonLibSSE::CommonLibSSE)
target_include_directories(BunnyHopperOfSkyrim PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(BunnyHopperOfSkyrim SYSTEM PRIVATE "${BHOS_JSON_INCLUDE}" "${BHOS_SMF_INCLUDE}")
target_compile_definitions(BunnyHopperOfSkyrim PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(BunnyHopperOfSkyrim PRIVATE /W4)
target_precompile_headers(BunnyHopperOfSkyrim PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/PCH.h")
target_link_options(BunnyHopperOfSkyrim PRIVATE /PDBALTPATH:BunnyHopperOfSkyrim.pdb)
set_target_properties(BunnyHopperOfSkyrim PROPERTIES PREFIX "" OUTPUT_NAME BunnyHopperOfSkyrim)

add_executable(bunnyhopper-tests EXCLUDE_FROM_ALL
  tests/bunnyhopper-tests.cpp
  src/Animations.cpp
  src/Settings.cpp
)
target_link_libraries(bunnyhopper-tests PRIVATE CommonLibSSE::CommonLibSSE)
target_include_directories(bunnyhopper-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(bunnyhopper-tests SYSTEM PRIVATE "${BHOS_JSON_INCLUDE}")
target_compile_definitions(bunnyhopper-tests PRIVATE ENABLE_COMMONLIBSSE_TESTING=1 NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(bunnyhopper-tests PRIVATE /W4 /WX)
target_link_options(bunnyhopper-tests PRIVATE /PDBALTPATH:bunnyhopper-tests.pdb)
