/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-08-16     JasonHu      first version
 */

#ifndef __DRV_RTL8139_H__
#define __DRV_RTL8139_H__

#include <rtdef.h>

#define ETH_ALEN 6  /* MAC addr */
#define ETH_ZLEN 60 /* Minimum length of data without CRC check */
#define ETH_DATA_LEN 1500 /* Maximum length of data in a frame */
#define ETH_FRAME_LEN 1514 /* Maximum Ethernet data length without CRC checksum */

#define ETH_MAX(a, b) ((a) > (b) ? (a) : (b))

#define RX_MSG_CNT 8  /* 4 msg queue */
#define RX_MSG_SIZE (ETH_FRAME_LEN + 4)  /* 4 save real msg size */

#define TX_CACHE_BUF_SIZE          (2048)

#define DEV_FLAGS_RXALL     (1 << 0) /* receive all pkgs */
#define DEV_FLAGS_RXFCS     (1 << 1) /* receive no crc check */

/* pci device info */
#define RTL8139_VENDOR_ID   0x10ec
#define RTL8139_DEVICE_ID   0x8139

#define RX_BUF_IDX  2   /* 32K ring */

#define RX_BUF_LEN  (8192 << RX_BUF_IDX)
#define RX_BUF_PAD  16      /* pad 16 bytes */
#define RX_BUF_WRAP_PAD 2048 /* spare padding to handle lack of packet wrap */

/* The total length of the receive buffer */
#define RX_BUF_TOTAL_LEN    (RX_BUF_LEN + RX_BUF_PAD + RX_BUF_WRAP_PAD)

/* Number of Tx descriptor registers. */
#define NUM_TX_DESC 4

/* max supported ethernet frame size -- must be at least (dev->mtu+14+4).*/
#define MAX_ETH_FRAME_SIZE  1536

/* Size of the Tx bounce buffers -- must be at least (dev->mtu+14+4). */
#define TX_BUF_SIZE MAX_ETH_FRAME_SIZE
#define TX_BUF_TOTAL_LEN    (TX_BUF_SIZE * NUM_TX_DESC)

/* PCI Tuning Parameters
   Threshold is bytes transferred to chip before transmission starts. */
#define TX_FIFO_THRESH  256 /* In bytes, rounded down to 32 byte units. */

/* The following settings are log_2(bytes)-4:  0 == 16 bytes .. 6==1024, 7==end of packet. */
#define RX_FIFO_THRESH  7   /* Rx buffer level before first PCI xfer.  */
#define RX_DMA_BURST    7   /* Maximum PCI burst, '7' is unlimited */
#define TX_DMA_BURST    6   /* Maximum PCI burst, '6' is 1024 */
#define TX_RETRY    8   /* 0-15.  retries = 16 + (TX_RETRY * 16) */

#define JUMP_TO(label)  goto label

enum
{
    HAS_MII_XCVR = 0x010000,
    HAS_CHIP_XCVR = 0x020000,
    HAS_LNK_CHNG = 0x040000,
};

#define RTL_NUM_STATS 4     /* number of ETHTOOL_GSTATS u64's */
#define RTL_REGS_VER 1      /* version of reg. data in ETHTOOL_GREGS */
#define RTL_MIN_IO_SIZE 0x80
#define RTL8139B_IO_SIZE 256
#define RTL8129_CAPS    HAS_MII_XCVR
#define RTL8139_CAPS    (HAS_CHIP_XCVR | HAS_LNK_CHNG)

