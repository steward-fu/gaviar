# tools 
GRUB_DIR	:= grub-2.04
OS_NAME		:= rtthread
QEMU		:= qemu-system-i386
GDB			:= gdb
TRUNC		:= truncate
MCOPY		:= mcopy

# file name
DISK0		:= disk0.img
DISK1		:= disk1.img
SZ_DISK0	:= 64M
SZ_DISK1	:= 64M
MKFS		:= mkfs.fat
ROM_DIR		:= ../../../userapps/root

RTTHREAD_ELF:= rtthread.elf

# config graphic window ? (y/n)
QEMU_WINDOW ?= n

# config netcard ? (y/n)
QEMU_NETCARD ?= y

# netcard name: rtl8139/pcnet
QEMU_NETCARD_NAME ?=pcnet

# netcard type: tap/user
QEMU_NET_MODE ?=user

# qemu args
QEMU_ARGS	:= 	-m 256m \
				-rtc base=localtime \
				-boot d \
				-cdrom $(OS_NAME).iso
				
ifeq ($(QEMU_WINDOW),y)
	QEMU_ARGS += -serial stdio
else
	QEMU_ARGS += -nographic
endif

ifeq ($(QEMU_NETCARD),y)
	QEMU_ARGS += -net nic,model=$(QEMU_NETCARD_NAME)
ifeq ($(QEMU_NET_MODE),tap)
	QEMU_ARGS += -net tap,ifname=tap0,script=no,downscript=no 
else
	QEMU_ARGS += -net user
endif

endif

QEMU_ARGS +=-drive id=disk0,file=$(DISK0),format=raw,if=none \
			-drive id=disk1,file=$(DISK1),format=raw,if=none \
			-device ahci,id=ahci \
			-device ide-hd,drive=disk0,bus=ahci.0 \
			-device ide-hd,drive=disk1,bus=ahci.1

all: makeiso makedisk

makeiso: $(RTTHREAD_ELF)
	@$(MAKE) -s -C $(GRUB_DIR) KERNEL=$(RTTHREAD_ELF) OS_NAME=$(OS_NAME)

makedisk:
	$(TRUNC) --size $(SZ_DISK0) $(DISK0)
	$(TRUNC) --size $(SZ_DISK1) $(DISK1)
	$(MKFS) -F32 $(DISK0)
	-$(MCOPY) -i $(DISK0) $(ROM_DIR)/bin/*.elf ::

run: makedisk makeiso
	$(QEMU) $(QEMU_ARGS)

qemudbg:
	$(QEMU) -S -gdb tcp::10001,ipv4 $(QEMU_ARGS)

# 连接gdb server: target remote localhost:10001
gdb:
	$(GDB) $(RTTHREAD_ELF)

clean:
	@$(MAKE) -s -C $(GRUB_DIR) clean