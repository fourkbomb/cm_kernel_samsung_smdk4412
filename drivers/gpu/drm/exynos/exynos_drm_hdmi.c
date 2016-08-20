/*
 * Copyright (C) 2011 Samsung Electronics Co.Ltd
 * Authors:
 *	Inki Dae <inki.dae@samsung.com>
 *	Seung-Woo Kim <sw0312.kim@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#include "drmP.h"

#include <linux/kernel.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#if defined(CONFIG_BUSFREQ_OPP) || defined(CONFIG_BUSFREQ_LOCK_WRAPPER)
#include <mach/dev.h>
#include <linux/cpufreq_slp.h>
#endif

#include <drm/exynos_drm.h>

#include "exynos_drm_drv.h"
#include "exynos_drm_hdmi.h"

#define to_context(dev)		platform_get_drvdata(to_platform_device(dev))
#define to_subdrv(dev)		to_context(dev)
#define get_ctx_from_subdrv(subdrv)	container_of(subdrv,\
					struct drm_hdmi_context, subdrv);

/* these callback points shoud be set by specific drivers. */
static struct exynos_hdmi_ops *hdmi_ops;
static struct exynos_mixer_ops *mixer_ops;

struct drm_hdmi_context {
	struct exynos_drm_subdrv	subdrv;
	struct exynos_drm_hdmi_context	*hdmi_ctx;
	struct exynos_drm_hdmi_context	*mixer_ctx;

#if defined(CONFIG_BUSFREQ_OPP) || defined(CONFIG_BUSFREQ_LOCK_WRAPPER)
	struct device           *bus_dev;
#endif
	bool	enabled[MIXER_WIN_NR];
	int	mode;
};

void exynos_hdmi_ops_register(struct exynos_hdmi_ops *ops)
{
	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (ops)
		hdmi_ops = ops;
}

void exynos_mixer_ops_register(struct exynos_mixer_ops *ops)
{
	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (ops)
		mixer_ops = ops;
}

static bool drm_hdmi_is_connected(struct device *dev)
{
	struct drm_hdmi_context *ctx = to_context(dev);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (hdmi_ops && hdmi_ops->is_connected)
		return hdmi_ops->is_connected(ctx->hdmi_ctx->ctx);

	return false;
}

static int drm_hdmi_get_edid(struct device *dev,
		struct drm_connector *connector, u8 *edid, int len)
{
	struct drm_hdmi_context *ctx = to_context(dev);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (hdmi_ops && hdmi_ops->get_edid)
		return hdmi_ops->get_edid(ctx->hdmi_ctx->ctx, connector, edid,
					  len);

	return 0;
}

static int drm_hdmi_check_timing(struct device *dev, void *timing)
{
	struct drm_hdmi_context *ctx = to_context(dev);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (hdmi_ops && hdmi_ops->check_timing)
		return hdmi_ops->check_timing(ctx->hdmi_ctx->ctx, timing);

	return 0;
}

static int drm_hdmi_power_on(struct device *dev, int mode)
{
	struct drm_hdmi_context *ctx = to_context(dev);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (hdmi_ops && hdmi_ops->power_on)
		return hdmi_ops->power_on(ctx->hdmi_ctx->ctx, mode);

	return 0;
}

static struct exynos_drm_display_ops drm_hdmi_display_ops = {
	.type = EXYNOS_DISPLAY_TYPE_HDMI,
	.is_connected = drm_hdmi_is_connected,
	.get_edid = drm_hdmi_get_edid,
	.check_timing = drm_hdmi_check_timing,
	.power_on = drm_hdmi_power_on,
};

static int drm_hdmi_enable_vblank(struct device *subdrv_dev)
{
	struct drm_hdmi_context *ctx = to_context(subdrv_dev);
	struct exynos_drm_subdrv *subdrv = &ctx->subdrv;
	struct exynos_drm_manager *manager = subdrv->manager;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (ctx->mode > DRM_MODE_DPMS_ON) {
		DRM_ERROR("%s:power down state.\n", __func__);
		goto out;
	}

	if (mixer_ops && mixer_ops->enable_vblank)
		return mixer_ops->enable_vblank(ctx->mixer_ctx->ctx,
						manager->pipe);

out:
	return 0;
}

static void drm_hdmi_disable_vblank(struct device *subdrv_dev)
{
	struct drm_hdmi_context *ctx = to_context(subdrv_dev);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (ctx->mode > DRM_MODE_DPMS_ON) {
		DRM_ERROR("%s:power down state.\n", __func__);
		return;
	}

	if (mixer_ops && mixer_ops->disable_vblank)
		return mixer_ops->disable_vblank(ctx->mixer_ctx->ctx);
}

