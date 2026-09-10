/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-08-16     JasonHu      first version
 */

#include <board.h>
#include <rtthread.h>
#include <rtdevice.h>
#include <rthw.h>
#include <netif/ethernetif.h>
#include <pci.h>
#include <mmu.h>

#define DBG_LVL DBG_INFO
#define DBG_TAG "RTL8139"
#include <rtdbg.h>

#include "drv_rtl8139.h"

#define DEV_NAME "e0"

#define GET_RTL8139(eth)    (struct eth_device_rtl8139 *)(eth)

struct eth_device_rtl8139
{
    /* inherit from Ethernet device */
    struct eth_device parent;
    /* interface address info. */
    rt_uint8_t dev_addr[ETH_ALEN];         /* MAC address  */

    rt_pci_device_t *pci_dev;   /* pci device info */

    rt_uint32_t iobase; /* io port base */
    rt_uint32_t irqno;  /* irq number */

    card_chip_t chipset;

    rt_spinlock_t lock; /* lock for rx packet */

    rt_uint8_t *rx_buffer;
    rt_uint8_t *rx_ring;
    rt_uint8_t  current_rx; /* CAPR, Current Address of Packet Read */
    rt_uint32_t rx_flags;
    rt_ubase_t rx_ring_dma; /* dma phy addr */
    rt_uint32_t rx_config;  /* receive config */
    struct rtl8139_status   rx_status;

    rt_uint8_t *tx_buffers;
    rt_uint8_t *tx_buffer[NUM_TX_DESC]; /* tx buffer pointer array */
    rt_uint32_t current_tx;
    rt_uint32_t dirty_tx;
    rt_size_t   tx_free_counts;
    rt_uint32_t tx_flags;
    rt_ubase_t tx_buffer_dma;  /* dma phy addr */
    struct rtl8139_status   tx_status;

    struct net_device_status stats; /* device stats */
    struct rtl_extra_status xstats; /* extra status */

    rt_uint32_t dev_flags;  /* flags of net device */
    rt_mq_t rx_mqueue;  /* msg queue for rx */
    rt_uint8_t linked;  /* eth device linked */
};

static struct eth_device_rtl8139 eth_dev;
static rt_uint8_t rx_cache_send_buf[RX_MSG_SIZE] = {0};     /* buf for rx packet, put size and data into mq */
static rt_uint8_t rx_cache_recv_buf[RX_MSG_SIZE] = {0};     /* buf for rx packet, get size and data from mq */
static rt_uint8_t tx_cache_pbuf[TX_CACHE_BUF_SIZE] = {0};   /* buf for tx packet, get data from pbuf payload */

/* rx config */
static const rt_uint32_t rtl8139_rx_config = RX_CFG_RCV_32K | RX_NO_WRAP |
    (RX_FIFO_THRESH << RX_CFG_FIFO_SHIFT) |
    (RX_DMA_BURST << RX_CFG_DMA_SHIFT);

/* tx config */
static const rt_uint32_t rtl8139_tx_config = TX_IFG96 | (TX_DMA_BURST << TX_DMA_SHIFT) |
    (TX_RETRY << TX_RETRY_SHIFT);

/* intr mask, 1: receive, 0: ignore */
static const rt_uint16_t rtl8139_intr_mask = PCI_ERR | PCS_TIMEOUT | RX_UNDERRUN | RX_OVERFLOW | RX_FIFO_OVER |
    TX_ERR | TX_OK | RX_ERR | RX_OK;

static int rtl8139_next_desc(int current_desc)
{
    return (current_desc == NUM_TX_DESC - 1) ? 0 : (current_desc + 1);
}

