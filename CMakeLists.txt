project(llvm-epp)
cmake_minimum_required(VERSION 2.8)

set(PACKAGE_NAME llvm-epp)
set(PACKAGE_VERSION 1.0)
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "ska124@sfu.ca")

set(CMAKE_CXX_FLAGS "-fno-rtti -std=c++1y -Werror -Wno-deprecated-declarations")
option(RT32 "32 bit EPP Runtime" OFF)
#option(TRACE_RUNTIME "Collect Run Length Encoded (RLE) Trace for EPP" OFF)

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



# 32 bit runtime
if(RT32)
    message(STATUS "Building 32 bit runtime for EPP")
    set(BITCODE_FLAGS "-m32")
else()
    set(BITCODE_FLAGS "")
endif()



find_program(LIT_COMMAND lit)

# Documentation 

find_package(Doxygen)
if(DOXYGEN_FOUND AND DOXYGEN_DOT_FOUND)
    message(STATUS "Doxygen found, the `doc` target is available to generate html documentation")
    configure_file(${CMAKE_SOURCE_DIR}/Doxyfile.in ${CMAKE_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(doc
        ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating API documentation for needle" VERBATIM
    )
endif()


############## FINAL PROJECT CONFIG #################

# And the project header and library paths
include_directories(${CMAKE_SOURCE_DIR}/include)
link_directories( ${LIBRARY_OUTPUT_PATH} )
set(CMAKE_TEMP_LIBRARY_PATH "${PROJECT_BINARY_DIR}/lib")
# TODO: Add install path to the list....


add_subdirectory(lib)
add_subdirectory(tools)
add_subdirectory(test)
