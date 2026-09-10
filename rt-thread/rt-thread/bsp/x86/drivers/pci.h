/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2021-08-04     JasonHu       first version
 */

#ifndef __PCI_H__
#define __PCI_H__

#include <rtdef.h>

#define PCI_CONFIG_ADDR    0xCF8    /* PCI configuration space address port */
#define PCI_CONFIG_DATA    0xCFC    /* PCI configuration space data port */

#define PCI_DEVICE_VENDER               0x00
#define PCI_STATUS_COMMAND              0x04
#define PCI_CLASS_CODE_REVISION_ID      0x08
#define PCI_BIST_HEADER_TYPE            0x0C
#define PCI_BASS_ADDRESS0               0x10
#define PCI_BASS_ADDRESS1               0x14
#define PCI_BASS_ADDRESS2               0x18
#define PCI_BASS_ADDRESS3               0x1C
#define PCI_BASS_ADDRESS4               0x20
#define PCI_BASS_ADDRESS5               0x24
#define PCI_CARD_BUS_POINTER            0x28
#define PCI_SUBSYSTEM_ID                0x2C
#define PCI_EXPANSION_ROM_BASE_ADDR     0x30
#define PCI_CAPABILITY_LIST             0x34
#define PCI_RESERVED                    0x38
#define PCI_IRQ_PIN_IRQ_LINE            0x3C

#define  PCI_COMMAND_IO                 0x1     /* Enable response in I/O space */
#define  PCI_COMMAND_MEMORY             0x2     /* Enable response in Memory space */
#define  PCI_COMMAND_MASTER             0x4     /* Enable bus mastering */
#define  PCI_COMMAND_SPECIAL            0x8     /* Enable response to special cycles */
#define  PCI_COMMAND_INVALIDATE         0x10    /* Use memory write and invalidate */
#define  PCI_COMMAND_VGA_PALETTE        0x20    /* Enable palette snooping */
#define  PCI_COMMAND_PARITY             0x40    /* Enable parity checking */
#define  PCI_COMMAND_WAIT               0x80    /* Enable address/data stepping */
#define  PCI_COMMAND_SERR               0x100   /* Enable SERR */
#define  PCI_COMMAND_FAST_BACK          0x200   /* Enable back-to-back writes */
#define  PCI_COMMAND_INTX_DISABLE       0x400   /* INTx Emulation Disable */

#define PCI_BASE_ADDR_MEM_MASK           (~0x0FUL)
#define PCI_BASE_ADDR_IO_MASK            (~0x03UL)

#define PCI_BAR_TYPE_INVALID     0
#define PCI_BAR_TYPE_MEM         1
#define PCI_BAR_TYPE_IO         2

#define PCI_MAX_BAR_NR 6        /* Each device has up to 6 address information */
#define PCI_MAX_BUS_NR 256      /* PCI has a total of 256 buses */
#define PCI_MAX_DEV_NR 32       /* There are a total of 32 devices on each PCI bus */
#define PCI_MAX_FUN_NR 8        /* PCI devices have a total of 8 function numbers */

#ifndef PCI_ANY_ID
#define PCI_ANY_ID (~0)
#endif

struct rt_pci_device_id
{
    rt_uint32_t vendor, device;        /* vendor and device id or PCI_ANY_ID */
    rt_uint32_t subvendor, subdevice;  /* subsystem's id or PCI_ANY_ID */
    rt_uint32_t class_value, class_mask;
};
typedef struct rt_pci_device_id rt_pci_device_id_t;

struct rt_pci_device_bar
{
    rt_uint32_t type;          /* Type of address bar (IO address/MEM address) */
    rt_uint32_t base_addr;
    rt_uint32_t length;        /* Length of address */
};
typedef struct rt_pci_device_bar rt_pci_device_bar_t;

struct rt_pci_device
{
    rt_list_t list;             /* list for all pci device */
    rt_uint8_t bus;             /* bus number */
    rt_uint8_t dev;             /* device number */
    rt_uint8_t function;        /* Function number */

    rt_uint16_t vendor_id;      /* Configuration space:Vendor ID */
    rt_uint16_t device_id;      /* Configuration space:Device ID */
    rt_uint16_t command;        /* Configuration space:Command */
    rt_uint16_t status;         /* Configuration space:Status */

    rt_uint32_t class_code;     /* Configuration space:Class Code */
    rt_uint8_t revision_id;     /* Configuration space:Revision ID */
    rt_uint8_t multi_function;
    rt_uint32_t card_bus_pointer;
    rt_uint16_t subsystem_vendor_id;
    rt_uint16_t subsystem_device_id;
    rt_uint32_t expansion_rom_base_addr;
    rt_uint32_t capability_list;

    rt_uint8_t irq_line;        /*Configuration space:IRQ line*/
    rt_uint8_t irq_pin;         /*Configuration space:IRQ pin*/
    rt_uint8_t min_gnt;
    rt_uint8_t max_lat;
    rt_pci_device_bar_t bars[PCI_MAX_BAR_NR];
};
typedef struct rt_pci_device rt_pci_device_t;

rt_uint32_t rt_pci_device_get_io_addr(rt_pci_device_t *device);
rt_uint32_t rt_pci_device_get_mem_addr(rt_pci_device_t *device);
rt_uint32_t rt_pci_device_get_mem_len(rt_pci_device_t *device);
rt_uint32_t rt_pci_device_get_irq_line(rt_pci_device_t *device);

void rt_pci_enable_bus_mastering(rt_pci_device_t *device);

rt_pci_device_t* rt_pci_device_get(rt_uint32_t vendor_id, rt_uint32_t device_id);
rt_pci_device_t* rt_pci_device_get_by_class_code(rt_uint32_t class_value, rt_uint32_t sub_class);

void rt_pci_device_bar_dump(rt_pci_device_bar_t *bar);
void rt_pci_device_dump(rt_pci_device_t *device);

rt_uint32_t rt_pci_device_read(rt_pci_device_t *device, rt_uint32_t reg);
void rt_pci_device_write(rt_pci_device_t *device, rt_uint32_t reg, rt_uint32_t value);

void rt_pci_init(void);

#endif  /* __PCI_H__ */
