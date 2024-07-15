# 设置最低 CMake 版本要求
cmake_minimum_required(VERSION 3.10)

# 设置项目名称和版本号
project(MyProject VERSION 1.0)

# 指定使用 C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # 强制使用指定的标准
set(CMAKE_CXX_EXTENSIONS ON)
# set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -S")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -S")
# set(CMAKE_CPP_FLAGS "${CMAKE_CPP_FLAGS} -S")

# 添加所有源文件到一个变量中
# file(GLOB_RECURSE  SOURCES "./*.cpp")

FILE(GLOB SOURCE_FILES ${CMAKE_SOURCE_DIR}/*.cpp )
message(${CMAKE_SOURCE_DIR})
message(${SOURCE_FILES})


# 添加可执行文件，并将所有源文件链接到该可执行文件
add_executable(MyProject ${SOURCE_FILES})

# 设置包含的目录（如果有头文件需要包含）
include_directories(./)


# 链接Windows库
# target_link_libraries(ComPortReadWrite PRIVATE
#     $<$<CXX_COMPILER_ID:MSVC>:kernel32>
#     $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:kernel32>
# )



# 设置链接的库（如果需要链接其他库）
# target_link_libraries(myExecutable myLibrary)
# target_link_libraries(main SDL2)  # new add

# 可以添加其他配置，如编译选项、特定平台的支持等

# 安装规则（如果需要安装生成的目标文件）
# install(TARGETS myExecutable DESTINATION bin)
# install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/README.md" DESTINATION share/doc/myproject)

# 可以添加测试，使用如下命令
# enable_testing()
# add_test(NAME mytest COMMAND myExecutable)