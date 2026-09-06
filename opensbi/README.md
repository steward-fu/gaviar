# OpenSBI
```
This OpenSBI is heavily modification version to fit the need for Gaviar handheld. Basically, we add the tiny bootloader function into the openSBI and add the display function for Gaviar handheld.
```

# Building Steps
```
$ wget https://github.com/steward-fu/website/releases/download/gaviar/thead_toolchain.tar.gz
$ tar xvf thead_toolchain.tar.gz
$ sudo mv thead /opt
$ export PATH=/opt/thead/bin:$PATH

$ export DIR=payload
$ export PLATFORM=allwinner/f133
$ export CROSS_COMPILE=riscv64-unknown-linux-gnu-

$ make distclean
$ CONFIG=BROM make
$ python3 scripts/gen_checksum.py ./build/platform/allwinner/f133/firmware/fw_jump.bin fw_jump.bin
$ dd if=fw_jump.bin of=$DIR/brom.bin bs=1024 count=32
$ rm -rf fw_jump.bin

$ make distclean
$ make
$ python3 scripts/gen_checksum.py ./build/platform/allwinner/f133/firmware/fw_jump.bin $DIR/fw_jump.bin
$ python3 scripts/gen_checksum.py ./build/platform/allwinner/f133/firmware/fw_payload.bin $DIR/fw_payload.bin

$ sudo dd if=$DIR/brom.bin    of=$2 bs=1024 seek=8
$ sudo dd if=$DIR/fw_jump.bin of=$2 bs=1024 seek=40
$ sudo dd if=$DIR/board.dtb   of=$2 bs=1024 seek=300
$ sudo dd if=$DIR/Image       of=$2 bs=1024 seek=400
