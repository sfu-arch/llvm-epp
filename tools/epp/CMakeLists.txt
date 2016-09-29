set(LLVM_USED_LIBS epp-inst)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake" 
               "${CMAKE_CURRENT_BINARY_DIR}/config.h" @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(epp
  main.cpp
)

llvm_map_components_to_libnames(REQ_LLVM_LIBRARIES ${LLVM_TARGETS_TO_BUILD}
        asmparser core linker bitreader bitwriter irreader ipo scalaropts
        analysis target mc support)

target_link_libraries(epp epp-inst inliner namer common simplify ${REQ_LLVM_LIBRARIES})

# Platform dependencies.
target_link_libraries(epp
    pthread
    dl
    curses
)

set_target_properties(epp
                      PROPERTIES
                      LINKER_LANGUAGE CXX
                      PREFIX "")

install(TARGETS epp 
  RUNTIME DESTINATION bin)

