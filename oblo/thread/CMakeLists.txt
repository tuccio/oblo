oblo_add_library(thread MODULE)

find_package(concurrentqueue)

target_link_libraries(
    oblo_thread
    PUBLIC
    oblo::core
    PRIVATE
    concurrentqueue::concurrentqueue
)