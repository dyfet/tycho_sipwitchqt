cmake_minimum_required(VERSION 3.1)
project(SipWitchQt VERSION 0.1.0 LANGUAGES CXX)
set(PROJECT_RELEASE "1")
set(PROJECT_COPYRIGHT "2017")

string(TOLOWER "${PROJECT_NAME}" PROJECT_ARCHIVE)
string(REGEX REPLACE "[.]" "," RC_VERSION ${PROJECT_VERSION})
set(RC_VERSION "${RC_VERSION},0")

if (POLICY CMP0063)
  cmake_policy(SET CMP0063 OLD)
endif()

if(UNIX AND (CMAKE_GENERATOR MATCHES "Unix"))
    set(CMAKE_MAINTAINER true)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/Custom.cmake OPTIONAL)
include(GNUInstallDirs)

# LinguistTools may be needed in the future...
find_package(Qt5 5.5.1 COMPONENTS Core Network Sql Gui Widgets REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(DESKTOP_NAME "sipwitchqt-desktop")

if(WIN32)
    set(PROJECT_PREFIX "C:/ProgramData/SipWitchQt")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(system_libs ssleay32 libeay32 advapi32 user32 crypt32 gdi32 ws2_32)
    set(desktop_rc "${CMAKE_CURRENT_BINARY_DIR}/desktop.rc")
    set(server_rc "${CMAKE_CURRENT_BINARY_DIR}/server.rc")
elseif(APPLE)
    if(NOT EXISTS "/usr/local/opt/libexosip")
        message(FATAL_ERROR "*** requires: brew install libexosip")
    endif()

    find_library(IOKIT_LIBRARY IOKit)
    find_library(CORE_LIBRARY CoreFoundation)
    find_library(APPKIT_LIBRARY AppKit)
    set(system_libs ${IOKIT_LIBRARY} ${CORE_LIBRARY})
    set(desktop_libs ${APPKIT_LIBRARY})
    include_directories("/usr/local/opt/libosip/include" "/usr/local/opt/libexosip/include")
    link_directories("/usr/local/opt/libosip/lib" "/usr/local/opt/libexosip/lib")
    find_path(MACDEPLOYQT_PATH macdeployqt PATH_SUFFIXES bin)

    set(DESKTOP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/Resources/logo.icns")
    set(DESKTOP_NAME  "SipWitchQt Desktop")
    set(MACOSX_BUNDLE_ICON_FILE "logo.icns")
    set(MACOSX_BUNDLE_DISPLAY_NAME "SipWitchQt Desktop")
    set(MACOSX_BUNDLE_INFO_STRING "SipWitchQt Desktop - Version ${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_BUNDLE_NAME "SipWitchQt Desktop")
    set(MACOSX_BUNDLE_COPYRIGHT "Copyright ${PROJECT_COPYRIGHT} Tycho Softworks.")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.tychosoft.sipwitchqt")
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

# Debug builds always run in repeatable pre-set configurations for testing
# and userdata allows one to create a private testing configuration.
# Release builds operate as normal applications without console debug, and
# with a preference for poduction as RelWithDebInfo.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(EXISTS "userdata/")
        set(PROJECT_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/userdata")
    else()
        # Assumes server will re-generate db as schema will change.  This
        # and preloading makes debug testing repeatable.  If you need
        # a persistent debug config, then create a userdata directory.
        file(REMOVE "testdata/local.db")
        file(REMOVE "testdata/client.db")
        set(PRELOAD_DATABASE true)
        set(PROJECT_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/testdata")
    endif()
    set(DESKTOP_PREFIX "${PROJECT_PREFIX}")
else()
    set(QT_NO_DEBUG_OUTPUT TRUE)
endif()

configure_file(config.hpp.in config.hpp)
configure_file(setup.iss.in setup.iss)
configure_file(desktop.rc.in desktop.rc)
configure_file(server.rc.in server.rc)
configure_file(sipwitchqt.spec.in sipwitchqt.spec)

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
file(GLOB dialogs_hdr Dialogs/*.hpp)
file(GLOB dialogs_src Dialogs/*.cpp)
file(GLOB dialogs_uic Dialogs/*.ui)
file(GLOB markdown *.md)
file(GLOB sipp_xml testdata/*.xml)

if(EXISTS "Translations/")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/translations")
    file(GLOB desktop_ts Translations/desktop_*.ts)
    set_source_files_properties(${desktop_ts} PROPERTIES OUTPUT_LOCATION "translations")
    qt5_add_translation(desktop_qm ${desktop_ts})
    add_custom_target(translate SOURCES ${desktop_ts})
endif()

if(APPLE)
    set_source_files_properties(${DESKTOP_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    set(desktop_mm Desktop/macos.mm)
endif()

set(voip_libs eXosip2 osip2 osipparser2)

add_executable(desktop-app WIN32 MACOSX_BUNDLE ${DESKTOP_ICON} ${common_src} ${common_hdr} ${connect_src} ${connect_hdr} ${desktop_src} ${desktop_hdr} ${desktop_uic} ${dialogs_src} ${dialogs_hdr} ${dialogs_uic} ${desktop_mm} ${desktop_qm} ${desktop_rc} Resources/desktop.qrc)
target_link_libraries(desktop-app Qt5::Core Qt5::Network Qt5::Gui Qt5::Widgets Qt5::Sql ${voip_libs} ${desktop_libs} ${system_libs})
set_target_properties(desktop-app PROPERTIES OUTPUT_NAME "${DESKTOP_NAME}")

add_executable(server-app ${common_src} ${common_hdr} ${database_src} ${database_hdr} ${server_src} ${server_hdr} ${server_rc})
target_link_libraries(server-app Qt5::Core Qt5::Network Qt5::Sql ${voip_libs} ${system_libs})
set_target_properties(server-app PROPERTIES OUTPUT_NAME "sipwitchqt-server")

add_custom_target(support-files SOURCES sipwitchqt.spec.in desktop.rc.in server.rc.in config.hpp.in setup.iss.in Doxyfile.in xdg/sipwitchqt.desktop xdg/sipwitchqt.appdata.xml LICENSE ${markdown} etc/sipwitchqt.conf etc/run testdata/service.conf testdata/siptest.sh ${sipp_xml})

add_subdirectory(utils)

find_program(DOXYGEN doxygen)
find_program(GRAPHVIZ dot)
if(DOXYGEN AND GRAPHVIZ)
    configure_file(Doxyfile.in Doxyfile)
    add_custom_target(docs
        COMMAND "${DOXYGEN}" Doxyfile
    )
endif()

install(TARGETS server-app DESTINATION "${CMAKE_INSTALL_SBINDIR}")
install(TARGETS desktop-app DESTINATION "${CMAKE_INSTALL_BINDIR}")

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/etc/sipwitchqt.conf"
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}"
)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/etc/run"
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/sv/sipwitchqt"
)

if(UNIX AND NOT APPLE)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/xdg/sipwitchqt.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/xdg/sipwitchqt.appdata.xml" DESTINATION "${CMAKE_INSTALL_DATADIR}/appdata")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Resources/logo.png" DESTINATION "${CMAKE_INSTALL_DATADIR}/pixmaps" RENAME sipwitchqt.png)
endif()

# staging releases; we do not stage debug because it uses testdata...

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(APPLE AND UNIX)
        add_custom_target(stage
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            DEPENDS desktop-app
            COMMAND rm -rf stage
            COMMAND mkdir -p stage
            COMMAND cp -a "${DESKTOP_NAME}.app" stage/
            COMMAND dsymutil "stage/${DESKTOP_NAME}.app/Contents/MacOS/${DESKTOP_NAME}"
            COMMAND mv "stage/${DESKTOP_NAME}.app/Contents/MacOS/${DESKTOP_NAME}.dSYM" stage/
            COMMAND "${MACDEPLOYQT_PATH}/macdeployqt" "stage/${DESKTOP_NAME}.app" -appstore-compliant ${APPSTORE_SIGN}
        )
    endif()

    if(WIN32)
        add_custom_target(stage
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            DEPENDS desktop-app server-app
            COMMAND "${CMAKE_COMMAND}" -E remove Setup.exe
            COMMAND "${CMAKE_COMMAND}" -E remove_directory stage
            COMMAND "${CMAKE_COMMAND}" -E make_directory stage
            COMMAND "${CMAKE_COMMAND}" -E copy sipwitchqt-desktop.exe stage
            COMMAND windeployqt --release --verbose=0 stage/sipwitchqt-desktop.exe
            COMMAND "${CMAKE_COMMAND}" -E copy "${BOOTSTRAP_PATH}/cares.dll" stage
            COMMAND "${CMAKE_COMMAND}" -E copy "${BOOTSTRAP_PATH}/zlib.dll" stage
            COMMAND "${CMAKE_COMMAND}" -E copy "${BOOTSTRAP_PATH}/ssleay32.dll" stage
            COMMAND "${CMAKE_COMMAND}" -E copy "${BOOTSTRAP_PATH}/libeay32.dll" stage
            COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" stage/COPYING.txt
            COMMAND iscc setup.iss
            COMMAND "${CMAKE_COMMAND}" -E rename Setup.exe stage/Setup.exe
            COMMAND cd stage && zip -rq ../stage.zip * && cd ..
            COMMAND "${CMAKE_COMMAND}" -E rename stage.zip stage/sipwitchqt-desktop.zip
            COMMAND zip -q stage/symbols.zip *.pdb
            COMMAND cd stage && zip Setup.zip Setup.exe && cd ..
        )
    endif()
endif()

# these targets mostly used in makefiles only...
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
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND ${CMAKE_BUILD_TOOL} clean
        COMMAND "${CMAKE_COMMAND}" -E remove -F "${PROJECT_ARCHIVE}-*.tar.gz"
        COMMAND "${CMAKE_COMMAND}" -E remove -F "${PROJECT_ARCHIVE}-*.rpm"
        COMMAND "${CMAKE_COMMAND}" -E remove -F "${CMAKE_CURRENT_BINARY_DIR}-*.changes"
        COMMAND "${CMAKE_COMMAND}" -E remove -F "${CMAKE_CURRENT_BINARY_DIR}-*.dsc"
        COMMAND "${CMAKE_COMMAND}" -E remove -F "${CMAKE_CURRENT_BINARY_DIR}-*.deb"
        COMMAND "${CMAKE_COMMAND}" -E remove -F "*.pdf"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "doc"
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
        DEPENDS support-docs
    )
endif()

