# Tier-1 regression tests: pure-function unit tests against the daemon
# core library. No event loop, no network, no replay fixtures required.
#
# Tier-2 (replay against .vpk goldens) is documented in tests/README.md
# and is gated on capturing fixture files from a live session.

find_package(Qt5 5.14 REQUIRED COMPONENTS Test)

add_executable(protoencoder_test
  protoencoder_test.cpp
)

target_link_libraries(protoencoder_test PRIVATE
  seq-daemon-core
  Qt5::Test
)

target_compile_options(protoencoder_test PRIVATE
  -Wall
  -Wno-unused-parameter
  -Wno-deprecated-declarations
)

add_test(NAME protoencoder_test COMMAND protoencoder_test)