/* Symbolic offsets to registers. */
enum rtl8139_registers
{
    MAC0        = 0,     /* Ethernet hardware address. */
    MAR0        = 8,     /* Multicast filter. */
    TX_STATUS0  = 0x10,  /* Transmit status (Four 32bit registers). */
    TX_ADDR0    = 0x20,  /* Tx descriptors (also four 32bit). */
    RX_BUF      = 0x30,
    CHIP_CMD    = 0x37,
    RX_BUF_PTR  = 0x38,
    RX_BUF_ADDR = 0x3A,
    INTR_MASK   = 0x3C,
    INTR_STATUS = 0x3E,
    TX_CONFIG   = 0x40,
    RX_CONFIG   = 0x44,
    TIMER       = 0x48,  /* A general-purpose counter. */
    RX_MISSED   = 0x4C,  /* 24 bits valid, write clears. */
    CFG9346     = 0x50,
    CONFIG0     = 0x51,
    CONFIG1     = 0x52,
    TIMER_INT   = 0x54,
    MEDIA_STATUS = 0x58,
    CONFIG3     = 0x59,
    CONFIG4     = 0x5A,  /* absent on RTL-8139A */
    HLT_CTL     = 0x5B,
    MULTI_INTR  = 0x5C,
    TX_SUMMARY  = 0x60,
    BASIC_MODE_CTRL     = 0x62,
    BASIC_MODE_STATUS   = 0x64,
    NWAY_ADVERT = 0x66,
    NWAY_LPAR   = 0x68,
    NWAY_EXPANSION = 0x6A,
    /* Undocumented registers, but required for proper operation. */
    FIFOTMS     = 0x70,  /* FIFO Control and test. */
    CSCR        = 0x74,  /* Chip Status and Configuration Register. */
    PARA78      = 0x78,
    FLASH_REG   = 0xD4, /* Communication with Flash ROM, four bytes. */
    PARA7C      = 0x7c,  /* Magic transceiver parameter register. */
    CONFIG5     = 0xD8,  /* absent on RTL-8139A */
};

enum clear_bit_masks
{
    MULTI_INTR_CLEAR    = 0xF000,
    CHIP_CMD_CLEAR  = 0xE2,
    CONFIG1_CLEAR   = (1 << 7) | (1 << 6) | (1 << 3) | (1 << 2) | (1 << 1),
};

enum chip_cmd_bits
{
    CMD_RESET       = 0x10,
    CMD_RX_ENABLE   = 0x08,
    CMD_TX_ENABLE   = 0x04,
    RX_BUFFER_EMPTY = 0x01,
};

/* Interrupt register bits, using my own meaningful names. */
enum intr_status_bits
{
    PCI_ERR     = 0x8000,
    PCS_TIMEOUT = 0x4000,
    RX_FIFO_OVER = 0x40,
    RX_UNDERRUN = 0x20,
    RX_OVERFLOW = 0x10,
    TX_ERR      = 0x08,
    TX_OK       = 0x04,
    RX_ERR      = 0x02,
    RX_OK       = 0x01,
    RX_ACK_BITS = RX_FIFO_OVER | RX_OVERFLOW | RX_OK,
};

enum tx_status_bits
{
    TX_HOST_OWNS    = 0x2000,
    TX_UNDERRUN     = 0x4000,
    TX_STAT_OK      = 0x8000,
    TX_OUT_OF_WINDOW = 0x20000000,
    TX_ABORTED      = 0x40000000,
    TX_CARRIER_LOST = 0x80000000,
};

enum rx_status_bits
{
    RX_MULTICAST    = 0x8000,
    RX_PHYSICAL     = 0x4000,
    RX_BROADCAST    = 0x2000,
    RX_BAD_SYMBOL   = 0x0020,
    RX_RUNT         = 0x0010,
    RX_TOO_LONG     = 0x0008,
    RX_CRC_ERR      = 0x0004,
    RX_BAD_ALIGN    = 0x0002,
    RX_STATUS_OK    = 0x0001,
};

/* Bits in rx_config. */
enum rx_mode_bits
{
    ACCEPT_ERR  = 0x20,
    ACCEPT_RUNT = 0x10,
    ACCEPT_BROADCAST    = 0x08,
    ACCEPT_MULTICAST    = 0x04,
    ACCEPT_MY_PHYS  = 0x02,
    ACCEPT_ALL_PHYS = 0x01,
};

/* Bits in TxConfig. */
enum tx_config_bits
{
    /* Interframe Gap Time. Only TxIFG96 doesn't violate IEEE 802.3 */
    TX_IFG_SHIFT    = 24,
    TX_IFG84        = (0 << TX_IFG_SHIFT), /* 8.4us / 840ns (10 / 100Mbps) */
    TX_IFG88        = (1 << TX_IFG_SHIFT), /* 8.8us / 880ns (10 / 100Mbps) */
    TX_IFG92        = (2 << TX_IFG_SHIFT), /* 9.2us / 920ns (10 / 100Mbps) */
    TX_IFG96        = (3 << TX_IFG_SHIFT), /* 9.6us / 960ns (10 / 100Mbps) */

