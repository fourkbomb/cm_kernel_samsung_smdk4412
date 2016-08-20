/*
 * Copyright (C) 2012 Samsung Electronics Co.Ltd
 * Authors:
 *	Eunchul Kim <chulspro.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include "drmP.h"
#include <linux/kernel.h>
#include <linux/module.h>
#include <drm/exynos_drm.h>

#include "exynos_drm_drv.h"
#include "exynos_drm_hdmi.h"
#include "exynos_hdmi.h"
#include "exynos_hdcp.h"
#include "regs-hdmi.h"

/*
 * HDCP is stand for High-bandwidth Digital Content Protection.
 * contains an integrated HDCP encription engine
 * for video/audio content protection.
 * supports version HDCP v1.1.
 * Exynos supports embedded HDCP key system.
 * The HDCP key value is fused during fabrication, based on customer's request.
 */

#define HDCP_AN_SIZE		8
#define HDCP_AKSV_SIZE	5
#define HDCP_BKSV_SIZE	5
#define HDCP_MAX_KEY_SIZE	16
#define HDCP_BCAPS_SIZE	1
#define HDCP_BSTATUS_SIZE	2
#define HDCP_SHA_1_HASH_SIZE	20
#define HDCP_MAX_DEVS	128
#define HDCP_KSV_SIZE	5

#define HDCP_KSV_FIFO_READY	(0x1 << 5)
#define HDCP_MAX_CASCADE_EXCEEDED	(0x1 << 3)
#define HDCP_MAX_DEVS_EXCEEDED	(0x1 << 7)

/* offset of HDCP port */
#define HDCP_BKSV	0x00
#define HDCP_RI	0x08
#define HDCP_AKSV	0x10
#define HDCP_AN	0x18
#define HDCP_SHA1	0x20
#define HDCP_BCAPS	0x40
#define HDCP_BSTATUS	0x41
#define HDCP_KSVFIFO	0x43

enum hdcp_error {
	HDCP_ERR_MAX_CASCADE,
	HDCP_ERR_MAX_DEVS,
	HDCP_ERR_REPEATER_ILLEGAL_DEVICE,
	HDCP_ERR_REPEATER_TIMEOUT,
};

enum hdcp_event {
	HDCP_EVENT_STOP	= 1 << 0,
	HDCP_EVENT_START	= 1 << 1,
	HDCP_EVENT_READ_BKSV_START	= 1 << 2,
	HDCP_EVENT_WRITE_AKSV_START	= 1 << 4,
	HDCP_EVENT_CHECK_RI_START	= 1 << 8,
	HDCP_EVENT_SECOND_AUTH_START	= 1 << 16
};

/*
 * A structure of event work information.
 *
 * @work: work structure.
 * @event: event id of hdcp.
 */
struct hdcp_event_work {
	struct work_struct	work;
	u32	event;
};

/*
 * A structure of context.
 *
 * @regs: memory mapped io registers.
 * @ddc_port: hdmi ddc port.
 * @event_work: work information of hdcp event
 * @wq: work queue struct
 * @is_repeater: true is repeater, false is sink
 *
 */
struct hdcp_context {
	void __iomem	*regs;
	struct i2c_client *ddc_port;
	struct hdcp_event_work event_work;
	struct workqueue_struct	*wq;
	bool is_repeater;
};

static struct i2c_client *hdcp_ddc;

static int hdcp_i2c_recv(struct i2c_client *client, u8 reg, u8 *buf, int len)
{
	int ret, retries = 5; /* FIXME:!! 5 */

	/* The core i2c driver will automatically retry the transfer if the
	 * adapter reports EAGAIN. However, we find that bit-banging transfers
	 * are susceptible to errors under a heavily loaded machine and
	 * generate spurious NAKs and timeouts. Retrying the transfer
	 * of the individual block a few times seems to overcome this.
	 */
	do {
		struct i2c_msg msgs[] = {
			[0] = {
				.addr = client->addr,
				.flags = 0,
				.len = 1,
				.buf = &reg
			},
			[1] = {
				.addr = client->addr,
				.flags = I2C_M_RD,
				.len = len,
				.buf = buf
			}
		};

		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == -ENXIO) {
			DRM_ERROR("failed to recv HDCP via I2C.\n");
			kfree(msgs);
			return ret;
		}
	} while (ret != 2 && --retries);

	if (ret == 2)
		DRM_INFO("%s:success to recv HDCP via I2C.\n", __func__);

	return ret == 2 ? 0 : -EIO;
}

