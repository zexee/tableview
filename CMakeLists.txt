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

find_package(ZLIB)
if(NOT ZLIB_FOUND)
    message(STATUS "System ZLIB not found; building zlib from source")
    include(FetchContent)
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(zlib
        URL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz
        URL_HASH SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
    )
    FetchContent_MakeAvailable(zlib)
    if(NOT TARGET ZLIB::ZLIB)
        add_library(ZLIB::ZLIB INTERFACE IMPORTED)
        target_link_libraries(ZLIB::ZLIB INTERFACE zlibstatic)
        target_include_directories(ZLIB::ZLIB INTERFACE
            "${zlib_SOURCE_DIR}" "${zlib_BINARY_DIR}")
    endif()
endif()

qt_add_executable(tableview WIN32
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
