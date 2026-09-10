/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author        Notes
 * 2021-08-04     JasonHu       first version
 */

#ifndef __DRV_AHCI_H__
#define __DRV_AHCI_H__

#include <rtdef.h>

enum AHCI_FIS_TYPE
{
    FIS_TYPE_REG_H2D    = 0x27,    // Register FIS - host to device
    FIS_TYPE_REG_D2H    = 0x34,    // Register FIS - device to host
    FIS_TYPE_DMA_ACT    = 0x39,    // DMA activate FIS - device to host
    FIS_TYPE_DMA_SETUP  = 0x41,    // DMA setup FIS - bidirectional
    FIS_TYPE_DATA       = 0x46,    // Data FIS - bidirectional
    FIS_TYPE_BIST       = 0x58,    // BIST activate FIS - bidirectional
    FIS_TYPE_PIO_SETUP  = 0x5F,    // PIO setup FIS - device to host
    FIS_TYPE_DEV_BITS   = 0xA1,    // Set device bits FIS - device to host
};

struct fis_reg_host_to_device
{
    rt_uint8_t fis_type;

    rt_uint8_t pmport:4;
    rt_uint8_t reserved0:3;
    rt_uint8_t c:1;

    rt_uint8_t command;
    rt_uint8_t feature_l;

    rt_uint8_t lba0;
    rt_uint8_t lba1;
    rt_uint8_t lba2;
    rt_uint8_t device;

    rt_uint8_t lba3;
    rt_uint8_t lba4;
    rt_uint8_t lba5;
    rt_uint8_t feature_h;

    rt_uint8_t count_l;
    rt_uint8_t count_h;
    rt_uint8_t icc;
    rt_uint8_t control;

    rt_uint8_t reserved1[4];
}__attribute__ ((packed));

struct fis_reg_device_to_host
{
    rt_uint8_t fis_type;

    rt_uint8_t pmport:4;
    rt_uint8_t reserved0:2;
    rt_uint8_t interrupt:1;
    rt_uint8_t reserved1:1;

    rt_uint8_t status;
    rt_uint8_t error;

    rt_uint8_t lba0;
    rt_uint8_t lba1;
    rt_uint8_t lba2;
    rt_uint8_t device;

    rt_uint8_t lba3;
    rt_uint8_t lba4;
    rt_uint8_t lba5;
    rt_uint8_t reserved2;

    rt_uint8_t count_l;
    rt_uint8_t count_h;
    rt_uint8_t reserved3[2];

    rt_uint8_t reserved4[4];
}__attribute__ ((packed));

struct fis_data
{
    rt_uint8_t fis_type;
    rt_uint8_t pmport:4;
    rt_uint8_t reserved0:4;
    rt_uint8_t reserved1[2];

    rt_uint32_t data[1];
}__attribute__ ((packed));

struct fis_pio_setup
{
    rt_uint8_t fis_type;

    rt_uint8_t pmport:4;
    rt_uint8_t reserved0:1;
    rt_uint8_t direction:1;
    rt_uint8_t interrupt:1;
    rt_uint8_t reserved1:1;

    rt_uint8_t status;
    rt_uint8_t error;

    rt_uint8_t lba0;
    rt_uint8_t lba1;
    rt_uint8_t lba2;
    rt_uint8_t device;

    rt_uint8_t lba3;
    rt_uint8_t lba4;
    rt_uint8_t lba5;
    rt_uint8_t reserved2;

    rt_uint8_t count_l;
    rt_uint8_t count_h;
    rt_uint8_t reserved3;
    rt_uint8_t e_status;

    rt_uint16_t transfer_count;
    rt_uint8_t reserved4[2];
}__attribute__ ((packed));

struct fis_dma_setup
{
    rt_uint8_t fis_type;

    rt_uint8_t pmport:4;
    rt_uint8_t reserved0:1;
    rt_uint8_t direction:1;
    rt_uint8_t interrupt:1;
    rt_uint8_t auto_activate:1;

    rt_uint8_t reserved1[2];

    rt_uint64_t dma_buffer_id;

    rt_uint32_t reserved2;

    rt_uint32_t dma_buffer_offset;

    rt_uint32_t transfer_count;

    rt_uint32_t reserved3;
}__attribute__ ((packed));

struct fis_dev_bits
{
    volatile rt_uint8_t fis_type;

    volatile rt_uint8_t pmport:4;
    volatile rt_uint8_t reserved0:2;
    volatile rt_uint8_t interrupt:1;
    volatile rt_uint8_t notification:1;

    volatile rt_uint8_t status;
    volatile rt_uint8_t error;

    volatile rt_uint32_t protocol;
}__attribute__ ((packed));

struct hba_port
{
    volatile rt_uint32_t command_list_base_l;
    volatile rt_uint32_t command_list_base_h;
    volatile rt_uint32_t fis_base_l;
    volatile rt_uint32_t fis_base_h;
    volatile rt_uint32_t interrupt_status;
    volatile rt_uint32_t interrupt_enable;
    volatile rt_uint32_t command;
    volatile rt_uint32_t reserved0;
    volatile rt_uint32_t task_file_data;
    volatile rt_uint32_t signature;
    volatile rt_uint32_t sata_status;
    volatile rt_uint32_t sata_control;
    volatile rt_uint32_t sata_error;
    volatile rt_uint32_t sata_active;
    volatile rt_uint32_t command_issue;
    volatile rt_uint32_t sata_notification;
    volatile rt_uint32_t fis_based_switch_control;
    volatile rt_uint32_t reserved1[11];
    volatile rt_uint32_t vendor[4];
}__attribute__ ((packed));

