/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <drm/drm_crtc.h>
#include <linux/debugfs.h>

#include "msm_drv.h"
#include "msm_mmu.h"
#include "sde_kms.h"
#include "sde_formats.h"
#include "sde_hw_mdss.h"
#include "sde_hw_util.h"
#include "sde_hw_intf.h"

static const char * const iommu_ports[] = {
		"mdp_0",
};

static const struct sde_hw_res_map res_table[INTF_MAX] = {
	{ SDE_NONE,	SDE_NONE,	SDE_NONE,	SDE_NONE },
	{ INTF_0,	SDE_NONE,	SDE_NONE,	SDE_NONE },
	{ INTF_1,	LM_0,		PINGPONG_0,	CTL_0 },
	{ INTF_2,	LM_1,		PINGPONG_1,	CTL_1 },
	{ INTF_3,	SDE_NONE,	SDE_NONE,	CTL_2 },
};


#define DEFAULT_MDP_SRC_CLK 300000000

/**
 * Controls size of event log buffer. Specified as a power of 2.
 */
#define SDE_EVTLOG_SIZE	1024

/*
 * To enable overall DRM driver logging
 * # echo 0x2 > /sys/module/drm/parameters/debug
 *
 * To enable DRM driver h/w logging
 * # echo <mask> > /sys/kernel/debug/dri/0/hw_log_mask
 *
 * See sde_hw_mdss.h for h/w logging mask definitions (search for SDE_DBG_MASK_)
 */
#define SDE_DEBUGFS_DIR "msm_sde"
#define SDE_DEBUGFS_HWMASKNAME "hw_log_mask"

int sde_disable(struct sde_kms *sde_kms)
{
	DBG("");

	clk_disable_unprepare(sde_kms->ahb_clk);
	clk_disable_unprepare(sde_kms->axi_clk);
	clk_disable_unprepare(sde_kms->core_clk);
	clk_disable_unprepare(sde_kms->vsync_clk);
	if (sde_kms->lut_clk)
		clk_disable_unprepare(sde_kms->lut_clk);

	return 0;
}

int sde_enable(struct sde_kms *sde_kms)
{
	DBG("");

	clk_prepare_enable(sde_kms->ahb_clk);
	clk_prepare_enable(sde_kms->axi_clk);
	clk_prepare_enable(sde_kms->core_clk);
	clk_prepare_enable(sde_kms->vsync_clk);
	if (sde_kms->lut_clk)
		clk_prepare_enable(sde_kms->lut_clk);

	return 0;
}

static int sde_debugfs_show_regset32(struct seq_file *s, void *data)
{
	struct sde_debugfs_regset32 *regset = s->private;
	void __iomem *base;
	int i;

	base = regset->base + regset->offset;

	for (i = 0; i < regset->blk_len; i += 4)
		seq_printf(s, "[%x] 0x%08x\n",
				regset->offset + i, readl_relaxed(base + i));

	return 0;
}

static int sde_debugfs_open_regset32(struct inode *inode, struct file *file)
{
	return single_open(file, sde_debugfs_show_regset32, inode->i_private);
}

static const struct file_operations sde_fops_regset32 = {
	.open =		sde_debugfs_open_regset32,
	.read =		seq_read,
	.llseek =	seq_lseek,
	.release =	single_release,
};

void sde_debugfs_setup_regset32(struct sde_debugfs_regset32 *regset,
		uint32_t offset, uint32_t length, void __iomem *base)
{
	if (regset) {
		regset->offset = offset;
		regset->blk_len = length;
		regset->base = base;
	}
}

void *sde_debugfs_create_regset32(const char *name, umode_t mode,
		void *parent, struct sde_debugfs_regset32 *regset)
{
	if (!name || !regset || !regset->base || !regset->blk_len)
		return NULL;

	/* make sure offset is a multiple of 4 */
	regset->offset = round_down(regset->offset, 4);

	return debugfs_create_file(name, mode, parent,
			regset, &sde_fops_regset32);
}

