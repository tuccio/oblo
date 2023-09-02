oblo_add_library(scene)

target_link_libraries(
    oblo_scene
    PUBLIC
    oblo::asset
    oblo::core
    PRIVATE
    nlohmann_json::nlohmann_json
    TinyGLTF::TinyGLTF
)