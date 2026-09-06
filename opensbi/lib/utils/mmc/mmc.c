/* SPDX-License-Identifier: BSD-2-Clause */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_atomic.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_bitops.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_string.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_console.h>
#include <sbi_utils/led/f133.h>
#include <sbi_utils/mmc/mmc.h>
#include <sbi_utils/mmc/f133.h>
#include <sbi_utils/dram/f133.h>
#include <sbi_utils/gpio/f133.h>
#include <sbi_utils/clock/f133.h>
#include <sbi_utils/common/f133.h>
#include <sbi_utils/serial/f133_uart.h>
#include <sbi_utils/display/f133_slcd.h>

char ext_csd[512] = {0};
unsigned char mmc_arg_addr[SUNXI_SDMMC_PARAMETER_REGION_SIZE_BYTE] = {0};

static struct mmc *mmc_devices[MAX_MMC_NUM] = {0};

int mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data)
{
    return mmc->send_cmd(mmc, cmd, data);
}

int mmc_send_status(struct mmc *mmc, int timeout)
{
    int err;
    struct mmc_cmd cmd;

    cmd.cmdidx = MMC_CMD_SEND_STATUS;
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = mmc->rca << 16;
    cmd.flags = 0;

    do {
        err = mmc_send_cmd(mmc, &cmd, NULL);
        if (err) {
            return err;
        } 
        else if (cmd.response[0] & MMC_STATUS_RDY_FOR_DATA) {
            break;
        }

        mdelay(1);
        if (cmd.response[0] & MMC_STATUS_MASK) {
            return COMM_ERR;
        }
    } while (timeout--);

    if (!timeout) {
        return TIMEOUT;
    }
    return 0;
}

int mmc_set_blocklen(struct mmc *mmc, int len)
{
    struct mmc_cmd cmd;

    if ((mmc->speed_mode == HSDDR52_DDR50) || (mmc->speed_mode == HS400)) {
        return 0;
    }

    cmd.cmdidx = MMC_CMD_SET_BLOCKLEN;
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = len;
    cmd.flags = 0;
    return mmc_send_cmd(mmc, &cmd, NULL);
}

struct mmc *find_mmc_device(int dev_num)
{
    if (mmc_devices[dev_num] != NULL) {
        return mmc_devices[dev_num];
    }
    return NULL;
}

int mmc_read_blocks(struct mmc *mmc, void *dst, unsigned long start, unsigned blkcnt)
{
    struct mmc_cmd cmd;
    struct mmc_data data;
    int timeout = 1000;

    if (blkcnt > 1) {
        cmd.cmdidx = MMC_CMD_READ_MULTIPLE_BLOCK;
    }
    else {
        cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;
    }

    if (mmc->high_capacity) {
        cmd.cmdarg = start;
    }
    else {
        cmd.cmdarg = start * mmc->read_bl_len;
    }

    cmd.resp_type = MMC_RSP_R1;
    cmd.flags     = 0;

    data.b.dest    = dst;
    data.blocks    = blkcnt;
    data.blocksize = mmc->read_bl_len;
    data.flags     = MMC_DATA_READ;

    if (mmc_send_cmd(mmc, &cmd, &data)) {
        return 0;
    }

    if (blkcnt > 1) {
        cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
        cmd.cmdarg = 0;
        cmd.resp_type = MMC_RSP_R1b;
        cmd.flags = 0;
        if (mmc_send_cmd(mmc, &cmd, NULL)) {
            return 0;
        }
        mmc_send_status(mmc, timeout);
    }
    return blkcnt;
}

unsigned long mmc_bread(int dev_num, unsigned long start, unsigned blkcnt, void *dst)
{
    unsigned cur, blocks_todo = blkcnt;
    struct mmc *mmc = find_mmc_device(dev_num);

    if (blkcnt == 0) {
        return 0;
    }

    if (!mmc) {
        return 0;
    }

    if ((start + blkcnt) > mmc->lba) {
        return 0;
    }

    if (mmc_set_blocklen(mmc, mmc->read_bl_len)) {
        return 0;
    }

    do {
        cur = (blocks_todo > mmc->b_max) ? mmc->b_max : blocks_todo;
        if (mmc_read_blocks(mmc, dst, start, cur) != cur) {
            return 0;
        }
        blocks_todo -= cur;
        start += cur;
        dst = (char *)dst + cur * mmc->read_bl_len;
    } while (blocks_todo > 0);
    return blkcnt;
}