    TX_LOOP_BACK    = (1 << 18) | (1 << 17), /* enable loopback test mode */
    TX_CRC          = (1 << 16),    /* DISABLE Tx pkt CRC append */
    TX_CLEAR_ABT    = (1 << 0), /* Clear abort (WO) */
    TX_DMA_SHIFT    = 8, /* DMA burst value (0-7) is shifted X many bits */
    TX_RETRY_SHIFT  = 4, /* TXRR value (0-15) is shifted X many bits */

    TX_VERSION_MASK = 0x7C800000, /* mask out version bits 30-26, 23 */
};

/* Bits in Config1 */
enum config1_bits
{
    CFG1_PM_ENABLE  = 0x01,
    CFG1_VPD_ENABLE = 0x02,
    CFG1_PIO    = 0x04,
    CFG1_MMIO   = 0x08,
    CFG1_LWAKE      = 0x10,     /* not on 8139, 8139A */
    CFG1_DRIVER_LOAD = 0x20,
    CFG1_LED0   = 0x40,
    CFG1_LED1   = 0x80,
    CFG1_SLEEP      = (1 << 1), /* only on 8139, 8139A */
    CFG1_PWRDN      = (1 << 0), /* only on 8139, 8139A */
};

/* Bits in Config3 */
enum config3_bits
{
    CFG3_FAST_ENABLE        = (1 << 0), /* 1    = Fast Back to Back */
    CFG3_FUNCTION_ENABLE    = (1 << 1), /* 1    = enable CardBus Function registers */
    CFG3_CLKRUN_ENABLE      = (1 << 2), /* 1    = enable CLKRUN */
    CFG3_CARD_BUS_ENABLE    = (1 << 3), /* 1    = enable CardBus registers */
    CFG3_LINK_UP            = (1 << 4), /* 1    = wake up on link up */
    CFG3_MAGIC              = (1 << 5), /* 1    = wake up on Magic Packet (tm) */
    CFG3_PARM_ENABLE        = (1 << 6), /* 0    = software can set twister parameters */
    CFG3_GNT                = (1 << 7), /* 1    = delay 1 clock from PCI GNT signal */
};

/* Bits in Config4 */
enum config4_bits
{
    CFG4_LWPTN  = (1 << 2), /* not on 8139, 8139A */
};

/* Bits in Config5 */
enum config5_bits
{
    CFG5_PME_STS    = (1 << 0), /* 1    = PCI reset resets PME_Status */
    CFG5_LAN_WAKE   = (1 << 1), /* 1    = enable LANWake signal */
    CFG5_LDPS       = (1 << 2), /* 0    = save power when link is down */
    CFG5_FIFO_ADDR_PTR = (1 << 3), /* Realtek internal SRAM testing */
    CFG5_UWF        = (1 << 4), /* 1 = accept unicast wakeup frame */
    CFG5_MWF        = (1 << 5), /* 1 = accept multicast wakeup frame */
    CFG5_BWF        = (1 << 6), /* 1 = accept broadcast wakeup frame */
};

enum rx_config_bits
{
    /* rx fifo threshold */
    RX_CFG_FIFO_SHIFT   = 13,
    RX_CFG_FIFO_NONE    = (7 << RX_CFG_FIFO_SHIFT),

    /* Max DMA burst */
    RX_CFG_DMA_SHIFT    = 8,
    RX_CFG_DMA_UNLIMITED = (7 << RX_CFG_DMA_SHIFT),

    /* rx ring buffer length */
    RX_CFG_RCV_8K   = 0,
    RX_CFG_RCV_16K  = (1 << 11),
    RX_CFG_RCV_32K  = (1 << 12),
    RX_CFG_RCV_64K  = (1 << 11) | (1 << 12),

    /* Disable packet wrap at end of Rx buffer. (not possible with 64k) */
    RX_NO_WRAP  = (1 << 7),
};

/* Twister tuning parameters from RealTek.
   Completely undocumented, but required to tune bad links on some boards. */
