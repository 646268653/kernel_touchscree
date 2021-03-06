/*
 *drivers/input/touchscreen/ft5x06_ex_fun.c
 *
 *FocalTech IC driver expand function for debug.
 *
 *Copyright (c) 2010  Focal tech Ltd.
 *
 *This software is licensed under the terms of the GNU General Public
 *License version 2, as published by the Free Software Foundation, and
 *may be copied, distributed, and modified under those terms.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *
 *Note:the error code of EIO is the general error in this file.
 */

#include "../tpd.h"

#include "tpd_custom_fts.h"

#include "focaltech_ex_fun.h"

#include <linux/netdevice.h>
#include <linux/mount.h>
//#include <linux/netdevice.h>
#include <linux/proc_fs.h>

#ifdef FTS_MCAP_TEST
#include "self-test/mcap_5x46_test_lib.h"
#include "mcap_5x46_test_config.h"

extern struct tpd_device *tpd;
extern struct i2c_client *g_focalclient;
#endif
/*******************************************************************************/
#define VENDOR_ID_DEFAULT               0x79
#define VENDOR_ID_BROKEN                0x00
#define VENDOR_ID_OFILM                 0x51
#define LCM_ID_AUO_B            0x12
#define LCM_ID_AUO_W            0x11
#define LCM_ID_SHARP_B          0x22
#define LCM_ID_SHARP_W          0x21

extern struct tpd_device *tpd; //linan
//extern int tp_type_flag; //modify_lwf_del
/*auto upgrade*/
struct CtpInfo
{
    u8  vendor_id;
    u8  lcm_id;
    u8  fw_version;
};

struct CtpFwInfo
{
    u8  vendor_id;
    u8  lcm_id;
    u8  *fw_data;
    u32 fw_length;
};

static struct CtpInfo cur_tp_info;

//upgrade app
//start:modified by fae for FT5346 20151113
static unsigned char aucFW_APP_00[] =
{
  #include "FT3617_BOE_8WX_V0x10_20160709_app.i"
};

static unsigned char aucFW_APP_01[] =
{
  #include "FT3617_BOE_8WX_V0x10_20160709_app.i"
};

struct CtpFwInfo ctp_fw_app[] =
{
    {0x79, 0x00, aucFW_APP_00, sizeof(aucFW_APP_00)},
    {0x79, 0x01, aucFW_APP_01, sizeof(aucFW_APP_01)},
};

/*******************************************************************************/
u8 *I2CDMABuf_va = NULL;
u64 I2CDMABuf_pa = NULL;

extern struct Upgrade_Info fts_updateinfo_curr;

/*******************************************************************************/
int fts_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf, u32 dw_lenth);

extern void mt_eint_unmask(unsigned int line);
extern void mt_eint_mask(unsigned int line);


static DEFINE_MUTEX(g_device_mutex);
#ifdef TPD_AUTO_DOWNLOAD
int  fts_ctpm_fw_preupgrade_hwreset(struct i2c_client * client, u8* pbt_buf, u32 dw_lenth);
#endif

/*******************************************************************************/
int fts_hidi2c_to_stdi2c(struct i2c_client * client)
{
    u8 auc_i2c_write_buf[10] = {0};
    u8 reg_val[10] = {0};
    int iRet = 0;

    auc_i2c_write_buf[0] = 0xEB;
    auc_i2c_write_buf[1] = 0xAA;
    auc_i2c_write_buf[2] = 0x09;

    reg_val[0] = reg_val[1] =  reg_val[2] = 0x00;

    iRet = fts_i2c_Write(client, auc_i2c_write_buf, 3);

    msleep(10);
    iRet = fts_i2c_Read(client, auc_i2c_write_buf, 0, reg_val, 3);

    FTS_DBG("Change to STDI2cValue,REG1 = 0x%x,REG2 = 0x%x,REG3 = 0x%x, iRet=%d\n",
            reg_val[0], reg_val[1], reg_val[2], iRet);

    if (reg_val[0] == 0xEB
        && reg_val[1] == 0xAA
        && reg_val[2] == 0x08)
    {
        pr_info("fts_hidi2c_to_stdi2c successful.\n");
        iRet = 1;
    }
    else
    {
        pr_info("fts_hidi2c_to_stdi2c error.\n");
        iRet = 0;
    }

    return iRet;
}

DEFINE_MUTEX(g_ft_i2c_lock);

/*
*fts_i2c_Read-read data and write data by i2c
*@client: handle of i2c
*@writebuf: Data that will be written to the slave
*@writelen: How many bytes to write
*@readbuf: Where to store data read from slave
*@readlen: How many bytes to read
*
*Returns negative errno, else the number of messages executed
*
*
*/
int fts_i2c_Read(struct i2c_client *client, u8 *writebuf,
                 int writelen, u8 *readbuf, int readlen)
{
    int ret, i;

    mutex_lock(&g_ft_i2c_lock);
    if (writelen > 0)
    {
        //DMA Write
        {
            for (i = 0 ; i < writelen; i++)
            {
                I2CDMABuf_va[i] = writebuf[i];
            }

            client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;

            if ((ret = i2c_master_send(client, I2CDMABuf_pa, writelen)) != writelen)
            {
                FTS_DBG( "###%s i2c write len=%x\n", __func__, ret);
            }
            //MSE_ERR("Sensor dma timing is %x!\r\n", this_client->timing);
            //return ret;
            client->addr = client->addr & I2C_MASK_FLAG & (~ I2C_DMA_FLAG);

        }
    }
    //DMA Read
    if (readlen > 0)
    {
        {

            client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;
            ret = i2c_master_recv(client, I2CDMABuf_pa, readlen);

            for (i = 0; i < readlen; i++)
            {
                readbuf[i] = I2CDMABuf_va[i];
            }
            client->addr = client->addr & I2C_MASK_FLAG & (~ I2C_DMA_FLAG);

        }
    }
    mutex_unlock(&g_ft_i2c_lock);
    return ret;
}
/*write data by i2c*/

int fts_i2c_Write(struct i2c_client *client, u8 *writebuf, int writelen)
{
    int ret;
    int i = 0;

    mutex_lock(&g_ft_i2c_lock);
    if (writelen > 0)
    {
        for (i = 0 ; i < writelen; i++)
        {
            I2CDMABuf_va[i] = writebuf[i];
        }

        client->addr = client->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;

        if ((ret = i2c_master_send(client, I2CDMABuf_pa, writelen)) != writelen)
        {
            FTS_DBG( "###%s i2c write len=%x\n", __func__, ret);
        }
        //MSE_ERR("Sensor dma timing is %x!\r\n", this_client->timing);
        client->addr = client->addr & I2C_MASK_FLAG & (~ I2C_DMA_FLAG);

    }
    mutex_unlock(&g_ft_i2c_lock);
    return ret;

}

void fts_tp_power_reset(void)
{
    #ifdef TPD_POWER_SOURCE_CUSTOM
    hwPowerDown(TPD_POWER_SOURCE_CUSTOM, "TP");
    #else
    hwPowerDown(TPD_POWER_SOURCE, "TP");
    #endif

    mdelay(5);
    #ifdef TPD_POWER_SOURCE_CUSTOM
    hwPowerOn(TPD_POWER_SOURCE_CUSTOM, VOL_2800, "TP");
    #else
    hwPowerOn(TPD_POWER_SOURCE, VOL_3300, "TP");
    #endif

    mdelay(300);
}

#ifdef FTS_MCAP_TEST
int focal_i2c_Read(unsigned char *writebuf,
                   int writelen, unsigned char *readbuf, int readlen)
{
    int ret;
    int i = 0;

    mutex_lock(&g_ft_i2c_lock);
    if (writelen > 0)
    {
        {
            for (i = 0 ; i < writelen; i++)
            {
                I2CDMABuf_va[i] = writebuf[i];
            }

            g_focalclient->addr = g_focalclient->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;

            if ((ret = i2c_master_send(g_focalclient, I2CDMABuf_pa, writelen)) != writelen)
            {
                FTS_DBG( "###%s i2c write len=%x\n", __func__, ret);
            }
            //MSE_ERR("Sensor dma timing is %x!\r\n", this_client->timing);
            //return ret;
            g_focalclient->addr = g_focalclient->addr & I2C_MASK_FLAG & (~ I2C_DMA_FLAG);

        }

    }

    if (readlen > 0)
    {
        {

            g_focalclient->addr = g_focalclient->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;
            ret = i2c_master_recv(g_focalclient, I2CDMABuf_pa, readlen);

            for (i = 0; i < readlen; i++)
            {
                readbuf[i] = I2CDMABuf_va[i];
            }
            g_focalclient->addr = g_focalclient->addr & I2C_MASK_FLAG & (~ I2C_DMA_FLAG);

        }
    }
    mutex_unlock(&g_ft_i2c_lock);
    return ret;
}
/*write data by i2c*/
int focal_i2c_Write(unsigned char *writebuf, int writelen)
{
    int ret;
    int i = 0;

    mutex_lock(&g_ft_i2c_lock);
    if (writelen > 0)
    {
        for (i = 0 ; i < writelen; i++)
        {
            I2CDMABuf_va[i] = writebuf[i];
        }

        g_focalclient->addr = g_focalclient->addr & I2C_MASK_FLAG | I2C_DMA_FLAG;

        if ((ret = i2c_master_send(g_focalclient, I2CDMABuf_pa, writelen)) != writelen)
        {
            FTS_DBG( "###%s i2c write err\n", __func__);
        }
        //MSE_ERR("Sensor dma timing is %x!\r\n", this_client->timing);
        g_focalclient->addr = g_focalclient->addr & I2C_MASK_FLAG & (~ I2C_DMA_FLAG);

    }
    mutex_unlock(&g_ft_i2c_lock);
    return ret;
}
#endif

