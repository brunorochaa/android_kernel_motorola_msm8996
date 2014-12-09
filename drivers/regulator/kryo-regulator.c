/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/kryo-regulator.h>

#define KRYO_REGULATOR_DRIVER_NAME	"kryo-regulator"

#define kvreg_err(kvreg, message, ...) \
	pr_err("%s: " message, (kvreg)->name, ##__VA_ARGS__)
#define kvreg_info(kvreg, message, ...) \
	pr_info("%s: " message, (kvreg)->name, ##__VA_ARGS__)
#define kvreg_debug(kvreg, message, ...) \
	pr_debug("%s: " message, (kvreg)->name, ##__VA_ARGS__)

/* CPUSS power domain register offsets */
#define APCC_PWR_CTL_OVERRIDE		0x38
#define APCC_PGS_RET_STATUS		0xe0

/* APCS CSR register offsets */
#define APCS_VERSION		0xfd0

/* Cluster power domain register offsets */
#define APC_LDO_VREF_SET	0x08
#define APC_RET_VREF_SET	0x10
#define APC_PWR_GATE_MODE	0x18
#define APC_PWR_GATE_DLY	0x28
#define APC_LDO_CFG		0x40
#define APC_IMEAS_CFG		0x48
#define APC_APM_CFG		0x50
#define APC_PGSCTL_STS		0x60

/* CPU power domain register offsets */
#define APC_CPU_MISC_EN		0x30

/* L2 power domain register offsets */
#define APC_L2_MISC_EN		0x28

/* Register bit mask definitions*/
#define PWR_GATE_SWITCH_MODE_MASK	GENMASK(0, 0)
#define VREF_MASK			GENMASK(6, 0)
#define APM_CFG_MASK			GENMASK(7, 0)
#define FSM_CUR_STATE_MASK		GENMASK(5, 4)
#define APC_PWR_GATE_DLY_MASK		GENMASK(11, 0)
#define IMEAS_CFG_MASK			GENMASK(15, 0)
#define APCC_PGS_MASK(cluster)		(0x7 << (0x3 * (cluster)))

/* Register bit definitions */
#define VREF_BIT_POS		0

/* Maximum delay to wait before declaring a Power Gate Switch timed out */
#define PWR_GATE_SWITCH_TIMEOUT_US	5

#define PWR_GATE_SWITCH_MODE_LDO	0
#define PWR_GATE_SWITCH_MODE_BHS	1
#define MSM8996_CPUSS_VER_1P1	0x10010000

#define LDO_N_VOLTAGES		0x80

struct kryo_regulator {
	struct list_head		link;
	struct regulator_desc		desc;
	struct regulator_dev		*rdev;
	struct regulator_dev		*retention_rdev;
	struct regulator_desc		retention_desc;
	const char			*name;
	enum kryo_supply_mode		mode;
	enum kryo_supply_mode		retention_mode;
	void __iomem			*reg_base;
	void __iomem			*pm_apcc_base;
	void __iomem			*pm_cpu0_base;
	void __iomem			*pm_cpu1_base;
	void __iomem			*pm_l2_base;
	struct dentry			*debugfs;
	struct mutex			lock;
	int				volt;
	int				retention_volt;
	int				vref_func_step_volt;
	int				vref_func_min_volt;
	int				vref_func_max_volt;
	int				vref_ret_step_volt;
	int				vref_ret_min_volt;
	int				vref_ret_max_volt;
	int				cluster_num;
	u32				version;
	bool				vreg_en;
};

static struct dentry *kryo_debugfs_base;
static DEFINE_MUTEX(kryo_regulator_list_mutex);
static LIST_HEAD(kryo_regulator_list);

static bool is_between(int left, int right, int value)
{
	if (left >= right && left >= value && value >= right)
		return true;
	if (left <= right && left <= value && value <= right)
		return true;

	return false;
}

static void kryo_masked_write(struct kryo_regulator *kvreg,
			      int reg, u32 mask, u32 val)
{
	u32 reg_val, orig_val;

	reg_val = orig_val = readl_relaxed(kvreg->reg_base + reg);
	reg_val &= ~mask;
	reg_val |= (val & mask);

	if (reg_val != orig_val) {
		writel_relaxed(reg_val, kvreg->reg_base + reg);

		/* Ensure write above completes */
		mb();
	}
}

static inline void kryo_pm_apcc_masked_write(struct kryo_regulator *kvreg,
			       int reg, u32 mask, u32 val)
{
	u32 reg_val, orig_val;

	reg_val = orig_val = readl_relaxed(kvreg->pm_apcc_base + reg);
	reg_val &= ~mask;
	reg_val |= (val & mask);

	if (reg_val != orig_val) {
		writel_relaxed(reg_val, kvreg->pm_apcc_base + reg);

		/* Ensure write above completes */
		mb();
	}
}

static inline int kryo_decode_retention_volt(struct kryo_regulator *kvreg,
					     int reg)
{
	return kvreg->vref_ret_min_volt + reg * kvreg->vref_ret_step_volt;
}

static inline int kryo_encode_retention_volt(struct kryo_regulator *kvreg,
					     int volt)
{
	int encoded_volt = DIV_ROUND_UP(volt - kvreg->vref_ret_min_volt,
				      kvreg->vref_ret_step_volt);

	if (encoded_volt >= LDO_N_VOLTAGES || encoded_volt < 0)
		return -EINVAL;
	else
		return encoded_volt;
}

static inline int kryo_decode_functional_volt(struct kryo_regulator *kvreg,
					      int reg)
{
	return kvreg->vref_func_min_volt + reg * kvreg->vref_func_step_volt;
}

static inline int kryo_encode_functional_volt(struct kryo_regulator *kvreg,
					      int volt)
{
	int encoded_volt = DIV_ROUND_UP(volt - kvreg->vref_func_min_volt,
				      kvreg->vref_func_step_volt);

	if (encoded_volt >= LDO_N_VOLTAGES || encoded_volt < 0)
		return -EINVAL;
	else
		return encoded_volt;
}

static int kryo_set_retention_volt(struct kryo_regulator *kvreg, int volt)
{
	int reg_val;

	reg_val = kryo_encode_retention_volt(kvreg, volt);
	if (reg_val < 0) {
		kvreg_err(kvreg, "unsupported LDO retention voltage, rc=%d\n",
			  reg_val);
		return reg_val;
	}

	kryo_masked_write(kvreg, APC_RET_VREF_SET, VREF_MASK,
			   reg_val << VREF_BIT_POS);

	kvreg->retention_volt = kryo_decode_retention_volt(kvreg, reg_val);
	kvreg_debug(kvreg, "Set LDO retention voltage=%d uV (0x%x)\n",
		    kvreg->retention_volt, reg_val);

	return 0;
}

static int kryo_set_ldo_volt(struct kryo_regulator *kvreg, int volt)
{
	int reg_val;

	/*
	 * Assume the consumer ensures the requested voltage satisfies the
	 * headroom and adjustment voltage requirements. The value may be
	 * rounded up if necessary, to match the LDO resolution. Configure it.
	 */
	reg_val = kryo_encode_functional_volt(kvreg, volt);
	if (reg_val < 0) {
		kvreg_err(kvreg, "unsupported LDO functional voltage, rc=%d\n",
			  reg_val);
		return reg_val;
	}

	kryo_masked_write(kvreg, APC_LDO_VREF_SET, VREF_MASK,
			   reg_val << VREF_BIT_POS);

	kvreg->volt = kryo_decode_functional_volt(kvreg, reg_val);
	kvreg_debug(kvreg, "Set LDO voltage=%d uV (0x%x)\n",
		    kvreg->volt, reg_val);

	return 0;
}

static int kryo_configure_mode(struct kryo_regulator *kvreg,
				enum kryo_supply_mode mode)
{
	u32 reg;
	int timeout = PWR_GATE_SWITCH_TIMEOUT_US;

	/* Configure LDO or BHS mode */
	kryo_masked_write(kvreg, APC_PWR_GATE_MODE, PWR_GATE_SWITCH_MODE_MASK,
			  mode == LDO_MODE ? PWR_GATE_SWITCH_MODE_LDO
			  : PWR_GATE_SWITCH_MODE_BHS);

	/* Complete register write before reading HW status register */
	mb();

	/* Delay to allow Power Gate Switch FSM to reach idle state */
	while (timeout > 0) {
		reg = readl_relaxed(kvreg->reg_base + APC_PGSCTL_STS);
		if (!(reg & FSM_CUR_STATE_MASK))
			break;

		udelay(1);
		timeout--;
	}

	if (timeout == 0) {
		kvreg_err(kvreg, "PGS switch to %s failed. APC_PGSCTL_STS=0x%x\n",
			  mode == LDO_MODE ? "LDO" : "BHS", reg);
		return -ETIMEDOUT;
	}

	kvreg->mode = mode;
	kvreg_debug(kvreg, "using %s mode\n", mode == LDO_MODE ? "LDO" : "BHS");

	return 0;
}

static int kryo_regulator_enable(struct regulator_dev *rdev)
{
	struct kryo_regulator *kvreg = rdev_get_drvdata(rdev);
	int rc;

	if (kvreg->vreg_en == true)
		return 0;

	mutex_lock(&kvreg->lock);
	rc = kryo_set_ldo_volt(kvreg, kvreg->volt);
	if (rc) {
		kvreg_err(kvreg, "set voltage failed, rc=%d\n", rc);
		goto done;
	}

	kvreg->vreg_en = true;
	kvreg_debug(kvreg, "enabled\n");

done:
	mutex_unlock(&kvreg->lock);

	return rc;
}

static int kryo_regulator_disable(struct regulator_dev *rdev)
{
	struct kryo_regulator *kvreg = rdev_get_drvdata(rdev);
	int rc;

	if (kvreg->vreg_en == false)
		return 0;

	mutex_lock(&kvreg->lock);
	kvreg->vreg_en = false;
	kvreg_debug(kvreg, "disabled\n");
	mutex_unlock(&kvreg->lock);

	return rc;
}

static int kryo_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct kryo_regulator *kvreg = rdev_get_drvdata(rdev);

	return kvreg->vreg_en;
}

static int kryo_regulator_set_voltage(struct regulator_dev *rdev,
			int min_volt, int max_volt, unsigned *selector)
{
	struct kryo_regulator *kvreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&kvreg->lock);
	if (!kvreg->vreg_en) {
		kvreg->volt = min_volt;
		mutex_unlock(&kvreg->lock);
		return 0;
	}

	rc = kryo_set_ldo_volt(kvreg, min_volt);
	if (rc)
		kvreg_err(kvreg, "set voltage failed, rc=%d\n", rc);

	mutex_unlock(&kvreg->lock);

	return rc;
}

static int kryo_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct kryo_regulator *kvreg = rdev_get_drvdata(rdev);

	return kvreg->volt;
}