static int hdcp_i2c_send(struct i2c_client *client, u8 reg, u8 *buf, int len)
{
	int ret, retries = 5; /* FIXME:!! 5 */
	u8 *msg;

	msg = kzalloc(len+1, GFP_KERNEL);

	msg[0] = reg;
	memcpy(&msg[1], buf, len);

	/* The core i2c driver will automatically retry the transfer if the
	 * adapter reports EAGAIN. However, we find that bit-banging transfers
	 * are susceptible to errors under a heavily loaded machine and
	 * generate spurious NAKs and timeouts. Retrying the transfer
	 * of the individual block a few times seems to overcome this.
	 */
	do {
		ret = i2c_master_send(client, msg, len+1);
		if (ret == -ENXIO) {
			DRM_ERROR("failed to send HDCP via I2C.\n");
			kfree(msg);
			return ret;
		}
	} while (ret != 2 && --retries);

	if (ret == 2)
		DRM_DEBUG_KMS("%s:success to send HDCP via I2C.\n", __func__);

	kfree(msg);
	return ret == 2 ? 0 : -EIO;
}

static inline u32 hdcp_reg_read(struct hdcp_context *ctx, u32 reg_id)
{
	return readl(ctx->regs + reg_id);
}

static inline u8 hdcp_reg_readb(struct hdcp_context *ctx, u32 reg_id)
{
	return readb(ctx->regs + reg_id);
}

static inline void hdcp_reg_read_bytes(struct hdcp_context *ctx, u32 reg_id,
		u8 *buf, int bytes)
{
	int i;

	for (i = 0; i < bytes; ++i)
		buf[i] = readb(ctx->regs + reg_id + i * 4);
}

static inline void hdcp_reg_write(struct hdcp_context *ctx, u32 reg_id,
	u8 value)
{
	writel(value, ctx->regs + reg_id);
}

static inline void hdcp_reg_write_bytes(struct hdcp_context *ctx,
	u32 reg_id, u8 *buf, u32 bytes)
{
	int i;

	for (i = 0; i < bytes; ++i)
		writeb(buf[i], ctx->regs + reg_id + i * 4);
}

static inline void hdcp_reg_write_mask(struct hdcp_context *ctx,
	u32 reg_id, u32 value, u32 mask)
{
	u32 old = readl(ctx->regs + reg_id);
	value = (value & mask) | (old & ~mask);
	writel(value, ctx->regs + reg_id);
}

static inline void hdcp_reg_writeb(struct hdcp_context *ctx, u32 reg_id,
	u8 value)
{
	writeb(value, ctx->regs + reg_id);
}

static u8 hdcp_get_init_mask(struct hdcp_context *ctx)
{
	DRM_DEBUG_KMS("%s\n", __func__);
	return hdcp_reg_readb(ctx, HDMI_INTC_CON);
}

static void hdcp_set_init_mask(struct hdcp_context *ctx, u8 mask, bool enable)
{
	DRM_DEBUG_KMS("%s:enable[%d]\n", __func__, enable);

	if (enable) {
		mask |= HDMI_INTC_EN_GLOBAL;
		hdcp_reg_write_mask(ctx, HDMI_INTC_CON, ~0, mask);
	} else
		hdcp_reg_write_mask(ctx, HDMI_INTC_CON, 0, HDMI_INTC_EN_GLOBAL);
}

static void hdcp_sw_reset(struct hdcp_context *ctx)
{
	u8 val;

	DRM_DEBUG_KMS("%s\n", __func__);

	val = hdcp_get_init_mask(ctx);

	/* FIXME:!! PLUG, UNPLUG needed ? */
	hdcp_set_init_mask(ctx, HDMI_INTC_EN_HPD_PLUG, 0);
	hdcp_set_init_mask(ctx, HDMI_INTC_EN_HPD_UNPLUG, 0);

	hdcp_reg_write_mask(ctx, HDMI_HPD, ~0, HDMI_HPD_SEL_I_HPD);
	hdcp_reg_write_mask(ctx, HDMI_HPD, 0, HDMI_SW_HPD_PLUGGED);
	hdcp_reg_write_mask(ctx, HDMI_HPD, ~0, HDMI_SW_HPD_PLUGGED);
	hdcp_reg_write_mask(ctx, HDMI_HPD, 0, HDMI_HPD_SEL_I_HPD);

	if (val & HDMI_INTC_EN_HPD_PLUG)
		hdcp_set_init_mask(ctx, HDMI_INTC_EN_HPD_PLUG, 1);

	if (val & HDMI_INTC_EN_HPD_UNPLUG)
		hdcp_set_init_mask(ctx, HDMI_INTC_EN_HPD_UNPLUG, 1);
}