int fts_write_reg(struct i2c_client *client, u8 regaddr, u8 regvalue)
{
    unsigned char buf[2] = {0};
    buf[0] = regaddr;
    buf[1] = regvalue;

    return fts_i2c_Write(client, buf, sizeof(buf));
}


int fts_read_reg(struct i2c_client *client, u8 regaddr, u8 *regvalue)
{
    return fts_i2c_Read(client, &regaddr, 1, regvalue, 1);
}

void fts_ctpm_hw_reset(void)
{
    mt_set_gpio_mode(GPIO_CTP_RST_PIN, GPIO_CTP_RST_PIN_M_GPIO);
    mt_set_gpio_dir(GPIO_CTP_RST_PIN, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
    mdelay(20);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ZERO);
    mdelay(20);
    mt_set_gpio_out(GPIO_CTP_RST_PIN, GPIO_OUT_ONE);
}

int fts_ctpm_sw_reset(struct i2c_client * client)
{
    int i_ret;

    i_ret = fts_write_reg(client, 0xfc, FT_UPGRADE_AA);

    if (i_ret > 0)
    {
        msleep(fts_updateinfo_curr.delay_aa);

        i_ret = fts_write_reg(client, 0xfc, FT_UPGRADE_55);
    }

    return i_ret;
}

int fts_ctpm_auto_clb(struct i2c_client *client)
{
    unsigned char uc_temp = 0x00;
    unsigned char i = 0;

    /*start auto CLB */
    msleep(200);

    fts_write_reg(client, 0, FTS_FACTORYMODE_VALUE);
    /*make sure already enter factory mode */
    msleep(100);
    /*write command to start calibration */
    fts_write_reg(client, 2, 0x4);
    msleep(300);
    if ((fts_updateinfo_curr.CHIP_ID == 0x11)
        || (fts_updateinfo_curr.CHIP_ID == 0x12)
        || (fts_updateinfo_curr.CHIP_ID == 0x13)
        || (fts_updateinfo_curr.CHIP_ID == 0x14)) //5x36,5x36i
    {
        for (i = 0; i < 100; i++)
        {
            fts_read_reg(client, 0x02, &uc_temp);
            if (0x02 == uc_temp ||
                0xFF == uc_temp)
            {
                /*if 0x02, then auto clb ok, else 0xff, auto clb failure*/
                break;
            }
            msleep(20);
        }
    }
    else
    {
        for (i = 0; i < 100; i++)
        {
            fts_read_reg(client, 0, &uc_temp);
            if (0x0 == ((uc_temp & 0x70) >> 4)) /*return to normal mode, calibration finish*/
            {
                break;
            }
            msleep(20);
        }
    }
    /*calibration OK*/
    fts_write_reg(client, 0, 0x40);  /*goto factory mode for store*/
    msleep(200);   /*make sure already enter factory mode*/
    fts_write_reg(client, 2, 0x5);  /*store CLB result*/
    msleep(300);
    fts_write_reg(client, 0, FTS_WORKMODE_VALUE);   /*return to normal mode */
    msleep(300);

    /*store CLB result OK */
    return 0;
}

/*
get app.i fw version
*/
u8 fts_ctpm_get_fw_version(u8 vendor_id, u8 lcm_id)
{
    int i = 0;
    int fw_cnt = sizeof(ctp_fw_app) / sizeof(struct CtpFwInfo);

    for (i = 0; i < fw_cnt; i++)
    {
        if ((vendor_id == ctp_fw_app[i].vendor_id)
            && (lcm_id == ctp_fw_app[i].lcm_id)
            && (NULL != ctp_fw_app[i].fw_data)
            && (2 < ctp_fw_app[i].fw_length))
        {
            return ctp_fw_app[i].fw_data[ctp_fw_app[i].fw_length - 2];
        }
    }

    return 0x00;    /*default value */
}

int fts_ctpm_get_fw_data(u8 vendor_id, u8 lcm_id, u8 **fw_data, u32 *fw_length)
{
    int i = 0;
    int fw_cnt = sizeof(ctp_fw_app) / sizeof(struct CtpFwInfo);

    if ((fw_data == NULL) || (fw_length == NULL))
    {
        return -EIO;
    }

    for (i = 0; i < fw_cnt; i++)
    {
        if ((vendor_id == ctp_fw_app[i].vendor_id)
            && (lcm_id == ctp_fw_app[i].lcm_id))
        {
            *fw_data = ctp_fw_app[i].fw_data;
            *fw_length = ctp_fw_app[i].fw_length;

            return 0;
        }
    }

    return -EIO;
}

int fts_ctpm_read_ctp_info_from_app(struct i2c_client *client,
                                    struct CtpInfo *ctp_info)

{
    int i_ret = 0;

    i_ret = fts_read_reg(client, FT_REG_FW_VER, &(ctp_info->fw_version));
    if (i_ret < 0)
        pr_err("rad FT_REG_FW_VER failed, ret: %d value: %x\n",
               i_ret, ctp_info->fw_version);

    i_ret = fts_read_reg(client, FT_REG_VENDOR_ID, &(ctp_info->vendor_id));
    if (i_ret < 0)
        pr_err("rad FT_REG_VENDOR_ID failed, ret: %d value: %x\n",
               i_ret, ctp_info->vendor_id);

    i_ret = fts_read_reg(client, FT_REG_LCM_ID, &(ctp_info->lcm_id));
    if (i_ret < 0)
        pr_err("read FT_REG_LCM_ID failed, ret: %d value: %x\n",
               i_ret, ctp_info->lcm_id);
//TPD_DMESG("[FTS] fts_ctpm_read_ctp_info_from_app  end...\r\n ");//linan
TPD_DMESG("[FTS] chenjg fts_ctpm_read_ctp_info_from_app: fw_version = %x,vendor_id=%x,lcm_id=%x", ctp_info->fw_version,ctp_info->vendor_id,ctp_info->lcm_id); //add by fae debug 20151116


    return i_ret;
}

int  fts_ctpm_read_ctp_info_from_flash(struct i2c_client * client,
                                       struct CtpInfo* ctp_info)
{
    u8 reg_val[4] = {0};
    u8 auc_i2c_write_buf[10] = {0};
    int i = 0, i_ret = 0, is_need_reset = 0;
TPD_DMESG("[FTS] fts_ctpm_read_ctp_info_from_flash  enter...\r\n "); //linan
    do
    {

        if (ctp_info == NULL)
        {
            return -EIO;
        }

        i_ret = fts_hidi2c_to_stdi2c(client);

        for (i = 0; i < FTS_UPGRADE_LOOP; i++)
        {
            /*********Step 1:Reset  CTPM *****/
            FTS_DBG("[FTS] Step 1:Reset  CTPM\n");

            #if 0//def TPD_HW_REST
            fts_ctpm_hw_reset();
            #else
            /*write 0xaa to register 0xfc */
            fts_write_reg(client, 0xfc, FT_UPGRADE_AA);
            msleep(fts_updateinfo_curr.delay_aa);

            /*write 0x55 to register 0xfc */
            fts_write_reg(client, 0xfc, FT_UPGRADE_55);
            #endif

            if (i <= 15)
            {
                msleep(fts_updateinfo_curr.delay_55 + i * 3);
            }
            else
            {
                msleep(fts_updateinfo_curr.delay_55 - (i - 15) * 2);
            }

            /*********Step 2:Enter upgrade mode *****/
            FTS_DBG("[FTS] Step 2:Enter upgrade mode \n");

            i_ret = fts_hidi2c_to_stdi2c(client);
            msleep(5);

            auc_i2c_write_buf[0] = FT_UPGRADE_55;
            fts_i2c_Write(client, auc_i2c_write_buf, 1);
            msleep(5);
            auc_i2c_write_buf[0] = FT_UPGRADE_AA;
            fts_i2c_Write(client, auc_i2c_write_buf, 1);

            /*********Step 3:check READ-ID***********************/
            msleep(fts_updateinfo_curr.delay_readid);
            auc_i2c_write_buf[0] = 0x90;
            auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
            fts_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);

            pr_warn("[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n", reg_val[0], reg_val[1]);
            if (reg_val[0] == fts_updateinfo_curr.upgrade_id_1
                && reg_val[1] != 0)
            {
                FTS_DBG("[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
                        reg_val[0], reg_val[1]);
                break;
            }
            else
            {
                FTS_DBG( "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
                         reg_val[0], reg_val[1]);

                continue;
            }
        }

        if (i >= FTS_UPGRADE_LOOP )
        {
            if ((reg_val[0] != 0x00) || (reg_val[1] != 0x00))
            {
                FTS_DBG( "[FTS] Step 3: CTPM ID error, break.\n");
                is_need_reset = 1;
            }
            break;
        }