static int kryo_regulator_set_bypass(struct regulator_dev *rdev,
				  bool enable)
{
	struct kryo_regulator *kvreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&kvreg->lock);
	rc = kryo_configure_mode(kvreg, enable);
	if (rc)
		kvreg_err(kvreg, "could not configure to %s mode\n",
			  enable == LDO_MODE ? "LDO" : "BHS");
	mutex_unlock(&kvreg->lock);

	return rc;
}

static int kryo_regulator_get_bypass(struct regulator_dev *rdev,
				  bool *enable)
{
	struct kryo_regulator *kvreg = rdev_get_drvdata(rdev);

	*enable = kvreg->mode;

	return 0;
}

static int kryo_regulator_list_voltage(struct regulator_dev *rdev,
				       unsigned selector)
{
	struct kryo_regulator *kvreg = rdev_get_drvdata(rdev);

	if (selector < kvreg->desc.n_voltages)
		return kryo_decode_functional_volt(kvreg, selector);
	else
		return 0;
}

static int kryo_regulator_retention_set_voltage(struct regulator_dev *rdev,
			int min_volt, int max_volt, unsigned *selector)
{
	struct kryo_regulator *kvreg = rdev_get_drvdata(rdev);
	int rc;

	mutex_lock(&kvreg->lock);
	rc = kryo_set_retention_volt(kvreg, min_volt);
	if (rc)
		kvreg_err(kvreg, "set voltage failed, rc=%d\n", rc);

	mutex_unlock(&kvreg->lock);

	return rc;
}

