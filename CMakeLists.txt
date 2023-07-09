cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

# Plugin Name
set(PLUGIN_NAME ksuite)

set(PLUGIN_SOURCE API/ksuiteapi.h API/sharedcache.cpp Plugin.cpp Plugin.h
         Views/SharedCache/DSCView.cpp Views/SharedCache/DSCView.h Views/SharedCache/LoadedImage.h
        Views/SharedCache/ObjC.cpp Views/SharedCache/ObjC.h Views/SharedCache/SharedCache.cpp
        Views/SharedCache/SharedCache.h Views/SharedCache/VM.cpp Views/SharedCache/VM.h
        XNU/Workflows/DarwinKernel.cpp XNU/Workflows/DarwinKernel.h )
set(UI_PLUGIN_SOURCE UI/priv.h
        XNU/UI/TypeSetter.cpp XNU/UI/TypeSetter.h XNU/UI/Types.h
        Tooling/ExportSegmentAsFile/ExportSegment.h UI/Actions/Actions.cpp UI/Actions/Actions.h
        UI/Actions/MultiShortcut.cpp UI/Actions/MultiShortcut.h UI/Callgraph/Callgraph.cpp UI/Callgraph/Callgraph.h
        UI/Callgraph/CallgraphGenerator.cpp UI/Callgraph/CallgraphGenerator.h UI/DockableSidebar.cpp
        UI/DockableSidebar.h UI/Notifications.cpp UI/Notifications.h UI/SharedCache/dscpicker.cpp
        UI/SharedCache/dscpicker.h UI/SharedCache/dscwidget.cpp UI/SharedCache/dscwidget.h UI/priv.h UI/shared.h
        UI/theme/Flattery.h)

if (UI_BUILD)
    # You may need to replace this with your path
    set(CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.4.1/clang_64/lib/cmake")
endif()

# BinaryNinja API Directory
set(BINJA_API_DIR binja)

# Non-user-serviceable parts below ---------------------------------------------

if (${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif ()

project(${PLUGIN_NAME})

add_subdirectory(${BINJA_API_DIR})

include_directories(${CMAKE_SOURCE_DIR})

set(_UI_PLUGIN_SOURCES )
set(UI_PLUGIN_LIBRARIES )
set(UI_COMPILE_DEFS )

if (UI_BUILD)

    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    set(_UI_PLUGIN_SOURCES ${UI_PLUGIN_SOURCE})
    set(UI_PLUGIN_LIBRARIES binaryninjaui Qt6::Core Qt6::Gui Qt6::Widgets)
    set(UI_COMPILE_DEFS UI_BUILD=1)

    find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)
    find_package(QT NAMES Qt6 COMPONENTS Widgets REQUIRED)

endif()

set(_PLUGIN_SOURCE ${PLUGIN_SOURCE} ${_UI_PLUGIN_SOURCES})
set("${CMAKE_C_FLAGS} -include${PROJECT_SOURCE_DIR}/Prefix.h")


add_library(${PLUGIN_NAME} SHARED ${_PLUGIN_SOURCE})

target_link_libraries(${PLUGIN_NAME} binaryninjaapi ${UI_PLUGIN_LIBRARIES})
target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_17 c_std_99)
target_compile_definitions(${PLUGIN_NAME} PRIVATE
        DEV_MODE=${DEV_MODE} ${UI_COMPILE_DEFS})
target_include_directories(${PLUGIN_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/API)
# Library targets linking against the Binary Ninja API need to be compiled with
# position-independent code on Linux.
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    target_compile_options(${PLUGIN_NAME} PRIVATE "-fPIC")
endif()

list(APPEND fcl ${_PLUGIN_SOURCE})
list(LENGTH fcl file_count)
message(STATUS "")

if (UI_BUILD)
    message(STATUS "BinaryNinja UI Plugin Config -=-=-=-======")
else()
    message(STATUS "BinaryNinja Headless Plugin Config -=-=-=-======")
endif()

message(STATUS "Project Name: ${PLUGIN_NAME}")
message(STATUS "Tracked Source Files: ${file_count}")
message(STATUS "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-======")

message(STATUS "")

# Support cmake --install
bn_install_plugin(${PLUGIN_NAME})
