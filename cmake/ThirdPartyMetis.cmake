SET(THIRDPARTY_BUILD_METIS ON CACHE BOOL
    "Build ModMetis library from ThirdParty")

IF (THIRDPARTY_BUILD_METIS)
    INCLUDE(ExternalProject)
    EXTERNALPROJECT_ADD(
        modmetis-4.0
        PREFIX ${TPSRC}
        URL ${TPURL}/modmetis-4.0.tar.bz2
        URL_MD5 "4267355e04dcc2ef36f9889a98c923a5"
        DOWNLOAD_DIR ${TPSRC}
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -DCMAKE_INSTALL_PREFIX:PATH=${TPSRC}/dist ${TPSRC}/src/modmetis-4.0
    )
    SET(METIS_LIB modmetis CACHE FILEPATH
        "METIS library" FORCE)
    LINK_DIRECTORIES(${TPSRC}/dist/lib)
ELSE (THIRDPARTY_BUILD_METIS)
    INCLUDE (FindMetis)
ENDIF (THIRDPARTY_BUILD_METIS)

