cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

# Plugin Name
set(PLUGIN_NAME ksuite)

set(PLUGIN_CDEFS KSUITE=1)

set(XNU_PLUGIN_SOURCE XNU/Workflows/DarwinKernel.cpp XNU/Workflows/DarwinKernel.h XNU/CPP/CPPTypeHelper.cpp XNU/CPP/CPPTypeHelper.h)
set(XNU_PLUGIN_UI_SOURCE XNU/UI/TypeSetter.cpp XNU/UI/TypeSetter.h XNU/CPP/Types.h)

set(CALLGRAPH_PLUGIN_SOURCE )
set(CALLGRAPH_PLUGIN_UI_SOURCE UI/Callgraph/Callgraph.cpp UI/Callgraph/Callgraph.h
        UI/Callgraph/CallgraphGenerator.cpp UI/Callgraph/CallgraphGenerator.h  )

set(NOTEPAD_PLUGIN_SOURCE Notepad/Notepad.cpp Notepad/Notepad.h )
set(NOTEPAD_PLUGIN_UI_SOURCE Notepad/NotepadUI.h Notepad/NotepadUI.cpp )

set(SHAREDCACHE_PLUGIN_SOURCE Views/SharedCache/DSCView.cpp Views/SharedCache/DSCView.h Views/SharedCache/LoadedImage.h
        Views/SharedCache/ObjC.cpp Views/SharedCache/ObjC.h Views/SharedCache/SharedCache.cpp
        Views/SharedCache/SharedCache.h Views/SharedCache/VM.cpp Views/SharedCache/VM.h API/sharedcache.cpp )
set(SHAREDCACHE_PLUGIN_UI_SOURCE UI/SharedCache/dscpicker.cpp
        UI/SharedCache/dscpicker.h UI/SharedCache/dscwidget.cpp UI/SharedCache/dscwidget.h )

set(THEME_SOURCE UI/Theme/Flattery.h UI/Theme/ThemeEditor.cpp UI/Theme/ThemeEditor.h UI/Theme/CSSBuilder.cpp UI/Theme/CSSBuilder.h)

set(PLUGIN_SOURCE API/ksuiteapi.h Plugin.cpp Plugin.h)
set(UI_PLUGIN_SOURCE UI/priv.h
        Tooling/ExportSegmentAsFile/ExportSegment.h UI/Actions/Actions.cpp UI/Actions/Actions.h
        UI/Actions/MultiShortcut.cpp UI/Actions/MultiShortcut.h UI/DockableSidebar.cpp
        UI/DockableSidebar.h UI/Notifications.cpp UI/Notifications.h UI/priv.h UI/shared.h)

if(NOT DEFINED THEME_BUILD)
    set(THEME_BUILD ON)
endif()

if (THEME_BUILD)
    set(PLUGIN_CDEFS ${PLUGIN_CDEFS} THEME_BUILD=1)
    set(UI_PLUGIN_SOURCE ${UI_PLUGIN_SOURCE} ${THEME_SOURCE})
endif()

if (XNU_BUILD)
    set(PLUGIN_SOURCE ${PLUGIN_SOURCE} ${XNU_PLUGIN_SOURCE})
    set(UI_PLUGIN_SOURCE ${UI_PLUGIN_SOURCE} ${XNU_PLUGIN_UI_SOURCE})
    set(PLUGIN_CDEFS ${PLUGIN_CDEFS} XNU_BUILD=1)
else()
    set(XNU_BUILD OFF)
endif()

if (NOTEPAD_BUILD)
    set(PLUGIN_SOURCE ${PLUGIN_SOURCE} ${NOTEPAD_PLUGIN_SOURCE})
    set(UI_PLUGIN_SOURCE ${UI_PLUGIN_SOURCE} ${NOTEPAD_PLUGIN_UI_SOURCE})
    set(PLUGIN_CDEFS ${PLUGIN_CDEFS} NOTEPAD_BUILD=1)
else()
    set(NOTEPAD_BUILD OFF)
endif()

