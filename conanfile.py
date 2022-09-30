from conans import ConanFile, AutoToolsBuildEnvironment
import re

class MainProject(ConanFile):
    name = "libethercat"
    license = "GPLv3"
    author = "Robert Burger <robert.burger@dlr.de>"
    url = f"https://rmc-github.robotic.dlr.de/common/{name}"
    description = "This library provides all functionality to communicate with EtherCAT slaves attached to a Network interface"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = "src/*", "include/*", "README.md", "project.properties", "libethercat.pc.in", "Makefile.am", "m4/*", "configure.ac", "LICENSE", "aminclude.am", "acinclude.m4", "tools/*", "doxygen.cfg", "config.sub"
    options = {
        "shared": [True, False],
        "max_slaves": "ANY",
        "max_groups": "ANY", 
        "max_pdlen": "ANY", 
        "max_mbx_entries": "ANY", 
        "max_init_cmd_data": "ANY",
        "max_slave_fmmu": "ANY",
        "max_slave_sm": "ANY"
    }
    default_options = {
        "shared": True, 
        "max_slaves": 256, 
        "max_groups": 8, 
        "max_pdlen": 3036, 
        "max_mbx_entries": 16, 
        "max_init_cmd_data": 2048,
        "max_slave_fmmu": 8,
        "max_slave_sm": 8
    }

    generators = "pkg_config"
    requires = [ "libosal/main@common/snapshot", ]

    def source(self):
        filedata = None
        filename = "project.properties"
        with open(filename, 'r') as f:
            filedata = f.read()
        with open(filename, 'w') as f:
            f.write(re.sub("VERSION *=.*[^\n]", f"VERSION = {self.version}", filedata))

    def build(self):
        self.run("autoreconf -i")
        autotools = AutoToolsBuildEnvironment(self)
        autotools.libs=[]
        autotools.include_paths=[]
        autotools.library_paths=[]

        args = []

        if self.settings.build_type == "Debug":
            print("doing libethercat DEBUG build!\n")
            autotools.flags = ["-O0", "-g"]
            args.append("--enable-assert")
        else:
            autotools.flags = ["-O2"]
            args.append("--disable-assert")

        if self.options.shared:
            args.append("--enable-shared")
            args.append("--disable-static")
        else:
            args.append("--disable-shared")
            args.append("--enable-static")

        cflags=""

        if self.options.max_slaves:
            autotools.defines.append("LEC_MAX_SLAVES=%d" % self.options.max_slaves)
        if self.options.max_groups:
            autotools.defines.append("LEC_MAX_GROUPS=%d" % self.options.max_groups)
        if self.options.max_pdlen:
            autotools.defines.append("LEC_MAX_PDLEN=%d" % self.options.max_pdlen)
        if self.options.max_mbx_entries:
            autotools.defines.append("LEC_MAX_MBX_ENTRIES=%d" % self.options.max_mbx_entries)
        if self.options.max_init_cmd_data:
            autotools.defines.append("LEC_MAX_INIT_CMD_DATA=%d" % self.options.max_init_cmd_data)
        if self.options.max_slave_fmmu:
            autotools.defines.append("LEC_MAX_SLAVE_FMMU=%d" % self.options.max_slave_fmmu)
        if self.options.max_slave_sm:
            autotools.defines.append("LEC_MAX_SLAVE_SM=%d" % self.options.max_slave_sm)

        args.append("--disable-silent-rules")

        autotools.configure(configure_dir=".", args=args)
        autotools.make()

    def package(self):
        autotools = AutoToolsBuildEnvironment(self)
        autotools.install()

    def package_info(self):
        self.cpp_info.includedirs = ['include']
        self.cpp_info.libs = ["ethercat"]
        self.cpp_info.bindirs = ['bin']
        self.cpp_info.resdirs = ['share']
