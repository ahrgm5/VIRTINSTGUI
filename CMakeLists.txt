cmake_minimum_required(VERSION 3.16)

# Set vcpkg toolchain if not provided via command line
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "C:/Users/aron.rezene/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "C:/Users/aron.rezene/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(InstrumentationApp VERSION 0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

## --- Qt Configuration ---
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

## --- Hardware & Imaging Dependencies ---
find_package(SDL3 CONFIG REQUIRED)

find_package(ic4 REQUIRED)

## --- NI-VISA Configuration ---
set(VISA_DEFAULT_ROOT "C:/Program Files/IVI Foundation/VISA/Win64")
set(VISA_DEFAULT_LIB "nivisa64")

find_path(VISA_INCLUDE_DIR
    NAMES visa.h
    PATHS "${VISA_DEFAULT_ROOT}/Include" "${CMAKE_CURRENT_SOURCE_DIR}/include"
    DOC "Path to NI-VISA include directory"
)

find_library(VISA_LIBRARY
    NAMES ${VISA_DEFAULT_LIB}
    PATHS "${VISA_DEFAULT_ROOT}/Lib_x64/msc"
    DOC "Path to NI-VISA library file"
)

if(VISA_INCLUDE_DIR AND VISA_LIBRARY)
    message(STATUS "Found NI-VISA Include: ${VISA_INCLUDE_DIR}")
    message(STATUS "Found NI-VISA Library: ${VISA_LIBRARY}")
else()
    message(FATAL_ERROR "NI-VISA not found! Please check your installation.")
endif()

## --- Project Sources ---
set(TS_FILES untitled_ti_ER.ts)

set(PROJECT_SOURCES
    # Qt GUI Files
    
        instrTest/VELMEX/single_BaudRate.cpp
	#instrTest/ZABER/simpleOperation.cpp
	#instrTest/SRSLDC502/simpleOperation.cpp
	#instrTest/IPGYLR/simpleOperation.cpp
        #main.cpp

    mainwindow.cpp
    mainwindow.h
    mainwindow.ui


    include/ipg.hpp
    include/vrm.hpp
    include/instrument.hpp
#include/arroyo.hpp
    include/remoteControl.hpp
    include/zaber.hpp
    include/velmex.hpp
    include/ldc.hpp
    include/imotoraxis.hpp
    include/pm100D.hpp


    # Hardware & Instrumentation Drivers
    src/ipg.cpp
    src/vrm.cpp
    src/instrument.cpp
#    src/arroyo.cpp
    src/remoteControl.cpp
    src/zaber.cpp
    src/velmex.cpp
    src/ldc.cpp
    src/pm100D.cpp
	
	
	#Routines Automation
	routines/routine.hpp
	routines/templates/align.hpp



)

## --- Executable Target Setup ---
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)


qt_add_executable(InstrumentationApp
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        resources.qrc

    )
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    add_executable(InstrumentationApp
        ${PROJECT_SOURCES}
    )
    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

## --- Includes and Linking ---
target_include_directories(InstrumentationApp PRIVATE
    ${VISA_INCLUDE_DIR}
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(InstrumentationApp PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    ${VISA_LIBRARY}
    SDL3::SDL3
    ic4::core
)

## --- Post-Build Steps ---
ic4_copy_runtime_to_target(InstrumentationApp)

if(MSVC)
    target_compile_options(InstrumentationApp PRIVATE /EHsc)
endif()

set_target_properties(InstrumentationApp PROPERTIES
    WIN32_EXECUTABLE TRUE
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(InstrumentationApp)
endif()


# Find .ini files in the project root
file(GLOB INI_FILES "${CMAKE_SOURCE_DIR}/*.ini")

# Force copy them right next to the executable during the build
if(INI_FILES)
    add_custom_command(TARGET InstrumentationApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${INI_FILES}
        $<TARGET_FILE_DIR:InstrumentationApp>
    )
endif()