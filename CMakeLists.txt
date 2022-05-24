cmake_minimum_required(VERSION 3.16)

if(EXISTS "${CMAKE_CURRENT_BINARY_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "In-source builds are not allowed, please create a 'build' subfolder and use `cmake ..` inside it.\n"
        "NOTE: cmake will now create CMakeCache.txt and CMakeFiles/*.\n"
        "You must delete them, or cmake will refuse to work.")
endif()

option(USE_CODE_TIDY "Use code static analysis on build" ON)

project(rtsp-cam
    VERSION 1.0.0
    LANGUAGES CXX)

if("${CMAKE_PROJECT_NAME}" STREQUAL "${PROJECT_NAME}")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GStreamer REQUIRED IMPORTED_TARGET gstreamer-rtsp-server-1.0 gstreamer-app-1.0)

if(DEFINED ENV{SDKTARGETSYSROOT})
  message(VERBOSE "Detected cross-compile SDK, fixing glib-2.0 include path")
  include_directories($ENV{SDKTARGETSYSROOT}/usr/lib/glib-2.0/include/)
endif()

add_executable(${PROJECT_NAME}
    src/CameraManager.cpp
    src/CameraManager.h
    src/EncodingPipeline.cpp
    src/EncodingPipeline.h
    src/IFrameProducer.h
    src/ImageWriter.cpp
    src/ImageWriter.h
    src/IStreamConsumer.h
    src/main.cpp
    src/StreamingServer.cpp
    src/StreamingServer.h
    src/StreamRecorder.cpp
    src/StreamRecorder.h)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
target_include_directories(
  ${PROJECT_NAME} PUBLIC ${GStreamer_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${GStreamer_LIBRARIES})

if(USE_CODE_TIDY)
    find_program(CLANG_TIDY_EXE
        NAMES clang-tidy-13 clang-tidy-12 clang-tidy
        DOC "Path to clang-tidy executable")
    if(CLANG_TIDY_EXE)
        message(STATUS "Using clang-tidy from ${CLANG_TIDY_EXE}")
        set_target_properties(${PROJECT_NAME} PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
    else()
        message(WARNING "clang-tidy not found, please install tool and relaunch configuration")
    endif()
endif()
