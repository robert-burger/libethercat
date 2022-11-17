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
        "max_slave_sm": "ANY",
        "max_datagrams": "ANY",
        "max_eeprom_cat_sm": "ANY",
        "max_eeprom_cat_fmmu": "ANY",
        "max_eeprom_cat_pdo": "ANY",
        "max_eeprom_cat_pdo_entries": "ANY",
        "max_eeprom_cat_strings": "ANY",
        "max_eeprom_cat_dc": "ANY",
        "max_string_len": "ANY",
        "max_data": "ANY",
        "max_ds402_subdevs": "ANY",
        "max_coe_emergencies": "ANY",
        "max_coe_emergency_msg_len": "ANY",
        "ecat_device": "ANY"
    }
    default_options = {
        "shared": True, 
        "max_slaves": 256, 
        "max_groups": 8, 
        "max_pdlen": 3036, 
        "max_mbx_entries": 16, 
        "max_init_cmd_data": 2048,
        "max_slave_fmmu": 8,
        "max_slave_sm": 8,
        "max_datagrams": 100,
        "max_eeprom_cat_sm": 8,
        "max_eeprom_cat_fmmu": 8,
        "max_eeprom_cat_pdo": 128,
        "max_eeprom_cat_pdo_entries": 32,
        "max_eeprom_cat_strings": 128,
        "max_eeprom_cat_dc": 8,
        "max_string_len": 128,
        "max_data": 4096,
        "max_ds402_subdevs": 4,
        "max_coe_emergencies": 10,
        "max_coe_emergency_msg_len": 32,
        "ecat_device": "sock_raw",
    }

    generators = "pkg_config"
    requires = [ "libosal/[~0]@common/snapshot", ]

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

        args.append("--enable-device=%s" % (self.options.ecat_device))

        args.append("--with-max-slaves=%d" % (self.options.max_slaves))
        args.append("--with-max-groups=%d" % (self.options.max_groups))
        args.append("--with-max-pdlen=%d" % (self.options.max_pdlen))
        args.append("--with-max-mbx-entries=%d" % (self.options.max_mbx_entries))
        args.append("--with-max-init-cmd-data=%d" % (self.options.max_init_cmd_data))
        args.append("--with-max-slave-fmmu=%d" % (self.options.max_slave_fmmu))
        args.append("--with-max-slave-sm=%d" % (self.options.max_slave_sm))
        args.append("--with-max-datagrams=%d" % (self.options.max_datagrams))
        args.append("--with-max-eeprom-cat-sm=%d" % (self.options.max_eeprom_cat_sm))
        args.append("--with-max-eeprom-cat-fmmu=%d" % (self.options.max_eeprom_cat_fmmu))
        args.append("--with-max-eeprom-cat-pdo=%d" % (self.options.max_eeprom_cat_pdo))
        args.append("--with-max-eeprom-cat-pdo-entries=%d" % (self.options.max_eeprom_cat_pdo_entries))
        args.append("--with-max-eeprom-cat-strings=%d" % (self.options.max_eeprom_cat_strings))
        args.append("--with-max-eeprom-cat-dc=%d" % (self.options.max_eeprom_cat_dc))
        args.append("--with-max-string-len=%d" % (self.options.max_string_len))
        args.append("--with-max-data=%d" % (self.options.max_data))
        args.append("--with-max-ds402-subdevs=%d" % (self.options.max_ds402_subdevs))
        args.append("--with-max-coe-emergencies=%d" % (self.options.max_coe_emergencies))

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