static void drm_hdmi_mode_fixup(struct device *subdrv_dev,
				struct drm_connector *connector,
				struct drm_display_mode *mode,
				struct drm_display_mode *adjusted_mode)
{
	struct drm_hdmi_context *ctx = to_context(subdrv_dev);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (hdmi_ops && hdmi_ops->mode_fixup)
		hdmi_ops->mode_fixup(ctx->hdmi_ctx->ctx, connector, mode,
				     adjusted_mode);
}

static void drm_hdmi_mode_set(struct device *subdrv_dev, void *mode)
{
	struct drm_hdmi_context *ctx = to_context(subdrv_dev);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (hdmi_ops && hdmi_ops->mode_set)
		hdmi_ops->mode_set(ctx->hdmi_ctx->ctx, mode);
}

static void drm_hdmi_get_max_resol(struct device *subdrv_dev,
				unsigned int *width, unsigned int *height)
{
	struct drm_hdmi_context *ctx = to_context(subdrv_dev);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (hdmi_ops && hdmi_ops->get_max_resol)
		hdmi_ops->get_max_resol(ctx->hdmi_ctx->ctx, width, height);
}

static void drm_hdmi_commit(struct device *subdrv_dev)
{
	struct drm_hdmi_context *ctx = to_context(subdrv_dev);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (ctx->mode > DRM_MODE_DPMS_ON) {
		DRM_ERROR("%s:power down state.\n", __func__);
		return;
	}

	if (hdmi_ops && hdmi_ops->commit)
		hdmi_ops->commit(ctx->hdmi_ctx->ctx);
}

static void drm_hdmi_dpms(struct device *subdrv_dev, int mode)
{
	struct drm_hdmi_context *ctx = to_context(subdrv_dev);

	DRM_DEBUG_KMS("%s:mode[%d]\n", __func__, mode);

	ctx->mode = mode;

	if (mixer_ops && mixer_ops->dpms)
		mixer_ops->dpms(ctx->mixer_ctx->ctx, mode);

	if (hdmi_ops && hdmi_ops->dpms)
		hdmi_ops->dpms(ctx->hdmi_ctx->ctx, mode);
}

static void drm_hdmi_apply(struct device *subdrv_dev)
{
	struct drm_hdmi_context *ctx = to_context(subdrv_dev);
	int i;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (ctx->mode > DRM_MODE_DPMS_ON) {
		DRM_ERROR("%s:power down state.\n", __func__);
		return;
	}

	for (i = 0; i < MIXER_WIN_NR; i++) {
		if (!ctx->enabled[i])
			continue;
		if (mixer_ops && mixer_ops->win_commit)
			mixer_ops->win_commit(ctx->mixer_ctx->ctx, i);
	}

	if (hdmi_ops && hdmi_ops->commit)
		hdmi_ops->commit(ctx->hdmi_ctx->ctx);
}

static struct exynos_drm_manager_ops drm_hdmi_manager_ops = {
	.dpms = drm_hdmi_dpms,
	.apply = drm_hdmi_apply,
	.enable_vblank = drm_hdmi_enable_vblank,
	.disable_vblank = drm_hdmi_disable_vblank,
	.mode_fixup = drm_hdmi_mode_fixup,
	.mode_set = drm_hdmi_mode_set,
	.get_max_resol = drm_hdmi_get_max_resol,
	.commit = drm_hdmi_commit,
};

static void drm_mixer_mode_set(struct device *subdrv_dev,
		struct exynos_drm_overlay *overlay)
{
	struct drm_hdmi_context *ctx = to_context(subdrv_dev);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (mixer_ops && mixer_ops->win_mode_set)
		mixer_ops->win_mode_set(ctx->mixer_ctx->ctx, overlay);
}

static void drm_mixer_commit(struct device *subdrv_dev, int zpos)
{
	struct drm_hdmi_context *ctx = to_context(subdrv_dev);
	int win = (zpos == DEFAULT_ZPOS) ? MIXER_DEFAULT_WIN : zpos;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (win < 0 || win > MIXER_WIN_NR) {
		DRM_ERROR("mixer window[%d] is wrong\n", win);
		return;
	}

	if (ctx->mode > DRM_MODE_DPMS_ON) {
		DRM_ERROR("%s:power down state.\n", __func__);
		return;
	}

	if (mixer_ops && mixer_ops->win_commit)
		mixer_ops->win_commit(ctx->mixer_ctx->ctx, win);

	ctx->enabled[win] = true;
}

