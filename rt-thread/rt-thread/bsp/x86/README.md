# RT-Thread Smart for x86

这是一份基础的RT-Thread Smart针对x86的版本、移植，主要是能够在qemu中执行。以下说明主要针对Linux的环境，如果是Windows环境，请使用Env工具，同时请自行处理生成iso的方法。

## 编译

编译RT-Thread Smart for x86版本，还需要一份支持musllib的工具链，可以通过以下地址获得：

*[i386-linux-musleabi_for_x86_64-pc-linux-gnu_latest.tar.bz2](http://117.143.63.254:9012/www/rt-smart/i386-linux-musleabi_for_x86_64-pc-linux-gnu_latest.tar.bz2)

下载后解压，然后在rtconfig.py中配置其中的EXEC_PATH变量

```python
if  CROSS_TOOL == 'gcc':
	PLATFORM 	= 'gcc'
	EXEC_PATH 	= '/home/jasonhu/Desktop/rtthread-smart/tools/gnu_gcc/i386-linux-musleabi_for_x86_64-pc-linux-gnu/bin'
```

然后在x86 bsp目录下执行scons命令来编译：

```bash
scons
```

### 配置

RT-Thread Smart for x86的版本也支持menuconfig的配置方式，在Linux下可以使用`scons --menuconfig`的方式进行配置。

因为menuconfig是一份字符界面的配置（Kconfig），在ubuntu下需要安装ncurses5的库

```bash
sudo apt install libncurses5-dev
```

## 运行

在ubuntu下运行，请确保你安装了`qemu-system-i386`，`grub2` 以及 `xorriso`软件包：

```bash
sudo apt install qemu-system-x86 grub2-common xorriso
```

然后执行`./run.sh`命令可以使用qemu来模拟执行(它也会生成可启动的iso文件)

```bash
xorriso 1.5.0 : RockRidge filesystem manipulator, libburnia project.

Drive current: -outdev 'stdio:bootable.iso'
Media current: stdio file, overwriteable
Media status : is blank
Media summary: 0 sessions, 0 data blocks, 0 data,  534m free
Added to ISO image: directory '/'='/tmp/grub.IcTOBu'
xorriso : UPDATE :     332 files added in 1 seconds
Added to ISO image: directory '/'='/home/jasonhu/Desktop/RTT/rtthread-smart/kernel/bsp/x86/root'
xorriso : UPDATE :     336 files added in 1 seconds
xorriso : NOTE : Copying to System Area: 512 bytes from file '/usr/lib/grub/i386-pc/boot_hybrid.img'
ISO image produced: 5679 sectors
Written to medium : 5679 sectors at LBA 0
Writing to 'stdio:bootable.iso' completed successfully.

 \ | /
- RT -     Thread Smart Operating System
 / | \     5.0.0 build Aug 23 2021
 2006 - 2020 Copyright by rt-thread team
lwIP-2.1.2 initialized!
[E/DBG] [ahci] no AHCI controllers present!
[E/PCNET32] device not find on pci device.

[E/RTL8139] device not find on pci device.

[I/sal.skt] Socket Abstraction Layer initialize success.
Dir /mnt mount failed!
Hello rtthread-smart x86!
msh />
```

在qemu下可以按Ctrl-A + X退出qemu。
