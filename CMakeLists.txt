cmake_minimum_required(VERSION 3.0.0)
project(naval_battle2 VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(naval_battle2_server server.c)
add_executable(naval_battle2_client client.c)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