void *sde_debugfs_get_root(struct sde_kms *sde_kms)
{
	return sde_kms ? sde_kms->debugfs_root : 0;
}

static int sde_debugfs_init(struct sde_kms *sde_kms)
{
	void *p;

	p = sde_hw_util_get_log_mask_ptr();

	if (!sde_kms || !p)
		return -EINVAL;

	if (sde_kms->dev && sde_kms->dev->primary)
		sde_kms->debugfs_root = sde_kms->dev->primary->debugfs_root;
	else
		sde_kms->debugfs_root = debugfs_create_dir(SDE_DEBUGFS_DIR, 0);

	/* allow debugfs_root to be NULL */
	debugfs_create_x32(SDE_DEBUGFS_HWMASKNAME,
			0644, sde_kms->debugfs_root, p);
	return 0;
}

static void sde_debugfs_destroy(struct sde_kms *sde_kms)
{
	/* don't need to NULL check debugfs_root */
	if (sde_kms) {
		debugfs_remove_recursive(sde_kms->debugfs_root);
		sde_kms->debugfs_root = 0;
	}
}

static void sde_prepare_commit(struct msm_kms *kms,
		struct drm_atomic_state *state)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);
	sde_enable(sde_kms);
}

static void sde_commit(struct msm_kms *kms, struct drm_atomic_state *old_state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state;
	int i;

	for_each_crtc_in_state(old_state, crtc, old_crtc_state, i)
		if (crtc->state->active)
			sde_crtc_commit_kickoff(crtc);
}

static void sde_complete_commit(struct msm_kms *kms,
		struct drm_atomic_state *state)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);
	sde_disable(sde_kms);
}

static void sde_wait_for_crtc_commit_done(struct msm_kms *kms,
		struct drm_crtc *crtc)
{
	sde_crtc_wait_for_commit_done(crtc);
}

static int modeset_init(struct sde_kms *sde_kms)
{
	struct msm_drm_private *priv = sde_kms->dev->dev_private;
	int i;
	int ret;
	struct sde_mdss_cfg *catalog = sde_kms->catalog;
	struct drm_device *dev = sde_kms->dev;
	struct drm_plane *primary_planes[MAX_PLANES];
	int primary_planes_idx = 0;

	int num_private_planes = catalog->mixer_count;

	ret = sde_irq_domain_init(sde_kms);
	if (ret)
		goto fail;

	/* Create the planes */
	for (i = 0; i < catalog->sspp_count; i++) {
		struct drm_plane *plane;
		bool primary = true;

		if (catalog->sspp[i].features & BIT(SDE_SSPP_CURSOR)
			|| !num_private_planes)
			primary = false;

		plane = sde_plane_init(dev, catalog->sspp[i].id, primary);
		if (IS_ERR(plane)) {
			DRM_ERROR("sde_plane_init failed\n");
			ret = PTR_ERR(plane);
			goto fail;
		}
		priv->planes[priv->num_planes++] = plane;

		if (primary)
			primary_planes[primary_planes_idx++] = plane;
		if (primary && num_private_planes)
			num_private_planes--;
	}

	/* Need enough primary planes to assign one per mixer (CRTC) */
	if (primary_planes_idx < catalog->mixer_count) {
		ret = -EINVAL;
		goto fail;
	}

	/*
	 * Enumerate displays supported
	 */
	sde_encoders_init(dev);

	/* Create one CRTC per display */
	for (i = 0; i < priv->num_encoders; i++) {
		/*
		 * Each CRTC receives a private plane. We start
		 * with first RGB, and then DMA and then VIG.
		 */
		struct drm_crtc *crtc;

		crtc = sde_crtc_init(dev, priv->encoders[i],
				primary_planes[i], i);
		if (IS_ERR(crtc)) {
			ret = PTR_ERR(crtc);
			goto fail;
		}
		priv->crtcs[priv->num_crtcs++] = crtc;
	}

	/*
	 * Iterate through the list of encoders and
	 * set the possible CRTCs
	 */
	for (i = 0; i < priv->num_encoders; i++)
		priv->encoders[i]->possible_crtcs = (1 << priv->num_crtcs) - 1;

	return 0;
fail:
	return ret;
}

