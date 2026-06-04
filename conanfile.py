from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeToolchain, CMakeDeps, cmake_layout
import re

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
    options = { 
        "with_clang_tools": [True, False],
        "build_docs": [True, False, "DocsOnly"]
    }
    default_options = { 
        "with_clang_tools": False,
        "build_docs": False
    }

    def set_version(self):
        if not self.version:
            self.version = "0.0.0-unknown"

    def validate(self):
        check_min_cppstd(self, "23")
        if not self.settings.os == "Linux":
            raise ConanInvalidConfiguration("Only Linux is supported")

    def build_requirements(self):
        self.tool_requires("cmake/[>=3.30]")
        if(self.options.build_docs != "DocsOnly"):
            self.test_requires("gtest/1.17.0")
        if self.options.build_docs:
            self.tool_requires("doxygen/[>=1.16.0 <1.17.0]")

    def requirements(self):
        if(self.options.build_docs != "DocsOnly"):
            self.requires("spdlog/[>=1.17.0 <2.0]")
            self.requires("toml11/[>=4.4.0]")
            self.requires("magic_enum/[>=0.9.7]")
            
            self.requires("libopenpresso/0.0.0-6-g2f5af31")
            self.requires("openpresso_proto/0.0.0-7-g407b710")
        
    def generate(self):
        major, minor, patch = self.__version_components()
        tc = CMakeToolchain(self)

        if self.options.with_clang_tools:
            self.__intall_clang_tools(tc)

        tc.cache_variables["CPACK_DEBIAN_PACKAGE_ARCHITECTURE"] = CONAN_TO_DEB_ARCH[str(self.settings.arch)]
        tc.cache_variables["OPENPRESSOD_VERSION"] = self.version
        tc.cache_variables["OPENPRESSOD_VERSION_MAJOR"] = str(major)
        tc.cache_variables["OPENPRESSOD_VERSION_MINOR"] = str(minor)
        tc.cache_variables["OPENPRESSOD_VERSION_PATCH"] = str(patch)
        tc.cache_variables["OPENPRESSOD_VERSION_PATCH"] = str(patch)
        tc.cache_variables["DOCS_ONLY"] = self.options.build_docs == "DocsOnly"
        tc.cache_variables["OPENPRESSOD_REPO_CHANNEL"] = self.__repo_channel()
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def __repo_channel(self) -> str:
        release_pattern = r'\d+\.\d+\.\d+(?:-rc\d+)?'
        canary_pattern = r'\d+\.\d+\.\d+-\d+-g[0-9a-f]{7}'
        if re.fullmatch(release_pattern, self.version):
            return "stable"
        if re.fullmatch(canary_pattern, self.version):
            return "canary"
        return "testing"

    def __version_components(self):
        try:
            pattern = r'(?P<major>\d+)\.(?P<minor>\d+)\.(?P<patch>\d+)'
            match = re.match(pattern, self.version)
            return int(match.group("major")), int(match.group("minor")), int(match.group("patch"))
        except Exception:
            return 0, 0, 0

    def __intall_clang_tools(self, cmtc):
        try:
            from conan.tools.system import PyEnv
        except ImportError:
            from conan.tools.system import PipEnv as PyEnv
        venv = PyEnv(self)
        venv.install(["clang-tidy", "clang-format"])
        venv.generate()
        bin_dir = getattr(venv, "bin_path", None) or getattr(venv, "bin_dir", None)
        cmtc.cache_variables["CMAKE_PROGRAM_PATH"] = bin_dir
            