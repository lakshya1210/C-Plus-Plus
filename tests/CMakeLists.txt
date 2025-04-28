cmake_minimum_required(VERSION 3.10)

# Find GTest package
find_package(GTest)

if(GTEST_FOUND)
    # Include GTest headers
    include_directories(${GTEST_INCLUDE_DIRS})
    
    # Add test executable
    add_executable(deribit_tests 
        test_main.cpp
        test_api_client.cpp
        test_order_manager.cpp
        test_websocket_server.cpp
        test_performance_monitor.cpp
    )
    
    # Link libraries
    target_link_libraries(deribit_tests
        PRIVATE
        ${GTEST_BOTH_LIBRARIES}
        ${Boost_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        Threads::Threads
    )
    
    # Add test to CTest
    add_test(NAME deribit_tests COMMAND deribit_tests)
else()
    message(WARNING "GTest not found, tests will not be built")
endif()