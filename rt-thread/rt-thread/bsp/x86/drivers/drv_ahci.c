/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2021-08-04     JasonHu       first version
 */

#include <rtconfig.h>

#ifdef BSP_DRV_AHCI

#include <rtthread.h>
#include <rtdef.h>
#include <rtdbg.h>
#include <rthw.h>
#include <ioremap.h>
#include <dma.h>
#include <mmu.h>

#include <board.h>

#include "drv_ahci.h"
#include "pci.h"

#define DEV_NAME "sd"

// #define RT_DRV_AHCI_DEBUG

#ifdef RT_DRV_AHCI_DEBUG
    #define dbgprint rt_kprintf
#else
    #define dbgprint(...)
#endif

/* memio info on the pci bar 5 */
#define PCI_AHCI_MEMIO_BAR   5

#define LOWER32(a) (rt_uint32_t)((a) & 0xffffffff)
#define LOWER8(a) (rt_uint8_t)((a) & 0xff)
#define HIGHER8(a) (rt_uint8_t)(((a) >> 8)  & 0xff)

/* maxim ports we support */
#define DRV_AHCI_PORT_NR    32

struct device_extension
{
    rt_uint64_t sector_count;   /* sectors in this disk. */
    rt_uint8_t type;    /* AHCI device type */
    rt_uint8_t port;    /* port for each device. */
    rt_uint32_t slots;  /* solts for device read/write transfer bits */
    struct rt_mutex lock;   /* lock for disk read/write */
    void *fis_vaddr;
    void *clb_vaddr;
    rt_hw_dma_t clb_dma;
    rt_hw_dma_t fis_dma;
    void *cmd_hdrs[HBA_COMMAND_HEADER_NUM];             /* command header */
    rt_hw_dma_t cmd_hdrs_dmas[HBA_COMMAND_HEADER_NUM];  /* command header dma */
};
typedef struct device_extension rt_device_extension_t;

static struct hba_memory *g_hba_base; /* hba memory io base addr */

static rt_err_t ahci_create_device(rt_device_extension_t *extension);

static rt_uint32_t ahci_flush_commands(struct hba_port *port)
{
    /* the commands may not take effect until the command
     * register is read again by software, because reasons.
     */
    rt_hw_dsb();
    volatile rt_uint32_t c = port->command;
    rt_hw_dmb();
    return c;
}

static void ahci_stop_port_command_engine(struct hba_port *port)
{
    rt_hw_dsb();
    port->command &= ~HBA_PxCMD_ST;
    rt_hw_dsb();
    port->command &= ~HBA_PxCMD_FRE;
    rt_hw_dmb();
    while((port->command & HBA_PxCMD_CR) || (port->command & HBA_PxCMD_FR))
    {
        rt_hw_cpu_pause();
    }
}

static void ahci_start_port_command_engine(struct hba_port *port)
{
    rt_hw_dmb();
    while(port->command & HBA_PxCMD_CR)
    {
        rt_hw_cpu_pause();
    }
    rt_hw_dsb();
    port->command |= HBA_PxCMD_FRE;
    rt_hw_dsb();
    port->command |= HBA_PxCMD_ST;
    ahci_flush_commands((struct hba_port *)port);
}

static struct hba_command_header *ahci_initialize_command_header(rt_device_extension_t *dev, struct hba_memory *abar,
                                                                 struct hba_port *port, int slot, int write,
                                                                 int atapi, int prd_entries, int fis_len)
{
    struct hba_command_header *hdr = (struct hba_command_header *)dev->clb_vaddr;
    hdr += slot;
    hdr->write = write ? 1 : 0;
    hdr->prdb_count = 0;
    hdr->atapi=atapi ? 1 : 0;
    hdr->fis_length = fis_len;
    hdr->prdt_len = prd_entries;
    hdr->prefetchable = 0;
    hdr->bist = 0;
    hdr->pmport = 0;
    hdr->reset = 0;
    return hdr;
}

