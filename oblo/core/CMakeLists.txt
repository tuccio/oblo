oblo_add_library(core)

find_package(utf8cpp  REQUIRED)
find_package(xxHash REQUIRED)

target_link_libraries(
    oblo_core
    PUBLIC
    xxhashct::xxhashct
    PRIVATE
    utf8cpp
    xxHash::xxhash
)

find_package(Tracy)

if(Tracy_FOUND)
    target_link_libraries(oblo_core PUBLIC Tracy::TracyClient)
endif()

find_package(Eigen3 REQUIRED)

target_link_libraries(oblo_test_core PUBLIC Eigen3::Eigen)

# Adding the visualizer here is enough for the MSVC solution
target_sources(oblo_core PRIVATE ${CMAKE_SOURCE_DIR}/visualizers/oblo-core.natvis)