int mmc_go_idle(struct mmc *mmc)
{
    int err;
    struct mmc_cmd cmd;

    mdelay(1);
    cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
    cmd.cmdarg = 0;
    cmd.resp_type = MMC_RSP_NONE;
    cmd.flags = 0;

    err = mmc_send_cmd(mmc, &cmd, NULL);
    if (err) {
        return err;
    }
    mdelay(2);
    return 0;
}

int sd_send_op_cond(struct mmc *mmc)
{
    int err;
    int timeout = 1000;
    struct mmc_cmd cmd;

    do {
        cmd.cmdidx = MMC_CMD_APP_CMD;
        cmd.resp_type = MMC_RSP_R1;
        cmd.cmdarg = 0;
        cmd.flags = 0;

        err = mmc_send_cmd(mmc, &cmd, NULL);
        if (err) {
            return err;
        }

        cmd.cmdidx    = SD_CMD_APP_SEND_OP_COND;
        cmd.resp_type = MMC_RSP_R3;
        cmd.cmdarg = mmc_host_is_spi(mmc) ? 0 : (mmc->voltages & 0xff8000);
        if (mmc->version == SD_VERSION_2) {
            cmd.cmdarg |= OCR_HCS;
        }

        err = mmc_send_cmd(mmc, &cmd, NULL);
        if (err) {
            return err;
        }
        mdelay(1);
    } while ((!(cmd.response[0] & OCR_BUSY)) && timeout--);

    if (timeout <= 0) {
        return UNUSABLE_ERR;
    }

    if (mmc->version != SD_VERSION_2) {
        mmc->version = SD_VERSION_1_0;
    }

    if (mmc_host_is_spi(mmc)) {
        cmd.cmdidx = MMC_CMD_SPI_READ_OCR;
        cmd.resp_type = MMC_RSP_R3;
        cmd.cmdarg = 0;
        cmd.flags = 0;
        err = mmc_send_cmd(mmc, &cmd, NULL);
        if (err) {
            return err;
        }
    }

    mmc->ocr = cmd.response[0];
    mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);
    mmc->rca = 0;
    return 0;
}

int mmc_send_op_cond(struct mmc *mmc)
{
    int err;
    int timeout = 10000;
    struct mmc_cmd cmd;

    mmc_go_idle(mmc);
    cmd.cmdidx = MMC_CMD_SEND_OP_COND;
    cmd.resp_type = MMC_RSP_R3;
    cmd.cmdarg = 0;
    cmd.flags = 0;

    err = mmc_send_cmd(mmc, &cmd, NULL);
    if (err) {
        return err;
    }

    mdelay(1);
    do {
        cmd.cmdidx = MMC_CMD_SEND_OP_COND;
        cmd.resp_type = MMC_RSP_R3;
        cmd.cmdarg = (mmc_host_is_spi(mmc) ? 0 :
            (mmc->voltages & (cmd.response[0] & OCR_VOLTAGE_MASK)) | (cmd.response[0] & OCR_ACCESS_MODE));

        if (mmc->host_caps & MMC_MODE_HC) {
            cmd.cmdarg |= OCR_HCS;
        }

        cmd.flags = 0;
        err = mmc_send_cmd(mmc, &cmd, NULL);
        if (err) {
            return err;
        }

        mdelay(1);
    } while (!(cmd.response[0] & OCR_BUSY) && timeout--);

    if (timeout <= 0) {
        return UNUSABLE_ERR;
    }

    if (mmc_host_is_spi(mmc)) {
        cmd.cmdidx = MMC_CMD_SPI_READ_OCR;
        cmd.resp_type = MMC_RSP_R3;
        cmd.cmdarg = 0;
        cmd.flags = 0;
        err = mmc_send_cmd(mmc, &cmd, NULL);
        if (err) {
            return err;
        }
    }

    mmc->version = MMC_VERSION_UNKNOWN;
    mmc->ocr = cmd.response[0];
    mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);
    mmc->rca = 1;
    return 0;
}

