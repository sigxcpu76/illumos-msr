A (very) draft MSR access driver for Illumos
============================================

The skeleton was the cpuid driver.

Usage
-----

The device file ```/dev/cpu/self/msr``` is open and read at the offset corresponding to the requested MSR. 64 bits are returned.

Included is a sample usage application that reads the Intel CPU cores temperature using various MSRs.

Issues:
- ```wrmsr``` untested
- no security checks



Install
-------

```make install```

Test
----

A single socket machine (Xeon E3-1265L V2):

```console
# ./cputemp
Found 8 CPU(s) in 1 socket(s)
Socket #0 temp : 41 °C
	Core #0 temp : 35 °C
	Core #1 temp : 37 °C
	Core #2 temp : 33 °C
	Core #3 temp : 41 °C
```

A dual socket machine (Xeon E5506, no package thermal monitoring):

```console
# ./cputemp
Found 8 CPU(s) in 2 socket(s)
Socket #0
        Core #0 temp : 39 °C
        Core #1 temp : 36 °C
        Core #2 temp : 40 °C
        Core #3 temp : 33 °C
Socket #1
        Core #0 temp : 38 °C
        Core #1 temp : 36 °C
        Core #2 temp : 40 °C
        Core #3 temp : 35 °C
```