/*
 * rcar_gen2 Core CPG Clocks
 *
 * Copyright (C) 2013  Ideas On Board SPRL
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 */

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/clk/shmobile.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/spinlock.h>

struct rcar_gen2_cpg {
	struct clk_onecell_data data;
	spinlock_t lock;
	void __iomem *reg;
};

#define CPG_SDCKCR			0x00000074
#define CPG_PLL0CR			0x000000d8
#define CPG_FRQCRC			0x000000e0
#define CPG_FRQCRC_ZFC_MASK		(0x1f << 8)
#define CPG_FRQCRC_ZFC_SHIFT		8

/* -----------------------------------------------------------------------------
 * Z Clock
 *
 * Traits of this clock:
 * prepare - clk_prepare only ensures that parents are prepared
 * enable - clk_enable only ensures that parents are enabled
 * rate - rate is adjustable.  clk->rate = parent->rate * mult / 32
 * parent - fixed parent.  No clk_set_parent support
 */

struct cpg_z_clk {
	struct clk_hw hw;
	void __iomem *reg;
};

#define to_z_clk(_hw)	container_of(_hw, struct cpg_z_clk, hw)

static unsigned long cpg_z_clk_recalc_rate(struct clk_hw *hw,
					   unsigned long parent_rate)
{
	struct cpg_z_clk *zclk = to_z_clk(hw);
	unsigned int mult;
	unsigned int val;

	val = (clk_readl(zclk->reg) & CPG_FRQCRC_ZFC_MASK)
	    >> CPG_FRQCRC_ZFC_SHIFT;
	mult = 32 - val;

	return div_u64((u64)parent_rate * mult, 32);
}

static long cpg_z_clk_round_rate(struct clk_hw *hw, unsigned long rate,
				 unsigned long *parent_rate)
{
	unsigned long prate  = *parent_rate;
	unsigned int mult;

	if (!prate)
		prate = 1;

	mult = div_u64((u64)rate * 32, prate);
	mult = clamp(mult, 1U, 32U);

	return *parent_rate / 32 * mult;
}

static int cpg_z_clk_set_rate(struct clk_hw *hw, unsigned long rate,
			      unsigned long parent_rate)
{
	struct cpg_z_clk *zclk = to_z_clk(hw);
	unsigned int mult;
	u32 val;

	mult = div_u64((u64)rate * 32, parent_rate);
	mult = clamp(mult, 1U, 32U);

	val = clk_readl(zclk->reg);
	val &= ~CPG_FRQCRC_ZFC_MASK;
	val |= (32 - mult) << CPG_FRQCRC_ZFC_SHIFT;
	clk_writel(val, zclk->reg);

	return 0;
}

static const struct clk_ops cpg_z_clk_ops = {
	.recalc_rate = cpg_z_clk_recalc_rate,
	.round_rate = cpg_z_clk_round_rate,
	.set_rate = cpg_z_clk_set_rate,
};

static struct clk * __init cpg_z_clk_register(struct rcar_gen2_cpg *cpg)
{
	static const char *parent_name = "pll0";
	struct clk_init_data init;
	struct cpg_z_clk *zclk;
	struct clk *clk;

	zclk = kzalloc(sizeof(*zclk), GFP_KERNEL);
	if (!zclk)
		return ERR_PTR(-ENOMEM);

	init.name = "z";
	init.ops = &cpg_z_clk_ops;
	init.flags = 0;
	init.parent_names = &parent_name;
	init.num_parents = 1;

	zclk->reg = cpg->reg + CPG_FRQCRC;
	zclk->hw.init = &init;

	clk = clk_register(NULL, &zclk->hw);
	if (IS_ERR(clk))
		kfree(zclk);

	return clk;
}

/* -----------------------------------------------------------------------------
 * CPG Clock Data
 */

/*
 *   MD		EXTAL		PLL0	PLL1	PLL3
 * 14 13 19	(MHz)		*1	*1
 *---------------------------------------------------
 * 0  0  0	15 x 1		x172/2	x208/2	x106
 * 0  0  1	15 x 1		x172/2	x208/2	x88
 * 0  1  0	20 x 1		x130/2	x156/2	x80
 * 0  1  1	20 x 1		x130/2	x156/2	x66
 * 1  0  0	26 / 2		x200/2	x240/2	x122
 * 1  0  1	26 / 2		x200/2	x240/2	x102
 * 1  1  0	30 / 2		x172/2	x208/2	x106
 * 1  1  1	30 / 2		x172/2	x208/2	x88
 *
 * *1 :	Table 7.6 indicates VCO ouput (PLLx = VCO/2)
 */
#define CPG_PLL_CONFIG_INDEX(md)	((((md) & BIT(14)) >> 12) | \
					 (((md) & BIT(13)) >> 12) | \
					 (((md) & BIT(19)) >> 19))
struct cpg_pll_config {
	unsigned int extal_div;
	unsigned int pll1_mult;
	unsigned int pll3_mult;
};

static const struct cpg_pll_config cpg_pll_configs[8] __initconst = {
	{ 1, 208, 106 }, { 1, 208,  88 }, { 1, 156,  80 }, { 1, 156,  66 },
	{ 2, 240, 122 }, { 2, 240, 102 }, { 2, 208, 106 }, { 2, 208,  88 },
};

