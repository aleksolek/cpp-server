cmake_minimum_required(VERSION 3.10)
project(HttpServer)

# Use C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Add executable
add_executable(http_server
    main.cpp
    server.cpp
    response.cpp
    utils.cpp
    http.cpp
)

# Include directories if you have headers
target_include_directories(http_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
