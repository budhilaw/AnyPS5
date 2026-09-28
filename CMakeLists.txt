cmake_minimum_required(VERSION 3.20)
project(Relinker CXX)

option(BUILD_TESTING "Build and register relinker tests" OFF)
if(BUILD_TESTING)
    enable_testing()
endif()

option(ANYPS5_ENABLE_SPIRV_TOOLS "Enable SPIRV-Tools validation and optimization" OFF)

# The guest x86-64 ABI runtime (C++ exception runtime built on libstdc++ internals, SysV va_list
# marshalling, x87 long double) only compiles on an x86-64 host with GCC. Other hosts get a
# host-only libc that serves the native libraries and tests but cannot execute guest code.
# On macOS the x86-64 build (Apple Clang, libc++) also carries the guest ABI: the runtime uses
# Itanium RTTI layouts spelled out for libc++ and Mach-O guest metadata (see docs/TechnicalDebt.md).
set(ANYPS5_HOST_X86_64 OFF)
if(CMAKE_OSX_ARCHITECTURES)
    if(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
        set(ANYPS5_HOST_X86_64 ON)
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64)$")
    set(ANYPS5_HOST_X86_64 ON)
endif()
if(ANYPS5_HOST_X86_64 AND (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR (APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")))
    set(ANYPS5_GUEST_ABI ON)
else()
    set(ANYPS5_GUEST_ABI OFF)
    message(STATUS "Guest x86-64 ABI runtime disabled: host is ${CMAKE_SYSTEM_PROCESSOR}/${CMAKE_CXX_COMPILER_ID} (needs x86-64 + GCC)")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT APPLE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

if(APPLE)
    # One SDL copy for every library: static copies would each register their own Cocoa
    # application classes, and AppKit rejects duplicates.
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(ANYPS5_SDL_TARGET SDL2)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(ANYPS5_SDL_TARGET SDL2-static)
endif()
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_VIDEO ON CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
if(APPLE)
    set(SDL_X11 OFF CACHE BOOL "" FORCE)
    set(SDL_COCOA ON CACHE BOOL "" FORCE)
    set(SDL_METAL ON CACHE BOOL "" FORCE)
else()
    set(SDL_X11 ON CACHE BOOL "" FORCE)
endif()
set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN ON CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_STATIC_PIC ON CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/SDL2)

add_subdirectory(core/shader/recompiler)
add_subdirectory(core/libs)
add_subdirectory(core/relinker)
