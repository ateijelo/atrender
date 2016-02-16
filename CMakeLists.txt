project(atrender)
cmake_minimum_required(VERSION 2.8)

add_executable(${PROJECT_NAME}
    main.cpp
    tilestore.h
    tilestore.cpp
    directorytilestore.h
    directorytilestore.cpp
    mbtiles.h
    mbtiles.cpp
)

find_library(SQLITE3 sqlite3)

target_compile_options(${PROJECT_NAME} PRIVATE --std=c++11 -Wall -g)
target_link_libraries(${PROJECT_NAME}
    mapnik icuuc pthread boost_program_options
    boost_system boost_filesystem mbedcrypto
    ${SQLITE3}
)
