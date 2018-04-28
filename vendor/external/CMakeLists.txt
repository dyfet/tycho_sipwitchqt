cmake_minimum_required(VERSION 3.1.0)
project(SipWitchQt-Externals VERSION 0.1 LANGUAGES C)

set(OSSL_VERSION "1.1.0h")
set(ARES_VERSION "1.11.0")

include(ExternalProject)
set(ENV{PKG_CONFIG_PATH} "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig")
set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}")
set_directory_properties(PROPERTIES EP_PREFIX "${CMAKE_CURRENT_BINARY_DIR}")

execute_process(
    COMMAND cmake -E make_directory ${CMAKE_INSTALL_PREFIX}
    COMMAND cmake -E make_directory ${CMAKE_INSTALL_PREFIX}/bin
    COMMAND cmake -E make_directory ${CMAKE_INSTALL_PREFIX}/lib
    COMMAND cmake -E make_directory ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig
    COMMAND cmake -E make_directory ${CMAKE_INSTALL_PREFIX}/include
)

if(WIN32)
    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
        set(BOOTSTRAP_DEVARCH "amd64")
        set(BOOTSTRAP_WIN64 true)
    else()
        set(BOOTSTRAP_DEVARCH "amd64_x86")
        set(BOOTSTRAP_WIN64 false)
    endif()

    if(BOOTSTRAP_WIN64)
        ExternalProject_add(openssl
            URL "https://www.openssl.org/source/openssl-${OSSL_VERSION}.tar.gz"
            CONFIGURE_COMMAND perl Configure VC-WIN64A no-shared enable-static-engine --prefix=${CMAKE_INSTALL_PREFIX} --openssldir=C:\\ProgramData\\SipWitchQt
            BUILD_COMMAND nmake
            INSTALL_COMMAND nmake install
            BUILD_IN_SOURCE 1
        )
    else()
        ExternalProject_add(openssl
            URL "https://www.openssl.org/source/openssl-${OSSL_VERSION}.tar.gz"
            CONFIGURE_COMMAND perl Configure VC-WIN32 no-shared enable-static-engine --prefix=${CMAKE_INSTALL_PREFIX} --openssldir=C:\\ProgramData\\SipWitchQt
            BUILD_COMMAND exit nmake
            INSTALL_COMMAND nmake install
            BUILD_IN_SOURCE 1
        )
    endif()


endif()

