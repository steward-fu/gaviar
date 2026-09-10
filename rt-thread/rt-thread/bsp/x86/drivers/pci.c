/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2021-08-04     JasonHu       first version
 */

#include "pci.h"
#include "board.h"

#include <rtthread.h>

// #define RT_PCI_DEBUG

#ifdef RT_PCI_DEBUG
    #define dbgprint rt_kprintf
#else
    #define dbgprint(...)
#endif

static rt_list_t g_pci_device_list_head;

static void pci_device_bar_init(rt_pci_device_bar_t *bar, rt_uint32_t addr_reg_val, rt_uint32_t len_reg_val)
{
    if (addr_reg_val == 0xffffffff) {
        addr_reg_val = 0;
    }
    /*we judge type by addr register bit 0, if 1, type is io, if 0, type is memory*/
    if (addr_reg_val & 1) {
        bar->type = PCI_BAR_TYPE_IO;
        bar->base_addr = addr_reg_val  & PCI_BASE_ADDR_IO_MASK;
        bar->length    = ~(len_reg_val & PCI_BASE_ADDR_IO_MASK) + 1;
    } else {
        bar->type = PCI_BAR_TYPE_MEM;
        bar->base_addr = addr_reg_val  & PCI_BASE_ADDR_MEM_MASK;
        bar->length    = ~(len_reg_val & PCI_BASE_ADDR_MEM_MASK) + 1;
    }
}

void rt_pci_device_bar_dump(rt_pci_device_bar_t *bar)
{
    rt_kprintf("        type: %s, ", bar->type == PCI_BAR_TYPE_IO ? "io base address" : "mem base address");
    rt_kprintf("        base address: %x, ", bar->base_addr);
    rt_kprintf("        len: %x\n", bar->length);
}

static void pci_device_init(rt_pci_device_t *device, rt_uint8_t bus, rt_uint8_t dev, rt_uint8_t function,
                            rt_uint16_t vendor_id, rt_uint16_t device_id, rt_uint32_t class_code,
                            rt_uint8_t revision_id, rt_uint8_t multi_function)
{
    device->bus = bus;
    device->dev = dev;
    device->function = function;

    device->vendor_id = vendor_id;
    device->device_id = device_id;
    device->multi_function = multi_function;
    device->class_code = class_code;
    device->revision_id = revision_id;
    int i;
    for (i = 0; i < PCI_MAX_BAR_NR; i++)
    {
        device->bars[i].type = PCI_BAR_TYPE_INVALID;
    }
    device->irq_line = -1;
}

static rt_uint32_t pci_read_config(rt_uint32_t bus, rt_uint32_t device, rt_uint32_t function, rt_uint32_t addr)
{
    rt_uint32_t reg = 0x80000000;
    reg |= (bus & 0xFF) << 16;
    reg |= (device & 0x1F) << 11;
    reg |= (function & 0x7) << 8;
    reg |= (addr & 0xFF) & 0xFC;    /*bit 0 and 1 always 0*/
    outl(PCI_CONFIG_ADDR, reg);
    return inl(PCI_CONFIG_DATA);
}

static void pci_write_config(rt_uint32_t bus, rt_uint32_t device, rt_uint32_t function, rt_uint32_t addr, rt_uint32_t val)
{
    rt_uint32_t reg = 0x80000000;
    reg |= (bus & 0xFF) << 16;
    reg |= (device & 0x1F) << 11;
    reg |= (function & 0x7) << 8;
    reg |= (addr & 0xFF) & 0xFC;    /*bit 0 and 1 always 0*/
    outl(PCI_CONFIG_ADDR, reg);
    outl(PCI_CONFIG_DATA, val);
}

static rt_pci_device_t *pci_create_device()
{
    rt_pci_device_t *device = rt_malloc(sizeof(rt_pci_device_t));
    if (device == RT_NULL)
    {
        return RT_NULL;
    }
    rt_list_insert_after(&g_pci_device_list_head, &device->list);
    return device;
}