int mmc_send_ext_csd(struct mmc *mmc, char *ext_csd)
{
    struct mmc_cmd cmd;
    struct mmc_data data;

    cmd.cmdidx = MMC_CMD_SEND_EXT_CSD;
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = 0;
    cmd.flags = 0;

    data.b.dest = ext_csd;
    data.blocks = 1;
    data.blocksize = 512;
    data.flags = MMC_DATA_READ;
    return mmc_send_cmd(mmc, &cmd, &data);
}

int mmc_update_phase(struct mmc *mmc)
{
    if (mmc->update_phase == NULL) {
        return 0;
    }
    return mmc->update_phase(mmc);
}

int mmc_switch(struct mmc *mmc, u8 set, u8 index, u8 value)
{
    int ret;
    struct mmc_cmd cmd;
    int timeout = 1000;

    cmd.cmdidx = MMC_CMD_SWITCH;
    cmd.resp_type = MMC_RSP_R1b;
    cmd.cmdarg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) | (index << 16) | (value << 8);
    cmd.flags = 0;
    mmc_send_cmd(mmc, &cmd, NULL);
    ret = mmc_update_phase(mmc);
    if (ret) {
        return ret;
    }
    mmc_send_status(mmc, timeout);
    return ret;
}

int mmc_change_freq(struct mmc *mmc)
{
    char cardtype;
    int err;
    int retry = 5;

    mmc->card_caps = 0;
    if (mmc_host_is_spi(mmc)) {
        return 0;
    }

    if (mmc->version < MMC_VERSION_4) {
        return 0;
    }
    mmc->card_caps |= MMC_MODE_4BIT | MMC_MODE_8BIT;
    err = mmc_send_ext_csd(mmc, ext_csd);
    if (err) {
        return err;
    }

    cardtype = ext_csd[196] & 0xff;
    do {
        err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING, 1);
        if (!err) {
            break;
        }
    } while (retry--);

    if (err) {
        return err;
    }

    err = mmc_send_ext_csd(mmc, ext_csd);
    if (err) {
        return err;
    }

    if (!ext_csd[185]) {
        return 0;
    }

    if (cardtype & EXT_CSD_CARD_TYPE_HS) {
        if (cardtype & EXT_CSD_CARD_TYPE_DDR_52) {
            mmc->card_caps |= MMC_MODE_DDR_52MHz;
            mmc->speed_mode = HSDDR52_DDR50;
        } 
        else {
            mmc->speed_mode = HSSDR52_SDR25;
        }
        mmc->card_caps |= MMC_MODE_HS_52MHz | MMC_MODE_HS;

    } 
    else {
        mmc->card_caps |= MMC_MODE_HS;
        mmc->speed_mode = DS26_SDR12;
    }
    return 0;
}

int mmc_switch_part(int dev_num, unsigned int part_num)
{
    struct mmc *mmc = find_mmc_device(dev_num);

    if (!mmc) {
        return -1;
    }
    return mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_PART_CONF, 
        (mmc->part_config & ~PART_ACCESS_MASK) | (part_num & PART_ACCESS_MASK));
}

int sd_switch(struct mmc *mmc, int mode, int group, u8 value, u8 *resp)
{
    struct mmc_cmd cmd;
    struct mmc_data data;

    cmd.cmdidx = SD_CMD_SWITCH_FUNC;
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = ((u32)mode << 31) | 0xffffff;
    cmd.cmdarg &= ~(0xf << (group * 4));
    cmd.cmdarg |= value << (group * 4);
    cmd.flags = 0;

    data.b.dest = (char *)resp;
    data.blocksize = 64;
    data.blocks = 1;
    data.flags = MMC_DATA_READ;
    return mmc_send_cmd(mmc, &cmd, &data);
}

