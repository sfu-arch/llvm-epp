
if(RT32 OR CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_definitions(-DRT32)
endif()

add_library(epp-inst
    EPPProfile.cpp
    EPPEncode.cpp
    EPPDecode.cpp
    AltCFG.cpp
    )


add_library(epp-rt-rle SHARED
    RuntimeRLE.cpp
)

add_library(epp-rt-agg SHARED
    RuntimeAgg.cpp
)

if(TRACE_RUNTIME)
    message(STATUS "Using RLE Trace Runtime for EPP")
add_custom_command(
    TARGET epp-rt-rle POST_BUILD
    COMMAND ln -sf ${CMAKE_BINARY_DIR}/lib/libepp-rt-rle.so ${CMAKE_BINARY_DIR}/lib/libepp-rt.so
)
else()
add_custom_command(
    TARGET epp-rt-agg POST_BUILD
    COMMAND ln -sf ${CMAKE_BINARY_DIR}/lib/libepp-rt-agg.so ${CMAKE_BINARY_DIR}/lib/libepp-rt.so
)
endif()

if(RT32 OR CMAKE_SIZEOF_VOID_P EQUAL 4)
    set_target_properties(epp-rt-agg PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
    set_target_properties(epp-rt-rle PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
endif()
