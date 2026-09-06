/* SPDX-License-Identifier: BSD-2-Clause */

#ifndef __MMC_MMC_H__
#define __MMC_MMC_H__

#define SD_VERSION_SD       0x20000
#define SD_VERSION_2        (SD_VERSION_SD | 0x20)
#define SD_VERSION_1_0      (SD_VERSION_SD | 0x10)
#define SD_VERSION_1_10     (SD_VERSION_SD | 0x1a)
#define MMC_VERSION_MMC     0x10000
#define MMC_VERSION_UNKNOWN (MMC_VERSION_MMC)
#define MMC_VERSION_1_2     (MMC_VERSION_MMC | 0x12)
#define MMC_VERSION_1_4     (MMC_VERSION_MMC | 0x14)
#define MMC_VERSION_2_2     (MMC_VERSION_MMC | 0x22)
#define MMC_VERSION_3       (MMC_VERSION_MMC | 0x30)
#define MMC_VERSION_4       (MMC_VERSION_MMC | 0x40)
#define MMC_VERSION_4_1     (MMC_VERSION_MMC | 0x41)
#define MMC_VERSION_4_2     (MMC_VERSION_MMC | 0x42)
#define MMC_VERSION_4_3     (MMC_VERSION_MMC | 0x43)
#define MMC_VERSION_4_41    (MMC_VERSION_MMC | 0x44)
#define MMC_VERSION_4_5     (MMC_VERSION_MMC | 0x45)
#define MMC_VERSION_5_0     (MMC_VERSION_MMC | 0x50)
#define MMC_VERSION_5_1     (MMC_VERSION_MMC | 0x51)

#define MMC_MODE_HS         (1 << 0)
#define MMC_MODE_HS_52MHz   (1 << 1)
#define MMC_MODE_4BIT       (1 << 2)
#define MMC_MODE_8BIT       (1 << 3)
#define MMC_MODE_SPI        (1 << 4)
#define MMC_MODE_HC         (1 << 5)
#define MMC_MODE_DDR_52MHz  (1 << 6)
#define MMC_MODE_HS200      (1 << 7)
#define MMC_MODE_HS400      (1 << 8)

#define SD_DATA_4BIT        0x00040000
#define IS_SD(x)            (x->version & SD_VERSION_SD)
#define MMC_DATA_READ       (1 << 0)
#define MMC_DATA_WRITE      (1 << 1)

#define MMC_CMD_MANUAL      1
#define NO_CARD_ERR         -16
#define UNUSABLE_ERR        -17
#define COMM_ERR            -18
#define TIMEOUT             -19

#define MMC_CMD_GO_IDLE_STATE           0
#define MMC_CMD_SEND_OP_COND            1
#define MMC_CMD_ALL_SEND_CID            2
#define MMC_CMD_SET_RELATIVE_ADDR       3
#define MMC_CMD_SET_DSR                 4
#define MMC_CMD_SWITCH                  6
#define MMC_CMD_SELECT_CARD             7
#define MMC_CMD_SEND_EXT_CSD            8
#define MMC_CMD_SEND_CSD                9
#define MMC_CMD_SEND_CID                10
#define MMC_CMD_STOP_TRANSMISSION       12
#define MMC_CMD_SEND_STATUS             13
#define MMC_CMD_SET_BLOCKLEN            16
#define MMC_CMD_READ_SINGLE_BLOCK       17
#define MMC_CMD_READ_MULTIPLE_BLOCK     18
#define MMC_CMD_WRITE_SINGLE_BLOCK      24
#define MMC_CMD_WRITE_MULTIPLE_BLOCK    25
#define MMC_CMD_ERASE_GROUP_START       35
#define MMC_CMD_ERASE_GROUP_END         36
#define MMC_CMD_ERASE                   38
#define MMC_CMD_APP_CMD                 55
#define MMC_CMD_SPI_READ_OCR            58
#define MMC_CMD_SPI_CRC_ON_OFF          59

#define SD_CMD_SEND_RELATIVE_ADDR       3
#define SD_CMD_SWITCH_FUNC              6
#define SD_CMD_SEND_IF_COND             8
#define SD_CMD_APP_SET_BUS_WIDTH        6
#define SD_CMD_ERASE_WR_BLK_START       32
#define SD_CMD_ERASE_WR_BLK_END         33
#define SD_CMD_APP_SEND_OP_COND         41
#define SD_CMD_APP_SEND_SCR             51

