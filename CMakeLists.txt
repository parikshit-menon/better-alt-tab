cmake_minimum_required(VERSION 3.20)
project(BetterAltTab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_WIN32_EXECUTABLE ON)
add_executable(BetterAltTab WIN32 src/main.cpp)
target_link_libraries(BetterAltTab PRIVATE user32 gdi32 dwmapi)
