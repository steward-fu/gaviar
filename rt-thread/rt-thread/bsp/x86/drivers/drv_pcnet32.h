/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2021-08-19     JasonHu      first version
 */

#ifndef __DRV_PCNET32_H__
#define __DRV_PCNET32_H__

#include <rtdef.h>

#define ETH_ALEN 6  /* MAC addr */
#define ETH_ZLEN 60 /* Minimum length of data without CRC check */
#define ETH_DATA_LEN 1500 /* Maximum length of data in a frame */
#define ETH_FRAME_LEN 1518 /* Maximum Ethernet data length */

#define RX_MSG_CNT 8  /* 4 msg queue */
#define RX_MSG_SIZE (ETH_FRAME_LEN + 4)  /* 4 save real msg size */

#define TX_CACHE_BUF_SIZE          (2048)

#define JUMP_TO(label)  goto label

#define PCNET32_VENDOR_ID   0x1022
#define PCNET32_DEVICE_ID   0x2000

/* Offsets from base I/O address. */
#define PCNET32_WIO_RDP     0x10
#define PCNET32_WIO_RAP     0x12
#define PCNET32_WIO_RESET   0x14
#define PCNET32_WIO_BDP     0x16

#define CSR0        0
#define CSR0_INIT   0x1
#define CSR0_START  0x2
#define CSR0_STOP   0x4
#define CSR0_TXPOLL 0x8
#define CSR0_INTEN  0x40
#define CSR0_IDON   0x0100
#define CSR0_NORMAL (CSR0_START | CSR0_INTEN)
#define CSR0_TINT   0x0200  /* Transmit Interrupt */
#define CSR0_RINT   0x0400  /* Receive Interrupt */
#define CSR0_MERR   0x0800  /* Memory Error */
#define CSR0_MISS   0x1000  /* Missed Frame */
#define CSR0_CERR   0x2000  /* Collision Error */
#define CSR0_BABL   0x4000  /* Babble is a transmitter time-out error. */

/* Error is set by the ORing of BABL, CERR, MISS, and MERR.
 * ERR remains set as long as any of the error flags are true.
 */
#define CSR0_ERR   0x8000

#define CSR1        1
#define CSR2        2
#define CSR3        3   /*  Interrupt Masks and Deferral Control */
#define CSR3_IDONM  (1 << 8)   /* Initialization Done Mask. */
#define CSR4        4   /* Test and Features Control */
#define CSR4_ASTRP_RCV  (1 << 10)   /* Auto Strip Receive */
#define CSR4_APAD_XMT   (1 << 11)   /* Auto Pad Transmit */

#define CSR5        5
#define CSR5_SUSPEND    0x0001
#define CSR6        6   /* RX/TX Descriptor Table Length */

#define CSR15       15  /* Mode */
#define CSR18       18  /* Current Receive Buffer Address Lower */
#define CSR19       19  /* Current Receive Buffer Address Upper */
#define CSR24       24  /* Base Address of Receive Descriptor Ring Lower */
#define CSR25       25  /* Base Address of Receive Descriptor Ring Upper */
#define CSR30       30  /* Base Address of Transmit Descriptor Ring Lower */
#define CSR31       31  /* Base Address of Transmit Descriptor Ring Upper */

#define CSR58       58  /* Software Style */
#define CSR58_PCNET_PCI_II   0x02

#define PCNET32_INIT_LOW    1
#define PCNET32_INIT_HIGH   2
#define PCNET32_MC_FILTER 8   /* broadcast filter */

#define CSR72       72  /* Receive Descriptor Ring Counter */
#define CSR74       74  /* Transmit Descriptor Ring Counter */

#define BCR2        2
#define BCR2_ASEL   (1 << 1)

#define PCNET32_TX_BUFFERS 8
#define PCNET32_RX_BUFFERS 32
#define PCNET32_LOG_TX_BUFFERS 3    /* 2^3 = 8 buffers */
#define PCNET32_LOG_RX_BUFFERS 5    /* 2^5 = 32 buffers */

#define PCNET32_RING_DE_SIZE     16

#define PCNET32_TX_RETRY     10 /* tx retry counter when no available descriptor entry */

#define PCNET32_DESC_STATUS_OWN 0x8000  /* card own the desc */

/**
 * End of Packet indicates that this is the last buffer used by
 * the PCnet-PCI II controller for this frame.
 */
#define PCNET32_DESC_STATUS_ENP 0x0100

/**
 * Start of Packet indicates that this
 * is the first buffer used by the
 * PCnet-PCI II controller for this
 * frame.
 */
#define PCNET32_DESC_STATUS_STP 0x0200

#endif  /* __DRV_PCNET32_H__ */