static void hdcp_encryption(struct hdcp_context *ctx, bool on)
{
	DRM_DEBUG_KMS("%s:on[%d]\n", __func__, on);

	/*
	 * hdcp encoder enable and bluecreen, audio mute
	 */
	if (on) {
		/* hdcp encoder on */
		hdcp_reg_write_mask(ctx, HDMI_ENC_EN, ~0, HDMI_HDCP_ENC_ENABLE);
		/* bluecreen off */
		hdcp_reg_write_mask(ctx, HDMI_CON_0, 0, HDMI_BLUE_SCR_EN);
		/* audio mute on */
		hdcp_reg_write(ctx, HDMI_AUI_CON,
			HDMI_AUI_CON_TRANS_EVERY_VSYNC);
		hdcp_reg_write_mask(ctx, HDMI_CON_0, ~0, HDMI_ASP_EN);
	} else {
		/* hdcp encoder off */
		hdcp_reg_write_mask(ctx, HDMI_ENC_EN, 0, HDMI_HDCP_ENC_ENABLE);
		/* bluecreen on */
		hdcp_reg_write_mask(ctx, HDMI_CON_0, ~0, HDMI_BLUE_SCR_EN);
		/* audio mute off */
		hdcp_reg_write(ctx, HDMI_AUI_CON, HDMI_AUI_CON_NO_TRAN);
		hdcp_reg_write_mask(ctx, HDMI_CON_0, 0, HDMI_ASP_EN);
	}
}

static int hdcp_loadkey(struct hdcp_context *ctx)
{
	u8 val;
	int retries = 1000; /* FIXME:!! 1000 */

	DRM_DEBUG_KMS("%s\n", __func__);

	hdcp_reg_write_mask(ctx, HDMI_EFUSE_CTRL, ~0,
		HDMI_EFUSE_CTRL_HDCP_KEY_READ);

	do {
		val = hdcp_reg_readb(ctx, HDMI_EFUSE_STATUS);
		if (val & HDMI_EFUSE_ECC_DONE)
			break;
		mdelay(1);
	} while (--retries);

	if (!retries)
		goto hdcp_err;

	val = hdcp_reg_readb(ctx, HDMI_EFUSE_STATUS);

	if (val & HDMI_EFUSE_ECC_FAIL)
		goto hdcp_err;

	DRM_DEBUG_KMS("%s:hdcp key read success.\n", __func__);

	return 0;

hdcp_err:
	DRM_ERROR("failed to read EFUSE val.\n");
	return -EINVAL;
}

static void hdcp_poweron(struct hdcp_context *ctx)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	hdcp_sw_reset(ctx);
	hdcp_encryption(ctx, 0);

	msleep(120);
	if (hdcp_loadkey(ctx) < 0) {
		DRM_ERROR("failed to load hdcp key.\n");
		return;
	}

	hdcp_reg_write(ctx, HDMI_GCP_CON, HDMI_GCP_CON_NO_TRAN);
	hdcp_reg_write(ctx, HDMI_STATUS_EN, HDMI_INT_EN_ALL);
	hdcp_reg_write(ctx, HDMI_HDCP_CTRL1, HDMI_HDCP_CP_DESIRED_EN);

	hdcp_set_init_mask(ctx, HDMI_INTC_EN_HDCP, 1);

	DRM_DEBUG_KMS("%s:start encription.\n", __func__);
}

static void hdcp_poweroff(struct hdcp_context *ctx)
{
	u8 val;

	DRM_DEBUG_KMS("%s\n", __func__);

	hdcp_set_init_mask(ctx, HDMI_INTC_EN_HDCP, 0);

	hdcp_reg_writeb(ctx, HDMI_HDCP_CTRL1, 0x0);
	hdcp_reg_write_mask(ctx, HDMI_HPD, 0, HDMI_HPD_SEL_I_HPD);

	val = HDMI_UPDATE_RI_INT_EN | HDMI_WRITE_INT_EN |
		HDMI_WATCHDOG_INT_EN | HDMI_WTFORACTIVERX_INT_EN;
	hdcp_reg_write_mask(ctx, HDMI_STATUS_EN, 0, val);
	hdcp_reg_write_mask(ctx, HDMI_STATUS_EN, ~0, val);

	hdcp_reg_write_mask(ctx, HDMI_SYS_STATUS, ~0, HDMI_INT_EN_ALL);

	hdcp_encryption(ctx, 0);

	hdcp_reg_writeb(ctx, HDMI_HDCP_CHECK_RESULT, HDMI_HDCP_CLR_ALL_RESULTS);

	DRM_DEBUG_KMS("%s:stop encription.\n", __func__);
}

