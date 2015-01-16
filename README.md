# A (very) draft MSR access driver for Illumos

The skeleton was the cpuid driver.

Usage: 

The device file ```/dev/cpu/self/msr``` is open and read at the offset corresponding to the requested MSR. 64 bits are returned.

Included is a sample usage application: ```coretemp``` which reads the Intel CPU cores temperature.

Issues:
- no wrmsr support
- crashes if wrong MSR register is used (?)
- the ```/dev/cpu/self/msr``` link is not created automatically. Should point to ```/devices/pseudo/msr@0:self```


