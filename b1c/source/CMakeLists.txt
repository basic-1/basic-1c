#[[
 BASIC1 compiler
 Copyright (c) 2021-2024 Nikolay Pletnev
 MIT license

 CMakeLists.txt: CMake file for BASIC1 compiler
]]


set(CMAKE_CXX_STANDARD 17)

# set project (target) name, B1_TARGET variable should be set before running cmake
set(B1_PROJECT_NAME $ENV{B1_TARGET})

cmake_minimum_required(VERSION 3.1)

project(${B1_PROJECT_NAME} LANGUAGES C CXX)

set(B1_CORE_SRC_DIR ../../b1core/source)
set(B1_EXT_SRC_DIR ../../b1core/source/ext)
set(B1_COMMON_SRC_DIR ../../common/source)

add_definitions(-DB1_PROJECT_NAME="${B1_PROJECT_NAME}")

include_directories(${B1_CORE_SRC_DIR})
include_directories(${B1_COMMON_SRC_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_executable(${B1_PROJECT_NAME} b1c.cpp
	errors.cpp
	${B1_COMMON_SRC_DIR}/b1cmp.cpp
	${B1_COMMON_SRC_DIR}/Utils.cpp
	${B1_COMMON_SRC_DIR}/moresym.cpp
	${B1_CORE_SRC_DIR}/b1.c
	${B1_CORE_SRC_DIR}/b1types.c
	${B1_CORE_SRC_DIR}/b1id.c
	${B1_CORE_SRC_DIR}/b1tok.c
	${B1_CORE_SRC_DIR}/b1rpn.c
	${B1_EXT_SRC_DIR}/exprg.cpp)
