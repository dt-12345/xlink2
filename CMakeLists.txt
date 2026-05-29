CMAKE_MINIMUM_REQUIRED(VERSION 3.18)

project(XLINK2 CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(XLINK_DEBUG "Debug" OFF)

if (XLINK_DEBUG)
    # I supposed I could do RelWithDebInfo but we can use the define in code for things
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -g3 -DNDEBUG")
endif()

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
endif()

if (MINGW)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
endif()

add_subdirectory(lib/fmt)

add_executable(xlink
    src/asset/blend.cpp
    src/asset/calltable.cpp
    src/asset/grid.cpp
    src/asset/handle.cpp
    src/asset/switch.cpp

    src/common/args.cpp
    src/common/error.cpp
    src/common/file.cpp
    src/common/guid.cpp
    src/common/hash.cpp
    src/common/reader.cpp

    src/db/db.cpp
    src/db/load.cpp
    src/db/parse.cpp
    src/db/pdt.cpp
    src/db/save.cpp
    src/db/text.cpp
    src/db/user.cpp
    src/db/username.cpp

    src/param/arrange.cpp
    src/param/curve.cpp
    src/param/param.cpp
    src/param/random.cpp

    src/property/property.cpp

    src/trigger/action.cpp
    src/trigger/always.cpp
    src/trigger/property.cpp

    src/main.cpp
)

# MSVC and Clang aren't supported rn but they're here bc I just copy/paste this from old projects lol
if (MSVC)
    target_compile_options(xlink PRIVATE /W4 /wd4244 /wd4127 /Zc:__cplusplus)
else()
    target_compile_options(xlink PRIVATE -Wall -Wextra -Werror -freflection)
endif()

if (XLINK_DEBUG)
    target_compile_definitions(xlink PRIVATE XLINK_DEBUG)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
  target_link_libraries(xlink PRIVATE stdc++exp)
endif()

target_link_libraries(xlink PRIVATE fmt)
target_include_directories(xlink PUBLIC src/include/)
target_include_directories(xlink PRIVATE lib/fmt/include/)