static int rtl8139_transmit(struct eth_device_rtl8139 *dev, rt_uint8_t *buf, rt_size_t len)
{
    rt_uint32_t entry;
    rt_uint32_t length = len;

    entry = dev->current_tx;

    rt_base_t level = rt_hw_interrupt_disable();

    if (dev->tx_free_counts > 0)
    {
        if (length < TX_BUF_SIZE)
        {
            if (length < ETH_ZLEN)
            {
                rt_memset(dev->tx_buffer[entry], 0, ETH_ZLEN);  /* pad zero */
            }
            rt_memcpy(dev->tx_buffer[entry], buf, length);
        }
        else
        {
            /* drop packet */
            dev->stats.tx_dropped++;
            dbg_log(DBG_WARNING, "dropped a packed!\n");

            rt_hw_interrupt_enable(level);
            return 0;
        }
        /*
        * Writing to tx_status triggers a DMA transfer of the data
        * copied to dev->tx_buffer[entry] above. Use a memory barrier
        * to make sure that the device sees the updated data.
        */
        rt_hw_dsb();

        outl(dev->iobase + TX_STATUS0 + (entry * 4), dev->tx_flags | ETH_MAX(length, (rt_uint32_t)ETH_ZLEN));
        inl(dev->iobase + TX_STATUS0 + (entry * 4)); // flush

        dev->current_tx = rtl8139_next_desc(dev->current_tx);

        --dev->tx_free_counts;
    } else {
        LOG_E("Stop Tx packet!\n");
        rt_hw_interrupt_enable(level);
        return -1;
    }
    rt_hw_interrupt_enable(level);
    return 0;
}

/* Initialize the Rx and Tx rings, along with various 'dev' bits. */
static void rtl8139_init_ring(struct eth_device_rtl8139 *dev)
{
    dev->current_rx = 0;
    dev->current_tx = 0;
    dev->dirty_tx = 0;

    /* set free counts */
    dev->tx_free_counts = NUM_TX_DESC;

    int i = 0;
    for (; i < NUM_TX_DESC; i++)
    {
        dev->tx_buffer[i] = (unsigned char *)&dev->tx_buffers[i * TX_BUF_SIZE];
    }
}

static void rtl8139_chip_reset(struct eth_device_rtl8139 *dev)
{
    /* software reset, to clear the RX and TX buffers and set everything back to defaults. */
    outb(dev->iobase + CHIP_CMD, CMD_RESET);

    /* wait reset done */
    for (;;)
    {
        rt_hw_dmb();
        if ((inb(dev->iobase + CHIP_CMD) & CMD_RESET) == 0)
        {
            break;
        }
        rt_hw_cpu_pause();
    }
}

static void rtl8139_set_rx_mode(struct eth_device_rtl8139 *dev)
{
    rt_base_t level = rt_hw_interrupt_disable();

    int rx_mode = ACCEPT_BROADCAST | ACCEPT_MY_PHYS | ACCEPT_MULTICAST;

    rx_mode |= (ACCEPT_ERR | ACCEPT_RUNT);

    rt_uint32_t tmp;
    tmp = rtl8139_rx_config | rx_mode;
    if (dev->rx_config != tmp)
    {
        outl(dev->iobase + RX_CONFIG, tmp);
        /* flush */
        inl(dev->iobase + RX_CONFIG);
        dev->rx_config = tmp;
    }

    /* filter packet */
    rt_uint32_t mac_filter[2];

    mac_filter[0] = mac_filter[1] = 0;

    outl(dev->iobase + MAR0 + 0, mac_filter[0]);
    inl(dev->iobase + MAR0 + 0);

    outl(dev->iobase + MAR0 + 4, mac_filter[1]);
    inl(dev->iobase + MAR0 + 4);

    rt_hw_interrupt_enable(level);
}

