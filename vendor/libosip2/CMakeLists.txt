cmake_minimum_required(VERSION 3.1)
Project(libosip2 VERSION 5.0.0 LANGUAGES C)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
if(WIN32)
    add_definitions(-DOSIP_MT)
else()
    add_definitions(-DOSIP_MT -DHAVE_SYS_TYPES_H -DHAVE_SYS_STAT_H -DHAVE_FCNTL_H -DHAVE_CTYPE_H -DHAVE_PTHREAD_H -DHAVE_SEMAPHORE_H -DHAVE_STRUCT_TIMEVAL -DHAVE_TIME_H -DHAVE_PTHREAD -DHAVE_UNISTD_H)
endif()

file(GLOB osipparser2_src ${CMAKE_CURRENT_SOURCE_DIR}/src/osipparser2/*.c)
file(GLOB osipparser2_inc ${CMAKE_CURRENT_SOURCE_DIR}/include/osipparser2/*.h)
file(GLOB osipheaders2_inc ${CMAKE_CURRENT_SOURCE_DIR}/include/osipparser2/headers/*.h)

add_library(osipparser2 STATIC ${osipparser2_src} ${osipparser2_inc})

file(GLOB osip2_src ${CMAKE_CURRENT_SOURCE_DIR}/src/osip2/*.c)
file(GLOB osip2_inc ${CMAKE_CURRENT_SOURCE_DIR}/include/osip2/*.h)
add_library(osip2 STATIC ${osip2_src} ${osip2_inc})
add_dependencies(osip2 osipparser2)

install(FILES ${osipheaders2_inc} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/osipparser2/headers)
install(FILES ${osipparser2_inc} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/osipparser2)
install(FILES ${osip2_inc} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/osip2)
install(TARGETS osip2 osipparser2 DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
