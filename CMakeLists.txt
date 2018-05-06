cmake_minimum_required(VERSION 3.1)
project(SipWitchQt VERSION 0.2.3 LANGUAGES CXX)
set(PROJECT_RELEASE "1")
set(PROJECT_COPYRIGHT "2017")

string(TOLOWER "${PROJECT_NAME}" PROJECT_ARCHIVE)
string(REGEX REPLACE "[.]" "," RC_VERSION ${PROJECT_VERSION})
set(RC_VERSION "${RC_VERSION},0")

if (POLICY CMP0063)
    cmake_policy(SET CMP0063 OLD)
endif()

if (POLICY CMP0058)
    cmake_policy(SET CMP0058 OLD)
endif()

if(UNIX AND (CMAKE_GENERATOR MATCHES "Unix"))
    set(CMAKE_MAINTAINER true)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/Custom.cmake OPTIONAL)
include(GNUInstallDirs)

find_package(Qt5 COMPONENTS Core Network Sql Gui Widgets REQUIRED)
find_package(Qt5LinguistTools)

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
    set(BOOTSTRAP_VENDOR_EXOSIP true)
    set(BOOTSTRAP_EXTERNAL true)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(system_libs libssl libcrypto advapi32 user32 crypt32 gdi32 ws2_32)
    set(desktop_rc "${CMAKE_CURRENT_BINARY_DIR}/desktop.rc")
    set(server_rc "${CMAKE_CURRENT_BINARY_DIR}/server.rc")
