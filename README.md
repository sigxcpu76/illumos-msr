A (very) draft MSR access driver for Illumos
============================================

The skeleton was the cpuid driver.

Usage
-----

The device file ```/dev/cpu/self/msr``` is open and read at the offset corresponding to the requested MSR. 64 bits are returned.

Included is a sample usage application that reads the Intel CPU cores temperature using various MSRs.

Issues:
- crashes kernel if invalid MSR register is used 
- ```wrmsr``` untested
- no security checks



Install
-------

```make install```

Test
----

```console
# ./cputemp
CPU 0: core_temp=34 package_temp=41 tj_max=94
CPU 1: core_temp=35 package_temp=41 tj_max=94
CPU 2: core_temp=33 package_temp=41 tj_max=94
CPU 3: core_temp=41 package_temp=41 tj_max=94
CPU 4: core_temp=34 package_temp=41 tj_max=94
CPU 5: core_temp=34 package_temp=41 tj_max=94
CPU 6: core_temp=33 package_temp=41 tj_max=94
CPU 7: core_temp=41 package_temp=41 tj_max=94
```
