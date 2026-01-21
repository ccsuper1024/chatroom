include_directories(${CMAKE_CURRENT_SOURCE_DIR})

file(GLOB_RECURSE SERVER_SOURCES 
    "net/*.cpp" 
    "http/*.cpp" 
    "websocket/*.cpp" 
    "rtsp/*.cpp" 
    "chatroom/*.cpp" 
    "utils/*.cpp"
)

add_library(chatroom_server_lib
    ${SERVER_SOURCES}
)
target_link_libraries(chatroom_server_lib
    PUBLIC chatroom_base
    PUBLIC nlohmann_json::nlohmann_json
    PUBLIC spdlog::spdlog
)

add_executable(chatroom_server
    main.cpp
)
target_link_libraries(chatroom_server
    PRIVATE chatroom_server_lib
    PRIVATE pthread
)

# Test executable
add_executable(chatroom_test
    tests/chatroom_test.cpp
    tests/protocol_test.cpp
)
target_link_libraries(chatroom_test
    PRIVATE chatroom_server_lib
    PRIVATE GTest::gtest_main
    PRIVATE pthread
)
