from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
from conan.tools.env import VirtualBuildEnv
from conan.tools.files import copy
from conan.tools.scm import Git
import os

required_conan_version = ">=1.54.0"

class ImGuiNodeEditorCOnan(ConanFile):
    name = "imgui-node-editor"
    homepage = "https://github.com/thedmd/imgui-node-editor"
    description = " Node Editor built using Dear ImGui "
    topics = ("middleware", "gamedev", "imgui",
              "immediate-gui", "game-development", "blueprint", "dear-imgui",
              "blueprints", "node-editor")
    license = "MIT"

    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }

    default_options = {
        "shared": False,
        "fPIC": True,
    }

    def export_sources(self):
        copy(self, "CMakeLists.txt", self.recipe_folder, self.export_sources_folder)
        
    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def requirements(self):
        self.requires(f"imgui/[>=1.91.5]")

    def source(self):
        git = Git(self)
        git.clone(url="https://github.com/thedmd/imgui-node-editor.git", target=".")
        git.checkout(commit="b302971455b3719ec9b5fb94b2f92d27c62b9ff0")

    def generate(self):
        env = VirtualBuildEnv(self)
        env.generate()

        tc = CMakeToolchain(self)
        tc.variables["SRC_DIR"] = self.source_folder.replace("\\", "/")
        tc.generate()

        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure(build_script_folder=self.export_sources_folder)
        cmake.build()

    def package(self):
        copy(self, pattern="LICENSE.txt", dst=os.path.join(self.package_folder, "licenses"), src=self.source_folder)

        cmake = CMake(self)
        cmake.install()
