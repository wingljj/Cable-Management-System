cmake_minimum_required(VERSION 3.16)

project(DapmQt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Sql)

add_executable(DapmQt WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/DatabaseManager.cpp
    src/DatabaseManager.h
)

target_link_libraries(DapmQt PRIVATE Qt5::Widgets Qt5::Sql)

if(MSVC)
    target_compile_options(DapmQt PRIVATE /W4 /permissive- /utf-8)
endif()
