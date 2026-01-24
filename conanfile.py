from conan import ConanFile
from conan.api.conan_api import ConanAPI
from conan.api.model.list import ListPattern
from conan.cli.cli import Cli
from conan.tools.files import copy, mkdir
from itertools import chain
import os


class ObloConanRecipe(ConanFile):
    name = "oblo"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"

    options = {
        "is_multiconfig": [True, False],
        "with_dotnet": [True, False],
        "with_tracy": [True, False],
    }

    default_options = {
        "is_multiconfig": False,
        "with_dotnet": True,
        "with_tracy": False,
    }

    def requirements(self):
        self._install_required_recipes()

        self.requires("assimp/5.4.3")
        self.requires("concurrentqueue/1.0.4")
        self.requires("cxxopts/2.2.1")
        self.requires("glslang/1.3.296.0")
        self.requires("gtest/1.10.0")
        self.requires("iconfontcppheaders/cci.20240128")
        self.requires("imgui/1.92.5-docking", override=True)
        self.requires("imguizmo/cci.20231114")
        self.requires("ktx/4.0.0")
        self.requires("meshoptimizer/0.20")
        self.requires("rapidjson/cci.20230929")
        self.requires("vulkan-headers/1.3.296.0", override=True)
        self.requires("vulkan-loader/1.3.290.0")
        self.requires("vulkan-memory-allocator/3.0.0")
        self.requires("spirv-cross/1.3.296.0")
        self.requires("stb/cci.20230920")
        self.requires("tinygltf/2.8.13")
        self.requires("utfcpp/4.0.1")
        self.requires("xxhash/0.8.2")

        # This is only needed for unit tests
        self.requires("eigen/3.4.0")

        if self.options.with_tracy:
            self.requires("tracy/0.10")

        if self.options.with_dotnet:
            self.requires("dotnet-sdk/9.0.203")

    def configure(self):
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
            tracy.on_demand = True

    def generate(self):
        imgui = self.dependencies["imgui"]
        src_dir = f"{imgui.package_folder}/res/bindings/"

        backends = {
            "Windows": ["win32"],
            "Linux": ["sdl2"],
        }

        for backend in backends[str(self.settings.os)]:
            copy(self, f"imgui_impl_{backend}.h", src_dir,
                 f"{self.recipe_folder}/3rdparty/imgui/{backend}/include")
            copy(self, f"imgui_impl_{backend}_*", src_dir,
                 f"{self.recipe_folder}/3rdparty/imgui/{backend}/src")
            copy(self, f"imgui_impl_{backend}.cpp", src_dir,
                 f"{self.recipe_folder}/3rdparty/imgui/{backend}/src")

        out_bin_dir = os.path.join(self.build_folder, "..", "out", "bin")

        if self.options.is_multiconfig:
            if self.settings.build_type == "Debug":
                out_dirs = [os.path.join(out_bin_dir, "Debug")]
            elif self.settings.build_type == "Release":
                out_dirs = [os.path.join(out_bin_dir, "Release"), os.path.join(
                    out_bin_dir, "RelWithDebInfo")]
            else:
                raise ValueError("Unsupported configuration")
        else:
            out_dirs = [out_bin_dir]

        for out_dir in out_dirs:
            for dep in self.dependencies.values():
                for bin_dir in chain(dep.cpp_info.libdirs, dep.cpp_info.bindirs):
                    # Some relative paths that are just lib/bin end up copying our own files recursively
                    # To fix that, check if the path is absolute
                    if os.path.isabs(bin_dir):
                        copy(self, "*.dylib", bin_dir, out_dir)
                        copy(self, "*.dll", bin_dir, out_dir)

        if self.options.with_dotnet:
            self._deploy_dotnet(out_bin_dir)

    def _install_required_recipes(self):
        conan_api = ConanAPI()
        conan_cli = Cli(conan_api)

        vulkanSdkVersion = "1.3.296.0"

        if not self._find_recipe(conan_api, f"spirv-tools/{vulkanSdkVersion}"):
            conan_cli.run(
                ["export", f"{self.recipe_folder}/conan/recipes/spirv-tools", "--version", vulkanSdkVersion])

        if not self._find_recipe(conan_api, f"glslang/{vulkanSdkVersion}"):
            conan_cli.run(
                ["export", f"{self.recipe_folder}/conan/recipes/glslang", "--version", vulkanSdkVersion])

        if not self._find_recipe(conan_api, f"dotnet-sdk/9.0.203"):
            conan_cli.run(
                ["export", f"{self.recipe_folder}/conan/recipes/dotnet-sdk", "--version", "9.0.203"])

    def _deploy_dotnet(self, dir):
        # Deploy .NET maintaining the install layout https://github.com/dotnet/designs/blob/main/accepted/2020/install-locations.md#net-core-install-layout
        _dotnet = self.dependencies["dotnet-sdk"]

        _dotnet_dir = os.path.join(dir, "dotnet")
        mkdir(self, _dotnet_dir)

        copy(self, "*", _dotnet.cpp_info.components["hostfxr"].bindirs[0], os.path.join(_dotnet_dir, "host"))
        copy(self, "*", _dotnet.cpp_info.components["runtime"].bindirs[0], os.path.join(_dotnet_dir, "shared"))

    def _find_recipe(self, conan_api: ConanAPI, package_ref: str) -> bool:
        try:
            r = conan_api.list.select(ListPattern(package_ref))
            return r != None
        except:
            return False