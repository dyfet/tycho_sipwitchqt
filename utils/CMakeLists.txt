# swlite-authorize, swlite-modify, swlite-extensions

FILE(GLOB man1 *.1)
FILE(GLOB ruby *.rb)

add_custom_target(manpages SOURCES ${man1})
add_custom_target(scripts SOURCES ${ruby})

install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/swlite-authorize.rb DESTINATION ${CMAKE_INSTALL_SBINDIR} RENAME swlite-authorize)

install(FILES ${man1} DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
