cmake_minimum_required(VERSION 3.1)
Project(libeXosip2 VERSION 4.1.0 LANGUAGES C)

if (POLICY CMP0063)
    cmake_policy(SET CMP0063 OLD)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_INSTALL_PREFIX}/include)
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../libosip2/include)

add_definitions(-DOSIP_MT -DHAVE_OPENSSL_SSL_H)
if(WIN32)
    add_definitions(-DWIN32)
else()
    add_definitions(-DHAVE_SYS_TYPES_H -DHAVE_SYS_STAT_H -DHAVE_FCNTL_H -DHAVE_CTYPE_H -DHAVE_PTHREAD_H -DHAVE_SEMAPHORE_H -DHAVE_STRUCT_TIMEVAL -DHAVE_TIME_H -DHAVE_PTHREAD -DHAVE_UNISTD_H)
    if(${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")  
        add_definitions(-DHAVE_SYS_SELECT_H)
    endif()
endif()

file(GLOB eXosip2_src ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
file(GLOB eXosip2_inc ${CMAKE_CURRENT_SOURCE_DIR}/include/eXosip2/*.h)

add_library(eXosip2 STATIC ${eXosip2_src} ${eXosip2_inc})

if(VENDOR_INSTALL)
    install(FILES ${eXosip2_inc} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/eXosip2)
    install(TARGETS eXosip2 DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
endif()