static int sde_hw_init(struct msm_kms *kms)
{
	return 0;
}

static long sde_round_pixclk(struct msm_kms *kms, unsigned long rate,
		struct drm_encoder *encoder)
{
	return rate;
}

static void sde_preclose(struct msm_kms *kms, struct drm_file *file)
{
}

static void sde_destroy(struct msm_kms *kms)
{
	struct sde_kms *sde_kms = to_sde_kms(kms);

	sde_debugfs_destroy(sde_kms);
	sde_irq_domain_fini(sde_kms);
	sde_hw_intr_destroy(sde_kms->hw_intr);
	kfree(sde_kms);
}

static const struct msm_kms_funcs kms_funcs = {
	.hw_init         = sde_hw_init,
	.irq_preinstall  = sde_irq_preinstall,
	.irq_postinstall = sde_irq_postinstall,
	.irq_uninstall   = sde_irq_uninstall,
	.irq             = sde_irq,
	.prepare_commit  = sde_prepare_commit,
	.commit          = sde_commit,
	.complete_commit = sde_complete_commit,
	.wait_for_crtc_commit_done = sde_wait_for_crtc_commit_done,
	.enable_vblank   = sde_enable_vblank,
	.disable_vblank  = sde_disable_vblank,
	.get_format      = sde_get_msm_format,
	.round_pixclk    = sde_round_pixclk,
	.preclose        = sde_preclose,
	.destroy         = sde_destroy,
};

static int get_clk(struct platform_device *pdev, struct clk **clkp,
		const char *name, bool mandatory)
{
	struct device *dev = &pdev->dev;
	struct clk *clk = devm_clk_get(dev, name);

	if (IS_ERR(clk) && mandatory) {
		DRM_ERROR("failed to get %s (%ld)\n", name, PTR_ERR(clk));
		return PTR_ERR(clk);
	}
	if (IS_ERR(clk))
		DBG("skipping %s", name);
	else
		*clkp = clk;

	return 0;
}

struct sde_kms *sde_hw_setup(struct platform_device *pdev)
{
	struct sde_kms *sde_kms;
	struct msm_kms *kms = NULL;
	int ret;

	sde_kms = kzalloc(sizeof(*sde_kms), GFP_KERNEL);
	if (!sde_kms)
		return NULL;

	msm_kms_init(&sde_kms->base, &kms_funcs);

	kms = &sde_kms->base;

	sde_kms->mmio = msm_ioremap(pdev, "mdp_phys", "SDE");
	if (IS_ERR(sde_kms->mmio)) {
		ret = PTR_ERR(sde_kms->mmio);
		goto fail;
	}
	DRM_INFO("Mapped Mdp address space @%pK\n", sde_kms->mmio);

	sde_kms->vbif = msm_ioremap(pdev, "vbif_phys", "VBIF");
	if (IS_ERR(sde_kms->vbif)) {
		ret = PTR_ERR(sde_kms->vbif);
		goto fail;
	}

	sde_kms->venus = devm_regulator_get_optional(&pdev->dev, "gdsc-venus");
	if (IS_ERR(sde_kms->venus)) {
		ret = PTR_ERR(sde_kms->venus);
		DBG("failed to get Venus GDSC regulator: %d", ret);
		sde_kms->venus = NULL;
	}

	if (sde_kms->venus) {
		ret = regulator_enable(sde_kms->venus);
		if (ret) {
			DBG("failed to enable venus GDSC: %d", ret);
			goto fail;
		}
	}

