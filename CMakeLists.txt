cmake_minimum_required(VERSION 3.20)
project(ChatRoom VERSION 1.0 LANGUAGES CXX)

# 设置C++20标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 生成 compile_commands.json 供 clangd 使用
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 添加编译选项
add_compile_options(-Wall -Wextra -Wpedantic -g)

# 设置依赖库下载目录到项目根目录下的 third_party
set(FETCHCONTENT_BASE_DIR ${CMAKE_SOURCE_DIR}/third_party)

# 引入FetchContent模块用于下载第三方库
include(FetchContent)

# 下载并配置spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://gitee.com/mirror_1/spdlog.git
    GIT_TAG        v1.11.0
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/spdlog
)

# 下载并配置nlohmann_json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://gitee.com/mirrors/nlohmann-json.git
    GIT_TAG        v3.11.2
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/json
)

# 下载并配置GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://gitee.com/mirrors/googletest.git
    GIT_TAG        v1.14.0
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/googletest
)
# 对于Windows: 防止覆盖父项目的编译器/链接器设置
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(spdlog json googletest)

enable_testing()

add_subdirectory(base)
add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(tests)
