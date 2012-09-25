/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/err.h>
#include <linux/io.h>
#include "mdss_io_util.h"

void dss_reg_w(struct dss_io_data *io, u32 offset, u32 value, u32 debug)
{
	u32 in_val;

	if (!io || !io->base) {
		DEV_ERR("%s: invalid input\n", __func__);
		return;
	}

	if (offset > io->len) {
		DEV_ERR("%s: offset out of range\n", __func__);
		return;
	}

	writel_relaxed(value, io->base + offset);
	if (debug) {
		in_val = readl_relaxed(io->base + offset);
		DEV_DBG("[%08x] => %08x [%08x]\n", (u32)(io->base + offset),
			value, in_val);
	}
} /* dss_reg_w */

u32 dss_reg_r(struct dss_io_data *io, u32 offset, u32 debug)
{
	u32 value;
	if (!io || !io->base) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	if (offset > io->len) {
		DEV_ERR("%s: offset out of range\n", __func__);
		return -EINVAL;
	}

	value = readl_relaxed(io->base + offset);
	if (debug)
		DEV_DBG("[%08x] <= %08x\n", (u32)(io->base + offset), value);

	return value;
} /* dss_reg_r */

void dss_reg_dump(void __iomem *base, u32 length, const char *prefix,
	u32 debug)
{
	if (debug)
		print_hex_dump(KERN_INFO, prefix, DUMP_PREFIX_OFFSET, 32, 4,
			(void *)base, length, false);
} /* dss_reg_dump */

static struct resource *msm_dss_get_res_byname(struct platform_device *pdev,
	unsigned int type, const char *name)
{
	struct resource *res = NULL;

	res = platform_get_resource_byname(pdev, type, name);
	if (!res)
		DEV_ERR("%s: '%s' resource not found\n", __func__, name);

	return res;
}


int msm_dss_ioremap_byname(struct platform_device *pdev,
	struct dss_io_data *io_data, const char *name)
{
	struct resource *res = NULL;

	if (!pdev || !io_data) {
		DEV_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}

	res = msm_dss_get_res_byname(pdev, IORESOURCE_MEM, name);
	if (!res) {
		DEV_ERR("%s: '%s' msm_dss_get_res_byname failed\n",
			__func__, name);
		return -ENODEV;
	}

	io_data->len = resource_size(res);
	io_data->base = ioremap(res->start, io_data->len);
	if (!io_data->base) {
		DEV_ERR("%s: '%s' ioremap failed\n", __func__, name);
		return -EIO;
	}

	return 0;
}

int msm_dss_config_vreg(struct device *dev, struct dss_vreg *in_vreg,
	int num_vreg, int config)
{
	int i = 0, rc = 0;
	struct dss_vreg *curr_vreg;

	if (config) {
		for (i = 0; i < num_vreg; i++) {
			curr_vreg = &in_vreg[i];
			curr_vreg->vreg = regulator_get(dev,
				curr_vreg->vreg_name);
			rc = IS_ERR(curr_vreg->vreg);
			if (rc) {
				DEV_ERR("%s: %s get failed. rc=%d\n",
					 __func__,
					 curr_vreg->vreg_name, rc);
				curr_vreg->vreg = NULL;
				goto vreg_get_fail;
			}
			if (curr_vreg->type == DSS_REG_LDO) {
				rc = regulator_set_voltage(
					curr_vreg->vreg,
					curr_vreg->min_voltage,
					curr_vreg->max_voltage);
				if (rc < 0) {
					DEV_ERR("%s: %s set voltage failed\n",
						__func__,
						curr_vreg->vreg_name);
					goto vreg_set_voltage_fail;
				}
				if (curr_vreg->optimum_voltage >= 0) {
					rc = regulator_set_optimum_mode(
						curr_vreg->vreg,
						curr_vreg->optimum_voltage);
					if (rc < 0) {
						DEV_ERR(
						"%s: %s set opt mode failed\n",
						__func__,
						curr_vreg->vreg_name);
						goto vreg_set_opt_mode_fail;
					}
				}
			}
		}
	} else {
		for (i = num_vreg-1; i >= 0; i--) {
			curr_vreg = &in_vreg[i];
			if (curr_vreg->vreg) {
				if (curr_vreg->type == DSS_REG_LDO) {
					if (curr_vreg->optimum_voltage >= 0) {
						regulator_set_optimum_mode(
							curr_vreg->vreg, 0);
					}
					regulator_set_voltage(curr_vreg->vreg,
						0, curr_vreg->max_voltage);
				}
				regulator_put(curr_vreg->vreg);
				curr_vreg->vreg = NULL;
			}
		}
	}
	return 0;

vreg_unconfig:
if (curr_vreg->type == DSS_REG_LDO)
	regulator_set_optimum_mode(curr_vreg->vreg, 0);

vreg_set_opt_mode_fail:
if (curr_vreg->type == DSS_REG_LDO)
	regulator_set_voltage(curr_vreg->vreg, 0, curr_vreg->max_voltage);

vreg_set_voltage_fail:
	regulator_put(curr_vreg->vreg);
	curr_vreg->vreg = NULL;

vreg_get_fail:
	for (i--; i >= 0; i--) {
		curr_vreg = &in_vreg[i];
		goto vreg_unconfig;
	}
	return rc;
} /* msm_dss_config_vreg */

int msm_dss_enable_vreg(struct dss_vreg *in_vreg, int num_vreg, int enable)
{
	int i = 0, rc = 0;
	if (enable) {
		for (i = 0; i < num_vreg; i++) {
			rc = IS_ERR(in_vreg[i].vreg);
			if (rc) {
				DEV_ERR("%s: %s regulator error. rc=%d\n",
					__func__, in_vreg[i].vreg_name, rc);
				goto disable_vreg;
			}
			rc = regulator_enable(in_vreg[i].vreg);
			if (rc < 0) {
				DEV_ERR("%s: %s enable failed\n",
					__func__, in_vreg[i].vreg_name);
				goto disable_vreg;
			}
		}
	} else {
		for (i = num_vreg-1; i >= 0; i--)
			if (regulator_is_enabled(in_vreg[i].vreg))
				regulator_disable(in_vreg[i].vreg);
	}
	return rc;

disable_vreg:
	for (i--; i >= 0; i--)
		regulator_disable(in_vreg[i].vreg);
	return rc;
} /* msm_dss_enable_vreg */

int msm_dss_enable_gpio(struct dss_gpio *in_gpio, int num_gpio, int enable)
{
	int i = 0, rc = 0;
	if (enable) {
		for (i = 0; i < num_gpio; i++) {
			rc = gpio_request(in_gpio[i].gpio,
				in_gpio[i].gpio_name);
			if (rc < 0) {
				DEV_ERR("%s: %s enable failed\n",
					__func__, in_gpio[i].gpio_name);
				goto disable_gpio;
			}
		}
	} else {
		for (i = num_gpio-1; i >= 0; i--)
			gpio_free(in_gpio[i].gpio);
	}
	return rc;

disable_gpio:
	for (i--; i >= 0; i--)
		gpio_free(in_gpio[i].gpio);
	return rc;
} /* msm_dss_enable_gpio */
