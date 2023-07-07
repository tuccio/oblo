add_subdirectory(imgui)

macro(oblo_3rdparty_add library)
    set_property(GLOBAL APPEND PROPERTY oblo_3rdparty_targets ${library})
endmacro(oblo_3rdparty_add)

oblo_3rdparty_add(imgui)
oblo_3rdparty_add(imgui_sdl_vk)
oblo_3rdparty_add(imgui_sdl_gl3)