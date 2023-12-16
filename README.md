# CastorOS

## Building COS requires:
 - SCons 4.0+
 - Clang 15
 - Qemu for simulation

## To build the source code run:
```shell
    scons
```

## To clean the source tree run:
```shell
    scons -c
```

## Building the documentation requires:
 - Doxygen
 - Graphviz
 - dia
 - Inkscape or png2pdf

## Running Command:
```shell
    qemu-system-x86_64 \
    -smp cpus=1 \
    -kernel build/sys/castor \
    -hda build/bootdisk.img \
    -nic none \
    -nographic
```
