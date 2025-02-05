oblo_add_library(scene MODULE)

find_package(ktx REQUIRED)
find_package(tinygltf REQUIRED)

target_link_libraries(
    oblo_scene
    PUBLIC
    oblo::core
    oblo::ecs
    oblo::modules
    oblo::resource
    PRIVATE
    oblo::log
    oblo::properties
    KTX::ktx
    TinyGLTF::TinyGLTF
)

target_link_libraries(
    oblo_test_scene
    PRIVATE
    oblo::properties
)