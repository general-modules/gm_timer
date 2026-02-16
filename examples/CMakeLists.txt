cmake_minimum_required(VERSION 3.10)

# 获取所有示例源文件
file(GLOB EXAMPLE_SOURCES "*.c")

foreach(example_source ${EXAMPLE_SOURCES})
  # 从文件名获取示例名（去掉扩展名）
  get_filename_component(example_name ${example_source} NAME_WE)

  # 创建可执行文件
  add_executable(${example_name} ${example_source})

  # 链接 gm_timer 库
  target_link_libraries(${example_name} gm_timer)
endforeach()
