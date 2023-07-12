from conan import ConanFile

class ObloConanRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators =  "CMakeDeps"
    default_options = {"glslang/*:enable_optimizer": False}

    def requirements(self):
        self.requires("assimp/5.0.1")
        self.requires("cxxopts/2.2.1")
        self.requires("cereal/1.3.0")
        self.requires("glew/2.1.0")
        self.requires("glslang/1.3.211.0")
        self.requires("gtest/1.10.0")
        self.requires("nlohmann_json/3.10.5")
        self.requires("vulkan-headers/1.3.211.0", override=True)
        self.requires("vulkan-loader/1.3.211.0")
        self.requires("vulkan-memory-allocator/3.0.0")
        self.requires("sdl/2.0.20")