static int hdcp_is_streaming(struct hdcp_context *ctx)
{
	int hpd;

	hpd = hdcp_reg_read(ctx, HDMI_HPD_STATUS);

	DRM_DEBUG_KMS("%s:hpd[%d]\n", __func__, hpd);

	return hpd;
}

static int hdcp_read_bcaps(struct hdcp_context *ctx)
{
	u8 bcaps = 0;

	DRM_DEBUG_KMS("%s\n", __func__);

	if (!hdcp_is_streaming(ctx))
		goto hdcp_err;

	if (hdcp_i2c_recv(ctx->ddc_port, HDCP_BCAPS, &bcaps,
	    HDCP_BCAPS_SIZE) < 0)
		goto hdcp_err;

	hdcp_reg_writeb(ctx, HDMI_HDCP_BCAPS, bcaps);

	if (bcaps & HDMI_HDCP_BCAPS_REPEATER)
		ctx->is_repeater = true;
	else
		ctx->is_repeater = false;

	DRM_DEBUG_KMS("%s:is_repeater[%d]\n", __func__, ctx->is_repeater);

	return 0;

hdcp_err:
	DRM_ERROR("failed to read bcaps.\n");

	return -EIO;
}

static int hdcp_read_bksv(struct hdcp_context *ctx)
{
	u8 bksv[HDCP_BKSV_SIZE];
	int i, j;
	u32 one = 0, zero = 0, result = 0;
	int retries = 14; /* FIXME:!! 14 */

	if (!hdcp_is_streaming(ctx))
		goto hdcp_err;

	memset(bksv, 0, sizeof(bksv));

	do {
		if (hdcp_i2c_recv(ctx->ddc_port, HDCP_BKSV, bksv,
		    HDCP_BKSV_SIZE) < 0)
			goto hdcp_err;

		for (i = 0; i < HDCP_BKSV_SIZE; i++)
			DRM_DEBUG_KMS("%s:bksv[%d][0x%x]\n",
				__func__, i, bksv[i]);

		for (i = 0; i < HDCP_BKSV_SIZE; i++) {
			for (j = 0; j < 8; j++) {
				result = bksv[i] & (0x1 << j);

				if (result == 0)
					zero++;
				else
					one++;
			}
		}

		if ((zero == 20) && (one == 20)) {
			hdcp_reg_write_bytes(ctx, HDMI_HDCP_BKSV(0), bksv,
				HDCP_BKSV_SIZE);
			break;
		}

		msleep(100);
	} while (--retries);

	if (!retries)
		goto hdcp_err;

	DRM_DEBUG_KMS("%s:retries[%d]\n", __func__, retries);

	return 0;

hdcp_err:
	DRM_ERROR("failed to read bksv.\n");

	return -EIO;
}

static int hdcp_bksv(struct hdcp_context *ctx)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	if (!hdcp_is_streaming(ctx))
		goto hdcp_err;

	if (hdcp_read_bcaps(ctx) < 0)
		goto hdcp_err;

	if (hdcp_read_bksv(ctx) < 0)
		goto hdcp_err;

	return 0;

hdcp_err:
	DRM_ERROR("failed to check bksv.\n");

	return -EIO;
}

