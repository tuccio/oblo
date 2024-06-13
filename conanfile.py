from conan import ConanFile
from conan.tools.files import copy
from itertools import chain
from os import path

class ObloConanRecipe(ConanFile):
    name = "oblo"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"

    options = {
        "is_multiconfig": [True, False],
        "with_tracy": [True, False],
    }

    default_options = {
        "is_multiconfig": False,
        "with_tracy": False,
    }

    def requirements(self):
        self.requires("assimp/5.0.1")
        self.requires("cxxopts/2.2.1")
        self.requires("efsw/1.3.1")
        self.requires("glew/2.1.0")
        self.requires("glslang/11.7.0")
        self.requires("gtest/1.10.0")
        self.requires("iconfontcppheaders/cci.20240128")
        self.requires("ktx/4.0.0")
        self.requires("imgui/1.89.9-docking", override=True)
        self.requires("imguizmo/cci.20231114")
        self.requires("nlohmann_json/3.11.2")
        self.requires("rapidjson/cci.20220822")
        self.requires("vulkan-headers/1.3.211.0", override=True)
        self.requires("vulkan-loader/1.3.211.0")
        self.requires("vulkan-memory-allocator/3.0.0")
        self.requires("spirv-cross/cci.20211113")
        self.requires("sdl/2.0.20")
        self.requires("stb/cci.20230920")
        self.requires("tinygltf/2.8.13")
        self.requires("xxhash/0.8.2")

        # This is only needed for unit tests
        self.requires("eigen/3.4.0")

        if self.options.with_tracy:
            self.requires("tracy/0.10")

    def configure(self):
        self.options["efsw/*"].shared = False
        self.options["eigen/*"].MPL2_only = True

        tinygltf = self.options["tinygltf/*"]
        tinygltf.stb_image = False
        tinygltf.stb_image_write = False

        stb = self.options["stb/*"]
        stb.with_deprecated = False

        glslang = self.options["glslang/*"]
        glslang.enable_optimizer = False
        glslang.spv_remapper = False
        glslang.hlsl = False
        glslang.build_executables = False

        if self.options.with_tracy:
            tracy = self.options["tracy/*"]
            tracy.enable = True
            tracy.shared = True

    def generate(self):
        imgui = self.dependencies["imgui"]
        src_dir = f"{imgui.package_folder}/res/bindings/"

        for backend in ["opengl3", "sdl2", "vulkan"]:
            copy(self, f"imgui_impl_{backend}.h", src_dir, f"{self.recipe_folder}/3rdparty/imgui/{backend}/include")
            copy(self, f"imgui_impl_{backend}_*", src_dir, f"{self.recipe_folder}/3rdparty/imgui/{backend}/src")
            copy(self, f"imgui_impl_{backend}.cpp", src_dir, f"{self.recipe_folder}/3rdparty/imgui/{backend}/src")

        out_bin_dir = path.join(self.build_folder, "..", "out", "bin")

        if self.options.is_multiconfig:
            if self.settings.build_type == "Debug":
                out_dirs = [path.join(out_bin_dir, "Debug")]
            elif self.settings.build_type == "Release":
                out_dirs = [path.join(out_bin_dir, "Release"), path.join(out_bin_dir, "RelWithDebInfo")]
            else:
                raise ValueError("Unsupported configuration")
        else:
                out_dirs = [out_bin_dir]

        for out_dir in out_dirs:
            for dep in self.dependencies.values():
                for bin_dir in chain(dep.cpp_info.libdirs, dep.cpp_info.bindirs):
                    # Some relative paths that are just lib/bin end up copying our own files recursively
                    # To fix that, check if the path is absolute
                    if path.isabs(bin_dir):
                        copy(self, "*.dylib", bin_dir, out_dir)
                        copy(self, "*.dll", bin_dir, out_dir)