enum cscr_bits
{
    CSCR_LINK_OK        = 0x0400,
    CSCR_LINK_CHANGE    = 0x0800,
    CSCR_LINK_STATUS    = 0x0f000,
    CSCR_LINK_DOWN_OFF_CMD  = 0x003c0,
    CSCR_LINK_DOWN_CMD  = 0x0f3c0,
};

enum config9346_bits
{
    CFG9346_LOCK    = 0x00,
    CFG9346_UNLOCK  = 0xC0,
};

typedef enum {
    CH_8139 = 0,
    CH_8139_K,
    CH_8139A,
    CH_8139A_G,
    CH_8139B,
    CH_8130,
    CH_8139C,
    CH_8100,
    CH_8100B_8139D,
    CH_8101,
} card_chip_t;

enum chip_flags {
    HAS_HLT_CLK = (1 << 0),
    HAS_LWAKE   = (1 << 1),
};

#define HW_REVID(b30, b29, b28, b27, b26, b23, b22) \
        ((b30) << 30 | (b29) << 29 | (b28) << 28 | (b27) << 27 | (b26) << 26 | (b23) << 23 | (b22) << 22)
#define HW_REVID_MASK   HW_REVID(1, 1, 1, 1, 1, 1, 1)

#define CHIP_INFO_NR    10

/* directly indexed by chip_t, above */
static const struct
{
    const char *name;
    rt_uint32_t version; /* from RTL8139C/RTL8139D docs */
    rt_uint32_t flags;
} rtl_chip_info[CHIP_INFO_NR] = {
    { "RTL-8139",
      HW_REVID(1, 0, 0, 0, 0, 0, 0),
      HAS_HLT_CLK,
    },
    { "RTL-8139 rev K",
      HW_REVID(1, 1, 0, 0, 0, 0, 0),
      HAS_HLT_CLK,
    },
    { "RTL-8139A",
      HW_REVID(1, 1, 1, 0, 0, 0, 0),
      HAS_HLT_CLK, /* XXX undocumented? */
    },
    { "RTL-8139A rev G",
      HW_REVID(1, 1, 1, 0, 0, 1, 0),
      HAS_HLT_CLK, /* XXX undocumented? */
    },
    { "RTL-8139B",
      HW_REVID(1, 1, 1, 1, 0, 0, 0),
      HAS_LWAKE,
    },
    { "RTL-8130",
      HW_REVID(1, 1, 1, 1, 1, 0, 0),
      HAS_LWAKE,
    },
    { "RTL-8139C",
      HW_REVID(1, 1, 1, 0, 1, 0, 0),
      HAS_LWAKE,
    },
    { "RTL-8100",
      HW_REVID(1, 1, 1, 1, 0, 1, 0),
      HAS_LWAKE,
    },
    { "RTL-8100B/8139D",
      HW_REVID(1, 1, 1, 0, 1, 0, 1),
      HAS_HLT_CLK /* XXX undocumented? */ | HAS_LWAKE,
    },
    { "RTL-8101",
      HW_REVID(1, 1, 1, 0, 1, 1, 1),
      HAS_LWAKE,
    }
};

struct rtl8139_status
{
    rt_ubase_t packets;
    rt_ubase_t bytes;
};

struct net_device_status
{
    rt_ubase_t tx_errors;
    rt_ubase_t tx_aborted_errors;
    rt_ubase_t tx_carrier_errors;
    rt_ubase_t tx_window_errors;
    rt_ubase_t tx_fifo_errors;
    rt_ubase_t tx_dropped;

    rt_ubase_t rx_errors;
    rt_ubase_t rx_length_errors;
    rt_ubase_t rx_missed_errors;
    rt_ubase_t rx_fifo_errors;
    rt_ubase_t rx_crc_errors;
    rt_ubase_t rx_frame_errors;
    rt_ubase_t rx_dropped;

    rt_ubase_t tx_packets;
    rt_ubase_t tx_bytes;

    rt_ubase_t collisions;
};

struct rtl_extra_status
{
    rt_ubase_t early_rx;
    rt_ubase_t tx_buf_mapped;
    rt_ubase_t tx_timeouts;
    rt_ubase_t rx_lost_in_ring;
};

#endif  /* __DRV_RTL8139_H__ */
