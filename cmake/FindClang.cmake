#
# Check for Clang.
#
# The following variables are set:
#  CLANG_EXE
#  CLANG_VERSION

find_program(CLANG_EXE NAMES "clang")

if(CLANG_EXE EQUAL "CLANG_EXE-NOTFOUND")
    set(CLANG_FOUND FALSE)
else()
    set(CLANG_FOUND TRUE)
endif()

execute_process(COMMAND ${CLANG_EXE} --version OUTPUT_VARIABLE CLANG_VERSION_TEXT OUTPUT_STRIP_TRAILING_WHITESPACE)
string(SUBSTRING ${CLANG_VERSION_TEXT} 14 5 CLANG_VERSION)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CLANG DEFAULT_MSG
                                  CLANG_VERSION)

mark_as_advanced(CLANG_VERSION)
