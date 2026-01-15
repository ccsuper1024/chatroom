include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(chatroom_server_lib
    src/http_server.cpp
    src/chatroom_server.cpp
)
target_link_libraries(chatroom_server_lib
    PUBLIC chatroom_base
    PUBLIC nlohmann_json::nlohmann_json
)

add_executable(chatroom_server
    src/server_main.cpp
)
target_link_libraries(chatroom_server
    PRIVATE chatroom_server_lib
    PRIVATE pthread
)

# 测试
add_executable(chatroom_test
    tests/chatroom_test.cpp
)
target_link_libraries(chatroom_test
    PRIVATE chatroom_server_lib
    PRIVATE GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(chatroom_test)
