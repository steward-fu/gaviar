#!/bin/bash
make CROSS_COMPILE=/opt/Xuantie-900-gcc-elf-newlib-x86_64-V2.6.1/bin/riscv64-unknown-elf- PLATFORM=riscv64-gaviar -j4
sudo xfel ddr f133; sudo xfel write 0x40000000 output/xboot.bin; sudo xfel exec 0x40000000;