/* SDHI divisors */
static const struct clk_div_table cpg_sdh_div_table[] = {
	{  0,  2 }, {  1,  3 }, {  2,  4 }, {  3,  6 },
	{  4,  8 }, {  5, 12 }, {  6, 16 }, {  7, 18 },
	{  8, 24 }, { 10, 36 }, { 11, 48 }, {  0,  0 },
};

static const struct clk_div_table cpg_sd01_div_table[] = {
	{  5, 12 }, {  6, 16 }, {  7, 18 }, {  8, 24 },
	{ 10, 36 }, { 11, 48 }, { 12, 10 }, {  0,  0 },
};

/* -----------------------------------------------------------------------------
 * Initialization
 */

static u32 cpg_mode __initdata;

static struct clk * __init
rcar_gen2_cpg_register_clock(struct device_node *np, struct rcar_gen2_cpg *cpg,
			     const struct cpg_pll_config *config,
			     const char *name)
{
	const struct clk_div_table *table = NULL;
	const char *parent_name;
	unsigned int shift;
	unsigned int mult = 1;
	unsigned int div = 1;

	if (!strcmp(name, "main")) {
		parent_name = of_clk_get_parent_name(np, 0);
		div = config->extal_div;
	} else if (!strcmp(name, "pll0")) {
		/* PLL0 is a configurable multiplier clock. Register it as a
		 * fixed factor clock for now as there's no generic multiplier
		 * clock implementation and we currently have no need to change
		 * the multiplier value.
		 */
		u32 value = clk_readl(cpg->reg + CPG_PLL0CR);
		parent_name = "main";
		mult = ((value >> 24) & ((1 << 7) - 1)) + 1;
	} else if (!strcmp(name, "pll1")) {
		parent_name = "main";
		mult = config->pll1_mult / 2;
	} else if (!strcmp(name, "pll3")) {
		parent_name = "main";
		mult = config->pll3_mult;
	} else if (!strcmp(name, "lb")) {
		parent_name = "pll1";
		div = cpg_mode & BIT(18) ? 36 : 24;
	} else if (!strcmp(name, "qspi")) {
		parent_name = "pll1_div2";
		div = (cpg_mode & (BIT(3) | BIT(2) | BIT(1))) == BIT(2)
		    ? 8 : 10;
	} else if (!strcmp(name, "sdh")) {
		parent_name = "pll1";
		table = cpg_sdh_div_table;
		shift = 8;
	} else if (!strcmp(name, "sd0")) {
		parent_name = "pll1";
		table = cpg_sd01_div_table;
		shift = 4;
	} else if (!strcmp(name, "sd1")) {
		parent_name = "pll1";
		table = cpg_sd01_div_table;
		shift = 0;
	} else if (!strcmp(name, "z")) {
		return cpg_z_clk_register(cpg);
	} else {
		return ERR_PTR(-EINVAL);
	}

	if (!table)
		return clk_register_fixed_factor(NULL, name, parent_name, 0,
						 mult, div);
	else
		return clk_register_divider_table(NULL, name, parent_name, 0,
						 cpg->reg + CPG_SDCKCR, shift,
						 4, 0, table, &cpg->lock);
}

static void __init rcar_gen2_cpg_clocks_init(struct device_node *np)
{
	const struct cpg_pll_config *config;
	struct rcar_gen2_cpg *cpg;
	struct clk **clks;
	unsigned int i;
	int num_clks;

	num_clks = of_property_count_strings(np, "clock-output-names");
	if (num_clks < 0) {
		pr_err("%s: failed to count clocks\n", __func__);
		return;
	}

	cpg = kzalloc(sizeof(*cpg), GFP_KERNEL);
	clks = kzalloc(num_clks * sizeof(*clks), GFP_KERNEL);
	if (cpg == NULL || clks == NULL) {
		/* We're leaking memory on purpose, there's no point in cleaning
		 * up as the system won't boot anyway.
		 */
		pr_err("%s: failed to allocate cpg\n", __func__);
		return;
	}

	spin_lock_init(&cpg->lock);

	cpg->data.clks = clks;
	cpg->data.clk_num = num_clks;

	cpg->reg = of_iomap(np, 0);
	if (WARN_ON(cpg->reg == NULL))
		return;

	config = &cpg_pll_configs[CPG_PLL_CONFIG_INDEX(cpg_mode)];

	for (i = 0; i < num_clks; ++i) {
		const char *name;
		struct clk *clk;

		of_property_read_string_index(np, "clock-output-names", i,
					      &name);

		clk = rcar_gen2_cpg_register_clock(np, cpg, config, name);
		if (IS_ERR(clk))
			pr_err("%s: failed to register %s %s clock (%ld)\n",
			       __func__, np->name, name, PTR_ERR(clk));
		else
			cpg->data.clks[i] = clk;
	}

	of_clk_add_provider(np, of_clk_src_onecell_get, &cpg->data);
}
CLK_OF_DECLARE(rcar_gen2_cpg_clks, "renesas,rcar-gen2-cpg-clocks",
	       rcar_gen2_cpg_clocks_init);

void __init rcar_gen2_clocks_init(u32 mode)
{
	cpg_mode = mode;

	of_clk_init(NULL);
}