static void rtl8139_hardware_start(struct eth_device_rtl8139 *dev)
{
    /* Bring old chips out of low-power mode. */
    if (rtl_chip_info[dev->chipset].flags & HAS_HLT_CLK)
    {
        outb(dev->iobase + HLT_CTL, 'R');
    }

    rtl8139_chip_reset(dev);

    /* unlock Config[01234] and BMCR register writes */
    outb(dev->iobase + CFG9346, CFG9346_UNLOCK);
    inb(dev->iobase + CFG9346);   // flush

    /* Restore our rtl8139a of the MAC address. */
    outl(dev->iobase + MAC0, *(rt_uint32_t *)(dev->dev_addr + 0));
    inl(dev->iobase + MAC0);

    outw(dev->iobase + MAC0 + 4, *(uint16_t *)(dev->dev_addr + 4));
    inw(dev->iobase + MAC0 + 4);

    dev->current_rx = 0;

    /* init Rx ring buffer DMA address */
    outl(dev->iobase + RX_BUF, dev->rx_ring_dma);
    inl(dev->iobase + RX_BUF);

    /* Must enable Tx/Rx before setting transfer thresholds! */
    outb(dev->iobase + CHIP_CMD, CMD_RX_ENABLE | CMD_TX_ENABLE);

    /* set receive config */
    dev->rx_config = rtl8139_rx_config | ACCEPT_BROADCAST | ACCEPT_MY_PHYS;

    outl(dev->iobase + RX_CONFIG, dev->rx_config);
    outl(dev->iobase + TX_CONFIG, rtl8139_tx_config);

    if (dev->chipset >= CH_8139B)
    {
        /* Disable magic packet scanning, which is enabled
         * when PM is enabled in Config1.  It can be reenabled
         * via ETHTOOL_SWOL if desired.
         * clear MAGIC bit
         */
        outb(dev->iobase + CONFIG3, inb(dev->iobase + CONFIG3) & ~CFG3_MAGIC);
    }

    /* Lock Config[01234] and BMCR register writes */
    outb(dev->iobase + CFG9346, CFG9346_LOCK);

    /* init Tx buffer DMA addresses */
    int i = 0;
    for (; i < NUM_TX_DESC; i++)
    {
        outl(dev->iobase + TX_ADDR0 + (i * 4), dev->tx_buffer_dma + (dev->tx_buffer[i] - dev->tx_buffers));
        /* flush */
        inl(dev->iobase + TX_ADDR0 + (i * 4));
    }

    outl(dev->iobase + RX_MISSED, 0);

    rtl8139_set_rx_mode(dev);

    /* no early-rx intr */
    outw(dev->iobase + MULTI_INTR, inw(dev->iobase + MULTI_INTR) & MULTI_INTR_CLEAR);

    /* make sure tx & rx enabled */
    uint8_t tmp = inb(dev->iobase + CHIP_CMD);
    if (!(tmp & CMD_RX_ENABLE) || !(tmp & CMD_TX_ENABLE))
    {
        outb(dev->iobase + CHIP_CMD, CMD_RX_ENABLE | CMD_TX_ENABLE);
    }

    /* enable 8139 intr mask */
    outw(dev->iobase + INTR_MASK, rtl8139_intr_mask);
}

static int rtl8139_tx_interrupt(struct eth_device_rtl8139 *dev)
{
    while (dev->tx_free_counts < NUM_TX_DESC)
    {
        int entry = dev->dirty_tx;

        /* read tx status */
        int tx_status = inl(dev->iobase + TX_STATUS0 + (entry * 4));

        /* no tx intr, exit */
        if (!(tx_status & (TX_STAT_OK | TX_UNDERRUN | TX_ABORTED)))
        {
            dbg_log(DBG_ERROR, "tx status not we want!\n");
            break;
        }

        /* NOTE: TxCarrierLost is always asserted at 100mbps. */
        if (tx_status & (TX_OUT_OF_WINDOW | TX_ABORTED))
        {
            dbg_log(DBG_ERROR, "Transmit error, Tx status %x\n", tx_status);
            dev->stats.tx_errors++;
            if (tx_status & TX_ABORTED)
            {
                dev->stats.tx_aborted_errors++;
                /* clear abort bit */
                outl(dev->iobase + TX_CONFIG, TX_CLEAR_ABT);

                /* set intr tx error */
                outw(dev->iobase + INTR_STATUS, TX_ERR);

                rt_hw_dsb();
            }
            if (tx_status & TX_CARRIER_LOST)
            {
                dev->stats.tx_carrier_errors++;
            }

            if (tx_status & TX_OUT_OF_WINDOW)
            {
                dev->stats.tx_window_errors++;
            }
        }
        else
        {
            if (tx_status & TX_UNDERRUN)
            {
                /* Add 64 to the Tx FIFO threshold. */
                if (dev->tx_flags < 0x00300000) {
                    dev->tx_flags += 0x00020000;
                }
                dev->stats.tx_fifo_errors++;
            }
            dev->stats.collisions += (tx_status >> 24) & 15;

            dev->tx_status.packets++;
            dev->tx_status.bytes += tx_status & 0x7ff;
        }

        dev->dirty_tx = rtl8139_next_desc(dev->dirty_tx);

        if (dev->tx_free_counts == 0)
        {
            rt_hw_dmb();
        }

        dev->tx_free_counts++;
    }
    return 0;
}

