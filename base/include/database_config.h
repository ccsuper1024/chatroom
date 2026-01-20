#pragma once
#include <string>

struct DatabaseConfig {
    std::string type = "sqlite"; // sqlite, mysql
    std::string path = "chatroom.db"; // for sqlite
    std::string host = "127.0.0.1";
    int port = 3306;
    std::string user = "root";
    std::string password = "";
    std::string db_name = "chatroom";
    
    // Pooling config
    int initial_size = 2;
    int max_size = 10;
};
