from conan import ConanFile

class ObloConanRecipe(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators =  "CMakeDeps"

    def requirements(self):
        self.requires("assimp/5.0.1")
        self.requires("brotli/1.1.0", override=True)
        self.requires("cxxopts/2.2.1")
        self.requires("glew/2.1.0")
        self.requires("glslang/8.13.3559")
        self.requires("gtest/1.10.0")
        self.requires("nlohmann_json/3.11.2")
        self.requires("vulkan-headers/1.3.211.0", override=True)
        self.requires("vulkan-loader/1.3.211.0", override=True)
        self.requires("vulkan-memory-allocator/3.0.0")
        self.requires("spirv-cross/cci.20211113")
        self.requires("sdl/2.0.20")
        self.requires("tinygltf/2.8.13")
        self.requires("qt/6.5.2")

    def configure(self):
        self.configure_qt()

    def configure_qt(self):
        qt = self.options["qt"]

        qt.shared = False
        qt.opengl = "no"
        qt.with_vulkan = True
        qt.openssl = False
        qt.with_pcre2 = True
        qt.with_glib = False
        qt.with_doubleconversion = True
        qt.with_freetype = True
        qt.with_fontconfig = True
        qt.with_icu = False
        qt.with_harfbuzz = True
        qt.with_libjpeg = False
        qt.with_libpng = True
        qt.with_sqlite3 = False
        qt.with_mysql = False
        qt.with_pq = False
        qt.with_odbc = False
        qt.with_zstd = False
        qt.with_brotli = True
        qt.with_dbus = False
        qt.with_libalsa = False
        qt.with_openal = False
        qt.with_gstreamer = False
        qt.with_pulseaudio = False
        qt.with_gssapi = False
        qt.with_md4c = True
        qt.with_x11 = True