int sd_change_freq(struct mmc *mmc)
{
    int err;
    struct mmc_cmd cmd;
    u32 scr[2];
    u32 switch_status[16];
    struct mmc_data data;
    int timeout;

    mmc->card_caps = 0;
    if (mmc_host_is_spi(mmc)) {
        return 0;
    }

    cmd.cmdidx = MMC_CMD_APP_CMD;
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = mmc->rca << 16;
    cmd.flags = 0;
    err = mmc_send_cmd(mmc, &cmd, NULL);
    if (err) {
        return err;
    }

    cmd.cmdidx = SD_CMD_APP_SEND_SCR;
    cmd.resp_type = MMC_RSP_R1;
    cmd.cmdarg = 0;
    cmd.flags = 0;
    timeout = 3;

retry_scr:
    data.b.dest = (char *)&scr;
    data.blocksize = 8;
    data.blocks = 1;
    data.flags = MMC_DATA_READ;
    err = mmc_send_cmd(mmc, &cmd, &data);
    if (err) {
        if (timeout--) {
            goto retry_scr;
        }
        return err;
    }

    mmc->scr[0] = __mmc_be32_to_cpu(scr[0]);
    mmc->scr[1] = __mmc_be32_to_cpu(scr[1]);

    switch ((mmc->scr[0] >> 24) & 0xf) {
    case 0:
        mmc->version = SD_VERSION_1_0;
        break;
    case 1:
        mmc->version = SD_VERSION_1_10;
        break;
    case 2:
        mmc->version = SD_VERSION_2;
        break;
    default:
        mmc->version = SD_VERSION_1_0;
        break;
    }

    if (mmc->scr[0] & SD_DATA_4BIT) {
        mmc->card_caps |= MMC_MODE_4BIT;
    }

    if (mmc->version == SD_VERSION_1_0) {
        return 0;
    }

    timeout = 4;
    while (timeout--) {
        err = sd_switch(mmc, SD_SWITCH_CHECK, 0, 1, (u8 *)&switch_status);

        if (err) {
            return err;
        }

        if (!(switch_status[7] & SD_HIGHSPEED_BUSY)) {
            break;
        }
    }

    if (!(switch_status[3] & SD_HIGHSPEED_SUPPORTED)) {
        return 0;
    }

    err = sd_switch(mmc, SD_SWITCH_SWITCH, 0, 1, (u8 *)&switch_status);
    if (err) {
        return err;
    }

    err = mmc_update_phase(mmc);
    if (err) {
        return err;
    }

    if ((switch_status[4] & 0x0f000000) == 0x01000000) {
        mmc->card_caps |= MMC_MODE_HS;
        mmc->speed_mode = HSSDR52_SDR25;
    }
    return 0;
}

static const int fbase[] = {
    10000, 100000, 1000000, 10000000,
};

static const int multipliers[] = {
    0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80,
};

void mmc_set_ios(struct mmc *mmc)
{
    mmc->set_ios(mmc);
}

void mmc_set_clock(struct mmc *mmc, u32 clock)
{
    if (clock > mmc->f_max) {
        clock = mmc->f_max;
    }

    if (clock < mmc->f_min) {
        clock = mmc->f_min;
    }
    mmc->clock = clock;
    mmc_set_ios(mmc);
}

void mmc_set_bus_width(struct mmc *mmc, u32 width)
{
    mmc->bus_width = width;
    mmc_set_ios(mmc);
}

