from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout

CONAN_TO_DEB_ARCH = {
    "armv8":   "arm64",
    "armv7hf": "armhf",
    "armv7":   "armhf",
    "armv6":   "armhel",
    "x86_64":  "amd64",
    "x86":     "i386",
}

class openpressod(ConanFile):
    name = "openpressod"
    settings = "os", "arch", "compiler", "build_type"

    def requirements(self):
        self.requires("spdlog/[>=1.17.0 <2.0]")
        self.requires("toml11/[>=4.4.0]")
        self.requires("magic_enum/[>=0.9.7]")
        
        self.requires("libopenpresso/1.0.0-rc2")
        self.requires("openpresso_proto/1.0.0-rc1")

    def build_requirements(self):
        self.test_requires("gtest/1.17.0")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["CPACK_DEBIAN_PACKAGE_ARCHITECTURE"] = CONAN_TO_DEB_ARCH[str(self.settings.arch)]
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
            