cmake_minimum_required(VERSION 3.20)
project(showeq-daemon CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(Qt5 5.14 REQUIRED COMPONENTS Core Network Xml WebSockets)
find_package(Protobuf REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PCAP REQUIRED libpcap)
pkg_check_modules(ZLIB REQUIRED zlib)

# --- protobuf ----------------------------------------------------------------
# The schema lives in the showeq-proto submodule under proto/.
set(SEQ_PROTO_DIR "${CMAKE_SOURCE_DIR}/proto")
set(SEQ_PROTO_FILES
  "${SEQ_PROTO_DIR}/seq/v1/events.proto"
  "${SEQ_PROTO_DIR}/seq/v1/client.proto"
)

if(NOT EXISTS "${SEQ_PROTO_DIR}/seq/v1/events.proto")
  message(FATAL_ERROR
    "showeq-proto submodule is not checked out. Run:\n"
    "  git submodule update --init --recursive")
endif()

set(SEQ_PROTO_GEN_DIR "${CMAKE_BINARY_DIR}/gen-proto")
file(MAKE_DIRECTORY "${SEQ_PROTO_GEN_DIR}")

set(SEQ_PROTO_SRCS)
set(SEQ_PROTO_HDRS)
foreach(proto ${SEQ_PROTO_FILES})
  get_filename_component(proto_name "${proto}" NAME_WE)
  get_filename_component(proto_dir "${proto}" DIRECTORY)
  file(RELATIVE_PATH proto_rel "${SEQ_PROTO_DIR}" "${proto_dir}")
  set(cc "${SEQ_PROTO_GEN_DIR}/${proto_rel}/${proto_name}.pb.cc")
  set(hh "${SEQ_PROTO_GEN_DIR}/${proto_rel}/${proto_name}.pb.h")
  add_custom_command(
    OUTPUT "${cc}" "${hh}"
    COMMAND protobuf::protoc
            --cpp_out ${SEQ_PROTO_GEN_DIR}
            --proto_path ${SEQ_PROTO_DIR}
            ${proto}
    DEPENDS "${proto}" protobuf::protoc
    COMMENT "protoc ${proto_rel}/${proto_name}.proto"
    VERBATIM
  )
  list(APPEND SEQ_PROTO_SRCS "${cc}")
  list(APPEND SEQ_PROTO_HDRS "${hh}")
endforeach()

add_library(seq-proto STATIC ${SEQ_PROTO_SRCS} ${SEQ_PROTO_HDRS})
target_include_directories(seq-proto PUBLIC "${SEQ_PROTO_GEN_DIR}")
target_link_libraries(seq-proto PUBLIC protobuf::libprotobuf)
set_target_properties(seq-proto PROPERTIES AUTOMOC OFF AUTORCC OFF AUTOUIC OFF)

# --- daemon ------------------------------------------------------------------
# Sources are added incrementally as files are extracted from showeq-c.
# Phase 1 starter set below; extend as extraction progresses.
set(SEQ_DAEMON_SOURCES
  src/main.cpp
  src/daemonapp.cpp
  src/wsserver.cpp
  src/sessionadapter.cpp
  src/protoencoder.cpp
)

add_executable(showeq-daemon ${SEQ_DAEMON_SOURCES})
target_include_directories(showeq-daemon PRIVATE
  src
  ${PCAP_INCLUDE_DIRS}
)
target_link_libraries(showeq-daemon PRIVATE
  seq-proto
  Qt5::Core
  Qt5::Network
  Qt5::Xml
  Qt5::WebSockets
  ${PCAP_LIBRARIES}
  ${ZLIB_LIBRARIES}
  Threads::Threads
)
target_compile_options(showeq-daemon PRIVATE
  -Wall -Wextra -Wpedantic
)

# --- tests -------------------------------------------------------------------
enable_testing()
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
  add_subdirectory(tests)
endif()
