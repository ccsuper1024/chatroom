include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(SQLite3 REQUIRED)

file(GLOB_RECURSE SERVER_SOURCES "src/*.cpp")
list(FILTER SERVER_SOURCES EXCLUDE REGEX "src/server_main.cpp")

add_library(chatroom_server_lib
    ${SERVER_SOURCES}
)
target_link_libraries(chatroom_server_lib
    PUBLIC chatroom_base
    PUBLIC nlohmann_json::nlohmann_json
    PUBLIC SQLite::SQLite3
)

add_executable(chatroom_server
    src/server_main.cpp
)
target_link_libraries(chatroom_server
    PRIVATE chatroom_server_lib
    PRIVATE pthread
)

# Test executable
add_executable(chatroom_test
    tests/chatroom_test.cpp
)
target_link_libraries(chatroom_test
    PRIVATE chatroom_server_lib
    PRIVATE GTest::gtest_main
    PRIVATE pthread
)


