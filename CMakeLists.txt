cmake_minimum_required(VERSION 3.9.0)

set(PROJECT_VERSION "0.0.1")
project(pathfinder VERSION ${PROJECT_VERSION} LANGUAGES CXX)

if(NOT DEFINED CMAKE_CXX_COMPILER_LAUNCHER)
	find_program(CCACHE ccache DOC "ccache tool path; set to OFF to disable")
	if(CCACHE)
		set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE})
		message(STATUS "[ccache] Enabled: ${CCACHE}")
	endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall)
add_compile_options(-Wextra)
add_compile_options(-Werror)
add_compile_options(-pedantic)
add_compile_options(-Wno-unknown-pragmas)
add_compile_options(-Wimplicit-fallthrough)
add_compile_options(-Wsign-conversion)

# Configuration-specific compiler settings.
set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g3 -DETH_DEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g3")

set(CMAKE_CXX_FLAGS        "-O3 -DNDEBUG")

set(
		sources
		adjacencies.cpp
		adjacencies.h
		binaryImporter.cpp
		binaryImporter.h
		db.cpp
		db.h
		encoding.h
		exceptions.h
		flow.cpp
		flow.h
		json.hpp
		keccak.cpp
		keccak.h
		main.cpp
		types.cpp
		types.h
)

add_library(pathfinder SHARED ${sources})