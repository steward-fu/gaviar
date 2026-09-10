/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-08-19     JasonHu      first version
 */

#include <board.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <rthw.h>
#include <netif/ethernetif.h>
#include <pci.h>
#include <mmu.h>

#define DBG_LVL DBG_INFO
#define DBG_TAG "PCNET32"
#include <rtdbg.h>

#include "drv_pcnet32.h"

#define DEV_NAME "e0"

#define GET_PCNET32(eth)    (struct eth_device_pcnet32 *)(eth)

/**
 * rx ring desc struct
 */
struct pcnet32_rx_desc
{
    rt_uint32_t base;   /* buffer base addr */
    rt_uint16_t buf_length; /* two`s complement of length */
    rt_uint16_t status; /* desc status */
    rt_uint16_t msg_length; /*  Message Byte Count is the length in bytes of the received message. */
    rt_uint16_t rpc_rcc;
    rt_uint32_t reserved;
} __attribute__ ((packed));

/**
 * tx ring desc struct
 */
struct pcnet32_tx_desc
{
    rt_uint32_t base;   /* buffer base addr */
    rt_uint16_t buf_length; /* two`s complement of length */
    rt_uint16_t status; /* desc status */
    rt_uint32_t misc;
    rt_uint32_t reserved;
} __attribute__ ((packed));

/**
 * The PCNET32 32-Bit initialization block, described in databook.
 * The Mode Register (CSR15) allows alteration of the chip's operating
 * parameters. The Mode field of the Initialization Block is copied directly
 * into CSR15. Normal operation is the result of configuring the Mode field
 * with all bits zero.
 */
struct pcnet32_init_block
{
    rt_uint16_t mode;
    rt_uint16_t tlen_rlen;
    rt_uint8_t  phys_addr[6];
    rt_uint16_t reserved;
    rt_uint32_t filter[2];
    /* Receive and transmit ring base, along with extra bits. */
    rt_uint32_t rx_ring;
    rt_uint32_t tx_ring;
} __attribute__ ((packed));

struct eth_device_pcnet32
{
    /* inherit from Ethernet device */
    struct eth_device parent;
    /* interface address info. */
    rt_uint8_t dev_addr[ETH_ALEN];         /* MAC address  */

    rt_pci_device_t *pci_dev;   /* pci device info */

    rt_uint32_t iobase; /* io port base */
    rt_uint32_t irqno;  /* irq number */

    rt_mq_t rx_mqueue;  /* msg queue for rx */

    struct pcnet32_init_block *init_block;

    rt_uint16_t rx_len_bits;
    rt_uint16_t tx_len_bits;

    rt_ubase_t rx_ring_dma_addr;
    rt_ubase_t tx_ring_dma_addr;

    rt_ubase_t init_block_dma_addr;

    rt_ubase_t rx_buffer_ptr;
    rt_ubase_t tx_buffer_ptr;  /* pointers to transmit/receive buffers */

    rt_size_t rx_buffer_count;   /* total number of receive buffers */
    rt_size_t tx_buffer_count;   /* total number of transmit buffers */

    rt_size_t buffer_size;  /* length of each packet buffer */

    rt_size_t de_size;    /* length of descriptor entry */

    struct pcnet32_rx_desc *rdes;   /* pointer to ring buffer of receive des */
    struct pcnet32_tx_desc *tdes;   /* pointer to ring buffer of transmit des */

    rt_uint32_t rx_buffers; /* physical address of actual receive buffers (< 4 GiB) */
    rt_uint32_t tx_buffers; /* physical address of actual transmit buffers (< 4 GiB) */
};
static struct eth_device_pcnet32 eth_dev;
static rt_uint8_t rx_cache_send_buf[RX_MSG_SIZE] = {0};     /* buf for rx packet, put size and data into mq */
static rt_uint8_t rx_cache_recv_buf[RX_MSG_SIZE] = {0};     /* buf for rx packet, get size and data from mq */
static rt_uint8_t tx_cache_pbuf[TX_CACHE_BUF_SIZE] = {0};   /* buf for tx packet, get data from pbuf payload */

/**
 * does the driver own the particular buffer?
 */
rt_inline rt_bool_t pcnet32_is_driver_own(struct eth_device_pcnet32 *dev, rt_bool_t is_tx, rt_uint32_t idx)
{
    return (rt_bool_t)(is_tx ? ((dev->tdes[idx].status & PCNET32_DESC_STATUS_OWN) == 0) :
                   ((dev->rdes[idx].status & PCNET32_DESC_STATUS_OWN) == 0));
}

