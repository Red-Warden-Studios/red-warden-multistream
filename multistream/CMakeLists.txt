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
    src/platforms.cpp
    src/platforms.hpp
    src/config.cpp
    src/config.hpp
    src/credentials.cpp
    src/credentials.hpp
    src/output-runner.cpp
    src/output-runner.hpp
    src/edit-dialog.cpp
    src/edit-dialog.hpp
    src/multistream-dock.cpp
    src/multistream-dock.hpp
    src/obs-audio.cpp
    src/obs-audio.hpp
    src/retry-governor.hpp
    src/obs-websocket-api.h
    src/resources.qrc
    src/dock-ui.cpp
    src/dock-ui.hpp
    src/upload-test.cpp
    src/upload-test.hpp
    src/importers.cpp
    src/importers.hpp
    src/import-dialog.cpp
    src/import-dialog.hpp
    src/session-report.cpp
    src/session-report.hpp
    src/report-dialog.cpp
    src/report-dialog.hpp
    src/platform-limits.cpp
    src/platform-limits.hpp
    src/bandwidth-test.cpp
    src/bandwidth-test.hpp
    src/bandwidth-dialog.cpp
    src/bandwidth-dialog.hpp
    src/update-check.cpp
    src/update-check.hpp
    src/update-manifest.cpp
    src/update-manifest.hpp
    src/connection-budget.hpp
    src/server-url.cpp
    src/server-url.hpp
)

# Unit tests for the pure logic (no OBS, no Qt). Run: build_x64\<config>\rws-multistream-tests.exe
option(BUILD_MULTISTREAM_TESTS "Build Red Warden Multistream unit tests" ON)
if(BUILD_MULTISTREAM_TESTS)
  add_executable(rws-multistream-tests tests/retry-governor-test.cpp)
  target_include_directories(rws-multistream-tests PRIVATE src)
  # Standalone upload-speed check: build_x64\<config>\rws-multistream-uploadtest.exe
  add_executable(rws-multistream-uploadtest tests/upload-test-cli.cpp src/upload-test.cpp)
  target_include_directories(rws-multistream-uploadtest PRIVATE src)
  if(WIN32)
    target_link_libraries(rws-multistream-uploadtest PRIVATE Winhttp)
  endif()

  add_executable(rws-multistream-importers-test tests/importers-test.cpp src/importers.cpp)
  target_include_directories(rws-multistream-importers-test PRIVATE src)
  target_link_libraries(rws-multistream-importers-test PRIVATE Qt6::Core)

  add_executable(rws-multistream-session-test tests/session-report-test.cpp src/session-report.cpp)
  target_include_directories(rws-multistream-session-test PRIVATE src)
  target_link_libraries(rws-multistream-session-test PRIVATE Qt6::Core)

  add_executable(rws-multistream-limits-test tests/platform-limits-test.cpp src/platform-limits.cpp)
  target_include_directories(rws-multistream-limits-test PRIVATE src)
  target_link_libraries(rws-multistream-limits-test PRIVATE Qt6::Core)

  add_executable(rws-multistream-update-test tests/update-check-test.cpp src/update-manifest.cpp)
  target_include_directories(rws-multistream-update-test PRIVATE src)
  target_link_libraries(rws-multistream-update-test PRIVATE Qt6::Core)

  add_executable(rws-multistream-serverurl-test tests/server-url-test.cpp src/server-url.cpp)
  target_include_directories(rws-multistream-serverurl-test PRIVATE src)
  target_link_libraries(rws-multistream-serverurl-test PRIVATE Qt6::Core)
endif()

if(WIN32)
  # CredWriteW / CredReadW for stream keys
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Advapi32 Winhttp)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
