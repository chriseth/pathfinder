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

if(EMSCRIPTEN)
	message("Using EMSCRIPTEN")
	include("$ENV{EMSCRIPTEN}/cmake/Modules/Platform/Emscripten.cmake")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --memory-init-file 0")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdata-sections -ffunction-sections -fvisibility=hidden")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
	# Re-enable exception catching (optimisations above -O1 disable it)
	#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s DISABLE_EXCEPTION_CATCHING=0")
	# Remove any code related to exit (such as atexit)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s EXIT_RUNTIME=0")
	# Remove any code related to filesystem access
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s FILESYSTEM=0")
	# Allow memory growth, but disable some optimisations
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ALLOW_MEMORY_GROWTH=1")
	# Disable eval()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s DYNAMIC_EXECUTION=0")
	# Disable greedy exception catcher
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s NODEJS_CATCH_EXIT=0")
	# Abort if linking results in any undefined symbols
	# Note: this is on by default in the CMake Emscripten module which we aren't using
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ERROR_ON_UNDEFINED_SYMBOLS=1")
	# Disallow deprecated emscripten build options.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s STRICT=1")
	# Export the Emscripten-generated auxiliary methods which are needed by solc-js.
	# Which methods of libsolc itself are exported is specified in libsolc/CMakeLists.txt.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s EXTRA_EXPORTED_RUNTIME_METHODS=['cwrap','ccall']")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s EXPORTED_FUNCTIONS='[\"_loadDB\",\"_signup\",\"_trust\",\"_transfer\",\"_edgeCount\",\"_flow\",\"_delayEdgeUpdates\",\"_performEdgeUpdates\"]' -s RESERVED_FUNCTION_POINTERS=20")

	# Build for webassembly target.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s WASM=1")
	# Set webassembly build to synchronous loading.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s WASM_ASYNC_COMPILATION=0")
	# Output a single js file with the wasm binary embedded as base64 string.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s SINGLE_FILE=1")

	# Disable warnings about not being pure asm.js due to memory growth.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-almost-asm")
endif()

set(
	sources
	binaryExporter.cpp
	binaryExporter.h
	binaryImporter.cpp
	binaryImporter.h
	db.cpp
	db.h
	encoding.h
	exceptions.h
	flow.cpp
	flow.h
	importGraph.cpp
	importGraph.h
	json.hpp
	keccak.cpp
	keccak.h
	main.cpp
	types.cpp
	types.h
)

add_executable(pathfinder ${sources})
