cmake_minimum_required(VERSION 3.10.0)
project(TinyWebServer VERSION 0.1.0 LANGUAGES C CXX)
add_executable(TinyWebServer sql_connection_pool.cpp http_conn.cpp main.cpp)
find_package(Threads REQUIRED)

# Gtest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)
add_executable(parser_test test/parser_test.cpp http_conn.cpp sql_connection_pool.cpp)
add_executable(process_test test/process_test.cpp http_conn.cpp sql_connection_pool.cpp)
add_executable(connection_pool_test test/database_conn_test.cpp sql_connection_pool.cpp)

target_link_libraries(TinyWebServer gtest mysqlclient Threads::Threads)
target_link_libraries(parser_test gtest_main mysqlclient Threads::Threads)
target_link_libraries(process_test gtest_main mysqlclient Threads::Threads)
target_link_libraries(connection_pool_test gtest_main mysqlclient Threads::Threads)
include(CTest)
enable_testing()
add_test(NAME HttpParserTest COMMAND parser_test)
add_test(NAME HttpProcessTest COMMAND process_test)
add_test(NAME DatabasePoolTest COMMAND connection_pool_test)