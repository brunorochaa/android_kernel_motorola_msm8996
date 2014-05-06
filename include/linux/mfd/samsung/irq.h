/* irq.h
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd
 *              http://www.samsung.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __LINUX_MFD_SEC_IRQ_H
#define __LINUX_MFD_SEC_IRQ_H

enum s2mpa01_irq {
	S2MPA01_IRQ_PWRONF,
	S2MPA01_IRQ_PWRONR,
	S2MPA01_IRQ_JIGONBF,
	S2MPA01_IRQ_JIGONBR,
	S2MPA01_IRQ_ACOKBF,
	S2MPA01_IRQ_ACOKBR,
	S2MPA01_IRQ_PWRON1S,
	S2MPA01_IRQ_MRB,

	S2MPA01_IRQ_RTC60S,
	S2MPA01_IRQ_RTCA1,
	S2MPA01_IRQ_RTCA0,
	S2MPA01_IRQ_SMPL,
	S2MPA01_IRQ_RTC1S,
	S2MPA01_IRQ_WTSR,

	S2MPA01_IRQ_INT120C,
	S2MPA01_IRQ_INT140C,
	S2MPA01_IRQ_LDO3_TSD,
	S2MPA01_IRQ_B16_TSD,
	S2MPA01_IRQ_B24_TSD,
	S2MPA01_IRQ_B35_TSD,

	S2MPA01_IRQ_NR,
};

#define S2MPA01_IRQ_PWRONF_MASK		(1 << 0)
#define S2MPA01_IRQ_PWRONR_MASK		(1 << 1)
#define S2MPA01_IRQ_JIGONBF_MASK	(1 << 2)
#define S2MPA01_IRQ_JIGONBR_MASK	(1 << 3)
#define S2MPA01_IRQ_ACOKBF_MASK		(1 << 4)
#define S2MPA01_IRQ_ACOKBR_MASK		(1 << 5)
#define S2MPA01_IRQ_PWRON1S_MASK	(1 << 6)
#define S2MPA01_IRQ_MRB_MASK		(1 << 7)

#define S2MPA01_IRQ_RTC60S_MASK		(1 << 0)
#define S2MPA01_IRQ_RTCA1_MASK		(1 << 1)
#define S2MPA01_IRQ_RTCA0_MASK		(1 << 2)
#define S2MPA01_IRQ_SMPL_MASK		(1 << 3)
#define S2MPA01_IRQ_RTC1S_MASK		(1 << 4)
#define S2MPA01_IRQ_WTSR_MASK		(1 << 5)

#define S2MPA01_IRQ_INT120C_MASK	(1 << 0)
#define S2MPA01_IRQ_INT140C_MASK	(1 << 1)
#define S2MPA01_IRQ_LDO3_TSD_MASK	(1 << 2)
#define S2MPA01_IRQ_B16_TSD_MASK	(1 << 3)
#define S2MPA01_IRQ_B24_TSD_MASK	(1 << 4)
#define S2MPA01_IRQ_B35_TSD_MASK	(1 << 5)

enum s2mps11_irq {
	S2MPS11_IRQ_PWRONF,
	S2MPS11_IRQ_PWRONR,
	S2MPS11_IRQ_JIGONBF,
	S2MPS11_IRQ_JIGONBR,
	S2MPS11_IRQ_ACOKBF,
	S2MPS11_IRQ_ACOKBR,
	S2MPS11_IRQ_PWRON1S,
	S2MPS11_IRQ_MRB,

	S2MPS11_IRQ_RTC60S,
	S2MPS11_IRQ_RTCA0,
	S2MPS11_IRQ_RTCA1,
	S2MPS11_IRQ_SMPL,
	S2MPS11_IRQ_RTC1S,
	S2MPS11_IRQ_WTSR,

	S2MPS11_IRQ_INT120C,
	S2MPS11_IRQ_INT140C,

	S2MPS11_IRQ_NR,
};

#define S2MPS11_IRQ_PWRONF_MASK		(1 << 0)
#define S2MPS11_IRQ_PWRONR_MASK		(1 << 1)
#define S2MPS11_IRQ_JIGONBF_MASK	(1 << 2)
#define S2MPS11_IRQ_JIGONBR_MASK	(1 << 3)
#define S2MPS11_IRQ_ACOKBF_MASK		(1 << 4)
#define S2MPS11_IRQ_ACOKBR_MASK		(1 << 5)
#define S2MPS11_IRQ_PWRON1S_MASK	(1 << 6)
#define S2MPS11_IRQ_MRB_MASK		(1 << 7)

#define S2MPS11_IRQ_RTC60S_MASK		(1 << 0)
#define S2MPS11_IRQ_RTCA1_MASK		(1 << 1)
#define S2MPS11_IRQ_RTCA0_MASK		(1 << 2)
#define S2MPS11_IRQ_SMPL_MASK		(1 << 3)
#define S2MPS11_IRQ_RTC1S_MASK		(1 << 4)
#define S2MPS11_IRQ_WTSR_MASK		(1 << 5)

#define S2MPS11_IRQ_INT120C_MASK	(1 << 0)
#define S2MPS11_IRQ_INT140C_MASK	(1 << 1)

enum s2mps14_irq {
	S2MPS14_IRQ_PWRONF,
	S2MPS14_IRQ_PWRONR,
	S2MPS14_IRQ_JIGONBF,
	S2MPS14_IRQ_JIGONBR,
	S2MPS14_IRQ_ACOKBF,
	S2MPS14_IRQ_ACOKBR,
	S2MPS14_IRQ_PWRON1S,
	S2MPS14_IRQ_MRB,

	S2MPS14_IRQ_RTC60S,
	S2MPS14_IRQ_RTCA1,
	S2MPS14_IRQ_RTCA0,
	S2MPS14_IRQ_SMPL,
	S2MPS14_IRQ_RTC1S,
	S2MPS14_IRQ_WTSR,

	S2MPS14_IRQ_INT120C,
	S2MPS14_IRQ_INT140C,
	S2MPS14_IRQ_TSD,

	S2MPS14_IRQ_NR,
};

/* Masks for interrupts are the same as in s2mps11 */
#define S2MPS14_IRQ_TSD_MASK		(1 << 2)

