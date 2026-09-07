#
# SPDX-License-Identifier: BSD-2-Clause
#

platform-cppflags-y  =
platform-cflags-y    = -Wno-pointer-to-int-cast -Wno-int-conversion -Wno-array-bounds -Wno-unused-value -Wno-unused-function -Wno-unused-variable
platform-cflags-y   += -DF133_TAG_KEY=$(F133_TAG_KEY) -DF133_TAG_ADR=$(F133_TAG_ADR)
platform-cflags-y   += -DDTB_LBA=$(DTB_LBA) -DDTB_BASE=$(DTB_BASE) -DDTB_SIZE=$(DTB_SIZE)
platform-cflags-y   += -DKERNEL_LBA=$(KERNEL_LBA) -DKERNEL_BASE=$(KERNEL_BASE) -DKERNEL_SIZE=$(KERNEL_SIZE)
platform-cflags-y   += -DOPENSBI_LBA=$(OPENSBI_LBA) -DOPENSBI_BASE=$(OPENSBI_BASE) -DOPENSBI_SIZE=$(OPENSBI_SIZE)
platform-asflags-y   =
platform-ldflags-y   =
platform-objs-y     += platform.o

F133_TAG_KEY         = 0x55AA55AA
F133_TAG_ADR         = 0x20000

# LBA = 300KB
# SIZE = 64KB
DTB_LBA              = 600
DTB_BASE             = 0x41500000
DTB_SIZE             = 0x10000

# LBA = 400KB
# SIZE = 8MB
KERNEL_LBA           = 800
KERNEL_BASE          = 0x40008000
KERNEL_SIZE          = 0xc00000

# LBA = 8KB + 32KB(BROM) = 40KB
# SIZE = 256KB
OPENSBI_LBA          = 80
OPENSBI_BASE         = 0x41000000
OPENSBI_SIZE         = 0x40000

ifeq ($(CONFIG),BROM)
    FW_TEXT_START    = 0x20000
else
    FW_TEXT_START    = 0x41000000
endif

FW_JUMP              = y
FW_JUMP_ADDR         = $(KERNEL_BASE)
FW_JUMP_FDT_ADDR     = $(DTB_BASE)

FW_PAYLOAD           = y
FW_PAYLOAD_OFFSET    = 0x30000
FW_PAYLOAD_FDT_ADDR  = $(DTB_BASE)
FW_PAYLOAD_ALIGN     = 0x1000

