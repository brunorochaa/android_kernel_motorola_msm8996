/*
 * Copyright (C) 2012-2014 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/of_platform.h>
#include <asm/hardware/cache-l2x0.h>

#include "bcm_kona_smc.h"
#include "kona.h"

void __init kona_l2_cache_init(void)
{
	int ret;

	if (!IS_ENABLED(CONFIG_CACHE_L2X0))
		return;

	ret = bcm_kona_smc_init();
	if (ret) {
		pr_info("Secure API not available (%d). Skipping L2 init.\n",
			ret);
		return;
	}

	bcm_kona_smc(SSAPI_ENABLE_L2_CACHE, 0, 0, 0, 0);

	/*
	 * The aux_val and aux_mask have no effect since L2 cache is already
	 * enabled.  Pass 0s for aux_val and 1s for aux_mask for default value.
	 */
	ret = l2x0_of_init(0, ~0);
	if (ret)
		pr_err("Couldn't enable L2 cache: %d\n", ret);
}