int mmc_read_info(struct mmc *mmc, void *buffer, unsigned short length)
{
    int ret = 0;
    int i = 0;
    int retry_read = 0;
    u32 sum = 0;
    u32 add_sum = 0;
    struct sunxi_sdmmc_parameter_region *pregion = (struct sunxi_sdmmc_parameter_region *)buffer;

retry:
    ret = mmc_read_blocks(mmc, buffer, SUNXI_SDMMC_PARAMETER_REGION_LBA_START, length);
    if (ret < 0) {
        if (retry_read < 3) {
            retry_read++;
            goto retry;
        } 
        goto err;
    }

    if (pregion->header.magic == SDMMC_PARAMETER_MAGIC) {
        add_sum = pregion->header.add_sum;
        pregion->header.add_sum = 0;
        sum = 0;
        for (i = 0; i < pregion->header.length; i++) {
            sum += ((unsigned char *)buffer)[i];
        }

        if (sum != add_sum) {
            if (retry_read < 3) {
                retry_read++;
                goto retry;
            }
            goto err;
        }
    } 
    else {
        if (retry_read < 3) {
            retry_read++;
            goto retry;
        } 
        goto err;
    }
    return 0;

err:
    return -1;
}

int mmc_startup(struct mmc *mmc)
{
    int err;
    u32 mult, freq;
    u64 cmult, csize, capacity;
    struct mmc_cmd cmd;
    int timeout = 1000;

    cmd.cmdidx = mmc_host_is_spi(mmc) ? MMC_CMD_SEND_CID : MMC_CMD_ALL_SEND_CID;
    cmd.resp_type = MMC_RSP_R2;
    cmd.cmdarg = 0;
    cmd.flags = 0;
    err = mmc_send_cmd(mmc, &cmd, NULL);
    if (err) {
        return err;
    }
    __memcpy(mmc->cid, cmd.response, 16);

    if (!mmc_host_is_spi(mmc)) {
        cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR;
        cmd.cmdarg = mmc->rca << 16;
        cmd.resp_type = MMC_RSP_R6;
        cmd.flags = 0;
        err = mmc_send_cmd(mmc, &cmd, NULL);
        if (err) {
            return err;
        }

        if (IS_SD(mmc)) {
            mmc->rca = (cmd.response[0] >> 16) & 0xffff;
        }
    }

    cmd.cmdidx = MMC_CMD_SEND_CSD;
    cmd.resp_type = MMC_RSP_R2;
    cmd.cmdarg = mmc->rca << 16;
    cmd.flags = 0;

    err = mmc_send_cmd(mmc, &cmd, NULL);
    mmc_send_status(mmc, timeout);
    if (err) {
        return err;
    }

    mmc->csd[0] = cmd.response[0];
    mmc->csd[1] = cmd.response[1];
    mmc->csd[2] = cmd.response[2];
    mmc->csd[3] = cmd.response[3];
    if (mmc->version == MMC_VERSION_UNKNOWN) {
        int version = (cmd.response[0] >> 26) & 0xf;

        switch (version) {
        case 0:
            mmc->version = MMC_VERSION_1_2;
            break;
        case 1:
            mmc->version = MMC_VERSION_1_4;
            break;
        case 2:
            mmc->version = MMC_VERSION_2_2;
            break;
        case 3:
            mmc->version = MMC_VERSION_3;
            break;
        case 4:
            mmc->version = MMC_VERSION_4;
            break;
        default:
            mmc->version = MMC_VERSION_1_2;
            break;
        }
    }

    freq = fbase[(cmd.response[0] & 0x7)];
    mult = multipliers[((cmd.response[0] >> 3) & 0xf)];
    mmc->tran_speed = freq * mult;
    mmc->read_bl_len = 1 << ((cmd.response[1] >> 16) & 0xf);

    if (IS_SD(mmc)) {
        mmc->write_bl_len = mmc->read_bl_len;
    }
    else {
        mmc->write_bl_len = 1 << ((cmd.response[3] >> 22) & 0xf);
    }

    if (mmc->high_capacity) {
        csize = (mmc->csd[1] & 0x3f) << 16 | (mmc->csd[2] & 0xffff0000) >> 16;
        cmult = 8;
    } 
    else {
        csize = (mmc->csd[1] & 0x3ff) << 2 | (mmc->csd[2] & 0xc0000000) >> 30;
        cmult = (mmc->csd[2] & 0x00038000) >> 15;
    }

    mmc->capacity = (csize + 1) << (cmult + 2);
    mmc->capacity *= mmc->read_bl_len;

    if (mmc->read_bl_len > 512) {
        mmc->read_bl_len = 512;
    }

    if (mmc->write_bl_len > 512) {
        mmc->write_bl_len = 512;
    }

    if (!mmc_host_is_spi(mmc)) {
        cmd.cmdidx = MMC_CMD_SELECT_CARD;
        cmd.resp_type = MMC_RSP_R1b;
        cmd.cmdarg = mmc->rca << 16;
        cmd.flags = 0;
        err = mmc_send_cmd(mmc, &cmd, NULL);
        if (err) {
            return err;
        }
    }

    if (!IS_SD(mmc)) {
        mmc_read_info(mmc, mmc_arg_addr, SUNXI_SDMMC_PARAMETER_REGION_SIZE_BYTE >> 9);
        mmc_update_host_caps_r(mmc->control_num);
    }

    mmc_set_clock(mmc, 25000000);
    mmc->erase_grp_size = 1;
    mmc->part_config    = MMCPART_NOAVAILABLE;
    if (!IS_SD(mmc) && (mmc->version >= MMC_VERSION_4)) {
        err = mmc_send_ext_csd(mmc, ext_csd);
        if (!err) {
            switch (ext_csd[192]) {
            case 0:
                mmc->version = MMC_VERSION_4;
                break;
            case 1:
                mmc->version = MMC_VERSION_4_1;
                break;
            case 2:
                mmc->version = MMC_VERSION_4_2;
                break;
            case 3:
                mmc->version = MMC_VERSION_4_3;
                break;
            case 5:
                mmc->version = MMC_VERSION_4_41;
                break;
            case 6:
                mmc->version = MMC_VERSION_4_5;
                break;
            case 7:
                mmc->version = MMC_VERSION_5_0;
                break;
            case 8:
                mmc->version = MMC_VERSION_5_1;
                break;
            }
        }

        if (!err & (ext_csd[192] >= 2)) {
            capacity = ext_csd[212] << 0 | ext_csd[213] << 8 | ext_csd[214] << 16 | ext_csd[215] << 24;
            capacity *= 512;
            if ((capacity >> 20) > 2 * 1024) {
                mmc->capacity = capacity;
            }
        }

        if (ext_csd[175]) {
            mmc->erase_grp_size = ext_csd[224] * 512 * 1024;
        }
        else {
            int erase_gsz, erase_gmul;
            erase_gsz  = (mmc->csd[2] & 0x00007c00) >> 10;
            erase_gmul = (mmc->csd[2] & 0x000003e0) >> 5;
            mmc->erase_grp_size = (erase_gsz + 1) * (erase_gmul + 1);
        }

        if (ext_csd[160] & PART_SUPPORT) {
            mmc->part_config = ext_csd[179];
        }
    }

    if (IS_SD(mmc)) {
        err = sd_change_freq(mmc);
    }
    else {
        err = mmc_change_freq(mmc);
    }

    if (err) {
        return err;
    }

    err = mmc_update_phase(mmc);
    if (err) {
        return err;
    }

    mmc->card_caps &= mmc->host_caps;
    if (!(mmc->card_caps & MMC_MODE_DDR_52MHz) && !IS_SD(mmc)) {
        if (mmc->speed_mode == HSDDR52_DDR50) {
            mmc->speed_mode = HSSDR52_SDR25;
        }
        else {
            mmc->speed_mode = DS26_SDR12;
        }
    }

    if (IS_SD(mmc)) {
        if (mmc->card_caps & MMC_MODE_4BIT) {
            cmd.cmdidx = MMC_CMD_APP_CMD;
            cmd.resp_type = MMC_RSP_R1;
            cmd.cmdarg = mmc->rca << 16;
            cmd.flags = 0;
            err = mmc_send_cmd(mmc, &cmd, NULL);
            if (err) {
                return err;
            }

            cmd.cmdidx = SD_CMD_APP_SET_BUS_WIDTH;
            cmd.resp_type = MMC_RSP_R1;
            cmd.cmdarg = 2;
            cmd.flags = 0;
            err = mmc_send_cmd(mmc, &cmd, NULL);
            if (err) {
                return err;
            }
            mmc_set_bus_width(mmc, 4);
        }

        if (mmc->card_caps & MMC_MODE_HS) {
            mmc->tran_speed = 50000000;
        }
        else {
            mmc->tran_speed = 25000000;
        }
    } 
    else {
        if (mmc->card_caps & MMC_MODE_8BIT) {
            if ((mmc->card_caps & MMC_MODE_DDR_52MHz)) {
                err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH, EXT_CSD_BUS_DDR_8);
                if (err) {
                    return err;
                }
                mmc_set_bus_width(mmc, 8);
            } 
            else {
                err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH, EXT_CSD_BUS_WIDTH_8);
                if (err) {
                    return err;
                }
                mmc_set_bus_width(mmc, 8);
            }
        } 
        else if (mmc->card_caps & MMC_MODE_4BIT) {
            if ((mmc->card_caps & MMC_MODE_DDR_52MHz)) {
                err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH, EXT_CSD_BUS_DDR_4);
                if (err) {
                    return err;
                }
                mmc_set_bus_width(mmc, 4);
            } 
            else {
                err = mmc_switch(mmc, EXT_CSD_CMD_SET_NORMAL, EXT_CSD_BUS_WIDTH, EXT_CSD_BUS_WIDTH_4);
                if (err) {
                    return err;
                }
                mmc_set_bus_width(mmc, 4);
            }
        }

        if (mmc->card_caps & MMC_MODE_DDR_52MHz) {
            mmc->tran_speed = 52000000;
        } 
        else if (mmc->card_caps & MMC_MODE_HS) {
            if (mmc->card_caps & MMC_MODE_HS_52MHz) {
                mmc->tran_speed = 52000000;
            }
            else {
                mmc->tran_speed = 26000000;
            }
        } 
        else {
            mmc->tran_speed = 26000000;
        }
    }
    mmc_set_clock(mmc, mmc->tran_speed);
    mmc->blksz = mmc->read_bl_len;
    mmc->lba = mmc->capacity >> 9;
    return 0;
}