static int kryo_regulator_retention_get_voltage(struct regulator_dev *rdev)
{
	struct kryo_regulator *kvreg = rdev_get_drvdata(rdev);

	return kvreg->retention_volt;
}

static int kryo_regulator_retention_set_bypass(struct regulator_dev *rdev,
				  bool enable)
{
	struct kryo_regulator *kvreg = rdev_get_drvdata(rdev);
	int timeout = PWR_GATE_SWITCH_TIMEOUT_US;
	int rc = 0;
	u32 reg_val;

	mutex_lock(&kvreg->lock);
	kryo_pm_apcc_masked_write(kvreg,
				  APCC_PWR_CTL_OVERRIDE,
				  APCC_PGS_MASK(kvreg->cluster_num),
				  enable ?
				  0 : APCC_PGS_MASK(kvreg->cluster_num));

	/* Ensure write above completes before proceeding */
	mb();

	if (kvreg->version < MSM8996_CPUSS_VER_1P1) {
		/* No status register, delay worst case */
		udelay(PWR_GATE_SWITCH_TIMEOUT_US);
	} else {
		while (timeout > 0) {
			reg_val = readl_relaxed(kvreg->pm_apcc_base
						+ APCC_PGS_RET_STATUS);
			if (!(reg_val & APCC_PGS_MASK(kvreg->cluster_num)))
				break;

			udelay(1);
			timeout--;
		}

		if (timeout == 0) {
			kvreg_err(kvreg, "PGS switch timed out. APCC_PGS_RET_STATUS=0x%x\n",
				  reg_val);
			rc = -ETIMEDOUT;
			goto done;
		}
	}

	/* Bypassed LDO retention operation == disallow LDO retention */
	kvreg_debug(kvreg, "%s LDO retention\n",
		    enable ? "enabled" : "disabled");
	kvreg->retention_mode = enable == LDO_MODE ? LDO_MODE
		: BHS_MODE;

done:
	mutex_unlock(&kvreg->lock);
	return rc;
}

