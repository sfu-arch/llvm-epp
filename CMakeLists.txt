project(llvm-epp)
cmake_minimum_required(VERSION 2.8)

set(PACKAGE_NAME llvm-epp)
set(PACKAGE_VERSION 1.0)
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "ska124@sfu.ca")

set(CMAKE_CXX_FLAGS "-fno-rtti -std=c++1y -Werror -Wno-deprecated-declarations")
option(RT32 "32 bit EPP Runtime" OFF)
option(TRACE_RUNTIME "Collect Run Length Encoded (RLE) Trace for EPP" OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/" ${CMAKE_MODULE_PATH})
############## LLVM CONFIGURATION #################

# LLVM_DIR must be set to the prefix of /share/llvm/cmake via commandline
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# We incorporate the CMake features provided by LLVM:
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

#option(LLVM_ENABLE_CXX11  "Enable C++11" ON)

option(LLVM_INCLUDE_TOOLS "Generate build targets for the LLVM tools." ON)
option(LLVM_BUILD_TOOLS
  "Build the LLVM tools. If OFF, just generate build targets." ON)

message("LLVM STATUS:
  Definitions ${LLVM_DEFINITIONS}
  Includes    ${LLVM_INCLUDE_DIRS}
  Libraries   ${LLVM_LIBRARY_DIRS}
  Targets     ${LLVM_TARGETS_TO_BUILD}")
# Now set the LLVM header and library paths:
include_directories( ${LLVM_INCLUDE_DIRS} )
link_directories( ${LLVM_LIBRARY_DIRS} )
add_definitions( ${LLVM_DEFINITIONS} )


# Find clang 

#set(CLANG_VERSION_REQ "${LLVM_PACKAGE_VERSION}")
#find_package(Clang)
#if(CLANG_FOUND)
    #if(${CLANG_VERSION} VERSION_EQUAL CLANG_VERSION_REQ) 
    #else()
        #message(FATAL_ERROR "Incompatible clang version -- need ${CLANG_VERSION_REQ}, found ${CLANG_VERSION}")
    #endif()
#else()
    #message(FATAL_ERROR "Pasha needs clang ${CLANG_VERSION_REQ} in PATH, Cmake will exit" )
#endif()


# 32 bit runtime
if(RT32)
    message(STATUS "Building 32 bit runtime for EPP")
    set(BITCODE_FLAGS "-m32")
else()
    set(BITCODE_FLAGS "")
endif()


#function(add_bitcode BC FILES) 
    #add_custom_command(
        #OUTPUT ${BC}
        #COMMAND ${CLANG_EXE} 
                #-c -g -emit-llvm ${BITCODE_FLAGS}
                #${FILES}
                #-o ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/${BC}
        #WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        #DEPENDS ${FILES}
    #)
    #add_custom_target(bitcode ALL
        #DEPENDS ${BC}
        #COMMENT "Generate bitcode for helpers"
    #)
#endfunction(add_bitcode)

# Debug Variable Definition

if(CMAKE_BUILD_TYPE EQUAL "Debug")
    add_definitions(-DPASHA_DEBUG)
endif()

find_program(LIT_COMMAND lit)

############## FINAL PROJECT CONFIG #################

# And the project header and library paths
include_directories(${CMAKE_SOURCE_DIR}/include)
link_directories( ${LIBRARY_OUTPUT_PATH} )
set(CMAKE_TEMP_LIBRARY_PATH "${PROJECT_BINARY_DIR}/lib")
# TODO: Add install path to the list....


add_subdirectory(lib)
add_subdirectory(tools)
add_subdirectory(test)