	sde_kms->vdd = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(sde_kms->vdd)) {
		ret = PTR_ERR(sde_kms->vdd);
		goto fail;
	}

	ret = regulator_enable(sde_kms->vdd);
	if (ret) {
		DBG("failed to enable regulator vdd: %d", ret);
		goto fail;
	}

	sde_kms->mmagic = devm_regulator_get_optional(&pdev->dev, "mmagic");
	if (IS_ERR(sde_kms->mmagic)) {
		ret = PTR_ERR(sde_kms->mmagic);
		DBG("failed to get mmagic GDSC regulator: %d", ret);
		sde_kms->mmagic = NULL;
	}

	/* mandatory clocks: */
	ret = get_clk(pdev, &sde_kms->axi_clk, "bus_clk", true);
	if (ret)
		goto fail;
	ret = get_clk(pdev, &sde_kms->ahb_clk, "iface_clk", true);
	if (ret)
		goto fail;
	ret = get_clk(pdev, &sde_kms->src_clk, "core_clk_src", true);
	if (ret)
		goto fail;
	ret = get_clk(pdev, &sde_kms->core_clk, "core_clk", true);
	if (ret)
		goto fail;
	ret = get_clk(pdev, &sde_kms->vsync_clk, "vsync_clk", true);
	if (ret)
		goto fail;

	/* optional clocks: */
	get_clk(pdev, &sde_kms->lut_clk, "lut_clk", false);
	get_clk(pdev, &sde_kms->mmagic_clk, "mmagic_clk", false);
	get_clk(pdev, &sde_kms->iommu_clk, "iommu_clk", false);

	if (sde_kms->mmagic) {
		ret = regulator_enable(sde_kms->mmagic);
		if (ret) {
			DRM_ERROR("failed to enable mmagic GDSC: %d\n", ret);
			goto fail;
		}
	}
	if (sde_kms->mmagic_clk) {
		clk_prepare_enable(sde_kms->mmagic_clk);
		if (ret) {
			DRM_ERROR("failed to enable mmagic_clk\n");
			goto undo_gdsc;
		}
	}

	return sde_kms;

undo_gdsc:
	if (sde_kms->mmagic)
		regulator_disable(sde_kms->mmagic);
fail:
	if (kms)
		sde_destroy(kms);

	return ERR_PTR(ret);
}

static int sde_translation_ctrl_pwr(struct sde_kms *sde_kms, bool on)
{
	int ret;

	if (on) {
		if (sde_kms->iommu_clk) {
			ret = clk_prepare_enable(sde_kms->iommu_clk);
			if (ret) {
				DRM_ERROR("failed to enable iommu_clk\n");
				goto undo_mmagic_clk;
			}
		}
	} else {
		if (sde_kms->iommu_clk)
			clk_disable_unprepare(sde_kms->iommu_clk);
		if (sde_kms->mmagic_clk)
			clk_disable_unprepare(sde_kms->mmagic_clk);
		if (sde_kms->mmagic)
			regulator_disable(sde_kms->mmagic);
	}

	return 0;

undo_mmagic_clk:
	if (sde_kms->mmagic_clk)
		clk_disable_unprepare(sde_kms->mmagic_clk);

	return ret;
}
int sde_mmu_init(struct sde_kms *sde_kms)
{
	struct sde_mdss_cfg *catalog = sde_kms->catalog;
	struct sde_hw_intf *intf = NULL;
	struct iommu_domain *iommu;
	struct msm_mmu *mmu;
	int i, ret;

	/*
	 * Make sure things are off before attaching iommu (bootloader could
	 * have left things on, in which case we'll start getting faults if
	 * we don't disable):
	 */
	sde_enable(sde_kms);
	for (i = 0; i < catalog->intf_count; i++) {
		intf = sde_hw_intf_init(catalog->intf[i].id,
				sde_kms->mmio,
				catalog);
		if (!IS_ERR_OR_NULL(intf)) {
			intf->ops.enable_timing(intf, 0x0);
			sde_hw_intf_deinit(intf);
		}
	}
	sde_disable(sde_kms);
	msleep(20);

	iommu = iommu_domain_alloc(&platform_bus_type);

	if (!IS_ERR_OR_NULL(iommu)) {
		mmu = msm_smmu_new(sde_kms->dev->dev, MSM_SMMU_DOMAIN_UNSECURE);
		if (IS_ERR(mmu)) {
			ret = PTR_ERR(mmu);
			DRM_ERROR("failed to init iommu: %d\n", ret);
			iommu_domain_free(iommu);
			goto fail;
		}

		ret = sde_translation_ctrl_pwr(sde_kms, true);
		if (ret) {
			DRM_ERROR("failed to power iommu: %d\n", ret);
			mmu->funcs->destroy(mmu);
			goto fail;
		}

		ret = mmu->funcs->attach(mmu, (const char **)iommu_ports,
				ARRAY_SIZE(iommu_ports));
		if (ret) {
			DRM_ERROR("failed to attach iommu: %d\n", ret);
			mmu->funcs->destroy(mmu);
			goto fail;
		}
	} else {
		dev_info(sde_kms->dev->dev,
			"no iommu, fallback to phys contig buffers for scanout\n");
		mmu = NULL;
	}
	sde_kms->mmu = mmu;

	sde_kms->mmu_id = msm_register_mmu(sde_kms->dev, mmu);
	if (sde_kms->mmu_id < 0) {
		ret = sde_kms->mmu_id;
		DRM_ERROR("failed to register sde iommu: %d\n", ret);
		goto fail;
	}

	return 0;
fail:
	return ret;

}

