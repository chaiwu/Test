cmake_minimum_required(VERSION 3.15)
project(HELLO VERSION 1.0.0)
set(SRC_FILES temp.c sort.c)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)   #set output location
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

string(TIMESTAMP COMPILE_TIME %Y%m%d-%H%M%S)
#configure_file(Config.h.in Config.h)
add_executable(${PROJECT_NAME} ${SRC_FILES})
message("test line 4 ",${PROJECT_SOURCE_DIR})
#add_library(hello SHARED hello.c) #生成动态库文件
set(CMAKE_C_FLAGS -std=c99)
# 将二进制可执行文件 myrun 安装到目录 ${PROJECT_SOURCE_DIR}
install(TARGETS ${PROJECT_NAME} 
    RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}
)
# add_library(hello STATIC hello.c) #生成静态库文件
# target_include_directories(${PROJECT_NAME} PUBLIC
#                            ${PROJECT_BINARY_DIR}
                        #    )
