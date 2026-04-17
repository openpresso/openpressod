from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout

class openpressod(ConanFile):
    name = "openpressod"
    settings = "os", "arch", "compiler", "build_type"

    def requirements(self):
        self.requires("spdlog/[>=1.17.0 <2.0]")
        self.requires("toml11/[>=4.4.0]")
        self.requires("magic_enum/[>=0.9.7]")
        
        self.requires("libopenpresso/0.0.0-2-gbed204c")
        self.requires("openpresso_proto/0.0.0-testing")
        self.requires("farmhash/cci.20190513")

    def build_requirements(self):
        self.test_requires("gtest/1.17.0")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
            