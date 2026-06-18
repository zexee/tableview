cmake_minimum_required(VERSION 3.20)

project(tableview VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

get_target_property(_qt_type Qt6::Core TYPE)
if(_qt_type STREQUAL "STATIC_LIBRARY")
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)
endif()

find_package(ZLIB REQUIRED)

qt_add_executable(tableview
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/TableModel.cpp
    src/TableModel.h
    src/DataTypes.h
    src/Codec.cpp
    src/Codec.h
    src/CsvReader.cpp
    src/CsvReader.h
    src/DbfReader.cpp
    src/DbfReader.h
    src/Filter.cpp
    src/Filter.h
    src/XlsxReader.cpp
    src/XlsxReader.h
    src/ZipReader.cpp
    src/ZipReader.h
    resources/resources.qrc
)

if(WIN32)
    target_sources(tableview PRIVATE resources/app.rc)
endif()

target_link_libraries(tableview PRIVATE Qt6::Widgets ZLIB::ZLIB)

if(_qt_type STREQUAL "STATIC_LIBRARY" AND NOT WIN32)
    target_link_options(tableview PRIVATE -static-libgcc -static-libstdc++)
endif()

if(WIN32)
    target_compile_definitions(tableview PRIVATE NOMINMAX)
endif()