static struct fis_reg_host_to_device *ahci_initialize_fis_host_to_device(rt_device_extension_t *dev, struct hba_memory *abar,
                                                                         struct hba_port *port, int slot, int cmdctl, int ata_command)
{
    struct hba_command_table *tbl = (struct hba_command_table *)(dev->cmd_hdrs[slot]);
    struct fis_reg_host_to_device *fis = (struct fis_reg_host_to_device *)(tbl->command_fis);

    rt_memset(fis, 0, sizeof(*fis));
    fis->fis_type = FIS_TYPE_REG_H2D;
    fis->command = ata_command;
    fis->c = cmdctl ? 1 : 0;
    return fis;
}

static void ahci_send_command(struct hba_port *port, int slot)
{
    port->interrupt_status = ~0;
    port->command_issue = (1 << slot);
    ahci_flush_commands(port);
}

static int ahci_write_prdt(rt_device_extension_t *dev, struct hba_memory *abar, struct hba_port *port,
                    int slot, int offset, int length, rt_ubase_t virt_buffer)
{
    int num_entries = ((length - 1) / PRDT_MAX_COUNT) + 1;
    struct hba_command_table *tbl = (struct hba_command_table *)(dev->cmd_hdrs[slot]);
    int i;
    struct hba_prdt_entry *prd;

    for(i = 0; i < num_entries - 1; i++)
    {
        rt_ubase_t phys_buffer;
        phys_buffer = rt_hw_vir2phy(virt_buffer);
        prd = &tbl->prdt_entries[i + offset];
        prd->byte_count = PRDT_MAX_COUNT - 1;
        prd->data_base_l = LOWER32(phys_buffer);
        prd->data_base_h = 0;
        prd->interrupt_on_complete = 0;

        length -= PRDT_MAX_COUNT;
        virt_buffer += PRDT_MAX_COUNT;
    }

    rt_ubase_t phys_buffer;
    phys_buffer = rt_hw_vir2phy(virt_buffer);
    prd = &tbl->prdt_entries[i + offset];
    prd->byte_count = length - 1;
    prd->data_base_l = LOWER32(phys_buffer);
    prd->data_base_h = 0;
    prd->interrupt_on_complete = 0;
    return num_entries;
}

static void ahci_reset_device(struct hba_memory *abar, struct hba_port *port, rt_device_extension_t *dev)
{
    dbgprint("[ahci] device port %d: sending COMRESET and reinitializing\n", dev->port);
    ahci_stop_port_command_engine(port);
    port->sata_error = ~0;
    /* power on, spin up */
    port->command |= 2;
    port->command |= 4;
    ahci_flush_commands(port);
    rt_thread_mdelay(1);

    /* initialize state */
    port->interrupt_status = ~0; /* clear pending interrupts */
    port->interrupt_enable = AHCI_DEFAULT_INT; /* we want some interrupts */
    port->command &= ~((1 << 27) | (1 << 26)); /* clear some bits */
    port->sata_control |= 1;
    rt_thread_mdelay(10);
    port->sata_control |= (~1);
    rt_thread_mdelay(10);
    port->interrupt_status = ~0; /* clear pending interrupts */
    port->interrupt_enable = AHCI_DEFAULT_INT; /* we want some interrupts */
    ahci_start_port_command_engine(port);
    dev->slots = 0;
    port->sata_error = ~0;
}

