cmake_minimum_required(VERSION 3.10)

# 定义静态库
add_library(gm_timer STATIC gm_timer.c)

# 指定需要链接的库
target_link_libraries(gm_timer PUBLIC rt)

# 添加头文件搜索路径
target_include_directories(gm_timer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
