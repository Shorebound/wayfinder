# cmake/GetCPM.cmake — bootstrap script for CPM.cmake
# Downloads CPM.cmake at configure time if not already cached.
# See https://github.com/cpm-cmake/CPM.cmake

set(CPM_DOWNLOAD_VERSION 0.40.8)

if(CPM_SOURCE_CACHE)
    set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}) OR "${CPM_DOWNLOAD_LOCATION}" IS_NEWER_THAN "")
    # Check if existing file is empty (failed download)
    if(EXISTS ${CPM_DOWNLOAD_LOCATION})
        file(SIZE "${CPM_DOWNLOAD_LOCATION}" CPM_FILE_SIZE)
        if(CPM_FILE_SIZE EQUAL 0)
            file(REMOVE "${CPM_DOWNLOAD_LOCATION}")
        endif()
    endif()
endif()

if(NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    get_filename_component(CPM_DOWNLOAD_DIR "${CPM_DOWNLOAD_LOCATION}" DIRECTORY)
    file(MAKE_DIRECTORY "${CPM_DOWNLOAD_DIR}")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION}
        STATUS CPM_DOWNLOAD_STATUS
    )
    list(GET CPM_DOWNLOAD_STATUS 0 CPM_DOWNLOAD_ERROR)
    if(CPM_DOWNLOAD_ERROR)
        list(GET CPM_DOWNLOAD_STATUS 1 CPM_DOWNLOAD_ERROR_MSG)
        message(FATAL_ERROR "Failed to download CPM.cmake: ${CPM_DOWNLOAD_ERROR_MSG}")
    endif()
endif()

include(${CPM_DOWNLOAD_LOCATION})
