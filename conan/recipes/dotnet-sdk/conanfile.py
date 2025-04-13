import os
from conan import ConanFile
from conan.tools.files import get, copy


class DotNetSDKConan(ConanFile):
    name = "dotnet-sdk"
    homepage = "https://github.com/dotnet/runtime"
    description = ".NET is a cross-platform runtime for cloud, mobile, desktop, and IoT apps."
    license = "MIT"

    package_type = "shared-library"
    settings = "os", "arch"

    options = {}
    default_options = {}

    short_paths = True

    def build(self):
        get(self, **self.conan_data["sdks"]["Windows"]
            [str(self.settings.arch)][self.version])

    def package(self):
        _sdk_dir = self.conan_data["sdk-dir"]["Windows"][str(
            self.settings.arch)][self.version]
        _full_sdk_dir = os.path.join(self.build_folder, _sdk_dir)
        _hostfxr_include_dir = os.path.join(self.package_folder, "include/hostfxr")
        copy(self, "hostfxr.h", _full_sdk_dir, _hostfxr_include_dir)
        copy(self, "coreclr_delegates.h", _full_sdk_dir, _hostfxr_include_dir)
        copy(self, "*", os.path.join(self.build_folder, "host"), os.path.join(self.package_folder, "host"))

        copy(self, "*", os.path.join(self.build_folder, "shared/Microsoft.NETCore.App"), os.path.join(self.package_folder, "shared/Microsoft.NETCore.App"))

        copy(self, "dotnet", self.build_folder, os.path.join(self.package_folder, "bin"))
        copy(self, "dotnet.exe", self.build_folder, os.path.join(self.package_folder, "bin"))

        copy(self, "*.txt", self.build_folder, os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        self.cpp_info.components["cli"].set_property("cmake_file_name", "dotnet::cli")
        self.cpp_info.components["cli"].bindirs = ["bin"]

        self.cpp_info.components["hostfxr"].set_property("cmake_file_name", "dotnet::hostfxr")
        self.cpp_info.components["hostfxr"].bindirs = ["host"]
        self.cpp_info.components["hostfxr"].includedirs = ["include/hostfxr"]

        self.cpp_info.components["runtime"].set_property("cmake_file_name", "dotnet::runtime")
        self.cpp_info.components["runtime"].bindirs = ["shared"]

