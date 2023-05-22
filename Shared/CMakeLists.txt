cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(kshared)
file(GLOB SHARED_CACHE_SRCS *.cpp *.h)

add_library(kshared SHARED ${SHARED_CACHE_SRCS} SharedCache.cpp SharedCache.h)

target_include_directories(kshared
        PUBLIC ${PROJECT_SOURCE_DIR})

set_target_properties(ksuite-sharedcache PROPERTIES
        CXX_STANDARD 17
        CXX_VISIBILITY_PRESET hidden
        CXX_STANDARD_REQUIRED ON
        VISIBILITY_INLINES_HIDDEN ON
        POSITION_INDEPENDENT_CODE ON
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/out)

if (NOT DEMO)
    #    add_subdirectory(python)
endif()


message(STATUS "")
message(STATUS "BinaryNinja View Config -=-=-=-======")
message(STATUS "Project Name: ksuite-sharedcache")
message(STATUS "Tracked Source Files: ${file_count}")
# message(STATUS "${SRC_FILES}")
message(STATUS "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-======")
message(STATUS "")

# Support cmake --install