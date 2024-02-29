# network_drivers
hacked linux network drivers for EtherCAT

## Build

- Switch to the linux version branch
  ```
  git checkout osl15.4/5.14.21-150400.13-rt
  ```
  
- Ensure that you also init and update the git submodules:

  ```
  git submodule init
  git submodule update
  ```

- Build drivers:
  ```
  make
  ```
  
- Install drivers (as root):
  ```
  make install
  ```

### Blacklist original linux igb driver.

type `vim /etc/modprobe.d/blacklist-igb.conf` and add following content:

```
# prevent linux from loading it's own igb network driver
blacklist igb
```

### Attaching network device as EtherCAT interface.

All network devices which should be used as an EtherCAT interface have to be 
specified explicitly. Therefore you have to pass the MAC-addressses to the 
kernel module. Type `vim /etc/default/grub` and change:

```
GRUB_CMDLINE_LINUX_DEFAULT="resume=/dev/disk/by-partlabel/swap0 showopts splash=silent  intel_iommu=on preempt=full quiet security=apparmor mitigations=auto processor.max_cstate=1 intel_idle.max_cstate=0 nortsched isolcpus=2,3,4,5 nohz_full=2,3,4,5 mitigations=off idle=poll nosmt systemd.unit=default-offline.target"
```

to 
 
```
GRUB_CMDLINE_LINUX_DEFAULT="resume=/dev/disk/by-partlabel/swap0 showopts splash=silent igb-libethercat.ethercat_mac_addr=00:1b:21:ed:35:41 intel_iommu=on preempt=full quiet security=apparmor mitigations=auto processor.max_cstate=1 intel_idle.max_cstate=0 nortsched isolcpus=2,3,4,5 nohz_full=2,3,4,5 mitigations=off idle=poll nosmt systemd.unit=default-offline.target"
```

### Rebuild grub config file

Enter

```
grub2-mkconfig -o /boot/grub2/grub.cfg
```

### Create udev rule for devices

Create `/etc/udev/rules.d/991-ecat.rules` with following content:

```
# udev rules for EtherCAT devices:
SUBSYSTEM=="ecat", KERNEL=="ecat?", MODE="0666"
```