        /*********Step 4: read vendor id from app param area***********************/
        FTS_DBG("[FTS] Step 4: read vendor id from flash\n");
        msleep(10);

        auc_i2c_write_buf[0] = 0x03;
        auc_i2c_write_buf[1] = 0x00;

        for (i = 0; i < FTS_UPGRADE_LOOP; i++)
        {
            auc_i2c_write_buf[2] = 0xd7;
            auc_i2c_write_buf[3] = 0x86;
            reg_val[0] = reg_val[1] = 0x00;

            i_ret = fts_i2c_Write(client, auc_i2c_write_buf, 4);
            if (i_ret < 0)
            {
                pr_err( "[FTS] Step 4: read lcm id from flash error when i2c write, i_ret = %d\n", i_ret);
                continue;
            }

            msleep(5);

            i_ret = fts_i2c_Read(client, auc_i2c_write_buf, 0, reg_val, 2);
            if (i_ret < 0)
            {
                pr_err( "[FTS] Step 4: read lcm id from flash error when i2c read, i_ret = %d\n", i_ret);
                continue;
            }

            ctp_info->lcm_id = reg_val[0];
            pr_warn("[FTS] Step 4: lcm id = 0x%x\n", ctp_info->lcm_id);

            auc_i2c_write_buf[2] = 0xd7;
            auc_i2c_write_buf[3] = 0x84;
            reg_val[0] = reg_val[1] = 0x00;

            i_ret = fts_i2c_Write(client, auc_i2c_write_buf, 4);
            if (i_ret < 0)
            {
                pr_err( "[FTS] Step 4: read vendor id from flash error when i2c write, i_ret = %d\n", i_ret);
                continue;
            }

            msleep(5);

            i_ret = fts_i2c_Read(client, auc_i2c_write_buf, 0, reg_val, 2);
            if (i_ret < 0)
            {
                pr_err( "[FTS] Step 4: read vendor id from flash error when i2c read, i_ret = %d\n", i_ret);
                continue;
            }

            ctp_info->vendor_id = reg_val[0];

            pr_warn("[FTS] Step 4: vendor id = 0x%x\n", ctp_info->vendor_id);
            break;
        }

        msleep(50);

        is_need_reset = 1;

    }
    while (0);

    /*********Step 5: reset the new FW***********************/
    if (is_need_reset)
    {
        pr_warn("Step 5: reset the FW\n");
        fts_ctpm_hw_reset();
        msleep(200);   //make sure CTP startup normally

        i_ret = fts_hidi2c_to_stdi2c(client);//Android to Std i2c.
        if (i_ret == 0)
        {
            FTS_DBG("HidI2c change to StdI2c i_ret = %d ! \n", i_ret);
        }

        msleep(10);
    }

	TPD_DMESG("[FTS] chenjg fts_ctpm_read_ctp_info_from_flash: fw_version = %x,vendor_id=%x,lcm_id=%x", ctp_info->fw_version,ctp_info->vendor_id,ctp_info->lcm_id); //add by fae debug 20151116

    if (i == FTS_UPGRADE_LOOP)
    {
        return -EIO;
    }

    return 0;
}

int fts_ctpm_fw_upgrade(struct i2c_client *client, u8 *pbt_buf, u32 dw_lenth)
{
    u8 reg_val[4] = {0};
    u32 i = 0;
    u8 is_5336_new_bootloader = 0;
    u8 is_5336_fwsize_30 = 0;
    u32 packet_number;
    u32 j = 0;
    u32 temp;
    u32 lenght;
    u8 packet_buf[FTS_PACKET_LENGTH + 6];
    u8 auc_i2c_write_buf[10];
    u8 bt_ecc;
    int i_ret;
    int reflash_flag = 0;
    u8 uc_tp_fm_ver;
    FTS_DBG("********************Enter CTP Auto fts_ctpm_fw_upgrade********************\n");

ft_fw_reflash_1:
    i_ret = fts_hidi2c_to_stdi2c(client);

    for (i = 0; i < FTS_UPGRADE_LOOP; i++)
    {
        msleep(100);

        pr_warn("[FTS] Upgrade Step 1:Reset  CTPM\n");

        /*********Step 1:Reset  CTPM *****/
        /*write 0xaa to register 0xfc */
        i_ret = fts_write_reg(client, 0xfc, FT_UPGRADE_AA);
       // msleep(fts_updateinfo_curr.delay_aa);
		msleep(10);

        /*write 0x55 to register 0xfc */
        i_ret |= fts_write_reg(client, 0xfc, FT_UPGRADE_55);
        msleep(200);
        if (i_ret < 0)
        {
            pr_err("[FTS] Upgrade Step 1 reset i2c transfer error\n");
        }
/*
        if (i <= FTS_UPGRADE_LOOP / 2)
        {
            msleep(fts_updateinfo_curr.delay_55 + i * 3);
        }
        else
        {
            msleep(fts_updateinfo_curr.delay_55 - (i - 15) * 2);
        }
*/
        /*********Step 2:Enter upgrade mode *****/
        pr_warn("[FTS] Upgrade Step 2:Enter upgrade mode \n");

        i_ret = fts_hidi2c_to_stdi2c(client);

		msleep(5);
        auc_i2c_write_buf[0] = FT_UPGRADE_55;
      //  i_ret = fts_i2c_Write(client, auc_i2c_write_buf, 1);
        //msleep(5);
        auc_i2c_write_buf[0] = FT_UPGRADE_AA;
        i_ret = fts_i2c_Write(client, auc_i2c_write_buf, 2);

        if (i_ret < 0)
        {
            pr_err("[FTS] Upgrade Step 1 reset i2c transfer error\n");
        }

        /*********Step 3:check READ-ID***********************/
        msleep(fts_updateinfo_curr.delay_readid);
		
        auc_i2c_write_buf[0] = 0x90;
        auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
        fts_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);

        pr_warn("[FTS] Upgrade Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n", reg_val[0], reg_val[1]);
        if (reg_val[0] == fts_updateinfo_curr.upgrade_id_1
            && reg_val[1] == fts_updateinfo_curr.upgrade_id_2)
        {
            pr_err("[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
                   reg_val[0], reg_val[1]);
            break;
        }
        else
        {
            pr_err("[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
                   reg_val[0], reg_val[1]);
            continue;
        }
    }

    if (i >= FTS_UPGRADE_LOOP)
    {
        return -EIO;
    }