static int kryo_regulator_retention_get_bypass(struct regulator_dev *rdev,
				  bool *enable)
{
	struct kryo_regulator *kvreg = rdev_get_drvdata(rdev);

	*enable = kvreg->retention_mode;

	return 0;
}

static int kryo_regulator_retention_list_voltage(struct regulator_dev *rdev,
				       unsigned selector)
{
	struct kryo_regulator *kvreg = rdev_get_drvdata(rdev);

	if (selector < kvreg->retention_desc.n_voltages)
		return kryo_decode_retention_volt(kvreg, selector);
	else
		return 0;
}

static struct regulator_ops kryo_regulator_ops = {
	.enable			= kryo_regulator_enable,
	.disable		= kryo_regulator_disable,
	.is_enabled		= kryo_regulator_is_enabled,
	.set_voltage		= kryo_regulator_set_voltage,
	.get_voltage		= kryo_regulator_get_voltage,
	.set_bypass		= kryo_regulator_set_bypass,
	.get_bypass		= kryo_regulator_get_bypass,
	.list_voltage		= kryo_regulator_list_voltage,
};

static struct regulator_ops kryo_regulator_retention_ops = {
	.set_voltage		= kryo_regulator_retention_set_voltage,
	.get_voltage		= kryo_regulator_retention_get_voltage,
	.set_bypass		= kryo_regulator_retention_set_bypass,
	.get_bypass		= kryo_regulator_retention_get_bypass,
	.list_voltage		= kryo_regulator_retention_list_voltage,
};

static void kryo_ldo_voltage_init(struct kryo_regulator *kvreg)
{
	kryo_set_retention_volt(kvreg, kvreg->retention_volt);
	kryo_set_ldo_volt(kvreg, kvreg->volt);
}

