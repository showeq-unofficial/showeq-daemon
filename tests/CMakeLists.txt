# Tier-1 regression tests: pure-function unit tests against the daemon
# core library. No event loop, no network, no replay fixtures required.
#
# Tier-2 (replay against .vpk goldens) is documented in tests/README.md
# and is gated on capturing fixture files from a live session.

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)

add_executable(protoencoder_test
  protoencoder_test.cpp
)

target_link_libraries(protoencoder_test PRIVATE
  seq-daemon-core
  Qt${QT_VERSION_MAJOR}::Test
)

target_compile_options(protoencoder_test PRIVATE
  -Wall
  -Wno-unused-parameter
  -Wno-deprecated-declarations
)

add_test(NAME protoencoder_test COMMAND protoencoder_test)

add_executable(seqcolor_test
  seqcolor_test.cpp
)

target_link_libraries(seqcolor_test PRIVATE
  seq-daemon-core
  Qt${QT_VERSION_MAJOR}::Test
)

target_compile_options(seqcolor_test PRIVATE
  -Wall
)

add_test(NAME seqcolor_test COMMAND seqcolor_test)

add_executable(packetinfo_test
  packetinfo_test.cpp
)

target_link_libraries(packetinfo_test PRIVATE
  seq-daemon-core
  Qt${QT_VERSION_MAJOR}::Test
)

target_compile_options(packetinfo_test PRIVATE
  -Wall
  -Wno-unused-parameter
  -Wno-deprecated-declarations
)

add_test(NAME packetinfo_test COMMAND packetinfo_test)

add_executable(itempacket_test
  itempacket_test.cpp
)

target_link_libraries(itempacket_test PRIVATE
  seq-daemon-core
  Qt${QT_VERSION_MAJOR}::Test
)

target_compile_options(itempacket_test PRIVATE
  -Wall
)

add_test(NAME itempacket_test COMMAND itempacket_test)

if(SEQ_USE_RUST)
  add_executable(rustdecode_test
    rustdecode_test.cpp
  )

  target_link_libraries(rustdecode_test PRIVATE
    seq-daemon-core
    Qt${QT_VERSION_MAJOR}::Test
  )

  target_compile_options(rustdecode_test PRIVATE
    -Wall
    -Wno-unused-parameter
    -Wno-deprecated-declarations
  )

  add_test(NAME rustdecode_test COMMAND rustdecode_test)
endif()