ft_fw_reflash_4:
    pr_warn("[FTS] Step 4:erase app and panel paramenter area\n");
    /*Step 4:erase app and panel paramenter area*/
    pr_warn("Step 4:erase app and panel paramenter area\n");
    auc_i2c_write_buf[0] = 0x61;
    i_ret = fts_i2c_Write(client, auc_i2c_write_buf, 1);    /*erase app area */
    msleep(fts_updateinfo_curr.delay_earse_flash);
    /*erase panel parameter area */

    for (i = 0; i < 15; i++)
    {
        auc_i2c_write_buf[0] = 0x6a;
        reg_val[0] = reg_val[1] = 0x00;
        i_ret |= fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 2);

        if (0xF0 == reg_val[0] && 0xAA == reg_val[1])
        {
            break;
        }
        msleep(50);
    }

    //write bin file length to FW bootloader.
    auc_i2c_write_buf[0] = 0xB0;
    auc_i2c_write_buf[1] = (u8) ((dw_lenth >> 16) & 0xFF);
    auc_i2c_write_buf[2] = (u8) ((dw_lenth >> 8) & 0xFF);
    auc_i2c_write_buf[3] = (u8) (dw_lenth & 0xFF);

    i_ret |= fts_i2c_Write(client, auc_i2c_write_buf, 4);

    if (i_ret < 0)
    {
        pr_err("[FTS] Upgrade Step 4 i2c transfer error\n");
    }

    pr_warn("[FTS] Step 5:write firmware(FW) to ctpm flash\n");
    /*********Step 5:write firmware(FW) to ctpm flash*********/
    bt_ecc = 0;
    pr_warn("Step 5:write firmware(FW) to ctpm flash\n");

    //dw_lenth = dw_lenth - 8;
    temp = 0;
    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
    packet_buf[0] = 0xbf;
    packet_buf[1] = 0x00;
    i_ret = 0;

    for (j = 0; j < packet_number; j++)
    {
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (u8) (temp >> 8);
        packet_buf[3] = (u8) temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (u8) (lenght >> 8);
        packet_buf[5] = (u8) lenght;

        for (i = 0; i < FTS_PACKET_LENGTH; i++)
        {
            packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
            bt_ecc ^= packet_buf[6 + i];
        }
        i_ret |= fts_i2c_Write(client, packet_buf, FTS_PACKET_LENGTH + 6);
        //msleep(10);

        for (i = 0; i < 30; i++)
        {
            auc_i2c_write_buf[0] = 0x6a;
            reg_val[0] = reg_val[1] = 0x00;
            i_ret |= fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 2);

            if ((j + 0x1000) == (((reg_val[0]) << 8) | reg_val[1]))
            {
                break;
            }
            msleep(1);

        }
    }

    if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
    {
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (u8) (temp >> 8);
        packet_buf[3] = (u8) temp;
        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        packet_buf[4] = (u8) (temp >> 8);
        packet_buf[5] = (u8) temp;

        for (i = 0; i < temp; i++)
        {
            packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
            bt_ecc ^= packet_buf[6 + i];
        }
        i_ret |= fts_i2c_Write(client, packet_buf, temp + 6);

        for (i = 0; i < 30; i++)
        {
            auc_i2c_write_buf[0] = 0x6a;
            reg_val[0] = reg_val[1] = 0x00;
            i_ret |= fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 2);

            if ((j + 0x1000) == (((reg_val[0]) << 8) | reg_val[1]))
            {
                break;
            }
            msleep(1);

        }
    }

    if (i_ret < 0)
    {
        pr_err("[FTS] Upgrade Step 5 i2c transfer error\n");
    }
    msleep(50);


    pr_warn("[FTS] Upgrade Step 6: read out checksum\n");
    /*********Step 6: read out checksum***********************/
    /*send the opration head */
    auc_i2c_write_buf[0] = 0x64;
    fts_i2c_Write(client, auc_i2c_write_buf, 1);
    msleep(300);

    temp = 0;
    auc_i2c_write_buf[0] = 0x65;
    auc_i2c_write_buf[1] = (u8)(temp >> 16);
    auc_i2c_write_buf[2] = (u8)(temp >> 8);
    auc_i2c_write_buf[3] = (u8)(temp);
    temp = dw_lenth;
    auc_i2c_write_buf[4] = (u8)(temp >> 8);
    auc_i2c_write_buf[5] = (u8)(temp);
    i_ret = fts_i2c_Write(client, auc_i2c_write_buf, 6);
    msleep(dw_lenth / 256);

    for (i = 0; i < 100; i++)
    {
        auc_i2c_write_buf[0] = 0x6a;
        reg_val[0] = reg_val[1] = 0x00;
        fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 2);

        if (0xF0 == reg_val[0] && 0x55 == reg_val[1])
        {
            break;
        }
        msleep(1);

    }

    auc_i2c_write_buf[0] = 0x66;
    fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);
    if (reg_val[0] != bt_ecc)
    {
        pr_err("[FTS] Upgrade Step 6: ecc error! FW=%02x bt_ecc=%02x\n",
               reg_val[0],
               bt_ecc);
        if (reflash_flag)
        {
            pr_err("[FTS] check ecc failed in twice, abort\n");
            return -EIO;
        }
        else
        {
            reflash_flag++;
            pr_err("[FTS] check ecc error, reflash again\n");
            goto ft_fw_reflash_4;
        }
    }


    pr_warn("[FTS] Upgrade Step 7: reset the new FW\n");
    /*********Step 7: reset the new FW***********************/
    auc_i2c_write_buf[0] = 0x07;
    fts_i2c_Write(client, auc_i2c_write_buf, 1);
    msleep(300);    /*make sure CTP startup normally */

    //fts_tp_power_reset();
    fts_read_reg(client, FT_REG_FW_VER, &uc_tp_fm_ver);

    if (pbt_buf[dw_lenth - 2] != uc_tp_fm_ver)
    {
        pr_err("[FTS] Upgrade Step 7 reset failed, %d\n", reflash_flag);
        if (reflash_flag < 4)
        {
            reflash_flag++;
            goto ft_fw_reflash_1;
        }
        else
        {
            pr_err("[FTS] reset error, abort\n");
            return -EIO;
        }
    }

    i_ret = fts_hidi2c_to_stdi2c(client);//Android to Std i2c.

    return 0;
}

int fts_ctpm_fw_upgrade_with_i_file(struct i2c_client *client,
                                    u8 *fw_data, u32 fw_len)
{
    int i_ret;

    /*judge the fw that will be upgraded
    * if illegal, then stop upgrade and return.
    */
    do
    {
        if (fw_data == NULL)
        {
            pr_err( "%s:FW data = NULL error\n", __func__);
            i_ret = -EIO;
            break;
        }

        if (fw_len < FTS_MIN_FW_LENGTH || fw_len > FTS_APP_FW_LENGTH)
        {
            pr_err( "%s:FW length error\n", __func__);
            i_ret = -EIO;
            break;
        }

        /*call the upgrade function */
        i_ret = fts_ctpm_fw_upgrade(client, fw_data, fw_len);

        if (i_ret != 0)
        {
            pr_err( "%s:upgrade failed. err.\n",
                    __func__);

            break;
        }
        else if (fts_updateinfo_curr.AUTO_CLB == AUTO_CLB_NEED)
        {
            fts_ctpm_auto_clb(client);  /*start auto CLB */
        }

    }
    while (0);

    return i_ret;
}


int  fts_ctpm_fw_preupgrade_hwreset(struct i2c_client * client, u8* pbt_buf, u32 dw_lenth)
{
    u8 reg_val[4] = {0};
    u32 i = 0;
    u32 packet_number;
    u32 j;
    u32 temp;
    u32 lenght;
    u8 packet_buf[FTS_PACKET_LENGTH + 6];
    u8 auc_i2c_write_buf[10];
    u8 bt_ecc;
    int i_ret;

    for (i = 0; i < FTS_UPGRADE_LOOP; i++)
    {
        /*********Step 1:Reset  CTPM *****/
        fts_ctpm_hw_reset();

        if (i <= 15)
        {
            msleep(fts_updateinfo_curr.delay_55 + i * 3);
        }
        else
        {
            msleep(fts_updateinfo_curr.delay_55 - (i - 15) * 2);
        }

        /*********Step 2:Enter upgrade mode *****/
        auc_i2c_write_buf[0] = FT_UPGRADE_55;
        i_ret = fts_i2c_Write(client, auc_i2c_write_buf, 1);
        if (i_ret < 0)
        {
            pr_err("[FTS] failed writing  0x55 ! \n");
            continue;
        }

        auc_i2c_write_buf[0] = FT_UPGRADE_AA;
        i_ret = fts_i2c_Write(client, auc_i2c_write_buf, 1);
        if (i_ret < 0)
        {
            pr_err("[FTS] failed writing  0xaa ! \n");
            continue;
        }

        /*********Step 3:check READ-ID***********************/
        msleep(1);
        auc_i2c_write_buf[0] = 0x90;
        auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] =
                                   0x00;
        reg_val[0] = reg_val[1] = 0x00;

        fts_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);

        if (reg_val[0] == 0x54
            && reg_val[1] == 0x22)
        {
            i_ret = 0x00;
            fts_read_reg(client, 0xd0, &i_ret);

            if (i_ret == 0)
            {
                pr_err("[FTS] Step 3: READ State fail \n");
                continue;
            }

            pr_warn("[FTS] Step 3: i_ret = %d \n", i_ret);


            pr_warn("[FTS] Step 3: READ CTPM ID OK,ID1 = 0x%x,ID2 = 0x%x\n",
                    reg_val[0], reg_val[1]);

            break;
        }
        else
        {
            dev_err(&client->dev, "[FTS] Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
                    reg_val[0], reg_val[1]);

            continue;
        }
    }

    if (i >= FTS_UPGRADE_LOOP )
    {
        return -EIO;
    }

    /*********Step 4:write firmware(FW) to ctpm flash*********/
    bt_ecc = 0;
    pr_warn("Step 5:write firmware(FW) to ctpm flash\n");

    dw_lenth = dw_lenth - 8;
    temp = 0;
    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
    packet_buf[0] = 0xae;
    packet_buf[1] = 0x00;

    pr_warn("packet_number: %d\n", packet_number);
    for (j = 0; j < packet_number; j++)
    {
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (u8) (temp >> 8);
        packet_buf[3] = (u8) temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (u8) (lenght >> 8);
        packet_buf[5] = (u8) lenght;

        for (i = 0; i < FTS_PACKET_LENGTH; i++)
        {
            packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
            bt_ecc ^= packet_buf[6 + i];
        }
        fts_i2c_Write(client, packet_buf, FTS_PACKET_LENGTH + 6);
    }

    if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
    {
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (u8) (temp >> 8);
        packet_buf[3] = (u8) temp;
        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        packet_buf[4] = (u8) (temp >> 8);
        packet_buf[5] = (u8) temp;

        for (i = 0; i < temp; i++)
        {
            packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
            bt_ecc ^= packet_buf[6 + i];
        }
        fts_i2c_Write(client, packet_buf, temp + 6);
    }

    temp = FT_APP_INFO_ADDR;
    packet_buf[2] = (u8) (temp >> 8);
    packet_buf[3] = (u8) temp;
    temp = 8;
    packet_buf[4] = (u8) (temp >> 8);
    packet_buf[5] = (u8) temp;
    for (i = 0; i < 8; i++)
    {
        packet_buf[6 + i] = pbt_buf[dw_lenth + i];
        bt_ecc ^= packet_buf[6 + i];
    }
    fts_i2c_Write(client, packet_buf, 6 + 8);

    /*********Step 5: read out checksum***********************/
    /*send the opration head */
    pr_warn("Step 6: read out checksum\n");
    auc_i2c_write_buf[0] = 0xcc;
    //msleep(2);
    fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);
    if (reg_val[0] != bt_ecc)
    {
        dev_err(&client->dev, "[FTS]--ecc error! FW=%02x bt_ecc=%02x\n",
                reg_val[0],
                bt_ecc);
        return -EIO;
    }
    pr_warn("checksum %X %X \n", reg_val[0], bt_ecc);
    pr_warn("Read flash and compare\n");

    msleep(50);

    /*********Step 6: start app***********************/
    pr_warn("Step 6: start app\n");
    auc_i2c_write_buf[0] = 0x08;
    fts_i2c_Write(client, auc_i2c_write_buf, 1);
    msleep(20);

    return 0;
}

