cmake_minimum_required(VERSION 3.10)

project(felixtoken)

set(EOSIO_WASM_OLD_BEHAVIOR "Off")
find_package(eosio.cdt)

add_contract( felixtoken felixtoken felixtoken.cpp )
target_include_directories( felixtoken PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( felixtoken ${CMAKE_SOURCE_DIR}/../ricardian )