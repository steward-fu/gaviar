#/bin/sh
tools/mkimage -T sunxi_toc1 -d tools/toc1.cfg sd.bin

# offset: 8K of spl，+48K of toc1
#sudo dd if=boot0_sdcard_sun20iw1p1_d1s.bin of=/dev/sdb bs=1024 seek=8
#sudo dd if=sd.bin of=/dev/sdb bs=1024 seek=56

exit 0