void rt_pci_device_dump(rt_pci_device_t *device)
{
    rt_kprintf("vendor id:      0x%x\n", device->vendor_id);
    rt_kprintf("device id:      0x%x\n", device->device_id);
    rt_kprintf("class code:     0x%x\n", device->class_code);
    rt_kprintf("revision id:    0x%x\n", device->revision_id);
    rt_kprintf("multi function: %d\n", device->multi_function);
    rt_kprintf("card bus CIS pointer: %x\n", device->card_bus_pointer);
    rt_kprintf("subsystem vendor id: %x\n", device->subsystem_vendor_id);
    rt_kprintf("subsystem device id: %x\n", device->subsystem_device_id);
    rt_kprintf("expansion ROM base address: %x\n", device->expansion_rom_base_addr);
    rt_kprintf("capability list pointer:  %x\n", device->capability_list);
    rt_kprintf("irq line: %d\n", device->irq_line);
    rt_kprintf("irq pin:  %d\n", device->irq_pin);
    rt_kprintf("min Gnt: %d\n", device->min_gnt);
    rt_kprintf("max Lat:  %d\n", device->max_lat);
    int i;
    for (i = 0; i < PCI_MAX_BAR_NR; i++)
    {
        if (device->bars[i].type != PCI_BAR_TYPE_INVALID)
        {
            rt_kprintf("bar %d:\n", i);
            rt_pci_device_bar_dump(&device->bars[i]);
        }
    }
    rt_kprintf("\n");
}

static void pci_scan_device(rt_uint8_t bus, rt_uint8_t device, rt_uint8_t function)
{
    rt_uint32_t val = pci_read_config(bus, device, function, PCI_DEVICE_VENDER);
    rt_uint32_t vendor_id = val & 0xffff;
    rt_uint32_t device_id = val >> 16;
    /*if vendor id is 0xffff, it means that this bus , device not exist!*/
    if (vendor_id == 0xffff)
    {
        return;
    }

    rt_pci_device_t *pci_dev = pci_create_device();
    if (pci_dev == RT_NULL)
    {
        return;
    }

    val = pci_read_config(bus, device, function, PCI_BIST_HEADER_TYPE);
    rt_uint8_t header_type = ((val >> 16));
    val = pci_read_config(bus, device, function, PCI_STATUS_COMMAND);

    pci_dev->command = val & 0xffff;
    pci_dev->status = (val >> 16) & 0xffff;

    val = pci_read_config(bus, device, function, PCI_CLASS_CODE_REVISION_ID);
    rt_uint32_t classcode = val >> 8;
    rt_uint8_t revision_id = val & 0xff;

    pci_device_init(pci_dev, bus, device, function, vendor_id, device_id, classcode, revision_id, (header_type & 0x80));

    int bar, reg;
    for (bar = 0; bar < PCI_MAX_BAR_NR; bar++)
    {
        reg = PCI_BASS_ADDRESS0 + (bar*4);
        val = pci_read_config(bus, device, function, reg);
        /*set 0xffffffff to bass address[0~5], so that if we pci_read_config again, it's addr len*/
        pci_write_config(bus, device, function, reg, 0xffffffff);

       /*pci_read_config bass address[0~5] to get addr len*/
        rt_uint32_t len = pci_read_config(bus, device, function, reg);
        /*pci_write_config the io/mem address back to confige space*/
        pci_write_config(bus, device, function, reg, val);

        if (len != 0 && len != 0xffffffff)
        {
            pci_device_bar_init(&pci_dev->bars[bar], val, len);
        }
    }

    val = pci_read_config(bus, device, function, PCI_CARD_BUS_POINTER);
    pci_dev->card_bus_pointer = val;

    val = pci_read_config(bus, device, function, PCI_SUBSYSTEM_ID);
    pci_dev->subsystem_vendor_id = val & 0xffff;
    pci_dev->subsystem_device_id = (val >> 16) & 0xffff;

    val = pci_read_config(bus, device, function, PCI_EXPANSION_ROM_BASE_ADDR);
    pci_dev->expansion_rom_base_addr = val;

    val = pci_read_config(bus, device, function, PCI_CAPABILITY_LIST);
    pci_dev->capability_list = val;

    val = pci_read_config(bus, device, function, PCI_IRQ_PIN_IRQ_LINE);
    if ((val & 0xff) > 0 && (val & 0xff) < 32)
    {
        int irq = val & 0xff;
        pci_dev->irq_line = irq;
        pci_dev->irq_pin = (val >> 8)& 0xff;
    }
    pci_dev->min_gnt = (val >> 16) & 0xff;
    pci_dev->max_lat = (val >> 24) & 0xff;
}

static void rt_pci_scan_all_buses()
{
    rt_uint32_t bus;
    rt_uint8_t device, function;
    for (bus = 0; bus < PCI_MAX_BUS_NR; bus++)
    {
        for (device = 0; device < PCI_MAX_DEV_NR; device++)
        {
           for (function = 0; function < PCI_MAX_FUN_NR; function++)
            {
                pci_scan_device(bus, device, function);
            }
        }
    }
}