if (CALLGRAPH_BUILD)
    set(PLUGIN_SOURCE ${PLUGIN_SOURCE} ${CALLGRAPH_PLUGIN_SOURCE})
    set(UI_PLUGIN_SOURCE ${UI_PLUGIN_SOURCE} ${CALLGRAPH_PLUGIN_UI_SOURCE})
    set(PLUGIN_CDEFS ${PLUGIN_CDEFS} CALLGRAPH_BUILD=1)
else()
    set(CALLGRAPH_BUILD OFF)
endif()

if (SHAREDCACHE_BUILD)
    set(PLUGIN_SOURCE ${PLUGIN_SOURCE} ${SHAREDCACHE_PLUGIN_SOURCE})
    set(UI_PLUGIN_SOURCE ${UI_PLUGIN_SOURCE} ${SHAREDCACHE_PLUGIN_UI_SOURCE})
    set(PLUGIN_CDEFS ${PLUGIN_CDEFS} SHAREDCACHE_BUILD=1)
else()
    set(SHAREDCACHE_BUILD OFF)
endif()

if (UI_BUILD)
    # You may need to replace this with your path
    if(APPLE)
        set(CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.4.3/clang_64/lib/cmake")
    elseif(MSVC) # doesnt work on gh runners
        set(CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.4.3/msvc2022_64/lib/cmake")
    else()
        set(CMAKE_PREFIX_PATH "$ENV{HOME}/Qt/6.4.3/gcc_64/lib/cmake")
    endif()
endif()

# BinaryNinja API Directory
set(BINJA_API_DIR binaryninja-api)

# Non-user-serviceable parts below ---------------------------------------------

if (${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif ()

project(${PLUGIN_NAME})

add_subdirectory(${BINJA_API_DIR})
add_subdirectory(libkbinja)

include_directories(${CMAKE_SOURCE_DIR})

set(_UI_PLUGIN_SOURCES )
set(UI_PLUGIN_LIBRARIES )
set(UI_COMPILE_DEFS )

if (UI_BUILD)

    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    set(_UI_PLUGIN_SOURCES ${UI_PLUGIN_SOURCE})
    set(UI_PLUGIN_LIBRARIES Qt6::Core Qt6::Gui Qt6::Widgets)
    set(UI_COMPILE_DEFS UI_BUILD=1)

    find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)
    find_package(QT NAMES Qt6 COMPONENTS Widgets REQUIRED)

endif()

set(_PLUGIN_SOURCE ${PLUGIN_SOURCE} ${_UI_PLUGIN_SOURCES})
set("${CMAKE_C_FLAGS} -include${PROJECT_SOURCE_DIR}/Prefix.h")


add_library(${PLUGIN_NAME} SHARED ${_PLUGIN_SOURCE})

if(APPLE)
    target_link_options(${PLUGIN_NAME} PUBLIC -undefined dynamic_lookup)
elseif(MSVC)
    target_link_options(${PLUGIN_NAME} PUBLIC "LINKER:/FORCE:UNRESOLVED")
else()
    target_link_options(${PLUGIN_NAME} PUBLIC "LINKER:--allow-shlib-undefined")
endif()
target_link_libraries(${PLUGIN_NAME} binaryninjaapi libkbinja ${UI_PLUGIN_LIBRARIES})
target_compile_features(${PLUGIN_NAME} PRIVATE cxx_std_17 c_std_99)
target_compile_definitions(${PLUGIN_NAME} PRIVATE
        DEV_MODE=${DEV_MODE} ${UI_COMPILE_DEFS} ${PLUGIN_CDEFS})
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
message(STATUS "XNU Helpers: ${XNU_BUILD}")
message(STATUS "Callgraph: ${CALLGRAPH_BUILD}")
message(STATUS "Notepad: ${NOTEPAD_BUILD}")
message(STATUS "Theme: ${THEME_BUILD}")
message(STATUS "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-======")

message(STATUS "")

# Support cmake --install
bn_install_plugin(${PLUGIN_NAME})
