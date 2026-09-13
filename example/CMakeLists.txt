add_executable(threadpool
    threadpoolTest.cpp
    ${PROJECT_SOURCE_DIR}/threadpool/src/Timestamp.cpp
)
target_include_directories(threadpool PRIVATE ${PROJECT_SOURCE_DIR}/threadpool/include)
target_link_libraries(threadpool pthread)
