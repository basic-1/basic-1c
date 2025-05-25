#[[
 STM8 intermediate code compiler
 Copyright (c) 2021-2025 Nikolay Pletnev
 MIT license

 CMakeLists.txt: CMake file for STM8 intermediate code compiler
]]


set(CMAKE_CXX_STANDARD 17)

# set project (target) name, B1_TARGET variable should be set before running cmake
set(B1_PROJECT_NAME $ENV{B1_TARGET})

cmake_minimum_required(VERSION 3.10)

project(${B1_PROJECT_NAME} LANGUAGES C CXX)

set(B1_CORE_SRC_DIR ../../b1core/source)
set(B1_EXT_SRC_DIR ../../b1core/source/ext)
set(B1_COMMON_SRC_DIR ../../common/source)

add_definitions(-DB1_PROJECT_NAME="${B1_PROJECT_NAME}")

include_directories(${B1_CORE_SRC_DIR})
include_directories(${B1_COMMON_SRC_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(${B1_PROJECT_NAME} c1stm8.cpp
	${B1_COMMON_SRC_DIR}/c1.cpp
	${B1_COMMON_SRC_DIR}/c1errors.cpp
	${B1_COMMON_SRC_DIR}/b1cmp.cpp
	${B1_COMMON_SRC_DIR}/trgsel.cpp
	${B1_COMMON_SRC_DIR}/Utils.cpp
	${B1_COMMON_SRC_DIR}/moresym.cpp
	${B1_CORE_SRC_DIR}/b1.c
	${B1_CORE_SRC_DIR}/b1types.c)

if(UNIX)
 target_link_libraries(${B1_PROJECT_NAME} stdc++fs)
endif()