#ifdef TPD_AUTO_DOWNLOAD
int fts_ctpm_fw_download(struct i2c_client *client, u8 *pbt_buf, u32 dw_lenth)
{
    u8 reg_val[4] = {0};
    u32 i = 0;
    u8 is_5336_new_bootloader = 0;
    u8 is_5336_fwsize_30 = 0;
    u32 packet_number;
    u32 j = 0;
    u32 temp;
    u32 lenght;
    u8 packet_buf[FTS_PACKET_LENGTH + 6];
    u8 auc_i2c_write_buf[10];
    u8 bt_ecc;
    int i_ret;

    for (i = 0; i < FTS_UPGRADE_LOOP; i++)
    {
        msleep(100);

        pr_warn("[FTS] Download Step 1:Reset  CTPM\n");
        /*********Step 1:Reset  CTPM *****/

        /*write 0xaa to register 0xfc */
        fts_write_reg(client, 0xfc, FT_UPGRADE_AA);
        msleep(fts_updateinfo_curr.delay_aa);

        /*write 0x55 to register 0xfc */
        fts_write_reg(client, 0xfc, FT_UPGRADE_55);

        if (i <= 15)
        {
            msleep(fts_updateinfo_curr.delay_55 + i * 3);
        }
        else
        {
            msleep(fts_updateinfo_curr.delay_55 - (i - 15) * 2);
        }

        pr_warn("[FTS] Download Step 2:Enter upgrade mode \n");
        /*********Step 2:Enter upgrade mode *****/

        auc_i2c_write_buf[0] = FT_UPGRADE_55;
        fts_i2c_Write(client, auc_i2c_write_buf, 1);
        msleep(5);
        auc_i2c_write_buf[0] = FT_UPGRADE_AA;
        fts_i2c_Write(client, auc_i2c_write_buf, 1);

        /*********Step 3:check READ-ID***********************/
        msleep(fts_updateinfo_curr.delay_readid);
        auc_i2c_write_buf[0] = 0x90;
        auc_i2c_write_buf[1] = auc_i2c_write_buf[2] = auc_i2c_write_buf[3] = 0x00;
        fts_i2c_Read(client, auc_i2c_write_buf, 4, reg_val, 2);

        pr_warn("[FTS] Download Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n", reg_val[0], reg_val[1]);
        if (reg_val[0] == fts_updateinfo_curr.download_id_1
            && reg_val[1] == fts_updateinfo_curr.download_id_2)
        {
            pr_err("[FTS] Download Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
                   reg_val[0], reg_val[1]);
            break;
        }
        else
        {
            pr_err("[FTS] Download Step 3: CTPM ID,ID1 = 0x%x,ID2 = 0x%x\n",
                   reg_val[0], reg_val[1]);

            continue;
        }
    }

    if (i >= FTS_UPGRADE_LOOP)
    {
        if (reg_val[0] != 0x00 || reg_val[1] != 0x00)
        {
            fts_ctpm_hw_reset();
            msleep(300);
        }
        return -EIO;
    }

    pr_warn("[FTS] Download Step 4:change to write flash mode\n");
    fts_write_reg(client, 0x09, 0x0a);

    /*Step 4:erase app and panel paramenter area*/
    pr_warn("[FTS] Download Step 4:erase app and panel paramenter area\n");
    auc_i2c_write_buf[0] = 0x61;
    fts_i2c_Write(client, auc_i2c_write_buf, 1);    /*erase app area */
    msleep(fts_updateinfo_curr.delay_earse_flash);
    /*erase panel parameter area */

    for (i = 0; i < 15; i++)
    {
        auc_i2c_write_buf[0] = 0x6a;
        reg_val[0] = reg_val[1] = 0x00;
        fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 2);

        if (0xF0 == reg_val[0] && 0xAA == reg_val[1])
        {
            break;
        }
        msleep(50);
    }

    pr_warn("[FTS] Download Step 5:write firmware(FW) to ctpm flash\n");
    /*********Step 5:write firmware(FW) to ctpm flash*********/
    bt_ecc = 0;

    //dw_lenth = dw_lenth - 8;
    temp = 0;
    packet_number = (dw_lenth) / FTS_PACKET_LENGTH;
    packet_buf[0] = 0xbf;
    packet_buf[1] = 0x00;

    pr_warn("[FTS] Download Step 5:packet_number: %d\n", packet_number);
    for (j = 0; j < packet_number; j++)
    {
        temp = j * FTS_PACKET_LENGTH;
        packet_buf[2] = (u8) (temp >> 8);
        packet_buf[3] = (u8) temp;
        lenght = FTS_PACKET_LENGTH;
        packet_buf[4] = (u8) (lenght >> 8);
        packet_buf[5] = (u8) lenght;

        for (i = 0; i < FTS_PACKET_LENGTH; i++)
        {
            packet_buf[6 + i] = pbt_buf[j * FTS_PACKET_LENGTH + i];
            bt_ecc ^= packet_buf[6 + i];
        }
        fts_i2c_Write(client, packet_buf, FTS_PACKET_LENGTH + 6);
        //msleep(10);

        for (i = 0; i < 30; i++)
        {
            auc_i2c_write_buf[0] = 0x6a;
            reg_val[0] = reg_val[1] = 0x00;
            fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 2);

            if ((j + 0x1000) == (((reg_val[0]) << 8) | reg_val[1]))
            {
                break;
            }
            //msleep(1);

        }
    }

    if ((dw_lenth) % FTS_PACKET_LENGTH > 0)
    {
        temp = packet_number * FTS_PACKET_LENGTH;
        packet_buf[2] = (u8) (temp >> 8);
        packet_buf[3] = (u8) temp;
        temp = (dw_lenth) % FTS_PACKET_LENGTH;
        packet_buf[4] = (u8) (temp >> 8);
        packet_buf[5] = (u8) temp;

        for (i = 0; i < temp; i++)
        {
            packet_buf[6 + i] = pbt_buf[packet_number * FTS_PACKET_LENGTH + i];
            bt_ecc ^= packet_buf[6 + i];
        }
        fts_i2c_Write(client, packet_buf, temp + 6);

        for (i = 0; i < 30; i++)
        {
            auc_i2c_write_buf[0] = 0x6a;
            reg_val[0] = reg_val[1] = 0x00;
            fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 2);

            if ((j + 0x1000) == (((reg_val[0]) << 8) | reg_val[1]))
            {
                break;
            }
            msleep(1);

        }
    }

    msleep(50);


    pr_warn("[FTS] Download Step 6: read out checksum\n");
    /*********Step 6: read out checksum***********************/
    auc_i2c_write_buf[0] = 0x64;
    fts_i2c_Write(client, auc_i2c_write_buf, 1);
    msleep(300);

    temp = 0;
    auc_i2c_write_buf[0] = 0x65;
    auc_i2c_write_buf[1] = (u8)(temp >> 16);
    auc_i2c_write_buf[2] = (u8)(temp >> 8);
    auc_i2c_write_buf[3] = (u8)(temp);
    temp = dw_lenth;
    auc_i2c_write_buf[4] = (u8)(temp >> 8);
    auc_i2c_write_buf[5] = (u8)(temp);
    i_ret = fts_i2c_Write(client, auc_i2c_write_buf, 6);
    msleep(dw_lenth / 256);

    for (i = 0; i < 100; i++)
    {
        auc_i2c_write_buf[0] = 0x6a;
        reg_val[0] = reg_val[1] = 0x00;
        fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 2);

        if (0xF0 == reg_val[0] && 0x55 == reg_val[1])
        {
            break;
        }
        msleep(1);

    }

    auc_i2c_write_buf[0] = 0x66;
    fts_i2c_Read(client, auc_i2c_write_buf, 1, reg_val, 1);
    if (reg_val[0] != bt_ecc)
    {
        pr_err("[FTS] Download Step 6: ecc error! FW=%02x bt_ecc=%02x\n",
               reg_val[0],
               bt_ecc);

        return -EIO;
    }


    pr_warn("[FTS] Download Step 7: reset the new FW\n");
    /*********Step 7: reset the new FW***********************/

    fts_tp_power_reset();

    i_ret = fts_hidi2c_to_stdi2c(client);//Android to Std i2c.

    if (i_ret == 0)
    {
        pr_err("HidI2c change to StdI2c i_ret = %d ! \n", i_ret);
    }

    return 0;
}