static void rtl8139_other_interrupt(struct eth_device_rtl8139 *dev, int status, int link_changed)
{
    /* Update the error count. */
    dev->stats.rx_missed_errors += inl(dev->iobase + RX_MISSED);
    outl(dev->iobase + RX_MISSED, 0);

    if ((status & RX_UNDERRUN) && link_changed && (dev->dev_flags & HAS_LNK_CHNG))
    {
        dev->linked = RT_FALSE; /* dev not linked */
        status &= ~RX_UNDERRUN;
    }

    if (status & (RX_UNDERRUN | RX_ERR))
    {
        dev->stats.rx_errors++;
    }

    if (status & PCS_TIMEOUT)
    {
        dev->stats.rx_length_errors++;
    }

    if (status & RX_UNDERRUN)
    {
        dev->stats.rx_fifo_errors++;
    }

    if (status & PCI_ERR)   /* error on pci */
    {
        rt_uint32_t pci_cmd_status;
        pci_cmd_status = rt_pci_device_read(dev->pci_dev, PCI_STATUS_COMMAND);
        rt_pci_device_write(dev->pci_dev, PCI_STATUS_COMMAND, pci_cmd_status);
        dbg_log(DBG_ERROR, "PCI Bus error %x\n", pci_cmd_status >> 16);
    }
}

static void rtl8139_rx_error(rt_uint32_t rx_status, struct eth_device_rtl8139 *dev)
{
    rt_uint8_t tmp;

    dev->stats.rx_errors++;

    /* rx error */
    if (!(rx_status & RX_STATUS_OK))
    {
        /* frame error */
        if (rx_status & (RX_BAD_SYMBOL | RX_BAD_ALIGN))
        {
            dev->stats.rx_frame_errors++;
        }

        /* long */
        if (rx_status & (RX_RUNT | RX_TOO_LONG))
        {
            dev->stats.rx_length_errors++;
        }

        /* CRC check */
        if (rx_status & RX_CRC_ERR)
        {
            dev->stats.rx_crc_errors++;
        }
    }
    else
    {
        /* receive ok, but lost */
        dev->xstats.rx_lost_in_ring++;
    }

    /* reset receive */
    tmp = inb(dev->iobase + CHIP_CMD);
    outb(dev->iobase + CHIP_CMD, tmp & ~CMD_RX_ENABLE);
    outb(dev->iobase + CHIP_CMD, tmp);
    outl(dev->iobase + RX_CONFIG, dev->rx_config);
    dev->current_rx = 0;
}

static void rtl8139_isr_ack(struct eth_device_rtl8139 *dev)
{
    rt_uint16_t status;

    status = inw(dev->iobase + INTR_STATUS) & RX_ACK_BITS;

    /* Clear out errors and receive interrupts */
    if (status != 0)
    {
        if (status & (RX_FIFO_OVER | RX_OVERFLOW))
        {
            dev->stats.rx_errors++;
            if (status & RX_FIFO_OVER)
            {
                dev->stats.rx_fifo_errors++;
            }
        }
        /* write rx ack */
        outw(dev->iobase + INTR_STATUS, RX_ACK_BITS);
        inw(dev->iobase + INTR_STATUS); // for flush
    }
}