static void drm_mixer_qos_ctrl(struct device *dev)
{
	struct drm_hdmi_context *ctx = to_context(dev);
	int win, val, count = 0;

	DRM_DEBUG_KMS("%s\n", __func__);

	for (win = 0; win < MIXER_WIN_NR; win++)
		if (ctx->enabled[win])
			count++;

	switch (count) {
	case 1:
		val = 160160;
		break;
	case 2 ... MIXER_WIN_NR:
		val = 267160;
		break;
	default:
		val = 0;
		break;
	}

	DRM_DEBUG_KMS("%s:count[%d]val[%d]\n", __func__, count, val);

#if defined(CONFIG_BUSFREQ_OPP) || defined(CONFIG_BUSFREQ_LOCK_WRAPPER)
	if (val == 0)
		dev_unlock(ctx->bus_dev, dev);
	else
		dev_lock(ctx->bus_dev, dev, val);
#endif
}

static void drm_mixer_enable(struct device *subdrv_dev, int zpos)
{
	struct drm_hdmi_context *ctx = to_context(subdrv_dev);
	int win = (zpos == DEFAULT_ZPOS) ? MIXER_DEFAULT_WIN : zpos;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (win < 0 || win > MIXER_WIN_NR) {
		DRM_ERROR("mixer window[%d] is wrong\n", win);
		return;
	}

	ctx->enabled[win] = true;
	drm_mixer_qos_ctrl(subdrv_dev);
}

static void drm_mixer_disable(struct device *subdrv_dev, int zpos)
{
	struct drm_hdmi_context *ctx = to_context(subdrv_dev);
	int win = (zpos == DEFAULT_ZPOS) ? MIXER_DEFAULT_WIN : zpos;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (win < 0 || win > MIXER_WIN_NR) {
		DRM_ERROR("mixer window[%d] is wrong\n", win);
		return;
	}

	if (ctx->mode > DRM_MODE_DPMS_ON) {
		DRM_ERROR("%s:power down state.\n", __func__);
		return;
	}

	if (mixer_ops && mixer_ops->win_disable)
		mixer_ops->win_disable(ctx->mixer_ctx->ctx, win);

	ctx->enabled[win] = false;
	drm_mixer_qos_ctrl(subdrv_dev);
}

static void drm_mixer_wait_for_vblank(struct device *subdrv_dev)
{
	struct drm_hdmi_context *ctx = to_context(subdrv_dev);

	DRM_DEBUG_KMS("%s\n", __FILE__);

	if (ctx->mode > DRM_MODE_DPMS_ON) {
		DRM_ERROR("%s:power down state.\n", __func__);
		return;
	}

	if (mixer_ops && mixer_ops->wait_for_vblank)
		mixer_ops->wait_for_vblank(ctx->mixer_ctx->ctx);
}

static struct exynos_drm_overlay_ops drm_hdmi_overlay_ops = {
	.mode_set = drm_mixer_mode_set,
	.commit = drm_mixer_commit,
	.enable = drm_mixer_enable,
	.disable = drm_mixer_disable,
	.wait_for_vblank = drm_mixer_wait_for_vblank,
};

static struct exynos_drm_manager hdmi_manager = {
	.pipe		= -1,
	.ops		= &drm_hdmi_manager_ops,
	.overlay_ops	= &drm_hdmi_overlay_ops,
	.display_ops	= &drm_hdmi_display_ops,
};

int exynos_drm_hdmi_audio(struct drm_device *drm_dev, void *data,
					struct drm_file *file)
{
	struct drm_exynos_hdmi_audio *audio = data;
	struct drm_exynos_file_private *file_priv = file->driver_priv;
	struct exynos_drm_hdmi_private *priv = file_priv->hdmi_priv;
	struct device *dev = priv->dev;
	struct drm_hdmi_context *ctx = to_context(dev);

	DRM_DEBUG_KMS("%s:type[%d]codec[%d]enable[%d]\n", __func__,
		audio->type, audio->codec, audio->enable);

	if (audio->type > HDMI_TYPE_MAX) {
		DRM_ERROR("invalid audio %d type.\n", audio->type);
		return -EINVAL;
	}

	if (audio->codec > HDMI_CODEC_MAX) {
		DRM_ERROR("invalid audio %d codec.\n", audio->codec);
		return -EINVAL;
	}

