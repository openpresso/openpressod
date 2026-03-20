from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout

class openpressod(ConanFile):
    name = "openpressod"
    settings = "os", "arch", "compiler", "build_type"

    def requirements(self):
        self.requires("grpc/1.72.0")
        self.requires("spdlog/[>=1.17.0 <2.0]")
        self.requires("toml11/[>=4.4.0]")
        self.requires("magic_enum/[>=0.9.7]")
        
        self.requires("libopenpresso/1.0.0-rc1")

    def build_requirements(self):
        self.tool_requires("protobuf/5.27.0")
        self.tool_requires("grpc/1.72.0")

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
            