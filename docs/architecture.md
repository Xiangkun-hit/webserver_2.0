# C++ Web服务器 架构笔记
## 1. 项目文件清单
- 基础工具：locker.h、block_queue.h
- 功能支撑: log.h/cpp、sql_connection_pool.h/cpp
- 核心支撑：lst_timer.h/cpp、threadpool.h
- 业务核心：http_conn.h/cpp
- 服务器核心：webserver.h/cpp
- 配置+入口：config.h/cpp、main.cpp

## 2. 模块依赖关系
- 基础工具 -> 所有线程安全模块
- 功能支撑 -> 业务核心+服务器核心
- 核心支撑 -> 服务器核心
- 业务核心 -> 服务器核心

## 3. 项目核心流程
- main() -> 解析配置 -> 初始化日志 -> 初始化数据库连接池 -> 初始化线程池 -> 创建epoll -> 监听端口
- -> 处理客户端连接 -> 线程池处理HTTP请求 ->响应客户端