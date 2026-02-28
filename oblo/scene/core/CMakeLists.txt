oblo_add_library(scene MODULE
    GENERATE_HEADERS_TARGET
)

find_package(Ktx REQUIRED)
find_package(RapidJSON REQUIRED)
find_package(TinyGLTF REQUIRED)
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
    TinyGLTF::Defines
    TinyGLTF::TinyGLTF
    Vulkan::Headers
)

target_link_libraries(
    oblo_test_scene
    PRIVATE
    oblo::properties
)