static int hdcp_check_repeater(struct hdcp_context *ctx)
{
	int ret = -EINVAL, val, i;
	u32 dev_cnt;
	u8 bcaps = 0;
	u8 status[HDCP_BSTATUS_SIZE];
	u8 rx_v[HDCP_SHA_1_HASH_SIZE];
	u8 ksv_list[HDCP_MAX_DEVS * HDCP_KSV_SIZE];
	int cnt;
	int retries1 = 50; /* FIXME:!! 50 */
	int retries2;

	DRM_DEBUG_KMS("%s\n", __func__);

	memset(status, 0, sizeof(status));
	memset(rx_v, 0, sizeof(rx_v));
	memset(ksv_list, 0, sizeof(ksv_list));

	do {
		if (hdcp_read_bcaps(ctx) < 0)
			goto hdcp_err;

		bcaps = hdcp_reg_readb(ctx, HDMI_HDCP_BCAPS);
		if (bcaps & HDCP_KSV_FIFO_READY) {
			DRM_DEBUG_KMS("%s:ksv fifo not ready.\n", __func__);
			break;
		}

		msleep(100);
	} while (--retries1);

	if (!retries1) {
		ret = HDCP_ERR_REPEATER_TIMEOUT;
		goto hdcp_err;
	}

	DRM_DEBUG_KMS("%s:ksv fifo ready.\n", __func__);

	if (hdcp_i2c_recv(ctx->ddc_port, HDCP_BSTATUS, status,
	    HDCP_BSTATUS_SIZE) < 0)
		goto hdcp_err;

	if (status[1] & HDCP_MAX_CASCADE_EXCEEDED) {
		ret = HDCP_ERR_MAX_CASCADE;
		goto hdcp_err;
	} else if (status[0] & HDCP_MAX_DEVS_EXCEEDED) {
		ret = HDCP_ERR_MAX_DEVS;
		goto hdcp_err;
	}

	hdcp_reg_writeb(ctx, HDMI_HDCP_BSTATUS_0, status[0]);
	hdcp_reg_writeb(ctx, HDMI_HDCP_BSTATUS_1, status[1]);

	DRM_DEBUG_KMS("%s:status0[0x%x],status1[0x%x]\n",
		__func__, status[0], status[1]);

	dev_cnt = status[0] & 0x7f;
	DRM_DEBUG_KMS("%s:dev_cnt[%d]\n", __func__, dev_cnt);

	if (dev_cnt) {
		if (hdcp_i2c_recv(ctx->ddc_port, HDCP_KSVFIFO, ksv_list,
			dev_cnt * HDCP_KSV_SIZE) < 0)
			goto hdcp_err;

		cnt = 0;
		do {
			hdcp_reg_write_bytes(ctx, HDMI_HDCP_KSV_LIST(0),
					&ksv_list[cnt * 5], HDCP_KSV_SIZE);

			val = HDMI_HDCP_KSV_WRITE_DONE;
			if (cnt == dev_cnt - 1)
				val |= HDMI_HDCP_KSV_END;

			hdcp_reg_write(ctx, HDMI_HDCP_KSV_LIST_CON, val);

			if (cnt < dev_cnt - 1) {
				retries2 = 10000; /* FIXME:!! 10000 */
				do {
					val = hdcp_reg_readb(ctx,
						HDMI_HDCP_KSV_LIST_CON);
					if (val & HDMI_HDCP_KSV_READ)
						break;
				} while (--retries2);

				if (!retries2)
					DRM_DEBUG_KMS("%s:ksv not readed.\n",
						__func__);
			}
			cnt++;
		} while (cnt < dev_cnt);
	} else
		hdcp_reg_writeb(ctx, HDMI_HDCP_KSV_LIST_CON,
			HDMI_HDCP_KSV_LIST_EMPTY);

	if (hdcp_i2c_recv(ctx->ddc_port, HDCP_SHA1, rx_v,
	    HDCP_SHA_1_HASH_SIZE) < 0)
		goto hdcp_err;

	for (i = 0; i < HDCP_SHA_1_HASH_SIZE; i++)
		DRM_DEBUG_KMS("%s:SHA-1 rx[0x%x]\n", __func__, rx_v[i]);

	hdcp_reg_write_bytes(ctx, HDMI_HDCP_SHA1(0), rx_v,
		HDCP_SHA_1_HASH_SIZE);

	val = hdcp_reg_readb(ctx, HDMI_HDCP_SHA_RESULT);
	if (val & HDMI_HDCP_SHA_VALID_RD) {
		if (val & HDMI_HDCP_SHA_VALID) {
			DRM_DEBUG_KMS("%s:SHA-1 result is ok.\n", __func__);
			hdcp_reg_writeb(ctx, HDMI_HDCP_SHA_RESULT, 0x0);
		} else {
			DRM_DEBUG_KMS("%s:SHA-1 result is not vaild.\n",
				__func__);
			hdcp_reg_writeb(ctx, HDMI_HDCP_SHA_RESULT, 0x0);
			goto hdcp_err;
		}
	} else {
		DRM_DEBUG_KMS("%s:SHA-1 result is not ready.\n", __func__);
		hdcp_reg_writeb(ctx, HDMI_HDCP_SHA_RESULT, 0x0);
		goto hdcp_err;
	}

	DRM_DEBUG_KMS("%s:done.\n", __func__);

	return 0;

hdcp_err:
	DRM_ERROR("failed to check repeater.\n");

	return ret;
}

static int hdcp_start_encryption(struct hdcp_context *ctx)
{
	u8 val;
	int retries = 10; /* FIXME:!! 10 */

	do {
		val = hdcp_reg_readb(ctx, HDMI_SYS_STATUS);

		if (val & HDMI_AUTHEN_ACK_AUTH) {
			hdcp_encryption(ctx, 1);
			break;
		}

		mdelay(1);
	} while (--retries);

	if (!retries)
		goto hdcp_err;

	DRM_DEBUG_KMS("%s:retries[%d]\n", __func__, retries);

	return 0;

hdcp_err:
	DRM_ERROR("failed to start encription.\n");
	hdcp_encryption(ctx, 0);

	return -EIO;
}

