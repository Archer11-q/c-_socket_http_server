#声明CMake项目所需最低CMake版本
cmake_minimum_required(VERSION 3.10)
#项目名称
project(cpp_socket_http_server VERSION 1.0.0 LANGUAGES CXX)

#全局编译配置（强制c++11，开启严格警告）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra -Wpedantic)

#编译产物输出目录（统一到build/bin，隔离源码，src目录保持干净）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#编译源码文件src/socket_server.cpp，生成可执行文件http_server
add_executable(http_server src/socket_server.cpp)

#链接系统线程库(Socket通信基础依赖，避免编译警告)
target_link_libraries(http_server PRIVATE pthread)

