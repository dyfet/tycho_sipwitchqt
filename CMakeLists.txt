cmake_minimum_required(VERSION 3.1)
project(SipWitchQt VERSION 0.1.0 LANGUAGES CXX)
string(TOLOWER "${PROJECT_NAME}" PROJECT_ARCHIVE)
set(PROJECT_RELEASE "1")

if (POLICY CMP0063)
  cmake_policy(SET CMP0063 OLD)
endif()

if(UNIX AND (CMAKE_GENERATOR MATCHES "Unix"))
    set(CMAKE_MAINTAINER true)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/Custom.cmake OPTIONAL)
include(GNUInstallDirs)

find_package(Qt5 5.5.1 COMPONENTS Core Network Sql Gui Widgets REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(WIN32)
    set(PROJECT_PREFIX "C:/ProgramData/SipWitchQt")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(system_libs ssleay32 libeay32 advapi32 user32 crypt32 gdi32 ws2_32)
elseif(APPLE)
    if(NOT EXISTS "/usr/local/opt/libexosip")
        message(FATAL_ERROR "*** requires: brew install libexosip")
    endif()

    find_library(IOKIT_LIBRARY IOKit)
    find_library(CORE_LIBRARY CoreFoundation)
    set(system_libs ${IOKIT_LIBRARY} ${CORE_LIBRARY})
    include_directories("/usr/local/opt/libosip/include" "/usr/local/opt/libexosip/include")
    link_directories("/usr/local/opt/libosip/lib" "/usr/local/opt/libexosip/lib")
else()
    include_directories("/usr/local/include")
    link_directories("/usr/local/lib")
    include(FindPkgConfig)
    pkg_check_modules(OSIP2 libosip2)
    pkg_check_modules(SYSTEMD libsystemd)
    if(NOT OSIP2_FOUND)
        message(FATAL_ERROR "*** requires osip2 and eXosip2 installed")
    endif()
    if(SYSTEMD_FOUND)
        set(system_libs "${system_libs}" ${SYSTEMD_LIBRARIES})
    endif()
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(system_libs "${system_libs}" -ltcmalloc_minimal)
    endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(PROJECT_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/testdata")
else()
    set(QT_NO_DEBUG_OUTPUT TRUE)
endif()

if(ENABLE_USERCONFIG)
    set(PROJECT_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/userdata")
endif()

configure_file(config.hpp.in config.hpp)

file(GLOB common_src Common/*.cpp)
file(GLOB connect_src Connect/*.cpp)
file(GLOB database_src Database/*.cpp)
file(GLOB server_src Server/*.cpp)
file(GLOB common_hdr Common/*.hpp)
file(GLOB connect_hdr Connect/*.hpp)
file(GLOB database_hdr Database/*.hpp)
file(GLOB server_hdr Server/*.hpp)
file(GLOB desktop_hdr Desktop/*.hpp)
file(GLOB desktop_src Desktop/*.cpp)
file(GLOB desktop_uic Desktop/*.ui)
file(GLOB markdown *.md)

set(voip_libs eXosip2 osip2 osipparser2)

add_executable(antisipate ${common_src} ${common_hdr} ${connect_src} ${connect_hdr} ${desktop_src} ${desktop_hdr} ${desktop_uic} Resources/desktop.qrc)
target_link_libraries(antisipate Qt5::Core Qt5::Network Qt5::Gui Qt5::Widgets ${voip_libs} ${system_libs})

add_executable(sipwitchqt ${common_src} ${common_hdr} ${database_src} ${database_hdr} ${server_src} ${server_hdr})
target_link_libraries(sipwitchqt Qt5::Core Qt5::Network Qt5::Sql ${voip_libs} ${system_libs})

add_custom_target(support SOURCES sipwitchqt.spec config.hpp.in Doxyfile.in)

add_subdirectory(utils)

find_program(DOXYGEN doxygen)
find_program(GRAPHVIZ dot)
if(DOXYGEN AND GRAPHVIZ)
    configure_file(Doxyfile.in Doxyfile)
    add_custom_target(docs SOURCES LICENSE ${markdown}
        COMMAND "${DOXYGEN}" Doxyfile
    )
else()
    add_custom_target(docs SOURCES LICENSE ${markdown})
endif()

install(TARGETS sipwitchqt DESTINATION "${CMAKE_INSTALL_SBINDIR}")

install(FILES "{CMAKE_CURRENT_SOURCE_DIR}/etc/sipwitchqt.conf"
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}"
)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/etc/run"
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/sv/sipwitchqt"
)

if(WIN32 AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    # bundling support
    find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt HINTS ${QTDIR} ENV QTDIR PATH_SUFFIXES bin)
    if(NOT STAGE_PREFIX)
        set(STAGE_PREFIX "../Archive")
    endif()
    add_custom_target(stage_programs
        DEPENDS sipwitchqt
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${STAGE_PREFIX}/Programs"
        COMMAND "${CMAKE_COMMAND}" -E copy $<TARGET_FILE:sipwitchqt> "${STAGE_PREFIX}/Programs"
    )
    add_custom_target(stage_data
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${STAGE_PREFIX}/Data/SipWitchQt"
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/etc/sipwitchqt.conf" "${STAGE_PREFIX}/Data/SipWitchQt/service.conf"
    )
    add_custom_target(stage_archive SOURCES README.md
        WORKING_DIRECTORY "${STAGE_PREFIX}/Programs"
        DEPENDS stage_programs stage_data
        COMMAND "${WINDEPLOYQT_EXECUTABLE}" -verbose=0 sipwitchqt.exe
    )
elseif(CMAKE_CROSSCOMPILING AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    # if cross-compiling, can also generate cross-build packages...
    set(CPACK_SYSTEM_NAME "${PROJECT_RELEASE}.cross")
    set(CPACK_PACKAGE_NAME "${PROJECT_ARCHIVE}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Sip server system daemon")
    set(CPACK_PACKAGE_VENDOR "Tycho Softworks")
    set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_STRIP_FILES TRUE)
    set(CPACK_RPM_PACKAGE_URL "https://gitlab.com/tychosoft/sipwitchqt")
    set(CPACK_RPM_PACKAGE_RELEASE ${PROJECT_RELEASE})
    set(CPACK_RPM_PACKAGE_REQUIRES "qt5-qtbase-mysql")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Tycho Softworks")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5sql5-mysql")
    include(CPack)
endif()

if(CMAKE_MAINTAINER)
    add_custom_target(dist
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E remove -F "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_ARCHIVE}-*.tar.gz"
        COMMAND git archive -o "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_ARCHIVE}-${PROJECT_VERSION}.tar.gz" --format tar.gz --prefix="${PROJECT_ARCHIVE}-${PROJECT_VERSION}/" "v${PROJECT_VERSION}" 2>/dev/null || git archive -o "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_ARCHIVE}-${PROJECT_VERSION}.tar.gz" --format tar.gz --prefix="${PROJECT_ARCHIVE}-${PROJECT_VERSION}/" HEAD   
    )

    add_custom_target(source
        DEPENDS dist
    )

    add_custom_target(distclean
        COMMAND ${CMAKE_BUILD_TOOL} clean
        COMMAND "${CMAKE_COMMAND}" -E remove -F "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_ARCHIVE}-*.tar.gz"
        COMMAND "${CMAKE_COMMAND}" -E remove -F "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_ARCHIVE}-*.rpm"
        COMMAND "${CMAKE_COMMAND}" -E remove -F "${CMAKE_CURRENT_BINARY_DIR}/*.pdf"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/doc"
    )

    add_custom_target(srpm
        DEPENDS dist
        COMMAND "${CMAKE_COMMAND}" -E remove -F "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_ARCHIVE}-*.rpm" 
        COMMAND rpmbuild -bs --nodeps "${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_ARCHIVE}.spec"
    )
    
    add_custom_target(pdf
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doc/latex"
        COMMAND make
        COMMAND mv refman.pdf "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_ARCHIVE}.pdf"
        DEPENDS docs
    )
endif()

