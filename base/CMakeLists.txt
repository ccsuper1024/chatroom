find_package(SQLite3 REQUIRED)

# Find MySQL
find_path(MYSQL_INCLUDE_DIR mysql/mysql.h)
find_library(MYSQL_LIB mysqlclient)

if (NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIB)
    message(FATAL_ERROR "MySQL not found. Please install libmysqlclient-dev")
endif()

add_library(chatroom_base
    src/logger.cpp
    src/stream_logger.cpp
    src/database_manager.cpp
    src/sqlite_database.cpp
    src/mysql_database.cpp
)

target_include_directories(chatroom_base
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PUBLIC ${MYSQL_INCLUDE_DIR}
)

target_link_libraries(chatroom_base
    PUBLIC spdlog::spdlog
    PUBLIC SQLite::SQLite3
    PUBLIC ${MYSQL_LIB}
    PUBLIC nlohmann_json::nlohmann_json
)