static int hdcp_second_auth(struct hdcp_context *ctx)
{
	int ret = 0;

	DRM_DEBUG_KMS("%s\n", __func__);

	if (!hdcp_is_streaming(ctx))
		goto hdcp_err;

	ret = hdcp_check_repeater(ctx);

	if (ret) {
		DRM_DEBUG_KMS("%s:ret[%d]\n", __func__, ret);

		switch (ret) {
		case HDCP_ERR_REPEATER_ILLEGAL_DEVICE:
			hdcp_reg_writeb(ctx, HDMI_HDCP_CTRL2, 0x1);
			mdelay(1);
			hdcp_reg_writeb(ctx, HDMI_HDCP_CTRL2, 0x0);

			DRM_DEBUG_KMS("%s:illegal device.\n", __func__);
			break;
		case HDCP_ERR_REPEATER_TIMEOUT:
			hdcp_reg_write_mask(ctx, HDMI_HDCP_CTRL1, ~0,
					HDMI_HDCP_SET_REPEATER_TIMEOUT);
			hdcp_reg_write_mask(ctx, HDMI_HDCP_CTRL1, 0,
					HDMI_HDCP_SET_REPEATER_TIMEOUT);

			DRM_DEBUG_KMS("%s:timeout.\n", __func__);
			break;
		case HDCP_ERR_MAX_CASCADE:
			DRM_DEBUG_KMS("%s:exceeded MAX_CASCADE.\n", __func__);
			break;
		case HDCP_ERR_MAX_DEVS:
			DRM_DEBUG_KMS("%s:exceeded MAX_DEVS.\n", __func__);
			break;
		default:
			break;
		}

		goto hdcp_err;
	}

	hdcp_start_encryption(ctx);

	return 0;

hdcp_err:
	DRM_ERROR("failed to check second authentication.\n");

	return -EIO;
}

static int hdcp_write_key(struct hdcp_context *ctx, int size,
	int reg, int offset)
{
	u8 buf[HDCP_MAX_KEY_SIZE];
	int cnt, zero = 0;
	int i;

	memset(buf, 0, sizeof(buf));
	hdcp_reg_read_bytes(ctx, reg, buf, size);

	for (cnt = 0; cnt < size; cnt++)
		if (buf[cnt] == 0)
			zero++;

	if (zero == size) {
		DRM_ERROR("%s: %s is null.\n", __func__,
			offset == HDCP_AN ? "An" : "Aksv");
		goto hdcp_err;
	}

	if (hdcp_i2c_send(ctx->ddc_port, offset, buf, size) < 0)
		goto hdcp_err;

	for (i = 1; i < size + 1; i++)
		DRM_ERROR("%s: %s%d[0x%x].\n", __func__,
			offset == HDCP_AN ? "An" : "Aksv", i, buf[i]);

	return 0;

hdcp_err:
	DRM_ERROR("failed to write %s key.\n",
		offset == HDCP_AN ? "An" : "Aksv");

	return -EIO;
}

static int hdcp_write_aksv(struct hdcp_context *ctx)
{
	DRM_DEBUG_KMS("%s\n", __func__);

	if (!hdcp_is_streaming(ctx))
		goto hdcp_err;

	if (hdcp_write_key(ctx, HDCP_AN_SIZE, HDMI_HDCP_AN(0), HDCP_AN) < 0)
		goto hdcp_err;

	DRM_DEBUG_KMS("%s:write an is done.\n", __func__);

	if (hdcp_write_key(ctx, HDCP_AKSV_SIZE, HDMI_HDCP_AKSV(0),
	    HDCP_AKSV) < 0)
		goto hdcp_err;

	msleep(100);

	DRM_DEBUG_KMS("%s:write aksv is done.\n", __func__);
	return 0;

hdcp_err:
	DRM_ERROR("failed to start aksv.\n");

	return -EIO;
}

