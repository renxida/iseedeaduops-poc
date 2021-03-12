# install msr-tools for rdmsr and wrmsr:
# read/write Model Specific Registers
if ! dpkg -s msr-tools >/dev/null 2>&1; then
  sudo apt install msr-tools
fi

# chown and chmod + setuid so that you can run pmc.cpp code outside 
# root
chown root $(which wrmsr)
chown root $(which rdmsr)
chmod +s $(which wrmsr)
chmod +s $(which rdmsr)

# make sure kernel module is enabled
modprobe msr

#enable rdpmc instruction
echo 2 > /sys/bus/event_source/devices/cpu/rdpmc
