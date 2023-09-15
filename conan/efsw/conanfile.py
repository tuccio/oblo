import os
from conan import ConanFile
from conan.tools.cmake import CMake
from conan.tools.scm import Git
from conan import ConanFile

class EfswPackage(ConanFile):
    name = "efsw"
    version = "1.3.1"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain"

    options = {
        "shared": [True, False],
    }

    default_options = {
        "shared": True
    }

    def layout(self):
        # The root of the project is one level above
        self.folders.root = "."
        # The source of the project (the root CMakeLists.txt) is the source folder
        self.folders.source = "."
        self.folders.build = "build"

    def source(self):
        git = Git(self)
        git.clone(url="https://github.com/SpartanJ/efsw.git", target=".")
        git.checkout(commit=self.version)

    def build(self):
        variables = {
            "BUILD_SHARED_LIBS": self.options.shared,
        }

        cmake = CMake(self)
        cmake.configure(variables=variables)
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()