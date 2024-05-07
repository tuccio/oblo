oblo_add_library(scene MODULE)

target_link_libraries(
    oblo_scene
    PUBLIC
    oblo::core
    oblo::ecs
    oblo::modules
    oblo::resource
    PRIVATE
    oblo::properties
    KTX::ktx
    nlohmann_json::nlohmann_json
    TinyGLTF::TinyGLTF
    Vulkan::Headers
)