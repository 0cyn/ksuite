cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(ksuiteapi)
file(GLOB BN_MACHO_API_SOURCES *.cpp *.h)
add_library(ksuiteapi STATIC ${BN_MACHO_API_SOURCES})

target_include_directories(ksuiteapi
        PUBLIC ${PROJECT_SOURCE_DIR})

target_link_libraries(ksuiteapi PUBLIC ksuite)

set_target_properties(ksuiteapi PROPERTIES
        CXX_STANDARD 17
        CXX_VISIBILITY_PRESET hidden
        CXX_STANDARD_REQUIRED ON
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ON
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/out)

if (NOT DEMO)
    #    add_subdirectory(python)
endif()