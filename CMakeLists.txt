cmake_minimum_required(VERSION 3.1)
include("${CMAKE_CURRENT_SOURCE_DIR}/Custom/include.cmake" OPTIONAL)
project(SipWitchQt${CUSTOM_SUFFIX} VERSION 0.4.0 LANGUAGES CXX)
set(PROJECT_RELEASE "1")
set(PROJECT_COPYRIGHT "2017-2018")

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

include(GNUInstallDirs)
if(${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")  
    set(QT_QMAKE_EXECUTABLE "/usr/local/lib/qt5/bin/qmake")
    set(Qt5_DIR "/usr/local/lib/qt5/cmake/Qt5")
    set(Qt5Core_DIR "/usr/local/lib/qt5/cmake/Qt5Core")
    set(CUSTOM_CONTAINER "OpenBSD") # kill desktop...
    set(BOOTSTRAP_VENDOR_EXOSIP true)
endif()

if(CUSTOM_CONTAINER)
    find_package(Qt5 COMPONENTS Core Network Sql REQUIRED)
else()
    find_package(Qt5 COMPONENTS Core Network Sql Gui Widgets REQUIRED)
    find_package(Qt5LinguistTools)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN 1)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(DESKTOP_NAME "sipwitchqt-desktop")

if(WIN32)
    set(PROJECT_PREFIX "C:/ProgramData/SipWitchQt${CUSTOM_SUFFIX}")
    set(BOOTSTRAP_VENDOR_EXOSIP true)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
    set(system_libs advapi32 user32 crypt32 gdi32 ws2_32)
    set(desktop_rc "${CMAKE_CURRENT_BINARY_DIR}/desktop.rc")
    set(server_rc "${CMAKE_CURRENT_BINARY_DIR}/server.rc")
    find_path(WINDEPLOYQT_PATH windeployqt.exe PATH_SUFFIXES bin)
elseif(APPLE)    
    if(NOT EXISTS "/usr/local/opt/libexosip")
        message(FATAL_ERROR "*** requires: brew install libexosip")
    endif()

    set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(CORE_LIBRARY CoreFoundation)
    find_library(APPKIT_LIBRARY AppKit)
    set(system_libs ${APPKIT_LIBRARY} ${IOKIT_LIBRARY} ${CORE_LIBRARY})
    include_directories("/usr/local/opt/libosip/include" "/usr/local/opt/libexosip/include")
    link_directories("/usr/local/opt/libosip/lib" "/usr/local/opt/libexosip/lib")
    find_path(MACDEPLOYQT_PATH macdeployqt PATH_SUFFIXES bin)

    set(DESKTOP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/Resources/logo.icns")
    set(DESKTOP_NAME  "SipWitchQt Desktop")
    set(DESKTOP_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/desktop.plist.in")
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
    if(BOOTSTRAP_VENDOR_EXOSIP)
        set(system_libs pthread)
    endif()
    include_directories("/usr/local/include")
    link_directories("/usr/local/lib")
    include(FindPkgConfig)
    if(NOT BOOTSTRAP_VENDOR_EXOSIP)
        pkg_check_modules(OSIP2 libosip2)
    endif()
    pkg_check_modules(SYSTEMD libsystemd)
    if(NOT CUSTOM_CONTAINER OR CUSTOM_CONTAINER MATCHES "avahi")
        pkg_check_modules(ZEROCONF avahi-client>=0.3.0)
    endif()
    if(NOT OSIP2_FOUND AND NOT BOOTSTRAP_VENDOR_EXOSIP)
        message(FATAL_ERROR "*** requires osip2 and eXosip2 installed")
    endif()
    if(SYSTEMD_FOUND)
        set(server_libs "${server_libs}" ${SYSTEMD_LIBRARIES})
    endif()
    if(ZEROCONF_FOUND)
        set(server_libs "${server_libs}" ${ZEROCONF_LIBRARIES})
    endif()
    find_library(HAVE_RT rt)
    if(HAVE_RT)
        set(server_libs "${server_libs}" ${HAVE_RT})
    endif()
endif()

# Third party installed/system libraries.  For windows, this
# is being supported with vcpkg.  For redhat and fedora, you 
# may need to use copr repo's; dyfet/develop for fedora and
# dyfet/stable for rhel7/centos7.  Maybe one day fedora will
# have a modern exosip2 in the main repo.

# Required libraries
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
set(system_libs ${OPENSSL_LIBRARIES} ${system_libs})

# Optional libraries
find_library(HAVE_JEMALLOC jemalloc)
if(HAVE_JEMALLOC)
    set(system_libs "${system_libs}" ${HAVE_JEMALLOC})
endif()

# Customization
if(CUSTOM_NAME)
    set(DESKTOP_NAME "${CUSTOM_NAME}")
    set(PLIST_DISPLAY_NAME "${CUSTOM_NAME}")
    set(PLIST_BUNDLE_ID "${CUSTOM_ID}")
    set(CUSTOM_DESKTOP_RESOURCES "Custom/custom.qrc")
    set(TRANSLATIONS_OUTPUT_DIRECTORY "${PROJECT_ARCHIVE}")
    set(XDG "Custom")
    set(ETC "Custom")
    set(LOGO "Custom/logo.png")
    add_definitions("-DCUSTOMIZE_DESKTOP")
else()
    set(APPLICATION_NAME "${PROJECT_NAME} Desktop")
    set(APPLICATION_EXEC "${DESKTOP_NAME}")
    set(APPLICATION_DOMAIN "tychosoft.com")
    set(APPLICATION_ORG "Tycho Softworks")
    set(TRANSLATIONS_OUTPUT_DIRECTORY "translations")
    set(PLIST_DISPLAY_NAME "SipWitchQt Desktop")
    set(PLIST_BUNDLE_ID "com.tychosoft.sipwitchqt")
    set(LOGO "Resources/logo.png")
    set(ETC "etc")
    set(XDG "xdg")
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

if(BOOTSTRAP_VENDOR_EXOSIP)
    include_directories(BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/libosip2/include")
    include_directories(BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/libeXosip2/include")
endif()

if(NOT CUSTOM_CONTAINER)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/translations_autogen")
    set_source_files_properties(${desktop_ts} PROPERTIES OUTPUT_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/translations_autogen")
    qt5_add_translation(desktop_qm ${desktop_ts})

    set(ts_desktop ${desktop_src} ${desktop_hdr} ${desktop_uic} ${dialogs_hdr} ${dialogs_src} ${dialogs_uic} ${connect_hdr} ${connect_src})
    add_custom_target(translations SOURCES ${desktop_ts}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMAND ${Qt5_LUPDATE_EXECUTABLE} ${ts_desktop} -ts translations/sipwitchqt-desktop_es.ts
    ) 
endif()

if(APPLE)
    set_source_files_properties(${DESKTOP_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    set(desktop_mm Desktop/macos.mm)
    set(server_mm Server/macos.mm)
endif()

set(voip_libs eXosip2 osip2 osipparser2)

if(NOT CUSTOM_CONTAINER)
    add_executable(desktop-app WIN32 MACOSX_BUNDLE ${DESKTOP_ICON} ${common_src} ${common_hdr} ${connect_src} ${connect_hdr} ${desktop_src} ${desktop_hdr} ${desktop_uic} ${dialogs_src} ${dialogs_hdr} ${dialogs_uic} ${desktop_mm} ${desktop_qm} ${desktop_rc} ${CUSTOM_DESKTOP_RESOURCES} Resources/desktop.qrc)
    target_link_libraries(desktop-app Qt5::Core Qt5::Network Qt5::Gui Qt5::Widgets Qt5::Sql ${voip_libs} ${desktop_libs} ${system_libs})
    set_target_properties(desktop-app PROPERTIES
        OUTPUT_NAME "${DESKTOP_NAME}"
        MACOSX_BUNDLE_INFO_PLIST "${DESKTOP_PLIST}"
    )
endif()

if(NOT CUSTOM_DESKTOP)
    add_executable(server-app ${common_src} ${common_hdr} ${database_src} ${database_hdr} ${server_src} ${server_hdr} ${server_mm} ${server_rc} ${CUSTOM_SERVER_RESOURCES})
    target_link_libraries(server-app Qt5::Core Qt5::Network Qt5::Sql ${voip_libs} ${server_libs} ${system_libs})
    set_target_properties(server-app PROPERTIES OUTPUT_NAME "sipwitchqt-server")
endif()

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
    if(NOT CUSTOM_CONTAINER)
        add_dependencies(desktop-app eXosip2 osip2 osipparser2)
    endif()
endif()

add_custom_target(support-files SOURCES desktop.plist.in desktop.rc.in server.rc.in config.hpp.in setup.iss.in Doxyfile.in ${XDG}/${PROJECT_ARCHIVE}.desktop ${XDG}/${PROJECT_ARCHIVE}.appdata.xml LICENSE ${markdown} etc/sipwitchd.service etc/sipwitchqt-server.1 ${ETC}/sipwitchqt.default ${ETC}/sipwitchqt.conf etc/sipwqt-sleep.sh etc/run testdata/service.conf testdata/siptest.sh ${sipp_xml} ${database_sql})

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

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${ETC}/sipwitchqt.conf"
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}" 
    PERMISSIONS OWNER_READ OWNER_WRITE
)

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${ETC}/sipwitchqt.default"
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/default" RENAME "sipwitchqt"
)


install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/etc/run"
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/sv/sipwitchqt"
)

if(NOT CUSTOM_CONTAINER)
    install(TARGETS desktop-app DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES ${desktop_qm} DESTINATION "${CMAKE_INSTALL_DATADIR}/${TRANSLATIONS_OUTPUT_DIRECTORY}")
endif()

if(UNIX AND NOT APPLE AND NOT CUSTOM_CONTAINER)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${XDG}/${PROJECT_ARCHIVE}.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${XDG}/${PROJECT_ARCHIVE}.appdata.xml" DESTINATION "${CMAKE_INSTALL_DATADIR}/metainfo")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${XDG}/${DESKTOP_NAME}.1" DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${LOGO}" DESTINATION "${CMAKE_INSTALL_DATADIR}/pixmaps" RENAME sipwitchqt.png)
endif()
if(UNIX AND NOT APPLE)
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/etc/sipwitchqt-server.1" DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/etc/sipwitchd.service" DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/systemd/system")
    install(PROGRAMS "${CMAKE_CURRENT_SOURCE_DIR}/etc/sipwqt-sleep.sh" DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/systemd/system-sleep")
endif()

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Database/mysql.sql" DESTINATION "${CMAKE_INSTALL_DATADIR}/schemas" RENAME sipwitch-mysql.sql)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Database/sqlite.sql" DESTINATION "${CMAKE_INSTALL_DATADIR}/schemas" RENAME sipwitch-sqlite.sql)

# staging releases; we do not stage debug because it uses testdata...

if(NOT CUSTOM_CONTAINER AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(APPLE)
        if(CMAKE_GENERATOR MATCHES "Xcode")
            set(OUTPUT_PREFIX "${CMAKE_BINARY_DIR}/Debug")
        else()
            set(OUTPUT_PREFIX "${CMAKE_BINARY_DIR}")
        endif()
        add_custom_command(TARGET desktop-app POST_BUILD
            WORKING_DIRECTORY "${OUTPUT_PREFIX}"
            COMMAND dsymutil "${OUTPUT_PREFIX}/${DESKTOP_NAME}.app/Contents/MacOS/${DESKTOP_NAME}"
            COMMAND rm -rf "${DESKTOP_NAME}.dSYM"
            COMMAND mv "${DESKTOP_NAME}.app/Contents/MacOS/${DESKTOP_NAME}.dSYM" "${DESKTOP_NAME}.dSYM"
            COMMAND mkdir -p "${DESKTOP_NAME}.app/Contents/Translations"
            COMMAND cp -a "${CMAKE_CURRENT_BINARY_DIR}/translations_autogen/sipwitchqt-desktop_*.qm" "${DESKTOP_NAME}.app/Contents/Translations"
            COMMAND "${MACDEPLOYQT_PATH}/macdeployqt" "${DESKTOP_NAME}.app" -dmg -always-overwrite -appstore-compliant ${APPSTORE_SIGN}
        )
    endif()

    if(WIN32 AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
        add_custom_command(TARGET desktop-app POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E remove Setup.exe
            COMMAND "${CMAKE_COMMAND}" -E remove_directory stage
            COMMAND "${CMAKE_COMMAND}" -E make_directory stage
            COMMAND "${CMAKE_COMMAND}" -E copy sipwitchqt-desktop.exe stage
            COMMAND xcopy /q "*.dll" stage
            COMMAND "${WINDEPLOYQT_PATH}/windeployqt" --release --verbose=0 stage/sipwitchqt-desktop.exe
            COMMAND xcopy /q "translations_autogen\\*.qm" "stage\\translations" >nul
            COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" stage/COPYING.txt
            COMMAND iscc setup.iss
            COMMAND "${CMAKE_COMMAND}" -E rename Setup.exe stage/Setup.exe
            COMMAND cd stage && zip -rq ../stage.zip * && cd ..
            COMMAND "${CMAKE_COMMAND}" -E rename stage.zip stage/sipwitchqt-desktop.zip
            COMMAND zip -q stage/Symbols.zip *.pdb
            COMMAND cd stage && zip Setup.zip Setup.exe && cd ..
        )
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
 
    add_custom_target(pdf
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/doc/latex"
        COMMAND make
        COMMAND mv refman.pdf "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_ARCHIVE}.pdf"
        DEPENDS support-docs
    )
    
    # Patch in packaging support.  These are not part of master, but rather a
    # packaging branch.  This is done so that we can also overlay different
    # packaging directory content for custom and special purpose builds, much
    # like the Custom directory itself.  OS's that redistribute would in any
    # case maintain their own packaging.  Finally I tend to add things in my
    # include.cmake specific to my ci environment and where I upload our builds. 
    include("${CMAKE_CURRENT_SOURCE_DIR}/redhat/include.cmake" OPTIONAL)
    include("${CMAKE_CURRENT_SOURCE_DIR}/debian/include.cmake" OPTIONAL)
endif()

