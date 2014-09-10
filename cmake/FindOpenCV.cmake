# - Find OpenCV
# Find OpenCV
#
#  This module defines the following variables:
#     OPENCV_FOUND
#     OPENCV_INCLUDE_DIR
#     OPENCV_LIBRARIES
#

include(FindPackageHandleStandardArgs)

find_path(OpenCV_DIR
    include/opencv2/opencv.hpp)
    
if(OpenCV_DIR)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH};${OpenCV_DIR})
    include(OpenCVConfig-version)
    include(OpenCVConfig)
    
    mark_as_advanced(OpenCV_FOUND)
    
    find_path(OpenCV_INCLUDE_DIR
        NAMES opencv2/opencv.hpp
        PATHS ${OpenCV_DIR}/include)
    
    set(FVER "${OpenCV_VERSION_MAJOR}${OpenCV_VERSION_MINOR}${OpenCV_VERSION_PATCH}")

    foreach(lib ${OpenCV_LIB_COMPONENTS})
        string(SUBSTRING ${lib} 7 -1 slib)
    
        find_library(OpenCV_${slib}_LIBRARY_RELEASE
            NAMES opencv_${slib}${FVER}
            PATHS ${OpenCV_LIB_DIR})
            
        mark_as_advanced(OpenCV_${slib}_LIBRARY_RELEASE)
            
        find_library(OpenCV_${slib}_LIBRARY_DEBUG
            NAMES opencv_${slib}${FVER}d
            PATHS ${OpenCV_LIB_DIR})
            
        mark_as_advanced(OpenCV_${slib}_LIBRARY_DEBUG)
            
        set(OpenCV_${slib}_LIBRARY
            optimized ${OpenCV_${slib}_LIBRARY_RELEASE}
            #debug ${OpenCV_${slib}_LIBRARY_DEBUG} # don't add debug
			CACHE FILE "OpenCV ${slib} component")
            
        mark_as_advanced(OpenCV_${slib}_LIBRARY)
            
        set(OpenCV_LIBRARIES
            ${OpenCV_LIBRARIES};${OpenCV_${slib}_LIBRARY}
            CACHE STRING "All OpenCV libraries")
    endforeach(lib)
    
    FIND_PACKAGE_HANDLE_STANDARD_ARGS(OpenCV 
        DEFAULT_MSG 
        OpenCV_INCLUDE_DIR 
        OpenCV_LIBRARIES)
endif()