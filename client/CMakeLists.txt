include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/server/include)

add_library(chatroom_client_lib
    src/chatroom_client.cpp
)
target_link_libraries(chatroom_client_lib
    PUBLIC chatroom_base
    PUBLIC nlohmann_json::nlohmann_json
)

add_executable(chatroom_client
    src/client_main.cpp
)
target_link_libraries(chatroom_client
    PRIVATE chatroom_client_lib
)
