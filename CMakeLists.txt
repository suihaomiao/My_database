#规定最小版本
cmake_minimum_required( VERSION 3.5)

#设置项目名称
project(my_datebase)

#添加源文件
set(SOURCES 
    main.c
)

#添加执行文件
add_executable(my_datebase ${SOURCES})

