/*
 * OMAP4-specific clock framework functions
 *
 * Copyright (C) 2009 Texas Instruments, Inc.
 *
 * Rajendra Nayak (rnayak@ti.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/errno.h>
#include "clock.h"

struct clk_functions omap2_clk_functions = {
	.clk_enable		= omap2_clk_enable,
	.clk_disable		= omap2_clk_disable,
	.clk_round_rate		= omap2_clk_round_rate,
	.clk_set_rate		= omap2_clk_set_rate,
	.clk_set_parent		= omap2_clk_set_parent,
	.clk_disable_unused	= omap2_clk_disable_unused,
};

/*
 * Dummy functions for DPLL control. Plan is to re-use
 * existing OMAP3 dpll control functions.
 */

unsigned long omap3_dpll_recalc(struct clk *clk)
{
	return 0;
}

int omap3_noncore_dpll_set_rate(struct clk *clk, unsigned long rate)
{
	return 0;
}

int omap3_noncore_dpll_enable(struct clk *clk)
{
	return 0;
}

void omap3_noncore_dpll_disable(struct clk *clk)
{
	return;
}

const struct clkops clkops_noncore_dpll_ops = {
	.enable		= &omap3_noncore_dpll_enable,
	.disable	= &omap3_noncore_dpll_disable,
};

void omap2_clk_prepare_for_reboot(void)
{
	return;
}
