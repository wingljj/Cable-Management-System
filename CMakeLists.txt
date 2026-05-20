cmake_minimum_required(VERSION 3.16)

project(DapmQt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Sql Test Xml Gui)
find_package(Qt5Gui REQUIRED COMPONENTS Private)

add_executable(DapmQt WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/DatabaseManager.cpp
    src/DatabaseManager.h
    src/CableImporter.cpp
    src/CableImporter.h
)

target_link_libraries(DapmQt PRIVATE Qt5::Widgets Qt5::Sql Qt5::Xml Qt5::GuiPrivate)

enable_testing()

add_executable(CableModuleTests
    tests/cable_module_tests.cpp
    src/DatabaseManager.cpp
    src/DatabaseManager.h
    src/CableImporter.cpp
    src/CableImporter.h
)

target_include_directories(CableModuleTests PRIVATE src)
target_link_libraries(CableModuleTests PRIVATE Qt5::Core Qt5::Sql Qt5::Test Qt5::Xml Qt5::GuiPrivate)
add_test(NAME CableModuleTests COMMAND CableModuleTests)

if(MSVC)
    target_compile_options(DapmQt PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(CableModuleTests PRIVATE /W4 /permissive- /utf-8)
endif()