/*
 * get the next desc buffer index
 */
rt_inline rt_uint32_t pcnet32_get_next_desc(struct eth_device_pcnet32 *dev, rt_uint32_t cur_idx, rt_uint32_t buf_count)
{
    return (cur_idx + 1) % buf_count;
}

static void pcnet32_init_rx_desc_entry(struct eth_device_pcnet32 *dev, rt_uint32_t idx)
{
    struct pcnet32_rx_desc *des = dev->rdes + idx;
    rt_memset(des, 0, dev->de_size);

    des->base = dev->rx_buffers + idx * dev->buffer_size;

    /* next 2 bytes are 0xf000 OR'd with the first 12 bits of the 2s complement of the length */
    rt_uint16_t bcnt = (rt_uint16_t)(-dev->buffer_size);
    bcnt &= 0x0fff;
    bcnt |= 0xf000; /* high 4 bits fixed 1 */
    des->buf_length = bcnt;

    /* finally, set ownership bit - transmit buffers are owned by us, receive buffers by the card */
    des->status = PCNET32_DESC_STATUS_OWN;
}

static void pcnet32_init_tx_desc_entry(struct eth_device_pcnet32 *dev, rt_uint32_t idx)
{
    struct pcnet32_tx_desc *des = dev->tdes + idx;
    rt_memset(des, 0, dev->de_size);

    des->base = dev->tx_buffers + idx * dev->buffer_size;

    /* next 2 bytes are 0xf000 OR'd with the first 12 bits of the 2s complement of the length */
    rt_uint16_t bcnt = (rt_uint16_t)(-dev->buffer_size);
    bcnt &= 0x0fff;
    bcnt |= 0xf000; /* high 4 bits fixed 1 */
    des->buf_length = bcnt;
}

static rt_uint16_t pcnet32_wio_read_mac(rt_uint32_t addr, int index)
{
    return inw(addr + index);
}

/*
 * write index to RAP, read data from RDP
 */
static rt_uint16_t pcnet32_wio_read_csr(rt_uint32_t addr, int index)
{
    outw(addr + PCNET32_WIO_RAP, index);
    return inw(addr + PCNET32_WIO_RDP);
}

/**
 * write index to RAP, write data to RDP
 */
static void pcnet32_wio_write_csr(rt_uint32_t addr, int index, rt_uint16_t val)
{
    outw(addr + PCNET32_WIO_RAP, index);
    outw(addr + PCNET32_WIO_RDP, val);
}

static void pcnet32_wio_write_bcr(rt_uint32_t addr, int index, rt_uint16_t val)
{
    outw(addr + PCNET32_WIO_RAP, index);
    outw(addr + PCNET32_WIO_BDP, val);
}

/*
 * Reset causes the device to cease operation and clear its internal logic.
 */
static void pcnet32_wio_reset(rt_uint32_t addr)
{
    inw(addr + PCNET32_WIO_RESET);
}

static int pcnet32_get_pci(struct eth_device_pcnet32 *dev)
{
    /* get pci device */
    rt_pci_device_t *pci_dev = rt_pci_device_get(PCNET32_VENDOR_ID, PCNET32_DEVICE_ID);
    if (pci_dev == RT_NULL)
    {
        LOG_E("device not find on pci device.\n");
        return -1;
    }
    dev->pci_dev = pci_dev;
    dbg_log(DBG_LOG, "find device, vendor id: 0x%x, device id: 0x%x\n",
          pci_dev->vendor_id, pci_dev->device_id);

    /* enable bus mastering */
    rt_pci_enable_bus_mastering(pci_dev);

    /* get io port address */
    dev->iobase = rt_pci_device_get_io_addr(pci_dev);
    if (dev->iobase == 0)
    {
        LOG_E("invalid pci device io address.\n");
        return -1;
    }
    dbg_log(DBG_LOG, "io base address: 0x%x\n", dev->iobase);
    /* get irq */
    dev->irqno = rt_pci_device_get_irq_line(pci_dev);
    if (dev->irqno == 0xff)
    {
        LOG_E("invalid irqno.\n");
        return -1;
    }
    dbg_log(DBG_LOG, "irqno %d\n", dev->irqno);
    return 0;
}

