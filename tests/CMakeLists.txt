enable_testing()

set(TEST_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/test_framework.hpp")

add_executable(test_item
    unit/test_item.cpp
    ${TEST_HEADER}
)
target_include_directories(test_item PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_item PRIVATE DropAndDragCore Threads::Threads)
add_test(NAME test_item COMMAND test_item)

add_executable(test_db
    unit/test_db.cpp
    ${TEST_HEADER}
)
target_include_directories(test_db PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_db PRIVATE DropAndDragCore SQLite::SQLite3 Threads::Threads)
add_test(NAME test_db COMMAND test_db)

add_executable(test_search
    unit/test_search.cpp
    ${TEST_HEADER}
)
target_include_directories(test_search PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_search PRIVATE DropAndDragCore Threads::Threads)
add_test(NAME test_search COMMAND test_search)

add_executable(test_event_bus
    unit/test_event_bus.cpp
    ${TEST_HEADER}
)
target_include_directories(test_event_bus PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_event_bus PRIVATE DropAndDragCore Threads::Threads)
add_test(NAME test_event_bus COMMAND test_event_bus)

add_executable(test_thread_pool
    unit/test_thread_pool.cpp
    ${TEST_HEADER}
)
target_include_directories(test_thread_pool PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_thread_pool PRIVATE DropAndDragCore Threads::Threads)
add_test(NAME test_thread_pool COMMAND test_thread_pool)