#define APC_PWR_GATE_DLY_INIT		0x00000101
#define APC_LDO_CFG_INIT		0x31f0e471
#define APC_APM_CFG_INIT		0x00000000
#define APC_IMEAS_CFG_INIT		0x0000eba2
#define APC_MISC_EN_VAL			0x00000001
static int kryo_hw_init(struct kryo_regulator *kvreg)
{
	/* Set up VREF_LDO and VREF_RET */
	kryo_ldo_voltage_init(kvreg);

	/* Program LDO, ODCM, and APM configuration registers */
	writel_relaxed(APC_LDO_CFG_INIT, kvreg->reg_base + APC_LDO_CFG);

	kryo_masked_write(kvreg, APC_IMEAS_CFG, IMEAS_CFG_MASK,
			    APC_IMEAS_CFG_INIT);

	kryo_masked_write(kvreg, APC_APM_CFG, APM_CFG_MASK,
			    APC_APM_CFG_INIT);

	/* Configure power gate sequencer delay */
	kryo_masked_write(kvreg, APC_PWR_GATE_DLY, APC_PWR_GATE_DLY_MASK,
			   APC_PWR_GATE_DLY_INIT);

	/* Enable ADC circuits in APC power gate switch */
	writel_relaxed(APC_MISC_EN_VAL, kvreg->pm_cpu0_base + APC_CPU_MISC_EN);
	writel_relaxed(APC_MISC_EN_VAL, kvreg->pm_cpu1_base + APC_CPU_MISC_EN);
	writel_relaxed(APC_MISC_EN_VAL, kvreg->pm_l2_base + APC_L2_MISC_EN);

	/* Complete the above writes before other accesses */
	mb();

	return 0;
}

static ssize_t kryo_dbg_mode_read(struct file *file, char __user *buff,
				   size_t count, loff_t *ppos)
{
	struct kryo_regulator *kvreg = file->private_data;
	char buf[10];
	int len = 0;
	u32 reg_val;

	if (!kvreg)
		return -ENODEV;

	/* Confirm HW state matches Kryo regulator device state */
	mutex_lock(&kvreg->lock);
	reg_val = readl_relaxed(kvreg->reg_base + APC_PWR_GATE_MODE);
	if (((reg_val & PWR_GATE_SWITCH_MODE_MASK) == PWR_GATE_SWITCH_MODE_LDO
	     && kvreg->mode != LDO_MODE) ||
	    ((reg_val & PWR_GATE_SWITCH_MODE_MASK) == PWR_GATE_SWITCH_MODE_BHS
	     && kvreg->mode != BHS_MODE)) {
		kvreg_err(kvreg, "HW state disagrees on PWR gate mode! reg=0x%x\n",
			  reg_val);
		len = snprintf(buf, sizeof(buf), "ERR\n");
	} else {
		len = snprintf(buf, sizeof(buf), "%s\n",
			       kvreg->mode == LDO_MODE ?
			       "LDO" : "BHS");
	}
	mutex_unlock(&kvreg->lock);

	return simple_read_from_buffer(buff, count, ppos, buf, len);
}

static int kryo_dbg_base_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static const struct file_operations kryo_dbg_mode_fops = {
	.open = kryo_dbg_base_open,
	.read = kryo_dbg_mode_read,
};

static void kryo_debugfs_init(struct kryo_regulator *kvreg)
{
	struct dentry *temp;

	if (IS_ERR_OR_NULL(kryo_debugfs_base)) {
		if (PTR_ERR(kryo_debugfs_base) != -ENODEV)
			kvreg_err(kvreg, "Base directory missing, cannot create debugfs nodes rc=%ld\n",
				  PTR_ERR(kryo_debugfs_base));
		return;
	}

	kvreg->debugfs = debugfs_create_dir(kvreg->name, kryo_debugfs_base);

	if (IS_ERR_OR_NULL(kvreg->debugfs)) {
		kvreg_err(kvreg, "debugfs directory creation failed rc=%ld\n",
			  PTR_ERR(kvreg->debugfs));
		return;
	}

	temp = debugfs_create_file("mode", S_IRUGO, kvreg->debugfs,
				   kvreg, &kryo_dbg_mode_fops);

	if (IS_ERR_OR_NULL(temp)) {
		kvreg_err(kvreg, "mode node creation failed rc=%ld\n",
			PTR_ERR(temp));
		return;
	}
}

static void kryo_debugfs_deinit(struct kryo_regulator *kvreg)
{
	debugfs_remove_recursive(kvreg->debugfs);
}

