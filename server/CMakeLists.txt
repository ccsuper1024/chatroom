# 服务器CMakeLists.txt

# 包含头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 服务器库
add_library(chatroom_server_lib
    src/http_server.cpp
    src/chatroom_server.cpp
)
target_link_libraries(chatroom_server_lib
    PUBLIC spdlog::spdlog
    PUBLIC nlohmann_json::nlohmann_json
)

# 服务器可执行文件
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
