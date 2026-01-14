# 客户端CMakeLists.txt

# 包含头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
# 需要访问服务器的json_utils.h
include_directories(${CMAKE_SOURCE_DIR}/server/include)

# 客户端库
add_library(chatroom_client_lib
    src/chatroom_client.cpp
)
target_link_libraries(chatroom_client_lib
    PUBLIC spdlog::spdlog
    PUBLIC nlohmann_json::nlohmann_json
)

# 客户端可执行文件
add_executable(chatroom_client
    src/client_main.cpp
)
target_link_libraries(chatroom_client
    PRIVATE chatroom_client_lib
)