static void kryo_debugfs_base_init(void)
{
	kryo_debugfs_base = debugfs_create_dir(KRYO_REGULATOR_DRIVER_NAME,
						NULL);
	if (IS_ERR_OR_NULL(kryo_debugfs_base)) {
		if (PTR_ERR(kryo_debugfs_base) != -ENODEV)
			pr_err("%s debugfs base directory creation failed rc=%ld\n",
			       KRYO_REGULATOR_DRIVER_NAME,
			       PTR_ERR(kryo_debugfs_base));
	}
}

static void kryo_debugfs_base_remove(void)
{
	debugfs_remove_recursive(kryo_debugfs_base);
}

static int kryo_regulator_init_data(struct platform_device *pdev,
				    struct kryo_regulator *kvreg)
{
	int rc = 0;
	struct device *dev = &pdev->dev;
	struct resource *res;
	struct device_node *node;
	void __iomem *temp;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pm-apc");
	if (!res) {
		dev_err(dev, "PM APC register address missing\n");
		return -EINVAL;
	}

	kvreg->reg_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!kvreg->reg_base) {
		dev_err(dev, "failed to map PM APC registers\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pm-apcc");
	if (!res) {
		dev_err(dev, "PM APCC register address missing\n");
		return -EINVAL;
	}

	kvreg->pm_apcc_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!kvreg->pm_apcc_base) {
		dev_err(dev, "failed to map PM APCC registers\n");
		return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "apcs-csr");
	if (!res) {
		dev_err(dev, "missing APCS CSR physical base address");
		return -EINVAL;
	}

	temp = ioremap(res->start, resource_size(res));
	if (!temp) {
		dev_err(dev, "failed to map APCS CSR registers\n");
		return -ENOMEM;
	}

	kvreg->version = readl_relaxed(temp + APCS_VERSION);
	iounmap(temp);

	rc = of_property_read_u32(dev->of_node,
				  "qcom,vref-functional-step-voltage",
				  &kvreg->vref_func_step_volt);
	if (rc < 0) {
		dev_err(dev, "qcom,vref-functional-step-voltage missing rc=%d\n",
			rc);
		return rc;
	}

	rc = of_property_read_u32(dev->of_node,
				  "qcom,vref-functional-min-voltage",
				  &kvreg->vref_func_min_volt);
	if (rc < 0) {
		dev_err(dev, "qcom,vref-functional-min-voltage missing rc=%d\n",
			rc);
		return rc;
	}

	kvreg->vref_func_max_volt = kryo_decode_functional_volt(kvreg,
							LDO_N_VOLTAGES - 1);

	rc = of_property_read_u32(dev->of_node,
				  "qcom,vref-retention-step-voltage",
				  &kvreg->vref_ret_step_volt);
	if (rc < 0) {
		dev_err(dev, "qcom,vref-retention-step-voltage missing rc=%d\n",
			rc);
		return rc;
	}

	rc = of_property_read_u32(dev->of_node,
				  "qcom,vref-retention-min-voltage",
				  &kvreg->vref_ret_min_volt);
	if (rc < 0) {
		dev_err(dev, "qcom,vref-retention-min-voltage missing rc=%d\n",
			rc);
		return rc;
	}

	kvreg->vref_ret_max_volt = kryo_decode_retention_volt(kvreg,
						      LDO_N_VOLTAGES - 1);

	rc = of_property_read_u32(dev->of_node, "qcom,ldo-default-voltage",
				  &kvreg->volt);
	if (rc < 0) {
		dev_err(dev, "qcom,ldo-default-voltage missing rc=%d\n", rc);
		return rc;
	}
	if (!is_between(kvreg->vref_func_min_volt,
			kvreg->vref_func_max_volt,
			kvreg->volt)) {
		dev_err(dev, "qcom,ldo-default-voltage=%d uV outside allowed range\n",
				kvreg->volt);
		return -EINVAL;
	}

	rc = of_property_read_u32(dev->of_node, "qcom,retention-voltage",
				  &kvreg->retention_volt);
	if (rc < 0) {
		dev_err(dev, "qcom,retention-voltage missing rc=%d\n", rc);
		return rc;
	}
	if (!is_between(kvreg->vref_ret_min_volt,
			kvreg->vref_ret_max_volt,
			kvreg->retention_volt)) {
		dev_err(dev, "qcom,retention-voltage=%d uV outside allowed range\n",
			kvreg->retention_volt);
		return -EINVAL;
	}

	node = of_parse_phandle(dev->of_node, "qcom,pm-cpu0", 0);
	if (!node) {
		dev_err(dev, "qcom,pm-cpu0 missing\n");
		return -EINVAL;

	}

	if (of_address_to_resource(node, 0, res)) {
		dev_err(dev, "no physical address for pm-cpu0 resource\n");
		return -EINVAL;
	}

	kvreg->pm_cpu0_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!kvreg->pm_cpu0_base) {
		dev_err(dev, "failed to map CPU0 PM registers\n");
		return -ENOMEM;
	}

	node = of_parse_phandle(dev->of_node, "qcom,pm-cpu1", 0);
	if (!node) {
		dev_err(dev, "qcom,pm-cpu1 missing\n");
		return -EINVAL;
	}

	if (of_address_to_resource(node, 0, res)) {
		dev_err(dev, "no physical address for pm-cpu1 resource\n");
		return -EINVAL;
	}

	kvreg->pm_cpu1_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!kvreg->pm_cpu1_base) {
		dev_err(dev, "failed to map CPU1 PM registers\n");
		return -ENOMEM;
	}

	node = of_parse_phandle(dev->of_node, "qcom,pm-l2", 0);
	if (!node) {
		dev_err(dev, "qcom,pm-l2 missing\n");
		return -EINVAL;
	}

	if (of_address_to_resource(node, 0, res)) {
		dev_err(dev, "no physical address for pm-l2 resource\n");
		return -EINVAL;
	}

	kvreg->pm_l2_base = devm_ioremap(dev, res->start, resource_size(res));
	if (!kvreg->pm_l2_base) {
		dev_err(dev, "failed to map L2 PM registers\n");
		return -ENOMEM;
	}

	rc = of_property_read_u32(dev->of_node, "qcom,cluster-num",
				  &kvreg->cluster_num);
	if (rc < 0) {
		dev_err(dev, "qcom,cluster-num missing rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int kryo_regulator_retention_init(struct kryo_regulator *kvreg,
				   struct platform_device *pdev,
				   struct device_node *ret_node)
{
	struct device *dev = &pdev->dev;
	struct regulator_init_data *init_data;
	struct regulator_config reg_config = {};
	int rc;

	init_data = of_get_regulator_init_data(dev, ret_node);
	if (!init_data) {
		kvreg_err(kvreg, "regulator init data is missing\n");
		return -EINVAL;
	}

	if (!init_data->constraints.name) {
		kvreg_err(kvreg, "regulator name is missing from constraints\n");
		return -EINVAL;
	}

	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_BYPASS
		| REGULATOR_CHANGE_VOLTAGE;
	init_data->constraints.input_uV = init_data->constraints.max_uV;

	kvreg->retention_desc.name		= init_data->constraints.name;
	kvreg->retention_desc.n_voltages	= LDO_N_VOLTAGES;
	kvreg->retention_desc.ops		= &kryo_regulator_retention_ops;
	kvreg->retention_desc.type		= REGULATOR_VOLTAGE;
	kvreg->retention_desc.owner		= THIS_MODULE;

	reg_config.dev = dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = kvreg;
	reg_config.of_node = ret_node;
	kvreg->retention_rdev = regulator_register(&kvreg->retention_desc,
						   &reg_config);
	if (IS_ERR(kvreg->retention_rdev)) {
		rc = PTR_ERR(kvreg->retention_rdev);
		kvreg_err(kvreg, "regulator_register failed, rc=%d\n", rc);
		return rc;
	}

	return rc;
}

static int kryo_regulator_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct kryo_regulator *kvreg;
	struct regulator_config reg_config = {};
	struct regulator_init_data *init_data = pdev->dev.platform_data;
	struct device_node *child;
	int rc = 0;

	if (!dev->of_node) {
		dev_err(dev, "Device tree node is missing\n");
		return -ENODEV;
	}

	init_data = of_get_regulator_init_data(dev, dev->of_node);

	if (!init_data) {
		dev_err(dev, "regulator init data is missing\n");
		return -EINVAL;
	}

	if (!init_data->constraints.name) {
		dev_err(dev, "regulator name is missing from constraints\n");
		return -EINVAL;
	}

	init_data->constraints.valid_ops_mask |= REGULATOR_CHANGE_VOLTAGE
		| REGULATOR_CHANGE_BYPASS | REGULATOR_CHANGE_STATUS;
	init_data->constraints.input_uV = init_data->constraints.max_uV;

	kvreg = devm_kzalloc(dev, sizeof(*kvreg), GFP_KERNEL);
	if (!kvreg) {
		dev_err(dev, "memory allocation failed\n");
		return -ENOMEM;
	}

	mutex_init(&kvreg->lock);
	rc = kryo_regulator_init_data(pdev, kvreg);
	if (rc) {
		dev_err(dev, "could not parse and ioremap all device tree properties\n");
		return rc;
	}

	kvreg->name		= init_data->constraints.name;
	kvreg->desc.name	= kvreg->name;
	kvreg->desc.n_voltages	= LDO_N_VOLTAGES;
	kvreg->desc.ops		= &kryo_regulator_ops;
	kvreg->desc.type	= REGULATOR_VOLTAGE;
	kvreg->desc.owner	= THIS_MODULE;
	kvreg->mode		= BHS_MODE;

	for_each_available_child_of_node(dev->of_node, child) {
		kryo_regulator_retention_init(kvreg, pdev, child);
		if (rc) {
			dev_err(dev, "could not initialize retention regulator, rc=%d\n",
				rc);
			return rc;
		}
		break;
	}

	/* CPUSS PM Register Initialization */
	rc = kryo_hw_init(kvreg);
	if (rc) {
		dev_err(dev, "unable to perform CPUSS PM initialization sequence\n");
		return rc;
	}

	reg_config.dev = dev;
	reg_config.init_data = init_data;
	reg_config.driver_data = kvreg;
	reg_config.of_node = dev->of_node;
	kvreg->rdev = regulator_register(&kvreg->desc, &reg_config);
	if (IS_ERR(kvreg->rdev)) {
		rc = PTR_ERR(kvreg->rdev);
		kvreg_err(kvreg, "regulator_register failed, rc=%d\n", rc);
		return rc;
	}

	platform_set_drvdata(pdev, kvreg);
	kryo_debugfs_init(kvreg);

	mutex_lock(&kryo_regulator_list_mutex);
	list_add_tail(&kvreg->link, &kryo_regulator_list);
	mutex_unlock(&kryo_regulator_list_mutex);

	kvreg_info(kvreg, "default LDO functional volt=%d uV, LDO retention volt=%d uV, Vref func=%d + %d*(val), cluster-num=%d\n",
		   kvreg->volt, kvreg->retention_volt,
		   kvreg->vref_func_min_volt,
		   kvreg->vref_func_step_volt,
		   kvreg->cluster_num);

	return rc;
}

