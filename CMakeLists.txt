cmake_minimum_required(VERSION 3.10)

project(WebServer)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-g -Wall -pthread)

# 【核心】包含所有头文件路径（让CMake找到.h文件）
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/http_conn
    ${PROJECT_SOURCE_DIR}/lock
    ${PROJECT_SOURCE_DIR}/log
    ${PROJECT_SOURCE_DIR}/mysql
    ${PROJECT_SOURCE_DIR}/threadpool
    ${PROJECT_SOURCE_DIR}/timer
)

# 【核心】自动收集所有.cpp源文件
# file(GLOB SRC_LIST
#     ${PROJECT_SOURCE_DIR}/*.cpp
#     ${PROJECT_SOURCE_DIR}/http_conn/*.cpp
#     ${PROJECT_SOURCE_DIR}/lock/*.cpp
#     ${PROJECT_SOURCE_DIR}/log/*.cpp
#     ${PROJECT_SOURCE_DIR}/mysql/*.cpp
#     ${PROJECT_SOURCE_DIR}/timer/*.cpp
# )

set(SRC_LIST
    # 根目录源文件
    ${PROJECT_SOURCE_DIR}/main.cpp
    ${PROJECT_SOURCE_DIR}/web_server.cpp
    # http模块
    ${PROJECT_SOURCE_DIR}/http_conn/http_conn.cpp
    # log模块
    ${PROJECT_SOURCE_DIR}/log/log.cpp
    # timer模块
    ${PROJECT_SOURCE_DIR}/timer/lst_timer.cpp
    # CGImysql模块
    ${PROJECT_SOURCE_DIR}/mysql/sql_connection_pool.cpp
)

# 生成可执行文件，名字叫 server
add_executable(server ${SRC_LIST})

target_link_libraries(server
    PRIVATE
    -lpthread
    -lmysqlclient
    -lrt
)