struct hba_memory
{
    volatile rt_uint32_t capability;
    volatile rt_uint32_t global_host_control;
    volatile rt_uint32_t interrupt_status;
    volatile rt_uint32_t port_implemented;
    volatile rt_uint32_t version;
    volatile rt_uint32_t ccc_control;
    volatile rt_uint32_t ccc_ports;
    volatile rt_uint32_t em_location;
    volatile rt_uint32_t em_control;
    volatile rt_uint32_t ext_capabilities;
    volatile rt_uint32_t bohc;

    volatile rt_uint8_t reserved[0xA0 - 0x2C];

    volatile rt_uint8_t vendor[0x100 - 0xA0];

    volatile struct hba_port ports[1];
}__attribute__ ((packed));

struct hba_received_fis
{
    volatile struct fis_dma_setup fis_ds;
    volatile rt_uint8_t pad0[4];

    volatile struct fis_pio_setup fis_ps;
    volatile rt_uint8_t pad1[12];

    volatile struct fis_reg_device_to_host fis_r;
    volatile rt_uint8_t pad2[4];

    volatile struct fis_dev_bits fis_sdb;
    volatile rt_uint8_t ufis[64];
    volatile rt_uint8_t reserved[0x100 - 0xA0];
}__attribute__ ((packed));

struct hba_command_header
{
    rt_uint8_t fis_length:5;
    rt_uint8_t atapi:1;
    rt_uint8_t write:1;
    rt_uint8_t prefetchable:1;

    rt_uint8_t reset:1;
    rt_uint8_t bist:1;
    rt_uint8_t clear_busy_upon_r_ok:1;
    rt_uint8_t reserved0:1;
    rt_uint8_t pmport:4;

    rt_uint16_t prdt_len;

    volatile rt_uint32_t prdb_count;

    rt_uint32_t command_table_base_l;
    rt_uint32_t command_table_base_h;

    rt_uint32_t reserved1[4];
}__attribute__ ((packed));

struct hba_prdt_entry
{
    rt_uint32_t data_base_l;
    rt_uint32_t data_base_h;
    rt_uint32_t reserved0;

    rt_uint32_t byte_count:22;
    rt_uint32_t reserved1:9;
    rt_uint32_t interrupt_on_complete:1;
}__attribute__ ((packed));

struct hba_command_table
{
    rt_uint8_t command_fis[64];
    rt_uint8_t acmd[16];
    rt_uint8_t reserved[48];
    struct hba_prdt_entry prdt_entries[1];
}__attribute__ ((packed));

#define HBA_COMMAND_HEADER_NUM 32

struct ata_identify
{
    rt_uint16_t ata_device;
    rt_uint16_t dont_care[48];
    rt_uint16_t cap0;
    rt_uint16_t cap1;
    rt_uint16_t obs[2];
    rt_uint16_t free_fall;
    rt_uint16_t dont_care_2[8];
    rt_uint16_t dma_mode0;
    rt_uint16_t pio_modes;
    rt_uint16_t dont_care_3[4];
    rt_uint16_t additional_supported;
    rt_uint16_t rsv1[6];
    rt_uint16_t serial_ata_cap0;
    rt_uint16_t rsv2;

    rt_uint16_t serial_ata_features;
    rt_uint16_t serial_ata_features_enabled;

    rt_uint16_t maj_ver;
    rt_uint16_t min_ver;

    rt_uint16_t features0;
    rt_uint16_t features1;
    rt_uint16_t features2;
    rt_uint16_t features3;
    rt_uint16_t features4;
    rt_uint16_t features5;

    rt_uint16_t udma_modes;
    rt_uint16_t dont_care_4[11];
    rt_uint64_t lba48_addressable_sectors;
    rt_uint16_t wqewqe[2];
    rt_uint16_t ss_1;
    rt_uint16_t rrrrr[4];
    rt_uint32_t ss_2;
    /* ...and more */
};

#define HBA_PxCMD_ST  (1 << 0)
#define HBA_PxCMD_FRE (1 << 4)
#define HBA_PxCMD_FR  (1 << 14)
#define HBA_PxCMD_CR  (1 << 15)

#define HBA_GHC_AHCI_ENABLE         (1 << 31)
#define HBA_GHC_INTERRUPT_ENABLE    (1 << 1)
#define HBA_GHC_RESET               (1 << 0)

#define ATA_CMD_IDENTIFY 0xEC

#define ATA_DEV_BUSY    0x80
#define ATA_DEV_DRQ     0x08
#define ATA_DEV_ERR     0x01

#define ATA_CMD_READ_DMA_EX  0x25
#define ATA_CMD_WRITE_DMA_EX 0x35

#define PRDT_MAX_COUNT   0x1000

#define PRDT_MAX_ENTRIES 65535

#define ATA_TFD_TIMEOUT  1000000
#define AHCI_CMD_TIMEOUT 1000000

#define ATA_SECTOR_SIZE  512

#define AHCI_DEFAULT_INT 0

#define SATA_SIG_ATA     0x00000101    // SATA drive
#define SATA_SIG_ATAPI   0xEB140101    // SATAPI drive
#define SATA_SIG_SEMB    0xC33C0101    // Enclosure management bridge
#define SATA_SIG_PM      0x96690101    // Port multiplier

enum AHCI_DEVICE_TYPE
{
    AHCI_DEV_NULL = 0,
    AHCI_DEV_SATA,
    AHCI_DEV_SEMB,
    AHCI_DEV_PM,
    AHCI_DEV_SATAPI
};

#define HBA_PORT_IPM_ACTIVE  1
#define HBA_PORT_DET_PRESENT 3

#endif /* __DRV_AHCI_H__ */