static rt_err_t ahci_port_dma_data_transfer(rt_device_extension_t *dev, struct hba_memory *abar, struct hba_port *port,
                                            int slot, int write, rt_ubase_t virt_buffer, int sectors, rt_uint64_t lba)
{
    struct fis_reg_host_to_device *fis;
    int timeout;
    int fis_len = sizeof(struct fis_reg_host_to_device) / 4;

    int ne = ahci_write_prdt(dev, abar, port, slot, 0, ATA_SECTOR_SIZE * sectors, virt_buffer);
    ahci_initialize_command_header(dev, abar, port, slot, write, 0, ne, fis_len);
    fis = ahci_initialize_fis_host_to_device(dev, abar, port, slot, 1, write ? ATA_CMD_WRITE_DMA_EX : ATA_CMD_READ_DMA_EX);

    fis->device = 1 << 6;
    fis->count_l = LOWER8(sectors);
    fis->count_h = HIGHER8(sectors);

    fis->lba0 = (unsigned char)( lba        & 0xFF);
    fis->lba1 = (unsigned char)((lba >> 8)  & 0xFF);
    fis->lba2 = (unsigned char)((lba >> 16) & 0xFF);
    fis->lba3 = (unsigned char)((lba >> 24) & 0xFF);
    fis->lba4 = (unsigned char)((lba >> 32) & 0xFF);
    fis->lba5 = (unsigned char)((lba >> 40) & 0xFF);
    port->sata_error = ~0;

    timeout = ATA_TFD_TIMEOUT;
    while ((port->task_file_data & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && --timeout)
    {
        rt_thread_yield();
    }
    if(!timeout)
    {
        goto port_hung;
    }

    port->sata_error = ~0;
    ahci_send_command(port, slot);
    timeout = ATA_TFD_TIMEOUT;
    while ((port->task_file_data & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && --timeout)
    {
        rt_thread_yield();
    }
    if(!timeout)
    {
        goto port_hung;
    }

    timeout = AHCI_CMD_TIMEOUT;
    while(--timeout)
    {
        if(!((port->sata_active | port->command_issue) & (1 << slot)))
            break;
        rt_thread_yield();
    }
    if(!timeout)
    {
        goto port_hung;
    }
    if(port->sata_error)
    {
        dbg_log(DBG_ERROR, "[ahci] device %d: ahci error\n", dev->port);
        goto error;
    }
    if(port->task_file_data & ATA_DEV_ERR)
    {
        dbg_log(DBG_ERROR, "[ahci] device %d: task file data error\n", dev->port);
        goto error;
    }
    return RT_EOK;
port_hung:
    dbg_log(DBG_ERROR, "[ahci] device %d: port hung\n", dev->port);
error:
    dbg_log(DBG_ERROR, "[ahci] device %d: tfd=%x, serr=%x\n",
            dev->port, port->task_file_data, port->sata_error);
    ahci_reset_device(abar, port, dev);
    return RT_ERROR;
}

static rt_err_t ahci_device_identify(rt_device_extension_t *dev, struct hba_memory *abar, struct hba_port *port)
{
    int fis_len = sizeof(struct fis_reg_host_to_device) / 4;
    rt_hw_dma_t dma;
    dma.size = 0x1000;
    dma.alignment = 0x1000;
    RT_ASSERT(rt_hw_dma_alloc(&dma) == RT_EOK);

    ahci_write_prdt(dev, abar, port, 0, 0, 512, (rt_ubase_t)dma.vaddr);
    ahci_initialize_command_header(dev, abar, port, 0, 0, 0, 1, fis_len);
    ahci_initialize_fis_host_to_device(dev, abar, port, 0, 1, ATA_CMD_IDENTIFY);

    int timeout = ATA_TFD_TIMEOUT;
    port->sata_error = ~0;
    while ((port->task_file_data & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && --timeout)
    {
        rt_hw_cpu_pause();
    }
    if(!timeout )
    {
        dbg_log(DBG_ERROR, "[ahci] device %d: identify 1: port hung\n", dev->port);
        dbg_log(DBG_ERROR, "[ahci] device %d: identify 1: tfd=%x, serr=%x\n",
                dev->port, port->task_file_data, port->sata_error);
        rt_hw_dma_free(&dma);
        return RT_ETIMEOUT;
    }

    ahci_send_command(port, 0);

    timeout = AHCI_CMD_TIMEOUT;
    while(--timeout)
    {
        if(!((port->sata_active | port->command_issue) & 1))
            break;
    }
    if(!timeout)
    {
        dbg_log(DBG_ERROR, "[ahci] device %d: identify 2: port hung\n", dev->port);
        dbg_log(DBG_ERROR, "[ahci] device %d: identify 2: tfd=%x, serr=%x\n",
                dev->port, port->task_file_data, port->sata_error);
        rt_hw_dma_free(&dma);
        return RT_ETIMEOUT;
    }

    struct ata_identify *identify = (struct ata_identify *) dma.vaddr;
    if (identify->lba48_addressable_sectors)
    {
        dev->sector_count = identify->lba48_addressable_sectors;
    }
    else
    {
        dev->sector_count = 0;
    }
    dbgprint("[ahci] device %d: num sectors=%d\n", dev->port, dev->sector_count);

    rt_hw_dma_free(&dma);

    if (!dev->sector_count)
    {
        dbg_log(DBG_ERROR, "[ahci] device %d invalid sectors ZERO.\n", dev->port);
        return RT_EINVAL;
    }
    return RT_EOK;
}

static rt_uint32_t ahci_check_type(volatile struct hba_port *port)
{
    port->command &= ~1;
    while(port->command & (1 << 15))
    {
        rt_hw_cpu_pause();
    }

    port->command &= ~(1 << 4);
    while(port->command & (1 << 14))
    {
        rt_hw_cpu_pause();
    }

    rt_hw_dsb();
    port->command |= 2;
    rt_hw_dsb();
    rt_thread_mdelay(10);

    rt_uint32_t s = port->sata_status;

    uint8_t ipm, det;
    ipm = (s >> 8) & 0x0F;
    det = s & 0x0F;
    if(ipm != HBA_PORT_IPM_ACTIVE || det != HBA_PORT_DET_PRESENT)
    {
        return AHCI_DEV_NULL;
    }

    switch (port->signature)
    {
    case SATA_SIG_ATAPI:
        return AHCI_DEV_SATAPI;
    case SATA_SIG_SEMB:
        return AHCI_DEV_SEMB;
    case SATA_SIG_PM:
        return AHCI_DEV_PM;
    default:
        return AHCI_DEV_SATA;
    }
    return AHCI_DEV_SATA;
}

int ahci_initialize_device(rt_device_extension_t *dev, struct hba_memory *abar)
{
    struct hba_port *port = (struct hba_port *)&abar->ports[dev->port];
    ahci_stop_port_command_engine(port);
    port->sata_error = ~0;
    /* power on, spin up */
    port->command |= (2 | 4);
    ahci_flush_commands(port);
    rt_thread_mdelay(2);

    /* initialize state */
    port->interrupt_status = ~0; /* clear pending interrupts */
    port->interrupt_enable = AHCI_DEFAULT_INT; /* we want some interrupts */

    port->command &= ~1;
    while(port->command & (1 << 15))
    {
        rt_hw_cpu_pause();
    }

    port->command &= ~((1 << 27) | (1 << 26) | 1); /* clear some bits */
    ahci_flush_commands(port);

    /* start reset sata */
    port->sata_control |= 1;
    rt_thread_mdelay(20);

    /* close DET, after init sata device done. */
    port->sata_control &= (~1);
    rt_thread_mdelay(10);
    while(!(port->sata_status & 1))
    {
        rt_hw_cpu_pause();
    }

    port->sata_error = ~0;
    port->command |= (1 << 28); /* set interface to active */
    while((port->sata_status >> 8) != 1)
    {
        rt_hw_cpu_pause();
    }

    port->interrupt_status = ~0; /* clear pending interrupts */
    port->interrupt_enable = AHCI_DEFAULT_INT; /* we want some interrupts */

    rt_ubase_t clb_phys, fis_phys;
    dev->clb_dma.size = 0x2000;
    dev->clb_dma.alignment = 0x1000;
    dev->fis_dma.size = 0x1000;
    dev->fis_dma.alignment = 0x1000;

    RT_ASSERT(rt_hw_dma_alloc(&dev->clb_dma) == RT_EOK);
    RT_ASSERT(rt_hw_dma_alloc(&dev->fis_dma) == RT_EOK);
    dev->clb_vaddr = (void *)dev->clb_dma.vaddr;
    dev->fis_vaddr = (void *)dev->fis_dma.vaddr;
    clb_phys = dev->clb_dma.paddr;
    fis_phys = dev->fis_dma.paddr;

    dev->slots=0;
    struct hba_command_header *hdr = (struct hba_command_header *)dev->clb_vaddr;
    int i;
    for(i = 0; i < HBA_COMMAND_HEADER_NUM; i++)
    {
        dev->cmd_hdrs_dmas[i].size = 0x1000;
        dev->cmd_hdrs_dmas[i].alignment = 0x1000;

        RT_ASSERT(rt_hw_dma_alloc(&dev->cmd_hdrs_dmas[i]) == RT_EOK);
        dev->cmd_hdrs[i] = (void *)dev->cmd_hdrs_dmas[i].vaddr;
        rt_memset(hdr, 0, sizeof(*hdr));

        hdr->command_table_base_l = LOWER32(dev->cmd_hdrs_dmas[i].paddr);
        hdr->command_table_base_h = 0;

        hdr++;
    }

    port->command_list_base_l = LOWER32(clb_phys);
    port->command_list_base_h = 0;

    port->fis_base_l = LOWER32(fis_phys);
    port->fis_base_h = 0;
    ahci_start_port_command_engine(port);

    port->sata_error = ~0;
    return ahci_device_identify(dev, abar, port);
}

static rt_uint32_t ahci_probe_ports(struct hba_memory *abar)
{
    rt_uint32_t pi = abar->port_implemented;
    dbgprint("[ahci] ports implemented: %x\n", pi);
    int counts = 0; /* exist device count */
    int i = 0;
    rt_device_extension_t *extension;
    while (i < DRV_AHCI_PORT_NR)
    {
        if (pi & 1)
        {
            rt_uint32_t type = ahci_check_type(&abar->ports[i]);
            if (type == AHCI_DEV_SATA) { /* SATA device */
                dbgprint("[ahci] detected SATA device on port %d\n", i);

                extension = rt_malloc(sizeof(rt_device_extension_t));
                if (extension == RT_NULL)
                {
                    dbg_log(DBG_ERROR, "[ahci] port %d alloc memory for extension failed!\n", i);
                    return counts;
                }
                extension->type = type;
                extension->port = i;
                rt_mutex_init(&extension->lock, "ahci", RT_IPC_FLAG_PRIO);
                if (ahci_initialize_device(extension, abar) == RT_EOK)\
                {
                    if (ahci_create_device(extension) == RT_EOK) {
                        counts++;
                    }
                    else
                    {
                        dbg_log(DBG_ERROR, "[ahci] failed to create device %d, disabling port!\n", i);
                        rt_free(extension);
                    }
                } else {
                    dbg_log(DBG_ERROR, "[ahci] failed to initialize device %d, disabling port.\n", i);
                }
            } else if(type == AHCI_DEV_SATAPI) { /* SATAPI device */
                dbg_log(DBG_WARNING, "[ahci] not support SATAPI device on port %d now!\n", i);
            } else if(type == AHCI_DEV_PM) { /* PM device */
                dbg_log(DBG_WARNING, "[ahci] not support Port multiplier on port %d now!\n", i);
            } else if(type == AHCI_DEV_SEMB) { /* SEMB device */
                dbg_log(DBG_WARNING, "[ahci] not support Enclosure management bridge on port %d now!\n", i);
            }
            /* do not deal other type now. */
        }
        i++;
        pi >>= 1;
    }
    return counts;
}

static int ahci_port_get_slot(rt_device_extension_t *dev)
{
    for(;;)
    {
        int i;
        rt_mutex_take(&dev->lock, RT_WAITING_FOREVER);
        for(i = 0; i < DRV_AHCI_PORT_NR; i++)
        {
            if(!(dev->slots & (1 << i)))
            {
                dev->slots |= (1 << i);
                rt_mutex_release(&dev->lock);
                return i;
            }
        }
        rt_mutex_release(&dev->lock);
        rt_thread_yield();
    }
}

void ahci_port_put_slot(rt_device_extension_t *dev, int slot)
{
    rt_mutex_take(&dev->lock, RT_WAITING_FOREVER);
    dev->slots &= ~(1 << slot);
    rt_mutex_release(&dev->lock);
}

/* since a DMA transfer must write to contiguous physical RAM, we need to allocate
 * buffers that allow us to create PRDT entries that do not cross a page boundary.
 * That means that each PRDT entry can transfer a maximum of PAGE_SIZE bytes (for
 * 0x1000 page size, that's 8 sectors). Thus, we allocate a buffer that is page aligned,
 * in a multiple of PAGE_SIZE, so that the PRDT will write to contiguous physical ram
 * (the key here is that the buffer need not be contiguous across multiple PRDT entries).
 */
static rt_size_t ahci_rw_multiple_do(rt_device_extension_t *dev, int rw, rt_uint64_t blk, unsigned char *out_buffer, int count)
{
    rt_uint32_t length = count * ATA_SECTOR_SIZE;
    rt_uint64_t end_blk = dev->sector_count;
    if (blk >= end_blk)
    {
        dbg_log(DBG_ERROR, "ahci: lba %d out of range %d\n", blk, end_blk);
        return 0;
    }

    if((blk + count) > end_blk)
    {
        count = end_blk - blk;
    }
    if(!count)
    {
        return 0;
    }

    int num_pages = ((ATA_SECTOR_SIZE * (count - 1)) / PAGE_SIZE) + 1;
    RT_ASSERT((length <= (unsigned)num_pages * 0x1000));
    rt_hw_dma_t dma;
    dma.size = 0x1000 * num_pages;
    dma.alignment = 0x1000;
    RT_ASSERT(rt_hw_dma_alloc(&dma) == RT_EOK);

    rt_size_t num_read_blocks = count;
    struct hba_port *port = (struct hba_port *)&g_hba_base->ports[dev->port];
    if(rw == 1)
    {
        rt_memcpy((void *)dma.vaddr, out_buffer, length);
    }

    int slot = ahci_port_get_slot(dev);
    if(ahci_port_dma_data_transfer(dev, g_hba_base, port, slot, rw == 1 ? 1 : 0, (rt_ubase_t)dma.vaddr, count, blk) != RT_EOK)
    {
        num_read_blocks = 0;
    }

    ahci_port_put_slot(dev, slot);

    if(rw == 0 && num_read_blocks)
    {
        rt_memcpy(out_buffer, (void *)dma.vaddr, length);
    }

    rt_hw_dma_free(&dma);
    return num_read_blocks;
}

/* and then since there is a maximum transfer amount because of the page size
 * limit, wrap the transfer function to allow for bigger transfers than that even.
 */
static rt_size_t ahci_rw_multiple(rt_device_extension_t *dev, int rw, rt_uint64_t blk, unsigned char *out_buffer, int count)
{
    int i = 0;
    rt_size_t ret = 0;
    int c = count;
    for(i = 0; i < count; i += (PRDT_MAX_ENTRIES * PRDT_MAX_COUNT) / ATA_SECTOR_SIZE)
    {
        int n = (PRDT_MAX_ENTRIES * PRDT_MAX_COUNT) / ATA_SECTOR_SIZE;
        if(n > c)
        {
            n = c;
        }
        ret += ahci_rw_multiple_do(dev, rw, blk+i, out_buffer + ret, n);
        c -= n;
    }
    return ret;
}

static rt_pci_device_t *ahci_get_pci_info(void)
{
    rt_pci_device_t *ahci = rt_pci_device_get(0x1, 0x6);
    if(ahci == RT_NULL)
    {
        ahci = rt_pci_device_get(0x8086, 0x8c03);
    }
    if(ahci == RT_NULL)
    {
        ahci = rt_pci_device_get(0x8086, 0x2922);
    }
    if(ahci == RT_NULL)
    {
        return RT_NULL;
    }

    dbgprint("[ahci] device vendorID %x deviceID %x class code %x\n", ahci->vendor_id, ahci->device_id, ahci->class_code);

    rt_pci_enable_bus_mastering(ahci);

    g_hba_base = rt_ioremap((void *) ahci->bars[PCI_AHCI_MEMIO_BAR].base_addr, ahci->bars[PCI_AHCI_MEMIO_BAR].length);
    if (g_hba_base == RT_NULL) {
        dbgprint("[ahci] device memio_remap on %x length %x failed!\n", ahci->bars[PCI_AHCI_MEMIO_BAR].base_addr, ahci->bars[PCI_AHCI_MEMIO_BAR].length);
        return RT_NULL;
    }
    mmu_flush_tlb();

    dbgprint("[ahci] mapping memory iobase from paddr %x to vaddr %x\n", ahci->bars[PCI_AHCI_MEMIO_BAR].base_addr, g_hba_base);
    dbgprint("[ahci] using interrupt %d\n", ahci->irq_line);
    return ahci;
}

static rt_err_t rt_ahci_init(rt_device_t dev)
{
    return RT_EOK;
}

static rt_err_t rt_ahci_open(rt_device_t dev, rt_uint16_t oflag)
{
    return RT_EOK;
}

static rt_err_t rt_ahci_close(rt_device_t dev)
{
    return RT_EOK;
}

/*
 * position: block page address, not bytes address
 * buffer: read buffer addr
 * size  : how many blocks
 */
static rt_size_t rt_ahci_read(rt_device_t device, rt_off_t position, void *buffer, rt_size_t size)
{
    return ahci_rw_multiple((rt_device_extension_t *)device->user_data, 0, position, (unsigned char *)buffer, size);
}

/*
 * position: block page address, not bytes address
 * buffer: write buffer addr
 * size  : how many blocks
 */
static rt_size_t rt_ahci_write(rt_device_t device, rt_off_t position, const void *buffer, rt_size_t size)
{
    return ahci_rw_multiple((rt_device_extension_t *)device->user_data, 1, position, (unsigned char *)buffer, size);
}

static rt_err_t rt_ahci_control(rt_device_t dev, int cmd, void *args)
{
    RT_ASSERT(dev != RT_NULL);
    RT_ASSERT(dev->user_data != NULL);
    RT_ASSERT(args != RT_NULL);

    rt_device_extension_t *extension = (rt_device_extension_t *)dev->user_data;
    rt_err_t err = RT_EOK;
    switch(cmd)
    {
    case RT_DEVICE_CTRL_BLK_GETGEOME:
        {
            struct rt_device_blk_geometry *geometry;

            geometry = (struct rt_device_blk_geometry *)args;
            if (geometry == RT_NULL)
            {
                return -RT_ERROR;
            }
            geometry->bytes_per_sector = ATA_SECTOR_SIZE;
            geometry->block_size = ATA_SECTOR_SIZE;
            geometry->sector_count = extension->sector_count;
            dbgprint("[ahci] getgeome: bytes_per_sector:%d, block_size:%d, sector_count:%d\n",
                geometry->bytes_per_sector, geometry->block_size, geometry->sector_count);
            break;
        }
    default:
        err = RT_ERROR;
        break;
    }
    return err;
}

static void rt_hw_ahci_isr(int vector, void *param)
{
    int i;
    for (i = 0; i < 32; i++)
    {
        if (g_hba_base->interrupt_status & (1 << i))
        {
            dbgprint("[ahci] interrupt on port %d occured!\n", i);
            g_hba_base->ports[i].interrupt_status = ~0;
            g_hba_base->interrupt_status = (1 << i);
            ahci_flush_commands((struct hba_port *)&g_hba_base->ports[i]);
        }
    }
}

static void ahci_init_hba(struct hba_memory *abar)
{
    if(abar->ext_capabilities & 1)
    {
        /* request BIOS/OS ownership handoff */
        abar->bohc |= (1 << 1);
        while((abar->bohc & 1) || !(abar->bohc & (1<<1)))
        {
            rt_hw_cpu_pause();
        }
    }
    /* enable the AHCI and reset it */
    abar->global_host_control |= HBA_GHC_AHCI_ENABLE;
    abar->global_host_control |= HBA_GHC_RESET;
    /* wait for reset to complete */
    while(abar->global_host_control & HBA_GHC_RESET)
    {
        rt_hw_cpu_pause();
    }

    /* enable the AHCI and interrupts */
    abar->global_host_control |= HBA_GHC_AHCI_ENABLE;
    abar->global_host_control |= HBA_GHC_INTERRUPT_ENABLE;
    rt_thread_mdelay(20);
    dbgprint("[ahci] caps: %x %x ver:%x ctl: %x\n", abar->capability, abar->ext_capabilities, abar->version, abar->global_host_control);
}

#ifdef RT_USING_DEVICE_OPS
const static struct rt_device_ops ahci_ops =
{
    rt_ahci_init,
    rt_ahci_open,
    rt_ahci_close,
    rt_ahci_read,
    rt_ahci_write,
    rt_ahci_control
};
#endif

static rt_err_t ahci_create_device(rt_device_extension_t *extension)
{
    static int ahci_next_device = 0;   /* first is sd0 */
    rt_device_t device = rt_device_create(RT_Device_Class_Block, 0);
    if (device == RT_NULL)
    {
        dbg_log(DBG_ERROR, "[ahci] create device failed!\n");
        return RT_ENOMEM;
    }
    device->user_data = (void *)extension;

#ifdef RT_USING_DEVICE_OPS
    device->ops         = &ahci_ops;
#else
    device->init = rt_ahci_init;
    device->open = rt_ahci_open;
    device->close = rt_ahci_close;
    device->read = rt_ahci_read;
    device->write = rt_ahci_write;
    device->control = rt_ahci_control;
#endif
    char devname[8] = {0};
    rt_sprintf(devname, "%s%c", DEV_NAME, '0' + ahci_next_device);
    ahci_next_device++;
    if (rt_device_register(device, devname, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_STANDALONE) != RT_EOK)
    {
        dbg_log(DBG_ERROR, "[ahci] register device failed!\n");
        rt_device_destroy(device);
        return RT_ENOMEM;
    }
    return RT_EOK;
}

static int rt_hw_ahci_init(void)
{
    /* 1. get pci info */
    rt_pci_device_t *ahci_pci = ahci_get_pci_info();
    if(ahci_pci == RT_NULL)
    {
        dbg_log(DBG_ERROR, "[ahci] no AHCI controllers present!\n");
        return RT_ERROR;
    }

    /* 2. install intr */
    if (rt_hw_interrupt_install(ahci_pci->irq_line, rt_hw_ahci_isr, RT_NULL, "ahci") < 0)
    {
        dbg_log(DBG_ERROR, "[ahci] install IRQ failed!\n");
        rt_iounmap(g_hba_base);
        return RT_ERROR;
    }
    rt_hw_interrupt_umask(ahci_pci->irq_line);

    /* 3. init ahci device */
    ahci_init_hba(g_hba_base);
    if (!ahci_probe_ports(g_hba_base))
    {
        dbg_log(DBG_ERROR, "[ahci] initializing ahci driver failed!.\n");
        rt_hw_interrupt_mask(ahci_pci->irq_line);
        rt_iounmap(g_hba_base);
        return RT_ERROR;
    }
    rt_kprintf("[ahci] disk driver init done!\n");
    return RT_EOK;
}

#ifdef RT_USING_COMPONENTS_INIT
INIT_DEVICE_EXPORT(rt_hw_ahci_init);
#endif
#endif  /* BSP_DRV_AHCI */