static int rtl8139_rx_interrupt(struct eth_device_rtl8139 *dev)
{
    int received = 0;
    rt_uint8_t *rx_ring = dev->rx_ring;
    rt_uint32_t current_rx = dev->current_rx;
    rt_uint32_t rx_size = 0;

    while (!(inb(dev->iobase + CHIP_CMD) & RX_BUFFER_EMPTY))
    {
        rt_uint32_t ring_offset = current_rx % RX_BUF_LEN;
        rt_uint32_t rx_status;

        rt_size_t pkt_size;

        rt_hw_dmb();

        /* read size+status of next frame from DMA ring buffer */
        rx_status = *(rt_uint32_t *)(rx_ring + ring_offset);

        /* size on high 16 bit */
        rx_size = rx_status >> 16;

        if (!(dev->dev_flags & DEV_FLAGS_RXFCS)) {
            pkt_size = rx_size - 4;
        } else {
            pkt_size = rx_size;
        }

        /* Packet copy from FIFO still in progress.
         * Theoretically, this should never happen
         * since early_rx is disabled.
         */
        if (rx_size == 0xfff0)
        {
            dbg_log(DBG_WARNING, "rx fifo copy in progress\n");
            dev->xstats.early_rx++;
            break;
        }

        /* If Rx err or invalid rx_size/rx_status received
         * (which happens if we get lost in the ring),
         * Rx process gets reset, so we abort any further
         * Rx processing.
         */
        if ((rx_size > (MAX_ETH_FRAME_SIZE + 4) || (rx_size < 8) || (!(rx_status & RX_STATUS_OK))))
        {
            if ((dev->dev_flags & DEV_FLAGS_RXALL) && (rx_size <= (MAX_ETH_FRAME_SIZE + 4)) &&
                (rx_size >= 8) && (!(rx_status & RX_STATUS_OK)))
            {
                dev->stats.rx_errors++;
                if (rx_status & RX_CRC_ERR)
                {
                    dev->stats.rx_crc_errors++;
                    JUMP_TO(keep_pkt);
                }

                if (rx_status & RX_RUNT)
                {
                    dev->stats.rx_length_errors++;
                    JUMP_TO(keep_pkt);
                }
            }
            /* rx error handle */
            rtl8139_rx_error(rx_status, dev);
            received = -1;
            JUMP_TO(out);
        }

keep_pkt:
        /* merge size and data into receive pkg */
        rt_memcpy(rx_cache_send_buf, &pkt_size, 4);
        rt_memcpy(&rx_cache_send_buf[4], &rx_ring[ring_offset + 4], pkt_size);

        rt_mq_send_interrupt(dev->rx_mqueue, rx_cache_send_buf, pkt_size + 4);
        eth_device_ready(&dev->parent); /* notify eth thread to read packet */

        dev->rx_status.packets++;
        dev->rx_status.bytes += pkt_size;

        received++;

        /* 4:for header length(length include 4 bytes CRC)
         * 3:for dword alignment
         */
        current_rx = (current_rx + rx_size + 4 + 3) & ~3;
        outw(dev->iobase + RX_BUF_PTR, (rt_uint16_t)(current_rx - 16));

        rtl8139_isr_ack(dev);
    }

    if (!received || rx_size == 0xfff0)
    {
        rtl8139_isr_ack(dev);
    }

    dev->current_rx = current_rx;
out:
    return received;
}

static void rt_hw_rtl8139_isr(int vector, void *param)
{
    struct eth_device_rtl8139 *dev = GET_RTL8139(param);

    rt_uint16_t status, ackstat;

    int link_changed = 0; /* avoid bogus "uninit" warning */

    rt_spin_lock(&dev->lock);

    status = inw(dev->iobase + INTR_STATUS);
    outw(dev->iobase + INTR_STATUS, status);

    if ((status & rtl8139_intr_mask) == 0)
    {
        dbg_log(DBG_LOG, "no interrupt occured on me!\n");
        rt_spin_unlock(&dev->lock);
        return;
    }

    /* check netif state whether running. */
    if (!dev->linked)
    {
        /* clear intr mask, don't receive intr forever */
        outw(dev->iobase + INTR_MASK, 0);
        JUMP_TO(out);
    }

    /* Acknowledge all of the current interrupt sources ASAP, but
       an first get an additional status bit from CSCR. */
    if (status & RX_UNDERRUN)
    {
        link_changed = inw(dev->iobase + CSCR) & CSCR_LINK_CHANGE;
    }

    ackstat = status & ~(RX_ACK_BITS | TX_ERR);
    if (ackstat)
    {
        outw(dev->iobase + INTR_STATUS, ackstat);
    }

    if (status & RX_ACK_BITS)
    {
        rtl8139_rx_interrupt(dev);
    }

    /* Check uncommon events with one test. */
    if (status & (PCI_ERR | PCS_TIMEOUT | RX_UNDERRUN | RX_ERR))
    {
        rtl8139_other_interrupt(dev, status, link_changed);
    }

    /* handle receive */
    if (status & (TX_OK | TX_ERR))
    {
        rtl8139_tx_interrupt(dev);

        if (status & TX_ERR)
        {
            outw(dev->iobase + INTR_STATUS, TX_ERR);
        }
    }
out:
    rt_spin_unlock(&dev->lock);
}

