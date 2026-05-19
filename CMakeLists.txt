cmake_minimum_required(VERSION 3.10)
project(ChatServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

include_directories(${CMAKE_SOURCE_DIR}/include)

enable_testing()

set(CHAT_SERVER_SOURCES
    src/chat_server.cpp
    src/ChatServer.cpp
    src/Metrics.cpp
    src/Logger.cpp
    src/ChatProtocol.cpp
    src/Config.cpp
    src/SocketFrameIO.cpp
    src/ChatRoomService.cpp
    src/ChatRoom.cpp
    src/ChatRoomManager.cpp
    src/Utils.cpp
    src/PersistenceSqlite.cpp
    src/WorkerReactor.cpp
)

add_executable(chat_server ${CHAT_SERVER_SOURCES})
target_link_libraries(chat_server pthread sqlite3)

set(CHAT_SERVER_TEST_SOURCES
    tests/chat_server_tests.cpp
    src/ChatProtocol.cpp
    src/ChatRoom.cpp
    src/ChatRoomManager.cpp
    src/ChatRoomService.cpp
    src/Config.cpp
    src/Logger.cpp
    src/Metrics.cpp
    src/PersistenceSqlite.cpp
)

add_executable(chat_server_tests ${CHAT_SERVER_TEST_SOURCES})
target_link_libraries(chat_server_tests pthread sqlite3)
add_test(NAME chat_server_tests COMMAND chat_server_tests)

add_executable(chat_protocol_fuzz tests/chat_protocol_fuzz.cpp src/ChatProtocol.cpp)
add_test(NAME chat_protocol_fuzz COMMAND chat_protocol_fuzz --corpus ${CMAKE_SOURCE_DIR}/tests/corpus/protocol --iterations 5000)
