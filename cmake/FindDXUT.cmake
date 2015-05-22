# Find DXUT

include(FindPackageHandleStandardArgs)

if (CMAKE_CL_64)
    set(DXUTARCH "x64")
else()
    set(DXUTARCH "Win32")
endif()

find_path(DXUT_SDK_PATH
    NAMES Core/DXUT.h)
    
find_library(DXUT_LIBRARY_RELEASE 
    NAMES DXUT
    PATHS ${DXUT_SDK_PATH}/Core/Bin/*/${DXUTARCH}/Release)

find_library(DXUT_LIBRARY_DEBUG
    NAMES DXUT
    PATHS ${DXUT_SDK_PATH}/Core/Bin/*/${DXUTARCH}/Debug)
    
find_library(DXUT_Opt_LIBRARY_RELEASE
    NAMES DXUTOpt
    PATHS ${DXUT_SDK_PATH}/Optional/Bin/*/${DXUTARCH}/Release)

find_library(DXUT_Opt_LIBRARY_DEBUG
    NAMES DXUTOpt
    PATHS ${DXUT_SDK_PATH}/Optional/Bin/*/${DXUTARCH}/Debug)
    
FIND_PACKAGE_HANDLE_STANDARD_ARGS(DXUT 
    DEFAULT_MSG 
    DXUT_LIBRARY_RELEASE 
    DXUT_Opt_LIBRARY_RELEASE 
    DXUT_SDK_PATH)
    
if(DXUT_FOUND)
    mark_as_advanced(DXUT_SDK_PATH DXUT_LIBRARY_RELEAE DXUT_LIBRARY_DEBUG DXUT_Opt_LIBRARY_RELEASE DXUT_Opt_LIBRARY_DEBUG)
    set(DXUT_INCLUDE_DIRS
        ${DXUT_SDK_PATH}/Core
        ${DXUT_SDK_PATH}/Optional
        CACHE STRING "")
    
    set(DXUT_LIBRARIES 
        optimized ${DXUT_LIBRARY_RELEASE} optimized ${DXUT_Opt_LIBRARY_RELEASE} 
        debug ${DXUT_LIBRARY_DEBUG} debug ${DXUT_Opt_LIBRARY_DEBUG} 
        CACHE STRING "")
endif()