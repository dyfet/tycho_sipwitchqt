# Bootstrap support for building required dependencies automatically

cmake_minimum_required(VERSION 2.8)

if(POLICY CMP0020)
    cmake_policy(SET CMP0020 NEW)
endif()

project("Bootstrap")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(OSSL_VERSION "1.0.2k")
set(OSIP_VERSION "4.1.0")
set(ARES_VERSION "1.12.0")

set(CMAKE_BUILD_TYPE Release)
set(PORTS_ORIGIN "http://pub.cherokeesofidaho.org/bootstrap")
set(PORTS_PREFIX "${CMAKE_CURRENT_BINARY_DIR}")
set(PORTS_SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}")
set(ENV{PKG_CONFIG_PATH} "${PORTS_PREFIX}/lib/pkgconfig")
set(ENV{MACOSX_DEPLOYMENT_TARGET} "10.10")

include(ExternalProject)

set(NPROC 1)
if(APPLE)
    execute_process(COMMAND sysctl -n hw.ncpu OUTPUT_VARIABLE NPROC)
elseif(UNIX)
    execute_process(COMMAND nproc OUTPUT_VARIABLE NPROC)
endif()
string(REGEX REPLACE "\n$" "" NPROC "${NPROC}")

# for windows we have to build a standard openssl distro to use

if(WIN32)
    if(PORTS_ARCH MATCHES "x86_64")
        ExternalProject_add(port-openssl
            PREFIX "${PORTS_PREFIX}"
            URL "${PORTS_ORIGIN}/openssl-${OSSL_VERSION}.tar.gz"
            PATCH_COMMAND perl Configure VC-WIN64A enable-static-engine no-shared --openssldir=${OPENSSL_PREFIX}
            CONFIGURE_COMMAND ms\\do_win64a.bat
            BUILD_COMMAND ${PORTS_SCRIPTS}/openssl-patch.cmd
            INSTALL_COMMAND nmake -f ms\\ntout.mak INSTALLTOP=../.. install
            BUILD_IN_SOURCE 1
        )
    else()
        ExternalProject_add(port-openssl
            PREFIX "${PORTS_PREFIX}"
            URL "${PORTS_ORIGIN}/openssl-${OSSL_VERSION}.tar.gz"
            PATCH_COMMAND perl Configure VC-WIN32 enable-static-engine no-shared --openssldir=${OPENSSL_PREFIX}
            CONFIGURE_COMMAND ms\\do_nasm.bat
            BUILD_COMMAND ${PORTS_SCRIPTS}/openssl-patch.cmd
            INSTALL_COMMAND nmake -f ms\\ntout.mak INSTALLTOP=../.. install
            BUILD_IN_SOURCE 1
        )
    endif()
    ExternalProject_add(port-libosip2
        PREFIX "${PORTS_PREFIX}"
        URL "${PORTS_ORIGIN}/libosip2-${OSIP_VERSION}.tar.gz"
        PATCH_COMMAND cmake -E copy ${CMAKE_CURRENT_SOURCE_DIR}/libosip2.cmake CMakeLists.txt
        BUILD_COMMAND cmake --build . --config Release
        INSTALL_COMMAND cmake --build . --config Release --target install
        LOG_BUILD ON
        BUILD_IN_SOURCE 1
    )

    ExternalProject_add(port-libeXosip2
        PREFIX "${PORTS_PREFIX}"
        URL "${PORTS_ORIGIN}/libeXosip2-${OSIP_VERSION}.tar.gz"
        PATCH_COMMAND cmake -E copy ${CMAKE_CURRENT_SOURCE_DIR}/libeXosip2.cmake CMakeLists.txt
        BUILD_COMMAND cmake --build . --config Release
        INSTALL_COMMAND cmake --build . --config Release --target install
        LOG_BUILD ON
        BUILD_IN_SOURCE 1
    )

    add_dependencies(port-libeXosip2 port-libosip2 port-openssl)
endif()

# Many Unix platforms have openssl as a default system library, Apple doesnt.

if(APPLE)
    # for release builds place openssl config in an easy to find output location
    if(PORTS_BUILD_TYPE MATCHES "Rel")
        set(OPENSSL_INSTALL_PREFIX "${PORTS_PREFIX}/ssl")
    else()
        set(OPENSSL_INSTALL_PREFIX "${OPENSSL_PREFIX}")
    endif()
    ExternalProject_add(port-openssl
        PREFIX "${PORTS_PREFIX}"
        URL "${PORTS_ORIGIN}/openssl-${OSSL_VERSION}.tar.gz"
        CONFIGURE_COMMAND ./Configure no-shared --openssldir=${OPENSSL_PREFIX} --prefix=${PORTS_PREFIX} darwin64-x86_64-cc
        BUILD_COMMAND make CC="${PORTS_SCRIPTS}/mac-clang.sh" -j ${NPROC} build_apps
        INSTALL_COMMAND make OPENSSLDIR="${OPENSSL_INSTALL_PREFIX}" install_sw
        BUILD_IN_SOURCE 1
    )
endif()

# libeXosip2 for unix platforms which have older versions in repo, and Apple

if(APPLE OR UNIX)
    ExternalProject_add(port-ares
        PREFIX "${PORTS_PREFIX}"
        URL "${PORTS_ORIGIN}/c-ares-${ARES_VERSION}.tar.gz"
        CONFIGURE_COMMAND ./configure --disable-shared --enable-static "--prefix=${PORTS_PREFIX}"
        BUILD_COMMAND make
        INSTALL_COMMAND make install
        BUILD_IN_SOURCE 1
    )

    ExternalProject_add(port-libosip2
        PREFIX "${PORTS_PREFIX}"
        URL "${PORTS_ORIGIN}/libosip2-${OSIP_VERSION}.tar.gz"
        CONFIGURE_COMMAND ./configure --enable-pthread --disable-shared --enable-static "--prefix=${PORTS_PREFIX}"
        BUILD_COMMAND make -j ${NPROC}
        INSTALL_COMMAND make install
        BUILD_IN_SOURCE 1
    )

    ExternalProject_add(port-libeXosip2
        PREFIX "${PORTS_PREFIX}"
        URL "${PORTS_ORIGIN}/libeXosip2-${OSIP_VERSION}.tar.gz"
        CONFIGURE_COMMAND ./configure --enable-pthread --disable-shared --enable-static --disable-tools --prefix=${PORTS_PREFIX}
        BUILD_COMMAND make -j ${NPROC}
        INSTALL_COMMAND make install
        BUILD_IN_SOURCE 1
    )
    add_dependencies(port-libeXosip2 port-libosip2 port-ares)
    if(APPLE)
        add_dependencies(port-libeXosip2 port-openssl)
    endif()

endif()
    
