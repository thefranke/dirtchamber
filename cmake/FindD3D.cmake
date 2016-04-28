# Find Microsoft Direct3D 11 in the Windows SDK

include(FindPackageHandleStandardArgs)

# find d3d
if (CMAKE_CL_64)
    set(LIBPATH "x64")
else()
    set(LIBPATH "x86")
endif()

set(WINDOWS_SDK_VERSION "8.1" CACHE STRING "Specify Windows SDK version")

set(DXSDK_HKEY "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Microsoft SDKs\\Windows\\v${WINDOWS_SDK_VERSION};InstallationFolder]")

get_filename_component(DXSDK_DIR ${DXSDK_HKEY} ABSOLUTE)

set(DXSDK_LIB_DIR "${DXSDK_DIR}/Lib/*/um/${LIBPATH}")
set(DXSDK_INC_DIR "${DXSDK_DIR}/Include")

find_path(D3D_INCLUDE_DIR
    NAMES D3D11.h
    PATHS ${DXSDK_INC_DIR}/um
    PATH_SUFFIXES Include/um)

find_path(WINDOWS_SDK_INCLUDE_DIR
    NAMES winapifamily.h
    PATHS ${DXSDK_INC_DIR}/shared
    PATH_SUFFIXES Include/shared)

find_library(WINDOWS_SDK_USP10_LIBRARY
    NAMES USP10
    PATHS ${DXSDK_LIB_DIR}
    PATH_SUFFIXES ${LIBPATH})

find_library(D3D_D3D11_LIBRARY
    NAMES d3d11
    PATHS ${DXSDK_LIB_DIR}
    PATH_SUFFIXES ${LIBPATH})

find_library(D3D_D3D10_LIBRARY
    NAMES d3d10
    PATHS ${DXSDK_LIB_DIR}
    PATH_SUFFIXES ${LIBPATH})

find_library(D3D_D3D9_LIBRARY
    NAMES d3d9
    PATHS ${DXSDK_LIB_DIR}
    PATH_SUFFIXES ${LIBPATH})

find_library(D3D_D3DCompiler_LIBRARY
    NAMES d3dcompiler
    PATHS ${DXSDK_LIB_DIR}
    PATH_SUFFIXES ${LIBPATH})

FIND_PACKAGE_HANDLE_STANDARD_ARGS(D3D
    DEFAULT_MSG
    D3D_INCLUDE_DIR
    D3D_D3D11_LIBRARY
    D3D_D3D10_LIBRARY
    D3D_D3D9_LIBRARY
    D3D_D3DCompiler_LIBRARY)

if(D3D_FOUND)
    mark_as_advanced(
        D3D_D3D11_LIBRARY
        D3D_D3D10_LIBRARY
        D3D_D3D9_LIBRARY
        D3D_D3DCompiler_LIBRARY)
endif()
