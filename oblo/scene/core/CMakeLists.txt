oblo_add_library(scene MODULE)

find_package(ktx REQUIRED)
find_package(rapidjson REQUIRED)
find_package(tinygltf REQUIRED)
find_package(VulkanHeaders REQUIRED)

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
    rapidjson
    KTX::ktx
    TinyGLTF::TinyGLTF
    Vulkan::Headers
)

target_link_libraries(
    oblo_test_scene
    PRIVATE
    oblo::properties
)