struct msm_kms *sde_kms_init(struct drm_device *dev)
{
	struct msm_drm_private *priv = dev->dev_private;
	struct platform_device *pdev = dev->platformdev;
	struct sde_mdss_cfg *catalog;
	struct sde_kms *sde_kms;
	struct msm_kms *msm_kms;
	int ret = 0;

	if (!dev || !dev->dev_private) {
		DRM_ERROR("invalid device\n");
		goto fail;
	}

	sde_kms = sde_hw_setup(pdev);
	if (IS_ERR(sde_kms)) {
		ret = PTR_ERR(sde_kms);
		goto fail;
	}

	sde_kms->dev = dev;
	msm_kms = &sde_kms->base;
	priv->kms = msm_kms;

	/*
	 * Currently hardcoding to MDSS version 1.7.0 (8996)
	 */
	catalog = sde_hw_catalog_init(1, 7, 0);
	if (!catalog)
		goto fail;

	sde_kms->catalog = catalog;

	/* we need to set a default rate before enabling.
	 * Set a safe rate first, before initializing catalog
	 * later set more optimal rate based on bandwdith/clock
	 * requirements
	 */

	clk_set_rate(sde_kms->src_clk, DEFAULT_MDP_SRC_CLK);
	sde_enable(sde_kms);
	sde_kms->hw_res.res_table = res_table;

	/*
	 * Now we need to read the HW catalog and initialize resources such as
	 * clocks, regulators, GDSC/MMAGIC, ioremap the register ranges etc
	 */
	sde_mmu_init(sde_kms);

	/*
	 * NOTE: Calling sde_debugfs_init here so that the drm_minor device for
	 *       'primary' is already created.
	 */
	sde_debugfs_init(sde_kms);
	msm_evtlog_init(&priv->evtlog, SDE_EVTLOG_SIZE,
			sde_debugfs_get_root(sde_kms));
	MSM_EVT(dev, 0, 0);

	/*
	 * modeset_init should create the DRM related objects i.e. CRTCs,
	 * planes, encoders, connectors and so forth
	 */
	modeset_init(sde_kms);

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;

	/*
	 * we can assume the max crtc width is equal to the max supported
	 * by LM_0
	 * Also fixing the max height to 4k
	 */
	dev->mode_config.max_width =  catalog->mixer[0].sblk->maxwidth;
	dev->mode_config.max_height = 4096;

	sde_kms->hw_intr = sde_rm_acquire_intr(sde_kms);

	if (IS_ERR_OR_NULL(sde_kms->hw_intr))
		goto fail;

	return msm_kms;

fail:
	if (msm_kms)
		sde_destroy(msm_kms);

	return ERR_PTR(ret);
}