int fts_ctpm_fw_download_with_i_file(struct i2c_client *client,
                                     u8 *fw_data, u32 fw_len)
{
    int i_ret;

    /*judge the fw that will be upgraded
    * if illegal, then stop upgrade and return.
    */
    do
    {
        if (fw_data == NULL)
        {
            pr_err( "%s:FW data = NULL error\n", __func__);
            i_ret = -EIO;
            break;
        }

        if (fw_len < FTS_MIN_FW_LENGTH || fw_len > FTS_ALL_FW_LENGTH)
        {
            pr_err( "%s:FW length error\n", __func__);
            i_ret = -EIO;
            break;
        }

        i_ret = fts_ctpm_fw_preupgrade_hwreset(client, aucFW_PRAM_BOOT, sizeof(aucFW_PRAM_BOOT));

        if (i_ret != 0)
        {
            pr_err( "%s:write pram failed. err.\n",
                    __func__);

            break;
        }

        /*call the upgrade function */
        i_ret = fts_ctpm_fw_download(client, fw_data, fw_len);

        if (i_ret != 0)
        {
            pr_err( "%s:upgrade failed. err.\n",
                    __func__);
        }

    }
    while (0);

    return i_ret;
}
#endif

int fts_ctpm_auto_upgrade(struct i2c_client *client)
{
    u8 *fw_data = NULL;
    u32 fw_length = 0;
    int i, i_ret;
    struct CtpInfo ctp_info = {0};
    u8 auc_i2c_write_buf;
    static bool version_check = true;
    int upgrade_ret = MZ_FAILED;

    FTS_DBG("********************Enter CTP Auto Upgrade********************\n");

    for (i = 0; i < 5; ++i)
    {        
        i_ret = fts_ctpm_read_ctp_info_from_app(client, &ctp_info);

	if((ctp_info.fw_version == 0)||(i_ret<0))
	{
		i_ret = fts_ctpm_read_ctp_info_from_flash(client, &ctp_info);
		
		FTS_DBG("[FTS] fts_ctpm_read_ctp_info_from_flash i_ret = %d \n",i_ret);//fae 20151116
		
		if(i_ret<0)
		{
			FTS_DBG("[FTS] Can not get LCM ID & vendor ID.\n");
			goto fw_exit;
		}
	 }

        FTS_DBG("[FTS]i_ret = %d, i = %d: lcm_id = 0x%x, fm_ver = 0x%x, vendor_id = 0x%x\n",
                i_ret, i, ctp_info.lcm_id,
                ctp_info.fw_version, ctp_info.vendor_id);
   
        //set tp_type
//modify_lwf_del        
/*        
	 if (ctp_info.lcm_id == 0x00)
	 {
	     tp_type_flag = 1;  //white
	 }
	 else if (ctp_info.lcm_id == 0x01)
	 {
	     tp_type_flag = 0;  //black
	 }
*/
        // get fw according to vendor_id and lcm_id
        i_ret = fts_ctpm_get_fw_data(ctp_info.vendor_id, ctp_info.lcm_id, &fw_data, &fw_length);
		
        if (i_ret != 0)
        {
            pr_err("can not find firmware using vendor_id %02x lcm_id %02x\n", ctp_info.vendor_id, ctp_info.lcm_id);
		if(i==4)
            goto fw_exit;

        }
        else
        {
            break;
        }
    }

    //version check
    if (version_check && (ctp_info.fw_version >= fw_data[fw_length - 2]))
    {
        pr_info("firmware is up-to-date ic %02x driver %02x, abort\n", ctp_info.fw_version, fw_data[fw_length - 2]);
        version_check = false;
        upgrade_ret = MZ_NO_ACTION;
        goto fw_exit;
    }

    //start upgrade
    if (i <= 5)
    {
        i_ret = fts_ctpm_fw_upgrade(client, fw_data, fw_length);
        if (i_ret < 0)
        {
            upgrade_ret = MZ_FAILED;
            pr_err("firmware upgrade failed, ret %d\n", i_ret);
        }
        else
        {
            upgrade_ret = MZ_SUCCESS;
            pr_info("***firmware upgrade success****\n");
        }
    }

fw_exit:
    // read current tp info
    fts_ctpm_read_ctp_info_from_flash(client, &cur_tp_info);
    return upgrade_ret;
}

/*sysfs debug*/

/*
*get firmware size

@firmware_name:firmware name
*note:the firmware default path is sdcard.
    if you want to change the dir, please modify by yourself.
*/
static int fts_GetFirmwareSize(char *firmware_name)
{
    struct file *pfile = NULL;
    struct inode *inode;
    unsigned long magic;
    off_t fsize = 0;
    char filepath[128];
    memset(filepath, 0, sizeof(filepath));

    sprintf(filepath, "%s", firmware_name);

    if (NULL == pfile)
    {
        pfile = filp_open(filepath, O_RDONLY, 0);
    }

    if (IS_ERR(pfile))
    {
        pr_err("error occured while opening file %s.\n", filepath);
        return -EIO;
    }

    inode = pfile->f_dentry->d_inode;
    magic = inode->i_sb->s_magic;
    fsize = inode->i_size;
    filp_close(pfile, NULL);
    return fsize;
}



/*
*read firmware buf for .bin file.

@firmware_name: fireware name
@firmware_buf: data buf of fireware

note:the firmware default path is sdcard.
    if you want to change the dir, please modify by yourself.
*/
static int fts_ReadFirmware(char *firmware_name,
                            unsigned char *firmware_buf)
{
    struct file *pfile = NULL;
    struct inode *inode;
    unsigned long magic;
    off_t fsize;
    char filepath[128];
    loff_t pos;
    mm_segment_t old_fs;

    memset(filepath, 0, sizeof(filepath));
    sprintf(filepath, "%s", firmware_name);
    if (NULL == pfile)
    {
        pfile = filp_open(filepath, O_RDONLY, 0);
    }
    if (IS_ERR(pfile))
    {
        pr_err("error occured while opening file %s.\n", filepath);
        return -EIO;
    }

    inode = pfile->f_dentry->d_inode;
    magic = inode->i_sb->s_magic;
    fsize = inode->i_size;
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    pos = 0;
    vfs_read(pfile, firmware_buf, fsize, &pos);
    filp_close(pfile, NULL);
    set_fs(old_fs);

    return 0;
}



/*
upgrade with *.bin file
*/

int fts_ctpm_fw_upgrade_with_app_file(struct i2c_client *client,
                                      char *firmware_name)
{
    u8 *pbt_buf = NULL;
    int i_ret = 0;
    int fwsize = fts_GetFirmwareSize(firmware_name);

    if (fwsize <= 0)
    {
        FTS_DBG( "%s ERROR:Get firmware size failed\n",
                 __func__);
        return -EIO;
    }

    if (fwsize < FTS_MIN_FW_LENGTH || fwsize > FTS_ALL_FW_LENGTH)
    {
        FTS_DBG( "%s:FW length error\n", __func__);
        return -EIO;
    }


    /*=========FW upgrade========================*/
    pbt_buf = kmalloc(fwsize + 1, GFP_ATOMIC);

    if (fts_ReadFirmware(firmware_name, pbt_buf))
    {
        FTS_DBG( "%s() - ERROR: request_firmware failed\n",
                 __func__);
        kfree(pbt_buf);
        //return -EIO;
        i_ret = -EIO;
        goto err_ret;
    }

    /*call the upgrade function */
    i_ret = fts_ctpm_fw_upgrade(client, pbt_buf, fwsize);
    if (i_ret != 0)
        FTS_DBG( "%s() - ERROR:[FTS] upgrade failed..\n",
                 __func__);
    else if (fts_updateinfo_curr.AUTO_CLB == AUTO_CLB_NEED)
    {
        fts_ctpm_auto_clb(client);
    }

err_ret:



    kfree(pbt_buf);

    return i_ret;
}

static u8 reg_dump_address = 0;
static u8 reg_dump_value = 0;

static ssize_t fts_tprwreg_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf)
{
    return sprintf(buf, "%02x: %02x\n", reg_dump_address, reg_dump_value);
}

static ssize_t fts_tprwreg_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
    struct i2c_client *client = container_of(dev, struct i2c_client, dev);
    ssize_t num_read_chars = 0;
    int retval;
    long unsigned int wmreg = 0;
    u8 regaddr = 0xff, regvalue = 0xff;
    u8 valbuf[5] = {0};

    memset(valbuf, 0, sizeof(valbuf));
    mutex_lock(&g_device_mutex);
    num_read_chars = count - 1;

    if (num_read_chars != 2)
    {
        if (num_read_chars != 4)
        {
            pr_info("please input 2 or 4 character\n");
            goto error_return;
        }
    }

    memcpy(valbuf, buf, num_read_chars);
    retval = strict_strtoul(valbuf, 16, &wmreg);

    if (0 != retval)
    {
        FTS_DBG( "%s() - ERROR: Could not convert the "\
                 "given input to a number." \
                 "The given input was: \"%s\"\n",
                 __func__, buf);
        goto error_return;
    }

    if (2 == num_read_chars)
    {
        /*read register*/
        regaddr = wmreg;
        if (fts_read_reg(client, regaddr, &regvalue) < 0)
            FTS_DBG( "Could not read the register(0x%02x)\n",
                     regaddr);
        else
        {
            reg_dump_address = regaddr;
            reg_dump_value = regvalue;
        }
    }
    else
    {
        regaddr = wmreg >> 8;
        regvalue = wmreg;
        if (fts_write_reg(client, regaddr, regvalue) < 0)
            FTS_DBG( "Could not write the register(0x%02x)\n",
                     regaddr);
        else
            FTS_DBG( "Write 0x%02x into register(0x%02x) successful\n",
                     regvalue, regaddr);
    }

