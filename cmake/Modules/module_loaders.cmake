function(_oblo_init_module_load_type type)
    add_custom_target(oblo_register_module_${type})
    set_target_properties(oblo_register_module_${type} PROPERTIES oblo_registered_modules "")
endfunction(_oblo_init_module_load_type)

_oblo_init_module_load_type(core)

function(_oblo_register_module_as module type)
    get_target_property(
        _modules
        oblo_register_module_${type}
        oblo_registered_modules
    )

    list(APPEND _modules ${module})

    set_target_properties(
        oblo_register_module_${type}
        PROPERTIES oblo_registered_modules "${_modules}"
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

        $<LIST:TRANSFORM,$<TARGET_PROPERTY:oblo_register_module_${type},oblo_registered_modules>,REPLACE,(.+),mm.load(\"${_module_prefix}\\0\");\n>
    }
}")

    # Make sure we inherit all the dependencies, so that the modules are built before the loader
    add_dependencies(oblo_${module} oblo_register_module_${type})

    target_include_directories(oblo_${module} PRIVATE ${_include_dir})
endfunction(_oblo_register_module_loader_as)

function(oblo_register_core_module module)
    _oblo_register_module_as(${module} core)
endfunction(oblo_register_core_module)

function(oblo_register_core_module_loader module)
    _oblo_register_module_loader_as(${module} core)
endfunction(oblo_register_core_module_loader)