static int hdcp_check_ri(struct hdcp_context *ctx)
{
	u8 ri[2] = {0, 0};
	u8 rj[2] = {0, 0};

	DRM_DEBUG_KMS("%s\n", __func__);

	ri[0] = hdcp_reg_readb(ctx, HDMI_HDCP_RI_0);
	ri[1] = hdcp_reg_readb(ctx, HDMI_HDCP_RI_1);

	if (hdcp_i2c_recv(ctx->ddc_port, HDCP_RI, rj, 2) < 0)
		goto hdcp_err;

	if ((ri[0] == rj[0]) && (ri[1] == rj[1]) && (ri[0] | ri[1]))
		hdcp_reg_writeb(ctx, HDMI_HDCP_CHECK_RESULT,
				HDMI_HDCP_RI_MATCH_RESULT_Y);
	else {
		hdcp_reg_writeb(ctx, HDMI_HDCP_CHECK_RESULT,
				HDMI_HDCP_RI_MATCH_RESULT_N);
		goto hdcp_err;
	}

	memset(ri, 0, sizeof(ri));
	memset(rj, 0, sizeof(rj));

	DRM_DEBUG_KMS("%s:done.\n", __func__);

	return 0;

hdcp_err:
	DRM_ERROR("failed to compare ri with rj.\n");

	return -EIO;
}

static void hdcp_reset_auth(struct hdcp_context *ctx)
{
	u8 val;

	DRM_DEBUG_KMS("%s\n", __func__);

	if (!hdcp_is_streaming(ctx))
		return;

	hdcp_reg_write(ctx, HDMI_HDCP_CTRL1, 0x0);
	hdcp_reg_write(ctx, HDMI_HDCP_CTRL2, 0x0);

	hdcp_encryption(ctx, 0);

	val = HDMI_UPDATE_RI_INT_EN | HDMI_WRITE_INT_EN |
		HDMI_WATCHDOG_INT_EN | HDMI_WTFORACTIVERX_INT_EN;
	hdcp_reg_write_mask(ctx, HDMI_STATUS_EN, 0, val);

	hdcp_reg_writeb(ctx, HDMI_HDCP_CHECK_RESULT, HDMI_HDCP_CLR_ALL_RESULTS);

	/* need some delay (at least 1 frame) */
	mdelay(16);

	hdcp_sw_reset(ctx);

	val = HDMI_UPDATE_RI_INT_EN | HDMI_WRITE_INT_EN |
		HDMI_WATCHDOG_INT_EN | HDMI_WTFORACTIVERX_INT_EN;
	hdcp_reg_write_mask(ctx, HDMI_STATUS_EN, ~0, val);
	hdcp_reg_write_mask(ctx, HDMI_HDCP_CTRL1, ~0, HDMI_HDCP_CP_DESIRED_EN);

	DRM_DEBUG_KMS("%s:done\n", __func__);
}

static void hdcp_event_wq(struct work_struct *work)
{
	struct hdcp_context *ctx = container_of((struct hdcp_event_work *)work,
		struct hdcp_context, event_work);
	struct hdcp_event_work *event_work = (struct hdcp_event_work *)work;

	DRM_DEBUG_KMS("%s:event[0x%x]\n", __func__, event_work->event);

	if (!hdcp_is_streaming(ctx))
		return;

	if (event_work->event & HDCP_EVENT_READ_BKSV_START) {
		if (hdcp_bksv(ctx) < 0)
			goto hdcp_err;
		else
			event_work->event &= ~HDCP_EVENT_READ_BKSV_START;
	}

	if (event_work->event & HDCP_EVENT_SECOND_AUTH_START) {
		if (hdcp_second_auth(ctx) < 0)
			goto hdcp_err;
		else
			event_work->event &= ~HDCP_EVENT_SECOND_AUTH_START;
	}

	if (event_work->event & HDCP_EVENT_WRITE_AKSV_START) {
		if (hdcp_write_aksv(ctx) < 0)
			goto hdcp_err;
		else
			event_work->event  &= ~HDCP_EVENT_WRITE_AKSV_START;
	}

	if (event_work->event & HDCP_EVENT_CHECK_RI_START) {
		if (hdcp_check_ri(ctx) < 0)
			goto hdcp_err;
		else
			event_work->event &= ~HDCP_EVENT_CHECK_RI_START;
	}

	return;

hdcp_err:
	hdcp_reset_auth(ctx);
}

static void hdcp_dpms(void *data, int mode)
{
	struct hdcp_context *ctx = data;

	DRM_DEBUG_KMS("%s:mode[%d]\n", __func__, mode);

	switch (mode) {
	case DRM_MODE_DPMS_ON:
		hdcp_poweron(ctx);
		break;
	case DRM_MODE_DPMS_STANDBY:
	case DRM_MODE_DPMS_SUSPEND:
	case DRM_MODE_DPMS_OFF:
		hdcp_poweroff(ctx);
		break;
	default:
		DRM_DEBUG_KMS("unknown dpms mode: %d\n", mode);
		break;
	}
}