#define SD_HIGHSPEED_BUSY               0x00020000
#define SD_HIGHSPEED_SUPPORTED          0x00020000
#define MMC_HS_TIMING                   0x00000100
#define MMC_HS_52MHZ                    2
#define MMC_DDR_52MHZ                   4
#define OCR_BUSY                        0x80000000
#define OCR_HCS                         0x40000000
#define OCR_VOLTAGE_MASK                0x007FFF80
#define OCR_ACCESS_MODE                 0x60000000
#define SECURE_ERASE                    0x80000000
#define MMC_STATUS_MASK                 (~0x0206BF7F)
#define MMC_STATUS_RDY_FOR_DATA         (1 << 8)
#define MMC_STATUS_CURR_STATE           (0xf << 9)
#define MMC_STATUS_ERROR                (1 << 19)

#define MMC_VDD_165_195      0x00000080    /* VDD voltage 1.65 - 1.95 */
#define MMC_VDD_20_21        0x00000100    /* VDD voltage 2.0 ~ 2.1 */
#define MMC_VDD_21_22        0x00000200    /* VDD voltage 2.1 ~ 2.2 */
#define MMC_VDD_22_23        0x00000400    /* VDD voltage 2.2 ~ 2.3 */
#define MMC_VDD_23_24        0x00000800    /* VDD voltage 2.3 ~ 2.4 */
#define MMC_VDD_24_25        0x00001000    /* VDD voltage 2.4 ~ 2.5 */
#define MMC_VDD_25_26        0x00002000    /* VDD voltage 2.5 ~ 2.6 */
#define MMC_VDD_26_27        0x00004000    /* VDD voltage 2.6 ~ 2.7 */
#define MMC_VDD_27_28        0x00008000    /* VDD voltage 2.7 ~ 2.8 */
#define MMC_VDD_28_29        0x00010000    /* VDD voltage 2.8 ~ 2.9 */
#define MMC_VDD_29_30        0x00020000    /* VDD voltage 2.9 ~ 3.0 */
#define MMC_VDD_30_31        0x00040000    /* VDD voltage 3.0 ~ 3.1 */
#define MMC_VDD_31_32        0x00080000    /* VDD voltage 3.1 ~ 3.2 */
#define MMC_VDD_32_33        0x00100000    /* VDD voltage 3.2 ~ 3.3 */
#define MMC_VDD_33_34        0x00200000    /* VDD voltage 3.3 ~ 3.4 */
#define MMC_VDD_34_35        0x00400000    /* VDD voltage 3.4 ~ 3.5 */
#define MMC_VDD_35_36        0x00800000    /* VDD voltage 3.5 ~ 3.6 */

#define MMC_SWITCH_MODE_CMD_SET         0x00
#define MMC_SWITCH_MODE_SET_BITS        0x01
#define MMC_SWITCH_MODE_CLEAR_BITS      0x02
#define MMC_SWITCH_MODE_WRITE_BYTE      0x03
#define SD_SWITCH_CHECK                 0
#define SD_SWITCH_SWITCH                1
#define EXT_CSD_PART_CONF               179
#define EXT_CSD_BUS_WIDTH               183
#define EXT_CSD_HS_TIMING               185
#define EXT_CSD_CARD_TYPE               196
#define EXT_CSD_REV                     192
#define EXT_CSD_SEC_CNT                 212

#define EXT_CSD_CMD_SET_NORMAL          (1 << 0)
#define EXT_CSD_CMD_SET_SECURE          (1 << 1)
#define EXT_CSD_CMD_SET_CPSECURE        (1 << 2)
#define EXT_CSD_CARD_TYPE_HS_26         (1 << 0)
#define EXT_CSD_CARD_TYPE_HS_52         (1 << 1)
#define EXT_CSD_CARD_TYPE_HS            (EXT_CSD_CARD_TYPE_HS_26 | EXT_CSD_CARD_TYPE_HS_52)
#define EXT_CSD_CARD_TYPE_DDR_1_8V      (1 << 2)
#define EXT_CSD_CARD_TYPE_DDR_1_2V      (1 << 3)
#define EXT_CSD_CARD_TYPE_DDR_52        (EXT_CSD_CARD_TYPE_DDR_1_8V | EXT_CSD_CARD_TYPE_DDR_1_2V)
#define EXT_CSD_CARD_TYPE_HS200_1_8V    (1 << 4)
#define EXT_CSD_CARD_TYPE_HS200_1_2V    (1 << 5)
#define EXT_CSD_CARD_TYPE_HS200         (EXT_CSD_CARD_TYPE_HS200_1_8V | EXT_CSD_CARD_TYPE_HS200_1_2V)
#define EXT_CSD_CARD_TYPE_HS400_1_8V    (1 << 6)
#define EXT_CSD_CARD_TYPE_HS400_1_2V    (1 << 7)
#define EXT_CSD_CARD_TYPE_HS400         (EXT_CSD_CARD_TYPE_HS400_1_8V | EXT_CSD_CARD_TYPE_HS400_1_2V)
#define EXT_CSD_BUS_WIDTH_1             0
#define EXT_CSD_BUS_WIDTH_4             1
#define EXT_CSD_BUS_WIDTH_8             2
#define EXT_CSD_BUS_DDR_4               5
#define EXT_CSD_BUS_DDR_8               6
#define EXT_CSD_TIMING_BC               0
#define EXT_CSD_TIMING_HS               1
#define EXT_CSD_TIMING_HS200            2
#define EXT_CSD_TIMING_HS400            3

