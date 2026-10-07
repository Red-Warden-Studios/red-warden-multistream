cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.cpp
    src/disk-probe.cpp
    src/disk-probe.hpp
    src/run-generation.hpp
    src/mic-test.cpp
    src/mic-test.hpp
    src/multistream-probe.cpp
    src/multistream-probe.hpp
    src/obs-probe.cpp
    src/obs-probe.hpp
    src/preflight-controller.cpp
    src/preflight-controller.hpp
    src/preflight-settings.cpp
    src/preflight-settings.hpp
    src/preflight-dock.cpp
    src/preflight-dock.hpp
    src/preflight-eval.cpp
    src/preflight-eval.hpp
    src/settings-dialog.cpp
    src/settings-dialog.hpp
    src/update-check.cpp
    src/update-check.hpp
    src/update-manifest.cpp
    src/update-manifest.hpp
    src/obs-websocket-api.h
)

# Unit tests (no OBS). Run: build_x64\<config>\rws-preflight-update-test.exe
option(BUILD_PREFLIGHT_TESTS "Build Red Warden Pre-Flight unit tests" ON)
if(BUILD_PREFLIGHT_TESTS)
  add_executable(rws-preflight-update-test tests/update-check-test.cpp src/update-manifest.cpp)
  target_include_directories(rws-preflight-update-test PRIVATE src)
  target_link_libraries(rws-preflight-update-test PRIVATE Qt6::Core)

  add_executable(rws-preflight-eval-test tests/preflight-eval-test.cpp src/preflight-eval.cpp)
  target_include_directories(rws-preflight-eval-test PRIVATE src)
  target_link_libraries(rws-preflight-eval-test PRIVATE Qt6::Core)

  add_executable(rws-preflight-disk-test tests/disk-probe-test.cpp src/disk-probe.cpp)
  target_include_directories(rws-preflight-disk-test PRIVATE src)
  target_link_libraries(rws-preflight-disk-test PRIVATE Qt6::Core)

  add_executable(rws-preflight-rungen-test tests/run-generation-test.cpp)
  target_include_directories(rws-preflight-rungen-test PRIVATE src)
endif()

if(WIN32)
  # WinHTTP for the update check; Advapi32 kept in step with Multistream's build
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Advapi32 Winhttp)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
