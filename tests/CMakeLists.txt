# 测试模块配置

# 包含必要的头文件路径
include_directories(${CMAKE_SOURCE_DIR}/server)
include_directories(${CMAKE_SOURCE_DIR}/client/include)

# 单元测试
add_executable(unit_tests
    unit/server_test.cpp
)

target_link_libraries(unit_tests
    PRIVATE chatroom_server_lib
    PRIVATE GTest::gtest_main
    PRIVATE pthread
)

# 注册测试
include(GoogleTest)
gtest_discover_tests(unit_tests)

# 集成测试
add_executable(integration_tests
    integration/integration_test.cpp
)

target_link_libraries(integration_tests
    PRIVATE chatroom_server_lib
    PRIVATE chatroom_client_lib
    PRIVATE GTest::gtest_main
    PRIVATE pthread
)

# 注册测试
gtest_discover_tests(integration_tests)
