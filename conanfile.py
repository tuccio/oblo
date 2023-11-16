from conan import ConanFile
from conan.tools.files import copy

class ObloConanRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators =  "CMakeDeps"

    def requirements(self):
        self.requires("assimp/5.0.1")
        self.requires("cxxopts/2.2.1")
        self.requires("efsw/1.3.1")
        self.requires("glew/2.1.0")
        self.requires("glslang/8.13.3559")
        self.requires("gtest/1.10.0")
        self.requires("ktx/4.0.0")
        self.requires("imgui/1.89.9-docking")
        self.requires("nlohmann_json/3.11.2")
        self.requires("rapidjson/cci.20220822")
        self.requires("vulkan-headers/1.3.211.0", override=True)
        self.requires("vulkan-loader/1.3.211.0")
        self.requires("vulkan-memory-allocator/3.0.0")
        self.requires("spirv-cross/cci.20211113")
        self.requires("sdl/2.0.20")
        self.requires("stb/cci.20230920")
        self.requires("tinygltf/2.8.13")

        # This is only needed for unit tests
        self.requires("eigen/3.4.0")

    def configure(self):
        self.options["efsw/*"].shared = False
        self.options["eigen/*"].MPL2_only = True
        
        tinygltf = self.options["tinygltf/*"]
        tinygltf.stb_image = False
        tinygltf.stb_image_write = False

        stb = self.options["stb/*"]
        stb.with_deprecated = False

    def generate(self):
        imgui = self.dependencies["imgui"]
        src_dir = f"{imgui.package_folder}/res/bindings/"

        for backend in ["opengl3", "sdl2", "vulkan"]:
            copy(self, f"imgui_impl_{backend}.h", src_dir, f"{self.recipe_folder}/3rdparty/imgui/{backend}/include")
            copy(self, f"imgui_impl_{backend}_*", src_dir, f"{self.recipe_folder}/3rdparty/imgui/{backend}/src")
            copy(self, f"imgui_impl_{backend}.cpp", src_dir, f"{self.recipe_folder}/3rdparty/imgui/{backend}/src")