#define R1_ILLEGAL_COMMAND              (1 << 22)
#define R1_APP_CMD                      (1 << 5)
#define MMC_RSP_PRESENT                 (1 << 0)
#define MMC_RSP_136                     (1 << 1)
#define MMC_RSP_CRC                     (1 << 2)
#define MMC_RSP_BUSY                    (1 << 3)
#define MMC_RSP_OPCODE                  (1 << 4)
#define MMC_RSP_NONE                    0
#define MMC_RSP_R1                      (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define MMC_RSP_R1b                     (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE | MMC_RSP_BUSY)
#define MMC_RSP_R2                      (MMC_RSP_PRESENT | MMC_RSP_136 | MMC_RSP_CRC)
#define MMC_RSP_R3                      (MMC_RSP_PRESENT)
#define MMC_RSP_R4                      (MMC_RSP_PRESENT)
#define MMC_RSP_R5                      (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define MMC_RSP_R6                      (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)
#define MMC_RSP_R7                      (MMC_RSP_PRESENT | MMC_RSP_CRC | MMC_RSP_OPCODE)

#define MMCPART_NOAVAILABLE             0xf
#define PART_ACCESS_MASK                7
#define PART_SUPPORT                    1
#define MMC_REG_FIFO_OS                 0x200
#define SDMMC_PARAMETER_MAGIC           0x6D6D6361
#define CARD_TYPE_SD                    0x8000001
#define CARD_TYPE_MMC                   0x8000000
#define CARD_TYPE_NULL                  0xffffffff
#define EXT_PARA1_1V8_GPIO_BIAS         1
#define EXT_PARA0_ID                    0x55000000
#define EXT_PARA0_TUNING_SUCCESS_FLAG   (1 << 0)

struct tune_sdly {
    u32 tm4_smx_fx[12];
};

struct mmc_cmd {
    unsigned cmdidx;
    unsigned resp_type;
    unsigned cmdarg;
    unsigned response[4];
    unsigned flags;
};

struct mmc_data {
    union {
        char *dest;
        const char *src;
    } b;
    unsigned flags;
    unsigned blocks;
    unsigned blocksize;
};

struct mmc {
    char name[32];
    void *priv;
    unsigned voltages;
    unsigned version;
    unsigned has_init;
    unsigned control_num;
    unsigned f_min;
    unsigned f_max;
    unsigned f_max_ddr;
    int high_capacity;
    unsigned bus_width;
    unsigned clock;
    unsigned card_caps;
    unsigned host_caps;
    unsigned ocr;
    unsigned scr[2];
    unsigned csd[4];
    unsigned cid[4];
    unsigned rca;
    unsigned part_config;
    unsigned part_num;
    unsigned tran_speed;
    unsigned read_bl_len;
    unsigned write_bl_len;
    unsigned erase_grp_size;
    unsigned long long capacity;
    int (*send_cmd) (struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data);
    void (*set_ios) (struct mmc *mmc);
    int (*init) (struct mmc *mmc);
    int (*update_phase) (struct mmc *mmc);
    struct tune_sdly tune_sdly;
    unsigned b_max;
    unsigned lba;
    unsigned blksz;
    char revision[8 + 8];
    unsigned int speed_mode;
};

#define mmc_host_is_spi(mmc) ((mmc)->host_caps & MMC_MODE_SPI)

unsigned long mmc_bread(int dev_num, unsigned long start, unsigned blkcnt, void *dst);

#endif