static int pcnet32_transmit(struct eth_device_pcnet32 *dev, rt_uint8_t *buf, rt_size_t len)
{
    if(len > ETH_FRAME_LEN)
    {
        len = ETH_FRAME_LEN;
    }

    rt_uint32_t tx_retry = PCNET32_TX_RETRY;

    while (tx_retry > 0)
    {
        /* the next available descriptor entry index is in tx_buffer_ptr */
        if(!pcnet32_is_driver_own(dev, RT_TRUE, dev->tx_buffer_ptr))
        {
            /* try encourage the card to send all buffers. */
            pcnet32_wio_write_csr(dev->iobase, CSR0, pcnet32_wio_read_csr(dev->iobase, CSR0) | CSR0_TXPOLL);
        }
        else
        {
            break;
        }
        --tx_retry;
    }

    if (!tx_retry)  /* retry end, no entry available */
    {
        dbg_log(DBG_ERROR, "transmit no available descriptor entry\n")
        return -1;
    }

    rt_memcpy((void *)(dev->tx_buffers + dev->tx_buffer_ptr * dev->buffer_size), buf, len);

    struct pcnet32_tx_desc *tdes = dev->tdes + dev->tx_buffer_ptr;
    /**
     * set the STP bit in the descriptor entry (signals this is the first
     * frame in a split packet - we only support single frames)
     */
    tdes->status |= PCNET32_DESC_STATUS_STP;

    /* similarly, set the ENP bit to state this is also the end of a packet */
    tdes->status |= PCNET32_DESC_STATUS_ENP;

    rt_uint16_t bcnt = (rt_uint16_t)(-len);
    bcnt &= 0xfff;
    bcnt |= 0xf000; /* high 4 bits fixed 1 */
    tdes->buf_length = bcnt;

    /* finally, flip the ownership bit back to the card */
    tdes->status |= PCNET32_DESC_STATUS_OWN;

    dev->tx_buffer_ptr = pcnet32_get_next_desc(dev, dev->tx_buffer_ptr, dev->tx_buffer_count);
    return 0;
}

static rt_err_t pcnet32_tx(rt_device_t device, struct pbuf *p)
{
    rt_err_t err = RT_EOK;
    /* copy data from pbuf to tx cache */
    pbuf_copy_partial(p, (void *)&tx_cache_pbuf[0], p->tot_len, 0);
    if (pcnet32_transmit(GET_PCNET32(device), tx_cache_pbuf, p->tot_len) < 0)
    {
        err = RT_ERROR;
    }
    return err;
}

static struct pbuf *pcnet32_rx(rt_device_t device)
{
    struct eth_device_pcnet32 *dev = GET_PCNET32(device);
    int recv_len = 0;
    struct pbuf *pbuf = RT_NULL;
    rt_err_t err;

    /* get data from rx queue. */
    err = rt_mq_recv_interruptible(dev->rx_mqueue, rx_cache_recv_buf, RX_MSG_SIZE, 0);
    if (err != RT_EOK)
    {
        return pbuf;
    }
    /* get recv len from rx cache, 0~3: recv len, 3-n: frame data */
    recv_len = *(int *)rx_cache_recv_buf;
    if (recv_len > 0)
    {
        pbuf = pbuf_alloc(PBUF_LINK, recv_len, PBUF_RAM);
        rt_memcpy(pbuf->payload, (char *)rx_cache_recv_buf + 4, recv_len);
    }
    return pbuf;
}

static rt_err_t pcnet32_control(rt_device_t device, int cmd, void *args)
{
    struct eth_device_pcnet32 *dev = GET_PCNET32(device);
    switch(cmd)
    {
    case NIOCTL_GADDR:
        /* get MAC address */
        if(args)
        {
            rt_memcpy(args, dev->dev_addr, ETH_ALEN);
        }
        else
        {
            return -RT_ERROR;
        }
        break;
    default :
        break;
    }
    return RT_EOK;
}

