include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(chatroom_server_lib
    src/http_server.cpp
    src/thread_pool.cpp
    src/http_codec.cpp
    src/event_loop.cpp
    src/channel.cpp
    src/acceptor.cpp
    src/tcp_connection.cpp
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