elseif(APPLE)    
    if(NOT EXISTS "/usr/local/opt/libexosip")
        message(FATAL_ERROR "*** requires: brew install libexosip")
    endif()

    find_library(IOKIT_LIBRARY IOKit)
    find_library(CORE_LIBRARY CoreFoundation)
    find_library(APPKIT_LIBRARY AppKit)
    set(system_libs ${APPKIT_LIBRARY} ${IOKIT_LIBRARY} ${CORE_LIBRARY})
    include_directories("/usr/local/opt/libosip/include" "/usr/local/opt/libexosip/include")
    link_directories("/usr/local/opt/libosip/lib" "/usr/local/opt/libexosip/lib")
    find_path(MACDEPLOYQT_PATH macdeployqt PATH_SUFFIXES bin)

    set(DESKTOP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/Resources/logo.icns")
    set(DESKTOP_NAME  "SipWitchQt Desktop")
    set(MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/desktop.plist")
    set(MACOSX_BUNDLE_ICON_FILE "logo.icns")
    set(MACOSX_BUNDLE_DISPLAY_NAME "SipWitchQt Desktop")
    set(MACOSX_BUNDLE_INFO_STRING "SipWitchQt Desktop - Version ${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}-${PROJECT_RELEASE}")
    set(MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_BUNDLE_NAME "SipWitchQt Desktop")
    set(MACOSX_BUNDLE_COPYRIGHT "Copyright ${PROJECT_COPYRIGHT} Tycho Softworks.")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.tychosoft.sipwitchqt")
else()
    if(EXISTS "/etc/redhat-release")
        set(BOOTSTRAP_VENDOR_EXOSIP true)
    endif()

    if(BOOTSTRAP_VENDOR_EXOSIP)
        set(system_libs ssl crypto pthread)
    else()
        set(system_libs crypto)
    endif()
    include_directories("/usr/local/include")
    link_directories("/usr/local/lib")
    include(FindPkgConfig)
    if(NOT BOOTSTRAP_VENDOR_EXOSIP)
        pkg_check_modules(OSIP2 libosip2)
    endif()
    pkg_check_modules(SYSTEMD libsystemd)
    pkg_check_modules(ZEROCONF avahi-client>=0.3.0)
    if(NOT OSIP2_FOUND AND NOT BOOTSTRAP_VENDOR_EXOSIP)
        message(FATAL_ERROR "*** requires osip2 and eXosip2 installed")
    endif()
    if(SYSTEMD_FOUND)
        set(server_libs "${server_libs}" ${SYSTEMD_LIBRARIES})
    endif()
    if(ZEROCONF_FOUND)
        set(server_libs "${server_libs}" ${ZEROCONF_LIBRARIES})
    endif()
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(system_libs "${system_libs}" -ltcmalloc_minimal)
    endif()
endif()

# Debug builds always run in repeatable pre-set configurations for testing
# and userdata allows one to create a private server testing configuration.
# Release builds operate as normal applications without console debug, and
# with a preference for poduction as RelWithDebInfo.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/userdata/")
        set(PROJECT_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/userdata")
        set(PRELOAD_DATABASE true)
    else()
        set(PRELOAD_DATABASE true)
        set(PROJECT_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/testdata")
    endif()
    set(DESKTOP_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/testdata")
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
file(GLOB database_sql Database/*.sql)
file(GLOB desktop_ts Translations/sipwitchqt-desktop_*.ts)

if(BOOTSTRAP_EXTERNAL)
    add_subdirectory(vendor/external)
    set(BOOTSTRAP_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/vendor/external")
    set(BOOTSTRAP_PATH "${BOOTSTRAP_PREFIX}/bin")
    include_directories("${BOOTSTRAP_PREFIX}/include")
    link_directories("${BOOTSTRAP_PREFIX}/lib")
endif()

if(BOOTSTRAP_VENDOR_EXOSIP)
    include_directories(BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/libosip2/include")
    include_directories(BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/libeXosip2/include")
endif()

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/translations_autogen")
set_source_files_properties(${desktop_ts} PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/translations_autogen")
qt5_add_translation(desktop_qm ${desktop_ts})

set(ts_desktop ${desktop_src} ${desktop_hdr} ${desktop_uic} ${dialogs_hdr} ${dialogs_src} ${dialogs_uic} ${connect_hdr} ${connect_src})
add_custom_target(translations SOURCES ${desktop_ts}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND ${Qt5_LUPDATE_EXECUTABLE} ${ts_desktop} -ts translations/sipwitchqt-desktop_es.ts
) 

if(APPLE)
    set_source_files_properties(${DESKTOP_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    set(desktop_mm Desktop/macos.mm)
    set(server_mm Server/macos.mm)
endif()

set(voip_libs eXosip2 osip2 osipparser2)

add_executable(desktop-app WIN32 MACOSX_BUNDLE ${DESKTOP_ICON} ${common_src} ${common_hdr} ${connect_src} ${connect_hdr} ${desktop_src} ${desktop_hdr} ${desktop_uic} ${dialogs_src} ${dialogs_hdr} ${dialogs_uic} ${desktop_mm} ${desktop_qm} ${desktop_rc} Resources/desktop.qrc)
target_link_libraries(desktop-app Qt5::Core Qt5::Network Qt5::Gui Qt5::Widgets Qt5::Sql ${voip_libs} ${desktop_libs} ${system_libs})
set_target_properties(desktop-app PROPERTIES
    OUTPUT_NAME "${DESKTOP_NAME}"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/desktop.plist.in"
)

add_executable(server-app ${common_src} ${common_hdr} ${database_src} ${database_hdr} ${server_src} ${server_hdr} ${server_mm} ${server_rc})
target_link_libraries(server-app Qt5::Core Qt5::Network Qt5::Sql ${voip_libs} ${server_libs} ${system_libs})
set_target_properties(server-app PROPERTIES OUTPUT_NAME "sipwitchqt-server")

if(WIN32)
    set(DIST_FORMAT zip)
else()
    set(DIST_FORMAT tar.gz)
endif()

add_custom_target(dist
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E remove -F "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_ARCHIVE}-*.${DIST_FORMAT}"
    COMMAND git archive -o "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_ARCHIVE}-${PROJECT_VERSION}.${DIST_FORMAT}" --format ${DIST_FORMAT} --prefix="${PROJECT_ARCHIVE}-${PROJECT_VERSION}/" "v${PROJECT_VERSION}" 2>/dev/null || git archive -o "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_ARCHIVE}-${PROJECT_VERSION}.${DIST_FORMAT}" --format ${DIST_FORMAT} --prefix="${PROJECT_ARCHIVE}-${PROJECT_VERSION}/" HEAD
)

if(BOOTSTRAP_VENDOR_EXOSIP)
    add_subdirectory(vendor/libosip2)
    add_subdirectory(vendor/libeXosip2)
    add_dependencies(server-app eXosip2 osip2 osipparser2)
    add_dependencies(desktop-app eXosip2 osip2 osipparser2)
endif()

if(BOOTSTRAP_EXTERNAL)
    add_dependencies(eXosip2 openssl)
endif()

add_custom_target(support-files SOURCES desktop.plist.in sipwitchqt.spec.in desktop.rc.in server.rc.in config.hpp.in setup.iss.in Doxyfile.in xdg/sipwitchqt.desktop xdg/sipwitchqt.appdata.xml LICENSE ${markdown} etc/sipwitchqt.conf etc/sipwqt-sleep.sh etc/run testdata/service.conf testdata/siptest.sh ${sipp_xml} ${database_sql})

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

install(FILES ${desktop_qm} DESTINATION "${CMAKE_INSTALL_DATADIR}/translations")

if(UNIX AND NOT APPLE)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/xdg/sipwitchqt.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/xdg/sipwitchqt.appdata.xml" DESTINATION "${CMAKE_INSTALL_DATADIR}/appdata")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Resources/logo.png" DESTINATION "${CMAKE_INSTALL_DATADIR}/pixmaps" RENAME sipwitchqt.png)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/etc/sipwitchd.service" DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/systemd/system")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/etc/sipwqt-sleep.sh" DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/systemd/system-sleep")
endif()

install(FILES "${CMAKE_CURRENTT_SOURCE_DIR}/Database/mysql.sql" DESTINATION "${CMAKE_INSTALL_DATADIR}/schemas" RENAME sipwitch-mysql.sql)
install(FILES "${CMAKE_CURRENTT_SOURCE_DIR}/Database/sqlite.sql" DESTINATION "${CMAKE_INSTALL_DATADIR}/schemas" RENAME sipwitch-sqlite.sql)

# staging releases; we do not stage debug because it uses testdata...

if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(APPLE)
        add_custom_command(TARGET desktop-app POST_BUILD
            COMMAND dsymutil "${DESKTOP_NAME}.app/Contents/MacOS/${DESKTOP_NAME}"
            COMMAND rm -rf "${DESKTOP_NAME}.dSYM"
            COMMAND mv "${DESKTOP_NAME}.app/Contents/MacOS/${DESKTOP_NAME}.dSYM" "${DESKTOP_NAME}.dSYM"
            COMMAND mkdir -p "${DESKTOP_NAME}.app/Contents/Translations"
            COMMAND cp -a "${CMAKE_CURRENT_BINARY_DIR}/translations_autogen/sipwitchqt-desktop_*.qm" "${DESKTOP_NAME}.app/Contents/Translations"
            COMMAND "${MACDEPLOYQT_PATH}/macdeployqt" "${DESKTOP_NAME}.app" -always-overwrite -appstore-compliant ${APPSTORE_SIGN}
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
            COMMAND xcopy /q "translations_autogen\\*.qm" "stage\\translations" >nul
            COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" stage/COPYING.txt
            COMMAND iscc setup.iss
            COMMAND "${CMAKE_COMMAND}" -E rename Setup.exe stage/Setup.exe
            COMMAND cd stage && zip -rq ../stage.zip * && cd ..
            COMMAND "${CMAKE_COMMAND}" -E rename stage.zip stage/sipwitchqt-desktop.zip
            COMMAND zip -q stage/Symbols.zip *.pdb
            COMMAND cd stage && zip Setup.zip Setup.exe && cd ..
        )
        add_dependencies(stage dist)
    endif()
endif()

# these targets mostly used in makefiles only...

if(CMAKE_MAINTAINER)
    add_custom_target(distclean
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMAND ${CMAKE_BUILD_TOOL} clean
        COMMAND "${CMAKE_COMMAND}" -E remove -F "${PROJECT_ARCHIVE}-*.tar.gz"
        COMMAND "${CMAKE_COMMAND}" -E remove -F "${PROJECT_ARCHIVE}-*.rpm"
        COMMAND "${CMAKE_COMMAND}" -E remove -F "${CMAKE_CURRENT_BINARY_DIR}-*.changes"
        COMMAND "${CMAKE_COMMAND}" -E remove -F "${CMAKE_CURRENT_BINARY_DIR}-*.dsc"
        COMMAND "${CMAKE_COMMAND}" -E remove -F "${CMAKE_CURRENT_BINARY_DIR}-*.deb"
        COMMAND "${CMAKE_COMMAND}" -E remove -F "${CMAKE_CURRENT_SOURCE_DIR}/testdata/*.db"
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