error_return:
    mutex_unlock(&g_device_mutex);

    return count;
}

#ifdef FTS_MCAP_TEST
static ssize_t ft5x46_appid_show(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
    char tmp[30];
    int count = 0;

    count += sprintf(tmp + count, "81");

    if ((cur_tp_info.lcm_id & 0xf0) == 0x10)
    {
        count += sprintf(tmp + count, "SHP");
    }
    else
    {
        count += sprintf(tmp + count, "AUO");
    }

    count += sprintf(tmp + count, "FOGS");

    if ((cur_tp_info.lcm_id & 0x0f) == 0x01)
    {
        count += sprintf(tmp + count, "W");
    }
    else
    {
        count += sprintf(tmp + count, "B");
    }

    count += sprintf(tmp + count, ":V%02X:FT5X46:%02X\n",
                     cur_tp_info.fw_version, cur_tp_info.lcm_id);

    memcpy(buf, tmp, count);
    return count;
}

static ssize_t ft5x46_appid_store(struct device *dev,
                                  struct device_attribute *attr, const char *buf, size_t count)
{
    return -EPERM;
}

static ssize_t ft5x46_ctptest_show(struct device *dev,
                                   struct device_attribute *attr, char *buf)
{
    int count = 0;
    char *errlog = NULL;
    int ret_c = 0;
    struct i2c_client *client = container_of(dev, struct i2c_client, dev);

    mutex_lock(&g_device_mutex);

    fts_ctpm_hw_reset();
    mdelay(300);

    Init_I2C_Write_Func(focal_i2c_Write);
    Init_I2C_Read_Func(focal_i2c_Read);

    if (cur_tp_info.vendor_id == 0x51)
    {
            SetParamData(ft5x46_test_config_ofilm);
    }
    else
    {
        pr_err("ctp test not found test config \n");
    }

    if (true == StartTestTP())
    {
        count += sprintf(buf + count, "0\n");
    }
    else
    {
        count += sprintf(buf + count, "1\n");
        count += sprintf(buf + count, "\n[FTS] tp test failure\n");
        errlog = kmalloc((2 * 1024), GFP_KERNEL);
        memset(errlog, 0, (2 * 1024));
        ret_c = focal_save_error_data(errlog, (2 * 1024));
        if (errlog != NULL)
        {
            memcpy(buf + count, errlog, ret_c);
            count += ret_c;
            kfree(errlog);
        }
    }

    mutex_unlock(&g_device_mutex);

    return count;
}

static ssize_t ft5x46_ctptest_store(struct device *dev,
                                    struct device_attribute *attr, const char *buf, size_t count)
{
    return -EPERM;
}
#endif

#ifdef MZ_HALL_MODE
int ft5x46_hall_mode_state = 0;
int ft5x46_hall_mode_switch(int state)
{
    int ret = 0;
    int _state = !!state;

#define COVER_MODE_EN_REG   0xC1
#define HOST_EVENT_MSG_REG  0xC3

    if (_state)  // enter hall mode
    {
        ret = fts_write_reg(g_focalclient, COVER_MODE_EN_REG, _state);
        ret |= fts_write_reg(g_focalclient, HOST_EVENT_MSG_REG, 2);
    }
    else     // exit hall mode
    {
        ret = fts_write_reg(g_focalclient, COVER_MODE_EN_REG, _state);
        ret |= fts_write_reg(g_focalclient, HOST_EVENT_MSG_REG, 0);
    }

    return ret;
}

static ssize_t ft5x46_hall_mode_store(struct device *dev,
                                      struct device_attribute *attr, const char *buf, size_t count)
{
    struct i2c_client *client = container_of(dev, struct i2c_client, dev);
    int state = simple_strtol(buf, NULL, 10);

    ft5x46_hall_mode_state = !!state;

    ft5x46_hall_mode_switch(ft5x46_hall_mode_state);

    return count;
}

static ssize_t ft5x46_hall_mode_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", ft5x46_hall_mode_state);
}

static DEVICE_ATTR(hall_mode, S_IRUGO | S_IWUSR, ft5x46_hall_mode_show,
                   ft5x46_hall_mode_store);
#endif

/*read and write register
*read example: echo 88 > ftstprwreg ---read register 0x88
*write example:echo 8807 > ftstprwreg ---write 0x07 into register 0x88
*
*note:the number of input must be 2 or 4.if it not enough,please fill in the 0.
*/
static DEVICE_ATTR(ftstprwreg, S_IRUGO | S_IWUSR, fts_tprwreg_show,
                   fts_tprwreg_store);

#ifdef FTS_MCAP_TEST
static DEVICE_ATTR(ctptest, S_IRUGO | S_IWUSR, ft5x46_ctptest_show, ft5x46_ctptest_store);
static DEVICE_ATTR(appid, S_IRUGO | S_IWUSR, ft5x46_appid_show, ft5x46_appid_store);
#endif

static int mz_tp_mode = MZ_UNKNOWN;
static int mz_tp_state = MZ_NO_ACTION;

void mz_tp_set_mode(int mode)
{
    mz_tp_mode = mode;
}

static ssize_t ft5x46_mode_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", mz_tp_mode);
}

static ssize_t ft5x46_fwupdate_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", mz_tp_state);
}

extern int tpd_firmware_upgrade_thread(void *unused);
static ssize_t ft5x46_fwupdate_store(struct device *dev,
                                     struct device_attribute *attr, const char *buf, size_t count)
{
    mz_tp_state = tpd_firmware_upgrade_thread(NULL);

    return count;
}

static DEVICE_ATTR(mode, S_IRUGO, ft5x46_mode_show, NULL);
static DEVICE_ATTR(fwupdate, S_IRUGO | S_IWUSR, ft5x46_fwupdate_show, ft5x46_fwupdate_store);

/* ROI */
#ifdef FTS_ROI
extern void tpd_roi_switch_open(void);
extern void tpd_roi_switch_close(void);
extern unsigned int tpd_roi_enable(void);
extern unsigned int tpd_roi_disable(void);
extern unsigned int tpd_roi_is_enable(void);
extern unsigned char* tpd_roi_get_rawdata(void);
extern void tpd_roi_clear_rawdata(void);

static ssize_t ts_roi_enable_show(struct device *dev,
                                  struct device_attribute *attr, char *buf)
{
    unsigned int value;
    int error;

    value = tpd_roi_is_enable();
    error = snprintf(buf, FTS_MAX_STR_LEN, "%d\n", value);
out:
    FTS_DBG("roi_enable_show done\n");
    return error;
}

static ssize_t ts_roi_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned int value;
    int error;

    error = sscanf(buf, "%u", &value);
    if (error <= 0)
    {
        FTS_DBG("sscanf return invaild :%d\n", error);
        error = -EINVAL;
        goto out;
    }
    FTS_DBG("sscanf value is %u\n", value);

    if (value)
    {
        tpd_roi_switch_open();
        error = tpd_roi_enable();
        if (!error)
        {
            FTS_DBG("ts_send_roi_store_cmd failed\n");
            error = -ENOMEM;
            goto out;
        }
    }
    else
    {
        tpd_roi_switch_close();
        tpd_roi_clear_rawdata();
        error = tpd_roi_disable();
        if (error)
        {
            FTS_DBG("ts_send_roi_store_cmd failed\n");
            error = -ENOMEM;
            goto out;
        }
    }

    error = count;
out:
    FTS_DBG("roi_enable_store done\n");
    return error;
}

static ssize_t ts_roi_data_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    int i, j = 0;
    unsigned char *roi_data_p = tpd_roi_get_rawdata();

    if (NULL == roi_data_p)
    {
        FTS_DBG("not define ROI for roi_data_show \n");
        return -ENOMEM;
    }

    //roi_data_temp <-- This is the buffer that has the ROI data you want to send to Qeexo
    memcpy(buf, roi_data_p + ROI_HEAD_DATA_LENGTH, ROI_DATA_SEND_LENGTH);

    return ROI_DATA_SEND_LENGTH;
}

static ssize_t ts_roi_data_debug_show(struct device *dev,
                                      struct device_attribute *attr, char *buf)
{
    int cnt = 0;
    int count = 0;
    int i = 0, j = 0;
    short roi_data_16[ROI_DATA_SEND_LENGTH / 2] = {0};
    unsigned char *roi_data_p = tpd_roi_get_rawdata();

    if (NULL == roi_data_p)
    {
        FTS_DBG("not define ROI for roi_data_show \n");
        return -ENOMEM;
    }
    
    FTS_DBG("ts_roi_data_debug_show CALLED \n");
    
    for (i = ROI_HEAD_DATA_LENGTH; i < ROI_DATA_READ_LENGTH; i += 2, j++)
    {
        roi_data_16[j] = ((roi_data_p[i]) | (roi_data_p[i + 1] << 8));
        cnt = snprintf(buf, ROI_PAGE_SIZE - count, "%4d ", roi_data_16[j]);
        buf += cnt;
        count += cnt;

        if ((j + 1) % 7 == 0)
        {
            cnt = snprintf(buf, ROI_PAGE_SIZE - count, "\n");
            buf += cnt;
            count += cnt;
        }
    }
    snprintf(buf, ROI_PAGE_SIZE - count, "\n");
    count++;
    return count;
}

