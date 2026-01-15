add_library(chatroom_base
    src/logger.cpp
    src/stream_logger.cpp
)

target_include_directories(chatroom_base
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(chatroom_base
    PUBLIC spdlog::spdlog
)