enum s5m8767_irq {
	S5M8767_IRQ_PWRR,
	S5M8767_IRQ_PWRF,
	S5M8767_IRQ_PWR1S,
	S5M8767_IRQ_JIGR,
	S5M8767_IRQ_JIGF,
	S5M8767_IRQ_LOWBAT2,
	S5M8767_IRQ_LOWBAT1,

	S5M8767_IRQ_MRB,
	S5M8767_IRQ_DVSOK2,
	S5M8767_IRQ_DVSOK3,
	S5M8767_IRQ_DVSOK4,

	S5M8767_IRQ_RTC60S,
	S5M8767_IRQ_RTCA1,
	S5M8767_IRQ_RTCA2,
	S5M8767_IRQ_SMPL,
	S5M8767_IRQ_RTC1S,
	S5M8767_IRQ_WTSR,

	S5M8767_IRQ_NR,
};

#define S5M8767_IRQ_PWRR_MASK		(1 << 0)
#define S5M8767_IRQ_PWRF_MASK		(1 << 1)
#define S5M8767_IRQ_PWR1S_MASK		(1 << 3)
#define S5M8767_IRQ_JIGR_MASK		(1 << 4)
#define S5M8767_IRQ_JIGF_MASK		(1 << 5)
#define S5M8767_IRQ_LOWBAT2_MASK	(1 << 6)
#define S5M8767_IRQ_LOWBAT1_MASK	(1 << 7)

#define S5M8767_IRQ_MRB_MASK		(1 << 2)
#define S5M8767_IRQ_DVSOK2_MASK		(1 << 3)
#define S5M8767_IRQ_DVSOK3_MASK		(1 << 4)
#define S5M8767_IRQ_DVSOK4_MASK		(1 << 5)

#define S5M8767_IRQ_RTC60S_MASK		(1 << 0)
#define S5M8767_IRQ_RTCA1_MASK		(1 << 1)
#define S5M8767_IRQ_RTCA2_MASK		(1 << 2)
#define S5M8767_IRQ_SMPL_MASK		(1 << 3)
#define S5M8767_IRQ_RTC1S_MASK		(1 << 4)
#define S5M8767_IRQ_WTSR_MASK		(1 << 5)

enum s5m8763_irq {
	S5M8763_IRQ_DCINF,
	S5M8763_IRQ_DCINR,
	S5M8763_IRQ_JIGF,
	S5M8763_IRQ_JIGR,
	S5M8763_IRQ_PWRONF,
	S5M8763_IRQ_PWRONR,

	S5M8763_IRQ_WTSREVNT,
	S5M8763_IRQ_SMPLEVNT,
	S5M8763_IRQ_ALARM1,
	S5M8763_IRQ_ALARM0,

	S5M8763_IRQ_ONKEY1S,
	S5M8763_IRQ_TOPOFFR,
	S5M8763_IRQ_DCINOVPR,
	S5M8763_IRQ_CHGRSTF,
	S5M8763_IRQ_DONER,
	S5M8763_IRQ_CHGFAULT,

	S5M8763_IRQ_LOBAT1,
	S5M8763_IRQ_LOBAT2,

	S5M8763_IRQ_NR,
};

#define S5M8763_IRQ_DCINF_MASK		(1 << 2)
#define S5M8763_IRQ_DCINR_MASK		(1 << 3)
#define S5M8763_IRQ_JIGF_MASK		(1 << 4)
#define S5M8763_IRQ_JIGR_MASK		(1 << 5)
#define S5M8763_IRQ_PWRONF_MASK		(1 << 6)
#define S5M8763_IRQ_PWRONR_MASK		(1 << 7)

#define S5M8763_IRQ_WTSREVNT_MASK	(1 << 0)
#define S5M8763_IRQ_SMPLEVNT_MASK	(1 << 1)
#define S5M8763_IRQ_ALARM1_MASK		(1 << 2)
#define S5M8763_IRQ_ALARM0_MASK		(1 << 3)

#define S5M8763_IRQ_ONKEY1S_MASK	(1 << 0)
#define S5M8763_IRQ_TOPOFFR_MASK	(1 << 2)
#define S5M8763_IRQ_DCINOVPR_MASK	(1 << 3)
#define S5M8763_IRQ_CHGRSTF_MASK	(1 << 4)
#define S5M8763_IRQ_DONER_MASK		(1 << 5)
#define S5M8763_IRQ_CHGFAULT_MASK	(1 << 7)

#define S5M8763_IRQ_LOBAT1_MASK		(1 << 0)
#define S5M8763_IRQ_LOBAT2_MASK		(1 << 1)

#define S5M8763_ENRAMP                  (1 << 4)

#endif /*  __LINUX_MFD_SEC_IRQ_H */