static void pcnet32_rx_packet(struct eth_device_pcnet32 *dev)
{
    while (pcnet32_is_driver_own(dev, RT_FALSE, dev->rx_buffer_ptr))
    {
        struct pcnet32_rx_desc *rdes = dev->rdes + dev->rx_buffer_ptr;
        rt_uint32_t plen = rdes->msg_length; /* msg len no need to negate it unlike BCNT above */

        void *pbuf = (void *)(dev->rx_buffers + dev->rx_buffer_ptr * dev->buffer_size);
        dbg_log(DBG_LOG, "recv packet on ring %d: buf=%p, len=%d\n", dev->rx_buffer_ptr, pbuf, plen);
        /* merge size and data into receive pkg */
        rt_memcpy(rx_cache_send_buf, &plen, 4);
        rt_memcpy(&rx_cache_send_buf[4], pbuf, plen);

        rt_mq_send_interrupt(dev->rx_mqueue, rx_cache_send_buf, plen + 4);
        eth_device_ready(&dev->parent); /* notify eth thread to read packet */

        /* hand the buffer back to the card */
        rdes->status = PCNET32_DESC_STATUS_OWN;

        dev->rx_buffer_ptr = pcnet32_get_next_desc(dev, dev->rx_buffer_ptr, dev->rx_buffer_count);
    }
}

static void rt_hw_pcnet32_isr(int vector, void *param)
{
    struct eth_device_pcnet32 *dev = GET_PCNET32(param);
    rt_uint32_t iobase = dev->iobase;
    rt_uint32_t csr0 = pcnet32_wio_read_csr(iobase, 0);

    if (csr0 & CSR0_RINT) /* recv packet */
    {
        dbg_log(DBG_LOG, "RX intr occur!\n");
        pcnet32_rx_packet(dev);
    }
    else if ((csr0 & CSR0_TINT))    /* packet transmitted */
    {
        dbg_log(DBG_LOG, "TX intr occur!\n");
    }
    else if ((csr0 & CSR0_IDON))
    {
        dbg_log(DBG_INFO, "init done\n");
    }
    else if ((csr0 & CSR0_MERR))
    {
        dbg_log(DBG_WARNING, "memory error!\n");
    }
    else if ((csr0 & CSR0_MISS))
    {
        dbg_log(DBG_WARNING, "missed frame!\n");
    }
    else if ((csr0 & CSR0_CERR))
    {
        dbg_log(DBG_WARNING, "collision error!\n");
    }
    else if ((csr0 & CSR0_BABL))
    {
        dbg_log(DBG_WARNING, "transmitter time-out error!\n");
    }
    else
    {
        dbg_log(DBG_WARNING, "unknown intr\n");
    }
    /* ack pcnet32 interrupt as handled */
    pcnet32_wio_write_csr(iobase, 0, csr0);
}

static rt_err_t pcnet32_alloc_ring_buffer(struct eth_device_pcnet32 *dev)
{
    dev->rdes = rt_malloc_align(dev->rx_buffer_count * dev->de_size, 16);
    if (dev->rdes == RT_NULL)
    {
        dbg_log(DBG_ERROR, "alloc memory for rx ring failed!");
        return RT_ERROR;
    }
    dev->tdes = rt_malloc_align(dev->tx_buffer_count * dev->de_size, 16);
    if (dev->tdes == RT_NULL)
    {
        dbg_log(DBG_ERROR, "alloc memory for tx ring failed!");
        rt_free_align(dev->rdes);
        return RT_ERROR;
    }

    dev->rx_buffers = (uint32_t)rt_malloc_align(dev->rx_buffer_count * dev->buffer_size, 16);
    if (dev->rx_buffers == 0)
    {
        dbg_log(DBG_ERROR, "alloc memory for rx ring buffer failed!");
        rt_free_align(dev->rdes);
        rt_free_align(dev->tdes);
        return RT_ERROR;
    }

    dev->tx_buffers = (uint32_t)rt_malloc_align(dev->tx_buffer_count * dev->buffer_size, 16);
    if (dev->tx_buffers == 0)
    {
        dbg_log(DBG_ERROR, "alloc memory for tx ring buffer failed!");
        rt_free_align(dev->rdes);
        rt_free_align(dev->tdes);
        rt_free_align((void *)dev->rx_buffers);
        return RT_ERROR;
    }
    dbg_log(DBG_LOG, "rdes:%p tdes:%p rbuf:%p tbuf:%p\n", dev->rdes, dev->tdes, dev->rx_buffers, dev->tx_buffers);

    int i = 0;
    for (i = 0; i < dev->rx_buffer_count; i++)
    {
        pcnet32_init_rx_desc_entry(dev, i);
    }
    for (i = 0; i < dev->tx_buffer_count; i++)
    {
        pcnet32_init_tx_desc_entry(dev, i);
    }
    return RT_EOK;
}

