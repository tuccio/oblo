oblo_add_library(log SHARED)

find_package(concurrentqueue)

target_link_libraries(
    oblo_log
    PUBLIC
    oblo::core
    oblo::modules
    PRIVATE
    oblo::thread
    concurrentqueue::concurrentqueue
)