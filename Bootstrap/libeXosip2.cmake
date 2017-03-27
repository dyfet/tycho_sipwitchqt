cmake_minimum_required(VERSION 2.8)
Project(libeXosip2)

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR}/../..)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_INSTALL_PREFIX}/include)

add_definitions(-DOSIP_MT -DWIN32 -DHAVE_OPENSSL_SSL_H)

file(GLOB eXosip2_src ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
file(GLOB eXosip2_inc ${CMAKE_CURRENT_SOURCE_DIR}/include/eXosip2/*.h)

add_library(eXosip2 STATIC ${eXosip2_src} ${eXosip2_inc})

install(FILES ${eXosip2_inc} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/eXosip2)
install(TARGETS eXosip2 DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