int mmc_send_if_cond(struct mmc *mmc)
{
    int err = 0;
    struct mmc_cmd cmd;

    cmd.cmdidx = SD_CMD_SEND_IF_COND;
    cmd.cmdarg = ((mmc->voltages & 0xff8000) != 0) << 8 | 0xaa;
    cmd.resp_type = MMC_RSP_R7;
    cmd.flags = 0;
    err = mmc_send_cmd(mmc, &cmd, NULL);
    if (err) {
        return err;
    }

    if ((cmd.response[0] & 0xff) != 0xaa) {
        return UNUSABLE_ERR;
    }
    mmc->version = SD_VERSION_2;
    return 0;
}

int mmc_init(struct mmc *mmc)
{
    int err = 0;

    if (mmc->has_init) {
        return 0;
    }

    err = mmc->init(mmc);
    if (err) {
        return err;
    }
    mmc_set_bus_width(mmc, 1);
    mmc_set_clock(mmc, 1);

    err = mmc_go_idle(mmc);
    if (err) {
        return err;
    }

    mmc->part_num = 0;
    err = mmc_send_if_cond(mmc);
    err = sd_send_op_cond(mmc);
    if (err) {
        err = mmc_send_op_cond(mmc);
        if (err) {
            return UNUSABLE_ERR;
        }
    }

    err = mmc_startup(mmc);
    if (err) {
        mmc->has_init = 0;
    } 
    else {
        mmc->has_init = 1;
    }
    return err;
}

int mmc_register(int dev_num, struct mmc *mmc)
{
    mmc_devices[dev_num] = mmc;

    if (!mmc->b_max) {
        mmc->b_max = 4096;
    }
    return mmc_init(mmc);
}

int mmc_unregister(int dev_num)
{
    mmc_devices[dev_num] = NULL;
    return 0;
}
