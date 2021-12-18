cmake_minimum_required(VERSION 3.19)
project(lookup_table C)

set(CMAKE_C_STANDARD 99)

link_libraries(crypto m)

add_executable(optimize utils.c index.c optimize.c)
add_executable(build utils.c index.c hash.c build.c)
add_executable(sort utils.c index.c sort.c)
add_executable(merge utils.c index.c  merge.c)
add_executable(lookup utils.c index.c hash.c lookup.c)
add_executable(checksort utils.c index.c checksort.c)
add_executable(checklookup utils.c index.c hash.c checklookup.c)