static rt_err_t rtl8139_init(rt_device_t device)
{
    struct eth_device_rtl8139 *dev = GET_RTL8139(device);

    /* alloc transmit buffer */
    dev->tx_buffers = (rt_uint8_t *) rt_malloc(TX_BUF_TOTAL_LEN);
    if (dev->tx_buffers == RT_NULL)
    {
        LOG_E("alloc memory for rtl8139 tx buffer failed!\n");
        return -1;
    }

    /* alloc receive buffer */
    dev->rx_ring = (rt_uint8_t *) rt_malloc(RX_BUF_TOTAL_LEN);
    if (dev->rx_ring == RT_NULL) {
        LOG_E("alloc memory for rtl8139 rx buffer failed!\n");
        rt_free(dev->tx_buffers);
        return -1;
    }

    /* create msg queue for eth rx */
    dev->rx_mqueue = rt_mq_create("rx_mqueue", RX_MSG_SIZE, RX_MSG_CNT, 0);
    if (dev->rx_mqueue == RT_NULL)
    {
        LOG_E("crete msg queue for rx buffer failed!\n");
        rt_free(dev->tx_buffers);
        rt_free(dev->rx_ring);
        return -1;
    }

    dev->tx_buffer_dma = (rt_ubase_t)rt_hw_vir2phy(dev->tx_buffers);
    dev->rx_ring_dma = (rt_ubase_t)rt_hw_vir2phy(dev->rx_ring);

    dev->tx_flags = (TX_FIFO_THRESH << 11) & 0x003f0000;

    /* init tx and rx ring */
    rtl8139_init_ring(dev);
    rtl8139_hardware_start(dev);

    dev->dev_flags = DEV_FLAGS_RXALL;
    dev->linked = RT_TRUE;

    eth_device_linkchange(&dev->parent, RT_TRUE);

    if (rt_hw_interrupt_install(dev->irqno, rt_hw_rtl8139_isr, (void *) dev, "rtl8139") < 0)
    {
        LOG_E("install IRQ failed!\n");
        rt_free(dev->tx_buffers);
        rt_free(dev->rx_ring);
        rt_mq_delete(dev->rx_mqueue);
        return RT_ERROR;
    }
    rt_hw_interrupt_umask(dev->irqno);

    dbg_log(DBG_INFO, "ethernet card init done.\n");

    return RT_EOK;
}

#ifdef RT_USING_DEVICE_OPS
const static struct rt_device_ops rtl8139_ops =
{
    rtl8139_init,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    RT_NULL,
    rtl8139_control
};
#endif

