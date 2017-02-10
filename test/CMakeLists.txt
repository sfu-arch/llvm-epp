
# For some reason this part of the LLVM scripts uses DIR instead of DIRS
set(LLVM_LIBRARY_DIR ${LLVM_LIBRARY_DIRS})

file(COPY lit.cfg srcs DESTINATION ${CMAKE_BINARY_DIR}/test)

configure_lit_site_cfg(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg
  )


# Set the depends list as a variable so that it can grow conditionally.
# NOTE: Sync the substitutions in test/lit.cfg when adding to this list.
set(LLVM_TEST_DEPENDS
          llvm-epp
          epp-rt
        )

add_lit_testsuite(check-epp "Running regression tests"
  ${CMAKE_CURRENT_BINARY_DIR}
  PARAMS llvm_site_config=${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg
         llvm_unit_site_config=${CMAKE_CURRENT_BINARY_DIR}/Unit/lit.site.cfg
  DEPENDS ${LLVM_TEST_DEPENDS}
  )
set_target_properties(check-epp PROPERTIES FOLDER "Tests")

add_lit_testsuites(epp-test ${CMAKE_CURRENT_SOURCE_DIR}
  PARAMS llvm_site_config=${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg
  DEPENDS ${LLVM_TEST_DEPENDS}
  )

# Setup a legacy alias for 'check-llvm'. This will likely change to be an
# alias for 'check-all' at some point in the future.
add_custom_target(check)
add_dependencies(check check-epp)
set_target_properties(check PROPERTIES FOLDER "Tests")