static DEVICE_ATTR(roi_enable, (S_IRUSR | S_IRGRP | S_IWUSR | S_IWGRP), ts_roi_enable_show, ts_roi_enable_store);
static DEVICE_ATTR(roi_data, (S_IRUSR | S_IRGRP), ts_roi_data_show, NULL);
static DEVICE_ATTR(roi_data_debug, (S_IRUSR | S_IRGRP), ts_roi_data_debug_show, NULL);
#endif

/*add your attr in here*/
static struct attribute *fts_attributes[] =
{
    &dev_attr_ftstprwreg.attr,
    &dev_attr_mode.attr,
    &dev_attr_fwupdate.attr,
    #ifdef MZ_HALL_MODE
    &dev_attr_hall_mode.attr,
    #endif
    #ifdef FTS_MCAP_TEST
    &dev_attr_ctptest.attr,
    &dev_attr_appid.attr,
    #endif
    #ifdef FTS_ROI
    &dev_attr_roi_enable.attr,
    &dev_attr_roi_data.attr,
    &dev_attr_roi_data_debug.attr,
    #endif
    NULL
};

static struct attribute_group fts_attribute_group =
{
    .attrs = fts_attributes
};

/*create sysfs for debug*/
int fts_dma_buffer_init(void)
{
    I2CDMABuf_va = (u8 *)dma_alloc_coherent(&tpd->dev->dev, FTS_DMA_BUF_SIZE, &I2CDMABuf_pa, GFP_KERNEL);

    if (!I2CDMABuf_va)
    {
        FTS_DBG("[FTS] %s Allocate DMA I2C Buffer failed!\n", __func__);
        return -EIO;
    }
    return 0;
}

int fts_dma_buffer_deinit(void)
{
    if (I2CDMABuf_va)
    {
        dma_free_coherent(&tpd->dev->dev, FTS_DMA_BUF_SIZE, I2CDMABuf_va, I2CDMABuf_pa);
        I2CDMABuf_va = NULL;
        I2CDMABuf_pa = 0;
        FTS_DBG("[FTS] %s free DMA I2C Buffer...\r\n", __func__);
    }
    return 0;
}

int fts_create_sysfs(struct i2c_client *client)
{
    int err;
    err = sysfs_create_group(&client->dev.kobj, &fts_attribute_group);
    if (0 != err)
    {
        FTS_DBG( "%s() - ERROR: sysfs_create_group() failed.\n",     __func__);
        sysfs_remove_group(&client->dev.kobj, &fts_attribute_group);
        return -EIO;
    }
    else
    {
        mutex_init(&g_device_mutex);
        pr_info("ft6x06:%s() - sysfs_create_group() succeeded.\n",
                __func__);
    }

    fts_hidi2c_to_stdi2c(client);

    return err;
}

void fts_release_sysfs(struct i2c_client *client)
{
    sysfs_remove_group(&client->dev.kobj, &fts_attribute_group);
    mutex_destroy(&g_device_mutex);
}
/*create apk debug channel*/

#define PROC_UPGRADE            0
#define PROC_READ_REGISTER      1
#define PROC_WRITE_REGISTER     2
#define PROC_RAWDATA            3
#define PROC_AUTOCLB            4
#define PROC_UPGRADE_INFO       5
#define PROC_WRITE_DATA         6
#define PROC_READ_DATA          7

#define PROC_NAME   "ft5x0x-debug"
static unsigned char proc_operate_mode = PROC_RAWDATA;
static struct proc_dir_entry *ft5x0x_proc_entry;

/*interface of write proc*/
static int ft5x0x_debug_write(struct file *filp, const char __user *buf,
                              size_t count, loff_t *offp)
{
    struct i2c_client *client = PDE_DATA(file_inode(filp));
    unsigned char writebuf[FTS_PACKET_LENGTH];
    int buflen = count;
    int writelen = 0;
    int ret = 0;

    if (copy_from_user(&writebuf, buf, buflen))
    {
        FTS_DBG( "%s:copy from user error\n", __func__);
        return -EFAULT;
    }
    proc_operate_mode = writebuf[0];

    switch (proc_operate_mode)
    {
        case PROC_UPGRADE:
            {
                char upgrade_file_path[128];
                memset(upgrade_file_path, 0, sizeof(upgrade_file_path));
                sprintf(upgrade_file_path, "%s", writebuf + 1);
                upgrade_file_path[buflen - 1] = '\0';
                FTS_DBG("%s\n", upgrade_file_path);
                disable_irq(client->irq);

                ret = fts_ctpm_fw_upgrade_with_app_file(client, upgrade_file_path);

                enable_irq(client->irq);
                if (ret < 0)
                {
                    FTS_DBG( "%s:upgrade failed.\n", __func__);
                    return ret;
                }
            }
            break;
        case PROC_READ_REGISTER:
            writelen = 1;
            FTS_DBG("%s:register addr=0x%02x\n", __func__, writebuf[1]);
            ret = fts_i2c_Write(client, writebuf + 1, writelen);
            if (ret < 0)
            {
                FTS_DBG( "%s:write iic error\n", __func__);
                return ret;
            }
            break;
        case PROC_WRITE_REGISTER:
            writelen = 2;
            ret = fts_i2c_Write(client, writebuf + 1, writelen);
            if (ret < 0)
            {
                FTS_DBG( "%s:write iic error\n", __func__);
                return ret;
            }
            break;
        case PROC_RAWDATA:
            break;
        case PROC_AUTOCLB:
            fts_ctpm_auto_clb(client);
            break;
        case PROC_READ_DATA:
        case PROC_WRITE_DATA:
            writelen = count - 1;
            ret = fts_i2c_Write(client, writebuf + 1, writelen);
            if (ret < 0)
            {
                FTS_DBG( "%s:write iic error\n", __func__);
                return ret;
            }
            break;

        default:
            break;
    }


    return count;
}

/*interface of read proc*/
static int ft5x0x_debug_read(struct file *filp, char __user *buf, size_t count,
                             loff_t *offp)
{
    struct i2c_client *client = PDE_DATA(file_inode(filp));
    int ret = 0, err = 0;
    u8 tx = 0, rx = 0;
    int i, j;
    unsigned char tmp[PAGE_SIZE];
    int num_read_chars = 0;
    int readlen = 0;
    u8 regvalue = 0x00, regaddr = 0x00;
    switch (proc_operate_mode)
    {
        case PROC_UPGRADE:
            /*after calling ft5x0x_debug_write to upgrade*/
            regaddr = 0xA6;
            ret = fts_read_reg(client, regaddr, &regvalue);
            if (ret < 0)
            {
                num_read_chars = sprintf(tmp, "%s", "get fw version failed.\n");
            }
            else
            {
                num_read_chars = sprintf(tmp, "current fw version:0x%02x\n", regvalue);
            }
            break;
        case PROC_READ_REGISTER:
            readlen = 1;
            ret = fts_i2c_Read(client, NULL, 0, tmp, readlen);
            if (ret < 0)
            {
                FTS_DBG( "%s:read iic error\n", __func__);
                return ret;
            }
            else
            {
                FTS_DBG("%s:value=0x%02x\n", __func__, tmp[0]);
            }
            num_read_chars = 1;
            break;
        case PROC_RAWDATA:
            break;
        case PROC_READ_DATA:
            readlen = count;
            ret = fts_i2c_Read(client, NULL, 0, tmp, readlen);
            if (ret < 0)
            {
                FTS_DBG( "%s:read iic error\n", __func__);
                return ret;
            }

            num_read_chars = readlen;
            break;
        case PROC_WRITE_DATA:
            break;
        default:
            break;
    }

    copy_to_user(buf, tmp, num_read_chars);
    //memcpy(page, buf, num_read_chars);

    return num_read_chars;
}

#ifdef FTS_ESD_CHECKER
extern int apk_debug_flag;
#endif

int ft5x0x_debug_open(struct inode * node, struct file *filp)
{
    #ifdef FTS_ESD_CHECKER
    apk_debug_flag++;
    #endif

    return 0;
}

int ft5x0x_debug_release(struct inode *node, struct file *filp)
{
    #ifdef FTS_ESD_CHECKER
    apk_debug_flag--;
    #endif

    return 0;
}

static struct file_operations ft5x0x_proc_fops =
{
    .open = ft5x0x_debug_open,
    .read = ft5x0x_debug_read,
    .write = ft5x0x_debug_write,
    .release = ft5x0x_debug_release,
};

int ft5x0x_create_apk_debug_channel(struct i2c_client * client)
{
    ft5x0x_proc_entry = proc_create_data(PROC_NAME, 0777, NULL,
                                         &ft5x0x_proc_fops, client);
    if (NULL == ft5x0x_proc_entry)
    {
        FTS_DBG( "Couldn't create proc entry!\n");
        return -ENOMEM;
    }
    /*
    else
    {
        dev_info(&client->dev, "Create proc entry success!\n");
        ft5x0x_proc_entry->data = client;
        ft5x0x_proc_entry->write_proc = ft5x0x_debug_write;
        ft5x0x_proc_entry->read_proc = ft5x0x_debug_read;
    }
    */
    return 0;
}

void ft5x0x_release_apk_debug_channel(void)
{
    if (ft5x0x_proc_entry)
    {
        remove_proc_entry(PROC_NAME, NULL);
    }
}


