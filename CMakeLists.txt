cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# Plugin Name
set(PLUGIN_NAME ksuite)

# You may need to replace this with your path
set(CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.4.1/clang_64/lib/cmake")

# BinaryNinja API Directory
set(BINJA_API_DIR binja)

# Non-user-serviceable parts below ---------------------------------------------

project(${PLUGIN_NAME})

add_subdirectory(${BINJA_API_DIR})
# add_subdirectory(Views)

include_directories(${CMAKE_SOURCE_DIR})

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_REQUIRED_LINK_OPTIONS "-arch arm64")
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE INTERNAL "" FORCE)

set(PLUGIN_SOURCE UI/priv.h API/ksuiteapi.h API/sharedcache.cpp Plugin.cpp Plugin.h
        Tooling/DarwinKernel/TypeSetter.cpp Tooling/DarwinKernel/TypeSetter.h Tooling/DarwinKernel/Types.h
        Tooling/ExportSegmentAsFile/ExportSegment.h UI/Actions/Actions.cpp UI/Actions/Actions.h
        UI/Actions/MultiShortcut.cpp UI/Actions/MultiShortcut.h UI/Callgraph/Callgraph.cpp UI/Callgraph/Callgraph.h
        UI/Callgraph/CallgraphGenerator.cpp UI/Callgraph/CallgraphGenerator.h UI/DockableSidebar.cpp
        UI/DockableSidebar.h UI/Notifications.cpp UI/Notifications.h UI/SharedCache/dscpicker.cpp
        UI/SharedCache/dscpicker.h UI/SharedCache/dscwidget.cpp UI/SharedCache/dscwidget.h UI/priv.h UI/shared.h
        UI/theme/Flattery.h Views/SharedCache/DSCView.cpp Views/SharedCache/DSCView.h Views/SharedCache/LoadedImage.h
        Views/SharedCache/ObjC.cpp Views/SharedCache/ObjC.h Views/SharedCache/SharedCache.cpp
        Views/SharedCache/SharedCache.h Views/SharedCache/VM.cpp Views/SharedCache/VM.h
        Workflows/DarwinKernel.cpp Workflows/DarwinKernel.h)
set("${CMAKE_C_FLAGS} -include${PROJECT_SOURCE_DIR}/Prefix.h")

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

find_package(QT NAMES Qt6 COMPONENTS Widgets REQUIRED)
find_package(Qt6 COMPONENTS Widgets REQUIRED)


add_library(${PLUGIN_NAME} SHARED ${PLUGIN_SOURCE})
target_link_libraries(${PLUGIN_NAME} binaryninjaapi binaryninjaui Qt6::Core Qt6::Gui Qt6::Widgets)
target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_17 c_std_99)
target_compile_definitions(${PLUGIN_NAME} PRIVATE
        DEV_MODE=${DEV_MODE})
target_include_directories(${PLUGIN_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/API)
# Library targets linking against the Binary Ninja API need to be compiled with
# position-independent code on Linux.
if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    target_compile_options(${PLUGIN_NAME} PRIVATE "-fPIC")
endif()

list(APPEND fcl ${SRC_FILES})
list(LENGTH fcl file_count)
message(STATUS "")
message(STATUS "BinaryNinja UI Plugin Config -=-=-=-======")
message(STATUS "Project Name: ${PLUGIN_NAME}")
message(STATUS "Tracked Source Files: ${file_count}")
message(STATUS "${SRC_FILES}")
message(STATUS "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-======")
message(STATUS "")


# Support cmake --install
bn_install_plugin(${PLUGIN_NAME})
# bn_install_plugin(ksuite-sharedcache)