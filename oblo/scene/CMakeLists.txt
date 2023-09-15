oblo_add_library(scene MODULE)

target_link_libraries(
    oblo_scene
    PUBLIC
    oblo::asset
    oblo::core
    oblo::modules
    PRIVATE
    oblo::engine
    nlohmann_json::nlohmann_json
    TinyGLTF::TinyGLTF
)