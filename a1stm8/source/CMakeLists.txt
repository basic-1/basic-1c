#[[
 STM8 assembler
 Copyright (c) 2021-2023 Nikolay Pletnev
 MIT license

 CMakeLists.txt: CMake file for STM8 assembler
]]


if(UNIX)
 set(CMAKE_CXX_STANDARD 14)
endif()

# set project (target) name, B1_TARGET variable should be set before running cmake
set(B1_PROJECT_NAME $ENV{B1_TARGET})

cmake_minimum_required(VERSION 3.1)

project(${B1_PROJECT_NAME} LANGUAGES C CXX)

set(B1_CORE_SRC_DIR ../../b1core/source)
set(B1_COMMON_SRC_DIR ../../common/source)

add_definitions(-DB1_PROJECT_NAME="${B1_PROJECT_NAME}")

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${B1_CORE_SRC_DIR})
include_directories(${B1_COMMON_SRC_DIR})

add_executable(${B1_PROJECT_NAME} a1stm8.cpp
	errors.cpp
	${B1_COMMON_SRC_DIR}/Utils.cpp
	${B1_COMMON_SRC_DIR}/moresym.cpp
        )

# link with math. libary
if(UNIX)
 target_link_libraries(${B1_PROJECT_NAME} m)
endif()
