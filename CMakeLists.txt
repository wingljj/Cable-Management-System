cmake_minimum_required(VERSION 3.16)

project(DapmQt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Sql Test Xml Gui AxContainer PrintSupport)
find_package(Qt5Gui REQUIRED COMPONENTS Private)

add_executable(DapmQt WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/DatabaseManager.cpp
    src/DatabaseManager.h
    src/CableImporter.cpp
    src/CableImporter.h
    src/CableLabelCodec.cpp
    src/CableLabelCodec.h
    src/CableScanSupport.cpp
    src/CableScanSupport.h
    src/CableScanDialog.cpp
    src/CableScanDialog.h
    src/CableLabelPrinter.cpp
    src/CableLabelPrinter.h
    third_party/qrcodegen/qrcodegen.cpp
    third_party/qrcodegen/qrcodegen.hpp
)

target_link_libraries(DapmQt PRIVATE Qt5::Widgets Qt5::Sql Qt5::Xml Qt5::GuiPrivate Qt5::AxContainer Qt5::PrintSupport)
target_include_directories(DapmQt PRIVATE src third_party/qrcodegen)

enable_testing()

add_executable(CableModuleTests
    tests/cable_module_tests.cpp
    src/DatabaseManager.cpp
    src/DatabaseManager.h
    src/CableImporter.cpp
    src/CableImporter.h
    src/CableLabelCodec.cpp
    src/CableLabelCodec.h
    src/CableScanSupport.cpp
    src/CableScanSupport.h
    src/CableLabelPrinter.cpp
    src/CableLabelPrinter.h
    third_party/qrcodegen/qrcodegen.cpp
    third_party/qrcodegen/qrcodegen.hpp
)

target_include_directories(CableModuleTests PRIVATE src)
target_include_directories(CableModuleTests PRIVATE third_party/qrcodegen)
target_link_libraries(CableModuleTests PRIVATE Qt5::Core Qt5::Sql Qt5::Test Qt5::Xml Qt5::GuiPrivate Qt5::AxContainer Qt5::PrintSupport)
add_test(NAME CableModuleTests COMMAND CableModuleTests)

if(MSVC)
    target_compile_options(DapmQt PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(CableModuleTests PRIVATE /W4 /permissive- /utf-8)
endif()
