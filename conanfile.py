from conan import ConanFile
from conan.tools.gnu import Autotools, AutotoolsToolchain
import re

class MainProject(ConanFile):
    name = "libethercat"
    license = "GPLv3"
    author = "Robert Burger <robert.burger@dlr.de>"
    url = f"https://rmc-github.robotic.dlr.de/common/{name}"
    description = "This library provides all functionality to communicate with EtherCAT slaves attached to a Network interface"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = ["*", "!.gitignore", "!bindings"]
    options = {
        "shared": [True, False],
        "max_slaves": ["ANY"],
        "max_groups": ["ANY"], 
        "max_pdlen": ["ANY"],
        "max_mbx_entries": ["ANY"], 
        "max_init_cmd_data": ["ANY"],
        "max_slave_fmmu": ["ANY"],
        "max_slave_sm": ["ANY"],
        "max_datagrams": ["ANY"],
        "max_eeprom_cat_sm": ["ANY"],
        "max_eeprom_cat_fmmu": ["ANY"],
        "max_eeprom_cat_pdo": ["ANY"],
        "max_eeprom_cat_pdo_entries": ["ANY"],
        "max_eeprom_cat_strings": ["ANY"],
        "max_eeprom_cat_dc": ["ANY"],
        "max_string_len": ["ANY"],
        "max_data": ["ANY"],
        "max_ds402_subdevs": ["ANY"],
        "max_coe_emergencies": ["ANY"],
        "max_coe_emergency_msg_len": ["ANY"],
        "hw_device_file": [ True, False ],
        "hw_device_sock_raw": [ True, False ],
        "hw_device_sock_raw_mmaped": [ True, False ],
        "hw_device_bpf": [ True, False ],
        "hw_device_pikeos": [ True, False ],
        "mbx_support_eoe" : [ True, False ],
        "mbx_support_foe" : [ True, False ],
        "mbx_support_soe" : [ True, False ],
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
        "hw_device_file": True,
        "hw_device_sock_raw": True,
        "hw_device_sock_raw_mmaped": True,
        "hw_device_bpf": False,
        "hw_device_pikeos": False,
        "mbx_support_eoe": True,
        "mbx_support_foe": True,
        "mbx_support_soe": True,
    }

    requires = [ "libosal/[>=0.0.3]@common/stable", ]
    generators = "PkgConfigDeps"

    def generate(self):
        tc = AutotoolsToolchain(self)
        tc.autoreconf_args = [ "--install" ]
        if self.settings.os == "pikeos":
            self.options.hw_device_file = False
            self.options.hw_device_sock_raw = False
            self.options.hw_device_sock_raw_mmaped = False
            self.options.hw_device_bpf = False
            self.options.hw_device_pikeos = True

            tc.update_configure_args({
                "--host": "%s-%s" % (self.settings.arch, self.settings.os),  # update flag '--host=my-gnu-triplet
            })

        tc.generate()

    def source(self):
        filedata = None
        filename = "project.properties"
        with open(filename, 'r') as f:
            filedata = f.read()
        with open(filename, 'w') as f:
            f.write(re.sub("VERSION *=.*[^\n]", f"VERSION = {self.version}", filedata))

    def build(self):
        print("os %s, compiler %s, build_type %s, arch %s" % (self.settings.os, self.settings.compiler, self.settings.build_type, self.settings.arch))
        autotools = Autotools(self)
        autotools.libs=[]
        autotools.include_paths=[]
        autotools.library_paths=[]

        args = []

        if self.settings.build_type == "Debug":
            print("doing libethercat DEBUG build!\n")
            autotools.flags = ["-O0", "-g"]
            args.append("--enable-assert")
            args.append("--enable-debug")
        else:
            autotools.flags = ["-O2"]
            args.append("--disable-assert")

        if self.options.shared:
            args.append("--enable-shared")
            args.append("--disable-static")
        else:
            args.append("--disable-shared")
            args.append("--enable-static")

        if self.options.hw_device_file:
            args.append("--enable-device-file")
        if self.options.hw_device_sock_raw:
            args.append("--enable-device-sock-raw")
        if self.options.hw_device_sock_raw_mmaped:
            args.append("--enable-device-sock-raw-mmaped")
        if self.options.hw_device_bpf:
            args.append("--enable-device-bpf")
        if self.options.hw_device_pikeos:
            args.append("--enable-device-pikeos")
        if not self.options.mbx_support_eoe:
            args.append("--disable-mbx-support-eoe")
        if not self.options.mbx_support_foe:
            args.append("--disable-mbx-support-foe")
        if not self.options.mbx_support_soe:
            args.append("--disable-mbx-support-soe")

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

        autotools.autoreconf()
        autotools.configure(args=args)
        autotools.make()

    def package(self):
        autotools = Autotools(self)
        autotools.install()

    def package_info(self):
        self.cpp_info.includedirs = ['include']
        self.cpp_info.libs = ["ethercat"]
        self.cpp_info.bindirs = ['bin']
        self.cpp_info.resdirs = ['share']
