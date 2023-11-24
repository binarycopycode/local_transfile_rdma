cmake_minimum_required(VERSION 3.6)
project(main_server)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC -pipe -std=c++17 -fno-exceptions")

set(CLIENT_SOURCES_FILES
    src/rdma_common.cc
    src/rdma_client.cc
    src/client_test.cc
)

set(SERVER_SOURCES_FILES
	src/rdma_common.cc
	src/rdma_server.cc
	src/server_test.cc
)

include_directories("${PROJECT_SOURCE_DIR}" "/usr/local/include/")


add_executable(client_test ${CLIENT_SOURCES_FILES})
target_include_directories(client_test
        PUBLIC  ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(client_test ibverbs)



add_executable(server_test ${SERVER_SOURCES_FILES})
target_include_directories(server_test
        PUBLIC  ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(server_test ibverbs)
