
add_library(epp-inst
    EPPProfile.cpp
    EPPEncode.cpp
    EPPDecode.cpp
    AuxGraph.cpp
    EPPPathPrinter.cpp
    SplitLandingPadPredsPass.cpp
    BreakSelfLoopsPass.cpp
)


add_library(epp-rt SHARED
    Runtime.cpp
)


install(TARGETS epp-rt
    LIBRARY DESTINATION lib)