static int kryo_regulator_remove(struct platform_device *pdev)
{
	struct kryo_regulator *kvreg = platform_get_drvdata(pdev);

	mutex_lock(&kryo_regulator_list_mutex);
	list_del(&kvreg->link);
	mutex_unlock(&kryo_regulator_list_mutex);

	regulator_unregister(kvreg->rdev);
	platform_set_drvdata(pdev, NULL);
	kryo_debugfs_deinit(kvreg);

	return 0;
}

static struct of_device_id kryo_regulator_match_table[] = {
	{ .compatible = "qcom,kryo-regulator", },
	{}
};

static struct platform_driver kryo_regulator_driver = {
	.probe	= kryo_regulator_probe,
	.remove	= kryo_regulator_remove,
	.driver	= {
		.name		= KRYO_REGULATOR_DRIVER_NAME,
		.of_match_table	= kryo_regulator_match_table,
		.owner		= THIS_MODULE,
	},
};

static int __init kryo_regulator_init(void)
{
	kryo_debugfs_base_init();
	return platform_driver_register(&kryo_regulator_driver);
}

static void __exit kryo_regulator_exit(void)
{
	platform_driver_unregister(&kryo_regulator_driver);
	kryo_debugfs_base_remove();
}

MODULE_DESCRIPTION("Kryo regulator driver");
MODULE_LICENSE("GPL v2");

arch_initcall(kryo_regulator_init);
module_exit(kryo_regulator_exit);
