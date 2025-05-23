function(_oblo_init_module_load_type type)
    add_custom_target(oblo_register_module_${type})
    set_target_properties(oblo_register_module_${type} PROPERTIES oblo_registered_modules "")
endfunction(_oblo_init_module_load_type)

_oblo_init_module_load_type(asset)
_oblo_init_module_load_type(core)
_oblo_init_module_load_type(editor)

function(_oblo_register_module_as module type)
    get_target_property(
        _modules
        oblo_register_module_${type}
        oblo_registered_modules
    )

    list(APPEND _modules ${module})

    set_target_properties(
        oblo_register_module_${type}
        PROPERTIES
        oblo_registered_modules "${_modules}"
        FOLDER ${OBLO_FOLDER_INTERNAL}
    )

    add_dependencies(oblo_register_module_${type} oblo_${module})
endfunction(_oblo_register_module_as)

function(_oblo_register_module_loader_as module type)
    # Name prefix added by oblo_add_library
    set(_module_prefix oblo_)

    set(_include_dir ${CMAKE_CURRENT_BINARY_DIR}/include)
    set(_gen_file ${_include_dir}/module_loader_${type}.gen.hpp)

    file(GENERATE OUTPUT ${_gen_file} CONTENT
        "// This file is generated from CMake, do not modify.
#include <oblo/modules/module_manager.hpp>

namespace oblo::gen
{
    inline void load_modules_${type}()
    {
        [[maybe_unused]] auto& mm = module_manager::get();

$<LIST:JOIN,$<LIST:TRANSFORM,$<TARGET_PROPERTY:oblo_register_module_${type},oblo_registered_modules>,REPLACE,(.+),        mm.load(\"${_module_prefix}\\0\")>,;\n>;
    }
}")
    set(_target ${_module_prefix}${module})

    # Make sure we inherit all the dependencies, so that the modules are built before the loader
    if (NOT TARGET ${_target})
        set(_target ${module})
    endif()

    add_dependencies(${_target} oblo_register_module_${type})

    target_include_directories(${_target} PRIVATE ${_include_dir})
endfunction(_oblo_register_module_loader_as)

function(oblo_register_core_module module)
    _oblo_register_module_as(${module} core)
endfunction(oblo_register_core_module)

function(oblo_register_core_module_loader module)
    _oblo_register_module_loader_as(${module} core)
endfunction(oblo_register_core_module_loader)

function(oblo_register_asset_module module)
    _oblo_register_module_as(${module} asset)
endfunction(oblo_register_asset_module)

function(oblo_register_asset_module_loader module)
    _oblo_register_module_loader_as(${module} asset)
endfunction(oblo_register_asset_module_loader)

function(oblo_register_editor_module module)
    _oblo_register_module_as(${module} editor)
endfunction(oblo_register_editor_module)

function(oblo_register_editor_module_loader module)
    _oblo_register_module_loader_as(${module} editor)
endfunction(oblo_register_editor_module_loader)