static void pcnet32_free_ring_buffer(struct eth_device_pcnet32 *dev)
{
    rt_free_align(dev->rdes);
    rt_free_align(dev->tdes);
    rt_free_align((void *)dev->rx_buffers);
    rt_free_align((void *)dev->tx_buffers);
}

static void pcnet32_print_init_block(struct eth_device_pcnet32 *dev)
{
    rt_uint32_t iobase = dev->iobase;

    struct pcnet32_init_block *init_block = dev->init_block;
    dbg_log(DBG_LOG, "============\nprint init block\n");
    dbg_log(DBG_LOG, "mode: %x, tlen_rlen:%x\n", init_block->mode, init_block->tlen_rlen);
    dbg_log(DBG_LOG, "mac: %x:%x:%x:%x:%x:%x\n",
            init_block->phys_addr[0],
            init_block->phys_addr[1],
            init_block->phys_addr[2],
            init_block->phys_addr[3],
            init_block->phys_addr[4],
            init_block->phys_addr[5]);
    dbg_log(DBG_LOG, "filter0: %x, filter1: %x\n", init_block->filter[0], init_block->filter[1]);
    dbg_log(DBG_LOG, "rx ring dma: %x, tx ring dma: %x\n", init_block->rx_ring, init_block->tx_ring);
    dbg_log(DBG_LOG, "init block dma: %x\n", dev->init_block_dma_addr);

    int i = 0;
    for (; i <= 46; i++)
    {
        dbg_log(DBG_LOG, "csr%d=%x\n", i, pcnet32_wio_read_csr(iobase, i));
    }
}

static rt_err_t pcnet32_init(rt_device_t device)
{
    struct eth_device_pcnet32 *dev = GET_PCNET32(device);
    rt_uint32_t iobase = dev->iobase;

    /* init buffer info */
    dev->rx_buffer_ptr = 0;
    dev->tx_buffer_ptr = 0;

    dev->rx_buffer_count = PCNET32_RX_BUFFERS;
    dev->tx_buffer_count = PCNET32_TX_BUFFERS;

    dev->buffer_size = ETH_FRAME_LEN;
    dev->de_size = PCNET32_RING_DE_SIZE;

    if (pcnet32_alloc_ring_buffer(dev) != RT_EOK)
    {
        return RT_ERROR;
    }

    dev->rx_ring_dma_addr = (rt_ubase_t)rt_hw_vir2phy(dev->rdes);
    dev->tx_ring_dma_addr = (rt_ubase_t)rt_hw_vir2phy(dev->tdes);

    /* create msg queue for eth rx */
    dev->rx_mqueue = rt_mq_create("rx_mqueue", RX_MSG_SIZE, RX_MSG_CNT, 0);
    if (dev->rx_mqueue == RT_NULL)
    {
        LOG_E("crete msg queue for rx buffer failed!\n");
        pcnet32_free_ring_buffer(dev);
        return RT_ERROR;
    }

    /* alloc init block, must 16 bit align */
    dev->init_block = rt_malloc_align(sizeof(struct pcnet32_init_block), 16);
    if (dev->init_block == RT_NULL)
    {
        dbg_log(DBG_ERROR, "alloc memory for init block failed!");
        rt_mq_delete(dev->rx_mqueue);
        pcnet32_free_ring_buffer(dev);
        return RT_ERROR;
    }
    dev->init_block_dma_addr = (rt_ubase_t)rt_hw_vir2phy(dev->init_block);

    dbg_log(DBG_LOG, "init block addr:%p size:%d\n", dev->init_block, sizeof(struct pcnet32_init_block));

    /* fill init block */
    dev->init_block->mode = 0;
    dev->tx_len_bits = (PCNET32_LOG_TX_BUFFERS << 4);
    dev->rx_len_bits = (PCNET32_LOG_RX_BUFFERS << 4);
    dev->init_block->tlen_rlen = (dev->tx_len_bits << 8) | dev->rx_len_bits;
    int i = 0;
    for (i = 0; i < ETH_ALEN; i++)
    {
        dev->init_block->phys_addr[i] = dev->dev_addr[i];
    }
    dev->init_block->filter[0] = 0x00000000;
    dev->init_block->filter[1] = 0x00000000;
    dev->init_block->rx_ring = dev->rx_ring_dma_addr;
    dev->init_block->tx_ring = dev->tx_ring_dma_addr;

    /* register init block, CSR1 save low 16 bit, CSR1 save high 16 bit */
    pcnet32_wio_write_csr(iobase, CSR1, (dev->init_block_dma_addr & 0xffff));
    pcnet32_wio_write_csr(iobase, CSR2, (dev->init_block_dma_addr >> 16) & 0xffff);

    /* register intr */
    if (rt_hw_interrupt_install(dev->irqno, rt_hw_pcnet32_isr, (void *) dev, "pcnet32") < 0)
    {
        LOG_E("install IRQ failed!\n");
        rt_free_align(dev->init_block);
        rt_mq_delete(dev->rx_mqueue);
        pcnet32_free_ring_buffer(dev);
        return RT_ERROR;
    }
    rt_hw_interrupt_umask(dev->irqno);

    /* Start init */
    pcnet32_wio_write_csr(iobase, CSR0, CSR0_INIT | CSR0_INTEN);
    dbg_log(DBG_LOG, "card init done.\n");

    /* add auto pad amd strip recv */
    rt_uint16_t csr4 = pcnet32_wio_read_csr(iobase, CSR4);
    pcnet32_wio_write_csr(iobase, CSR4, csr4 | CSR4_ASTRP_RCV | CSR4_APAD_XMT);

    /* start work */
    pcnet32_wio_write_csr(iobase, CSR0, CSR0_START | CSR0_INTEN);

    pcnet32_print_init_block(dev);

    eth_device_linkchange(&dev->parent, RT_TRUE);
    return RT_EOK;
}

