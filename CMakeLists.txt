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

# QtCore is the target; Gui/Widgets get linked for value types (QColor,
# QPen, QBrush, QFont, QSizePolicy, QKeySequence) that extracted headers
# reference. The daemon only instantiates QCoreApplication, never
# QGuiApplication or QApplication, so it still runs without a display.
#
# Default to Qt5; pass -DSEQ_USE_QT6=ON to build against Qt6.
# Qt6 additionally requires Core5Compat for the SAX XML API
# (QXmlDefaultHandler / QXmlSimpleReader / QXmlAttributes) used by
# packetinfo.cpp's opcode-XML loader. Tracked as a follow-up port to
# QXmlStreamReader in MODERNIZATION_PLAN.md.
option(SEQ_USE_QT6 "Build against Qt6 instead of Qt5" OFF)
if(SEQ_USE_QT6)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Xml WebSockets Core5Compat)
    set(QT_VERSION_MAJOR 6)
else()
    find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets Network Xml WebSockets)
    set(QT_VERSION_MAJOR 5)
endif()
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

# --- daemon core library -----------------------------------------------------
# Everything except the WebSocket/main wiring goes into a static library so
# the test binaries (tests/) can link against the same code without dragging
# in QWebSockets or the command-line parser. Daemon executable adds the
# remaining pieces (main.cpp + WsServer + SessionAdapter + DaemonApp wiring).
#
# Sources extracted from showeq/src (authoritative list derived from
# showeq/src/Makefile.am's showeq_SOURCES, minus all QWidget-bound files).
set(SEQ_DAEMON_EXTRACTED_SOURCES
  src/bazaarlog.cpp
  src/category.cpp
  src/datalocationmgr.cpp
  src/datetimemgr.cpp
  src/diagnosticmessages.cpp
  src/eqstr.cpp
  src/filter.cpp
  src/filteredspawnlog.cpp
  src/filtermgr.cpp
  src/group.cpp
  src/guild.cpp
  src/guildshell.cpp
  src/logger.cpp
  src/mapcore.cpp
  src/message.cpp
  src/messagefilter.cpp
  src/messages.cpp
  src/messageshell.cpp
  src/netstream.cpp
  src/packet.cpp
  src/packetcapture.cpp
  src/packetcaptureprovider.cpp
  src/packetformat.cpp
  src/packetfragment.cpp
  src/packetinfo.cpp
  src/packetlog.cpp
  src/packetstream.cpp
  src/player.cpp
  src/spawn.cpp
  src/spawnlog.cpp
  src/spawnmonitor.cpp
  src/spawnshell.cpp
  src/spells.cpp
  src/spellshell.cpp
  src/util.cpp
  src/vpacket.cpp
  src/xmlconv.cpp
  src/xmlpreferences.cpp
  src/zonemgr.cpp
)

set(SEQ_DAEMON_CORE_NEW_SOURCES
  src/protoencoder.cpp
  src/combatrouter.cpp
  src/filesink.cpp
  src/opcodestats.cpp
  src/prefsbroker.cpp
  src/seq_globals.cpp
)

add_library(seq-daemon-core STATIC
  ${SEQ_DAEMON_EXTRACTED_SOURCES}
  ${SEQ_DAEMON_CORE_NEW_SOURCES}
)

target_include_directories(seq-daemon-core PUBLIC
  src
  ${PCAP_INCLUDE_DIRS}
)

target_link_libraries(seq-daemon-core PUBLIC
  seq-proto
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Gui
  Qt${QT_VERSION_MAJOR}::Widgets
  Qt${QT_VERSION_MAJOR}::Network
  Qt${QT_VERSION_MAJOR}::Xml
  ${PCAP_LIBRARIES}
  ${ZLIB_LIBRARIES}
  Threads::Threads
)
if(SEQ_USE_QT6)
  target_link_libraries(seq-daemon-core PUBLIC Qt6::Core5Compat)
endif()

target_compile_options(seq-daemon-core PRIVATE
  -Wall
  # Extracted code triggers a lot of benign warnings. Re-tighten these
  # once the legacy files have been cleaned up in a dedicated pass.
  -Wno-unused-parameter
  -Wno-deprecated-declarations
)

# Matches showeq's Makefile.am: INCLUDES = ... -DPKGDATADIR="..."
# datalocationmgr.cpp reads this; tests get the same install path so the
# extracted code keeps its single source of truth.
target_compile_definitions(seq-daemon-core PRIVATE
  PKGDATADIR="${CMAKE_INSTALL_PREFIX}/share/showeq-daemon/"
)

# --- daemon executable -------------------------------------------------------
add_executable(showeq-daemon
  src/main.cpp
  src/daemonapp.cpp
  src/wsserver.cpp
  src/sessionadapter.cpp
)

target_link_libraries(showeq-daemon PRIVATE
  seq-daemon-core
  Qt${QT_VERSION_MAJOR}::WebSockets
)

target_compile_options(showeq-daemon PRIVATE
  -Wall
  -Wno-unused-parameter
  -Wno-deprecated-declarations
)

# --- tests -------------------------------------------------------------------
enable_testing()
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
  add_subdirectory(tests)
endif()