static int rtl8139_get_pci(struct eth_device_rtl8139 *dev)
{
    /* get pci device */
    rt_pci_device_t *pci_dev = rt_pci_device_get(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID);
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

static int rtl8139_init_board(struct eth_device_rtl8139 *dev)
{
    /* check for missing/broken hardware */
    if (inl(dev->iobase + TX_CONFIG) == 0xFFFFFFFF)
    {
        dbg_log(DBG_ERROR, "chip not responding, ignoring board.\n");
        return -1;
    }

    rt_uint32_t version = inl(dev->iobase + TX_CONFIG) & HW_REVID_MASK;
    int i = 0;
    for (; i < CHIP_INFO_NR; i++)
    {
        if (version == rtl_chip_info[i].version) {
            dev->chipset = i;
            JUMP_TO(chip_match);
        }
    }

    /* if unknown chip, assume array element #0, original RTL-8139 in this case */
    i = 0;

    dbg_log(DBG_LOG, "unknown chip version, assuming RTL-8139\n");
    dbg_log(DBG_LOG, "TxConfig = 0x%x\n", inl(dev->iobase + TX_CONFIG));
    dev->chipset = 0;

chip_match:
    dbg_log(DBG_LOG, "chipset id (%x) == index %d, '%s'\n",
            version, i, rtl_chip_info[i].name);
    /* start netcard */
    if (dev->chipset >= CH_8139B)
    {
        dbg_log(DBG_WARNING, "PCI PM wakeup, not support now!\n");
    }
    else
    {
        rt_uint8_t tmp = inb(dev->iobase + CONFIG1);
        tmp &= ~(CFG1_SLEEP | CFG1_PWRDN);
        outb(dev->iobase + CONFIG1, tmp);
    }

    /* reset chip */
    rtl8139_chip_reset(dev);
    return 0;
}

static int rtl8139_init_hw(struct eth_device_rtl8139 *dev)
{
    rt_pci_device_t *pci_dev = dev->pci_dev;

    /* check version */
    if (pci_dev->vendor_id  == RTL8139_VENDOR_ID && pci_dev->device_id == RTL8139_DEVICE_ID &&
        pci_dev->revision_id >= 0x20)
    {
        dbg_log(DBG_LOG, "This (id %04x:%04x rev %02x) is an enhanced 8139C+ chip, use 8139cp\n",
                pci_dev->vendor_id, pci_dev->device_id, pci_dev->revision_id);
    }

    if (rtl8139_init_board(dev) < 0)
    {
        return -1;
    }

    /* get MAC from pci config */
    int i = 0;
    for (; i < ETH_ALEN; i++) {
        dev->dev_addr[i] = inb(dev->iobase + MAC0 + i);
    }
    dbg_log(DBG_INFO, "MAC addr: %x:%x:%x:%x:%x:%x\n", dev->dev_addr[0], dev->dev_addr[1],
            dev->dev_addr[2], dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);

    rt_spin_lock_init(&dev->lock);

    /* Put the chip into low-power mode. */
    if (rtl_chip_info[dev->chipset].flags & HAS_HLT_CLK)
    {
        outb(dev->iobase + HLT_CTL, 'H');   /* 'R' would leave the clock running. */
    }
    return 0;
}

static rt_err_t rtl8139_tx(rt_device_t device, struct pbuf *p)
{
    rt_err_t err = RT_EOK;
    /* copy data from pbuf to tx cache */
    pbuf_copy_partial(p, (void *)&tx_cache_pbuf[0], p->tot_len, 0);
    if (rtl8139_transmit(GET_RTL8139(device), tx_cache_pbuf, p->tot_len) < 0)
    {
        err = RT_ERROR;
    }
    return err;
}

static struct pbuf *rtl8139_rx(rt_device_t device)
{
    struct eth_device_rtl8139 *dev = GET_RTL8139(device);
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

static rt_err_t rtl8139_control(rt_device_t device, int cmd, void *args)
{
    struct eth_device_rtl8139 *dev = GET_RTL8139(device);
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

static int rt_hw_rtl8139_init(void)
{
    rt_memset(&eth_dev, 0x0, sizeof(eth_dev));

    if (rtl8139_get_pci(&eth_dev) < 0)
    {
        return -1;
    }

    if (rtl8139_init_hw(&eth_dev) < 0)
    {
        return -1;
    }

    /* set device opts */
#ifdef RT_USING_DEVICE_OPS
    eth_dev.parent.parent.ops        = &rtl8139_ops;
#else
    eth_dev.parent.parent.init       = rtl8139_init;
    eth_dev.parent.parent.open       = RT_NULL;
    eth_dev.parent.parent.close      = RT_NULL;
    eth_dev.parent.parent.read       = RT_NULL;
    eth_dev.parent.parent.write      = RT_NULL;
    eth_dev.parent.parent.control    = rtl8139_control;
#endif
    eth_dev.parent.parent.user_data  = RT_NULL;
    eth_dev.parent.eth_rx     = rtl8139_rx;
    eth_dev.parent.eth_tx     = rtl8139_tx;

    /* register ETH device */
    if (eth_device_init(&(eth_dev.parent), DEV_NAME) != RT_EOK)
    {
        return -1;
    }
    return 0;
}
INIT_DEVICE_EXPORT(rt_hw_rtl8139_init);