#ifdef RT_USING_DEVICE_OPS
const static struct rt_device_ops pcnet32_ops =
{
    pcnet32_init,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    pcnet32_control
};
#endif

static rt_err_t pcnet32_init_hw(struct eth_device_pcnet32 *dev)
{
    rt_uint32_t iobase = dev->iobase;

    /* reset card to 16 bit io mode */
    pcnet32_wio_reset(iobase);

    /* use dealy to wait reset done, at least 1 microsecond */
    rt_thread_delay(1);

    /* switch to 32 bit soft-style mode, use 32 bit struct */
    pcnet32_wio_write_bcr(iobase, 20, 0x102);

    /* stop card work */
    pcnet32_wio_write_csr(iobase, 0, 0x4);

    /* read mac addr */
    rt_uint16_t mac0 = pcnet32_wio_read_mac(iobase, 0);
    rt_uint16_t mac1 = pcnet32_wio_read_mac(iobase, 2);
    rt_uint16_t mac2 = pcnet32_wio_read_mac(iobase, 4);

    dev->dev_addr[0] = mac0 & 0xff;
    dev->dev_addr[1] = (mac0 >> 8) & 0xff;
    dev->dev_addr[2] = mac1 & 0xff;
    dev->dev_addr[3] = (mac1 >> 8) & 0xff;
    dev->dev_addr[4] = mac2 & 0xff;
    dev->dev_addr[5] = (mac2 >> 8) & 0xff;

    dbg_log(DBG_INFO, "MAC addr: %x:%x:%x:%x:%x:%x\n",
        dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
        dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);
    return RT_EOK;
}

static int rt_hw_pcnet32_init(void)
{
    rt_memset(&eth_dev, 0x0, sizeof(eth_dev));

    if (pcnet32_get_pci(&eth_dev) < 0)
    {
        return -1;
    }

    if (pcnet32_init_hw(&eth_dev) != RT_EOK)
    {
        return -1;
    }

    /* set device opts */
#ifdef RT_USING_DEVICE_OPS
    eth_dev.parent.parent.ops        = &pcnet32_ops;
#else
    eth_dev.parent.parent.init       = pcnet32_init;
    eth_dev.parent.parent.open       = RT_NULL;
    eth_dev.parent.parent.close      = RT_NULL;
    eth_dev.parent.parent.read       = RT_NULL;
    eth_dev.parent.parent.write      = RT_NULL;
    eth_dev.parent.parent.control    = pcnet32_control;
#endif
    eth_dev.parent.parent.user_data  = RT_NULL;
    eth_dev.parent.eth_rx     = pcnet32_rx;
    eth_dev.parent.eth_tx     = pcnet32_tx;

    /* register ETH device */
    if (eth_device_init(&(eth_dev.parent), DEV_NAME) != RT_EOK)
    {
        return -1;
    }
    return 0;
}
INIT_DEVICE_EXPORT(rt_hw_pcnet32_init);
