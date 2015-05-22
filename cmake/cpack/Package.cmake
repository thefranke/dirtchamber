set(DIRTCHAMBER_BINARY_DIR "bin/release")

# install redist package
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${DIRTCHAMBER_BINARY_DIR})
include(InstallRequiredSystemLibraries)

# common config
set(CPACK_PACKAGE_NAME "Dirtchamber")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Dirtchamber")
set(CPACK_PACKAGE_VENDOR "Tobias Alexander Franke")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/Readme.md")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/Readme.md")
set(CPACK_RESOURCE_FILE_WELCOME "${CMAKE_SOURCE_DIR}/Readme.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/cmake/cpack/license.txt")
set(CPACK_PACKAGE_ICON "${CMAKE_SOURCE_DIR}/doc/logo.png")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Dirtchamber")

set(CPACK_PACKAGE_EXECUTABLES vct "Voxel Cone Tracer" 
                              lpv "Light Propagation Volume Renderer")
                              
if(OPENCV_FOUND)
    set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES}
                                  delta_vct "DVCT Viewer" 
                                  delta_lpv "DLPV Viewer")
endif()

# WiX
set(CPACK_WIX_UPGRADE_GUID "00B776BF-6115-489E-85CD-49AF8559A7F8")
set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/cmake/cpack/banner.png")
set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/cmake/cpack/dialog.png")
set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT "http://www.tobias-franke.eu/projects/dirtchamber")
set(CPACK_WIX_PROPERTY_ARPHELPLINK "http://tobias-franke.eu/projects/dirtchamber/doc")

include(CPack)