rt_pci_device_t *rt_pci_device_get(rt_uint32_t vendor_id, rt_uint32_t device_id)
{
    rt_pci_device_t* device;

    rt_list_for_each_entry(device, &g_pci_device_list_head, list)
    {
        if (device->vendor_id == vendor_id && device->device_id == device_id)
        {
            return device;
        }
    }
    return RT_NULL;
}

rt_pci_device_t *rt_pci_device_get_by_class_code(rt_uint32_t class, rt_uint32_t sub_class)
{
    rt_pci_device_t* device;
    rt_uint32_t class_code = ((class & 0xff) << 16) | ((sub_class & 0xff) << 8);

    rt_list_for_each_entry(device, &g_pci_device_list_head, list)
    {
        if ((device->class_code & 0xffff00) == class_code)
        {
            return device;
        }
    }
    return RT_NULL;
}

void rt_pci_enable_bus_mastering(rt_pci_device_t *device)
{
    rt_uint32_t val = pci_read_config(device->bus, device->dev, device->function, PCI_STATUS_COMMAND);
    dbgprint("PCI enable bus mastering: before command: %x\n", val);
    val |= 0x04;
    pci_write_config(device->bus, device->dev, device->function, PCI_STATUS_COMMAND, val);
    val = pci_read_config(device->bus, device->dev, device->function, PCI_STATUS_COMMAND);
    dbgprint("PCI enable bus mastering: after command: %x\n", val);
}

rt_uint32_t rt_pci_device_read(rt_pci_device_t *device, rt_uint32_t reg)
{
    return pci_read_config(device->bus, device->dev, device->function, reg);
}

void rt_pci_device_write(rt_pci_device_t *device, rt_uint32_t reg, rt_uint32_t value)
{
    pci_write_config(device->bus, device->dev, device->function, reg, value);
}

rt_uint32_t rt_pci_device_get_io_addr(rt_pci_device_t *device)
{
    int i;
    for (i = 0; i < PCI_MAX_BAR_NR; i++)
    {
        if (device->bars[i].type == PCI_BAR_TYPE_IO)
        {
            return device->bars[i].base_addr;
        }
    }
    return 0;
}

rt_uint32_t rt_pci_device_get_mem_addr(rt_pci_device_t *device)
{
    int i;
    for (i = 0; i < PCI_MAX_BAR_NR; i++)
    {
        if (device->bars[i].type == PCI_BAR_TYPE_MEM)
        {
            return device->bars[i].base_addr;
        }
    }
    return 0;
}

rt_uint32_t rt_pci_device_get_mem_len(rt_pci_device_t *device)
{
    int i;
    for(i = 0; i < PCI_MAX_BAR_NR; i++)
    {
        if(device->bars[i].type == PCI_BAR_TYPE_MEM)
        {
            return device->bars[i].length;
        }
    }
    return 0;
}

rt_uint32_t rt_pci_device_get_irq_line(rt_pci_device_t *device)
{
    return device->irq_line;
}

static rt_uint32_t pic_get_connected_counts()
{
    rt_uint32_t n = 0;
    rt_pci_device_t *device;
    rt_list_for_each_entry(device, &g_pci_device_list_head, list)
    {
        n++;
    }
    return n;
}

#ifdef RT_USING_FINSH
static void rt_pci_device_list(rt_pci_device_t *device)
{
    rt_kprintf("device bus: %d, device: %d function: %d\n", device->bus, device->dev, device->function);
    rt_kprintf("    vendor id:      0x%x\n", device->vendor_id);
    rt_kprintf("    device id:      0x%x\n", device->device_id);
    rt_kprintf("    class code:     0x%x\n", device->class_code);
    rt_kprintf("    irq line: %d\n", device->irq_line);
    int i;
    for (i = 0; i < PCI_MAX_BAR_NR; i++)
    {
        if (device->bars[i].type != PCI_BAR_TYPE_INVALID)
        {
            rt_kprintf("    bar %d:\n", i);
            rt_pci_device_bar_dump(&device->bars[i]);
        }
    }
    rt_kprintf("\n");
}

static void list_pci_device()
{
    rt_kprintf("list pci device:\n");
    rt_pci_device_t *device;
    rt_list_for_each_entry(device, &g_pci_device_list_head, list)
    {
        rt_pci_device_list(device);
    }
}
#endif  /* RT_USING_FINSH */

void rt_pci_init(void)
{
    rt_list_init(&g_pci_device_list_head);
    rt_pci_scan_all_buses();
    rt_kprintf("PCI device: device found %d.\n", pic_get_connected_counts());
}

#ifdef RT_USING_FINSH
#include <finsh.h>
MSH_CMD_EXPORT(list_pci_device, list PCI device on computer)
#endif /* RT_USING_FINSH */
