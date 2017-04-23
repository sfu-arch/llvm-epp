
add_library(epp-inst
    EPPProfile.cpp
    EPPEncode.cpp
    EPPDecode.cpp
    AltCFG.cpp
    EPPPathPrinter.cpp
)


add_library(epp-rt SHARED
    Runtime.cpp
)


install(TARGETS epp-rt
    LIBRARY DESTINATION lib)