	if (hdmi_ops && hdmi_ops->audio_control)
		hdmi_ops->audio_control(ctx->hdmi_ctx->ctx, audio);

	return 0;
}
EXPORT_SYMBOL_GPL(exynos_drm_hdmi_audio);

static int hdmi_subdrv_probe(struct drm_device *drm_dev,
		struct device *dev)
{
	struct exynos_drm_subdrv *subdrv = to_subdrv(dev);
	struct drm_hdmi_context *ctx;
	struct platform_device *pdev = to_platform_device(dev);
	struct exynos_drm_common_hdmi_pd *pd;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	pd = pdev->dev.platform_data;

	if (!pd) {
		DRM_DEBUG_KMS("platform data is null.\n");
		return -EFAULT;
	}

	if (!pd->hdmi_dev) {
		DRM_DEBUG_KMS("hdmi device is null.\n");
		return -EFAULT;
	}

	if (!pd->mixer_dev) {
		DRM_DEBUG_KMS("mixer device is null.\n");
		return -EFAULT;
	}

	ctx = get_ctx_from_subdrv(subdrv);

	ctx->hdmi_ctx = (struct exynos_drm_hdmi_context *)
				to_context(pd->hdmi_dev);
	if (!ctx->hdmi_ctx) {
		DRM_DEBUG_KMS("hdmi context is null.\n");
		return -EFAULT;
	}

	ctx->hdmi_ctx->drm_dev = drm_dev;

	ctx->mixer_ctx = (struct exynos_drm_hdmi_context *)
				to_context(pd->mixer_dev);
	if (!ctx->mixer_ctx) {
		DRM_DEBUG_KMS("mixer context is null.\n");
		return -EFAULT;
	}

	ctx->mixer_ctx->drm_dev = drm_dev;

	return 0;
}

static int hdmi_subdrv_open(struct drm_device *drm_dev, struct device *dev,
							struct drm_file *file)
{
	struct drm_exynos_file_private *file_priv = file->driver_priv;
	struct exynos_drm_hdmi_private *priv;

	DRM_INFO("%s\n", __func__);

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		DRM_ERROR("failed to allocate priv.\n");
		return -ENOMEM;
	}

	priv->dev = dev;
	file_priv->hdmi_priv = priv;

	return 0;
}

static void hdmi_subdrv_close(struct drm_device *drm_dev, struct device *dev,
							struct drm_file *file)
{
	struct drm_exynos_file_private *file_priv = file->driver_priv;

	DRM_INFO("%s\n", __func__);

	kfree(file_priv->hdmi_priv);

	return;
}

static int __devinit exynos_drm_hdmi_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct exynos_drm_subdrv *subdrv;
	struct drm_hdmi_context *ctx;

	DRM_DEBUG_KMS("%s\n", __FILE__);

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		DRM_LOG_KMS("failed to alloc common hdmi context.\n");
		return -ENOMEM;
	}

	subdrv = &ctx->subdrv;

	subdrv->dev = dev;
	subdrv->manager = &hdmi_manager;
	subdrv->probe = hdmi_subdrv_probe;
	subdrv->open = hdmi_subdrv_open;
	subdrv->close = hdmi_subdrv_close;

#if defined(CONFIG_BUSFREQ_OPP) || defined(CONFIG_BUSFREQ_LOCK_WRAPPER)
	/* To lock bus frequency in OPP mode */
	ctx->bus_dev = dev_get("exynos-busfreq");
#endif

	platform_set_drvdata(pdev, subdrv);

	exynos_drm_subdrv_register(subdrv);

	return 0;
}

static int __devexit exynos_drm_hdmi_remove(struct platform_device *pdev)
{
	struct drm_hdmi_context *ctx = platform_get_drvdata(pdev);

	DRM_DEBUG_KMS("%s\n", __FILE__);

#if defined(CONFIG_BUSFREQ_OPP) || defined(CONFIG_BUSFREQ_LOCK_WRAPPER)
	/* To unlock bus frequency in OPP mode */
	dev_put("exynos-busfreq");
#endif

	exynos_drm_subdrv_unregister(&ctx->subdrv);
	kfree(ctx);

	return 0;
}

struct platform_driver exynos_drm_common_hdmi_driver = {
	.probe		= exynos_drm_hdmi_probe,
	.remove		= __devexit_p(exynos_drm_hdmi_remove),
	.driver		= {
		.name	= "exynos-drm-hdmi",
		.owner	= THIS_MODULE,
	},
};