static void hdcp_commit(void *data)
{
	struct hdcp_context *ctx = data;
	u32 event = 0;
	u8 flag;

	DRM_DEBUG_KMS("%s\n", __func__);
	if (!hdcp_is_streaming(ctx))
		return;

	flag = hdcp_reg_readb(ctx, HDMI_SYS_STATUS);

	if (flag & HDMI_WTFORACTIVERX_INT_OCC) {
		event |= HDCP_EVENT_READ_BKSV_START;
		hdcp_reg_write_mask(ctx, HDMI_SYS_STATUS, ~0,
			HDMI_WTFORACTIVERX_INT_OCC);
		hdcp_reg_write(ctx, HDMI_HDCP_I2C_INT, 0x0);
	}

	if (flag & HDMI_WRITE_INT_OCC) {
		event |= HDCP_EVENT_WRITE_AKSV_START;
		hdcp_reg_write_mask(ctx, HDMI_SYS_STATUS, ~0,
			HDMI_WRITE_INT_OCC);
		hdcp_reg_write(ctx, HDMI_HDCP_AN_INT, 0x0);
	}

	if (flag & HDMI_UPDATE_RI_INT_OCC) {
		event |= HDCP_EVENT_CHECK_RI_START;
		hdcp_reg_write_mask(ctx, HDMI_SYS_STATUS, ~0,
			HDMI_UPDATE_RI_INT_OCC);
		hdcp_reg_write(ctx, HDMI_HDCP_RI_INT, 0x0);
	}

	if (flag & HDMI_WATCHDOG_INT_OCC) {
		event |= HDCP_EVENT_SECOND_AUTH_START;
		hdcp_reg_write_mask(ctx, HDMI_SYS_STATUS, ~0,
			HDMI_WATCHDOG_INT_OCC);
		hdcp_reg_write(ctx, HDMI_HDCP_WDT_INT, 0x0);
	}

	if (!event) {
		DRM_DEBUG_KMS("%s:unknown irq\n", __func__);
		return;
	}

	ctx->event_work.event |= event;
	queue_work(ctx->wq, (struct work_struct *)&ctx->event_work);
}

static struct exynos_hdcp_ops hdmi_ops = {
	/* manager */
	.dpms = hdcp_dpms,
	.commit = hdcp_commit,
};

void exynos_hdcp_attach_ddc_client(struct i2c_client *ddc)
{
	if (ddc)
		hdcp_ddc = ddc;
}

int exynos_hdcp_register(void *data, void __iomem *regs)
{
	struct hdcp_context *ctx = data;

	if (!hdcp_ddc) {
		DRM_ERROR("failed to get ddc port.\n");
		return -ENODEV;
	}

	ctx->ddc_port = hdcp_ddc;
	ctx->regs = regs;

	return 0;
}

static int __devinit hdcp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct exynos_drm_hdmi_context *drm_hdcp_ctx;
	struct hdcp_context *ctx;
	int ret = -EINVAL;

	DRM_DEBUG_KMS("%s\n", __func__);

	drm_hdcp_ctx = kzalloc(sizeof(struct exynos_drm_hdmi_context),
		GFP_KERNEL);
	if (!drm_hdcp_ctx) {
		DRM_ERROR("failed to allocate common hdmi context.\n");
		return -ENOMEM;
	}

	ctx = kzalloc(sizeof(struct hdcp_context), GFP_KERNEL);
	if (!ctx) {
		DRM_ERROR("failed to get ctx memory.\n");
		ret = -ENOMEM;
		goto err_free;
	}

	ctx->wq = create_workqueue("hdcp");
	if (!ctx->wq) {
		ret = -ENOMEM;
		goto err_workqueue;
	}

	INIT_WORK((struct work_struct *)&ctx->event_work, hdcp_event_wq);
	drm_hdcp_ctx->ctx = (void *)ctx;
	platform_set_drvdata(pdev, drm_hdcp_ctx);

	exynos_hdcp_ops_register(&hdmi_ops);

	dev_info(dev, "drm hdcp registered successfully.\n");

	return 0;

err_workqueue:
	kfree(ctx);
err_free:
	kfree(drm_hdcp_ctx);
	return ret;
}

static int __devexit hdcp_remove(struct platform_device *pdev)
{
	struct exynos_drm_hdmi_context *drm_hdcp_ctx =
		platform_get_drvdata(pdev);
	struct hdcp_context *ctx = drm_hdcp_ctx->ctx;

	DRM_DEBUG_KMS("%s\n", __func__);
	kfree(ctx);
	kfree(drm_hdcp_ctx);

	return 0;
}

struct platform_driver hdcp_driver = {
	.probe		= hdcp_probe,
	.remove		= __devexit_p(hdcp_remove),
	.driver		= {
		.name	= "exynos-hdcp",
		.owner	= THIS_MODULE,
	},
};

