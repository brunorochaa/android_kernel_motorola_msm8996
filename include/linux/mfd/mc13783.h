/*
 * Copyright 2010 Yong Shen <yong.shen@linaro.org>
 * Copyright 2009-2010 Pengutronix
 * Uwe Kleine-Koenig <u.kleine-koenig@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 */
#ifndef __LINUX_MFD_MC13783_H
#define __LINUX_MFD_MC13783_H

#include <linux/mfd/mc13xxx.h>

#define	MC13783_REG_SW1A		0
#define	MC13783_REG_SW1B		1
#define	MC13783_REG_SW2A		2
#define	MC13783_REG_SW2B		3
#define	MC13783_REG_SW3		4
#define	MC13783_REG_PLL		5
#define	MC13783_REG_VAUDIO	6
#define	MC13783_REG_VIOHI	7
#define	MC13783_REG_VIOLO	8
#define	MC13783_REG_VDIG	9
#define	MC13783_REG_VGEN	10
#define	MC13783_REG_VRFDIG	11
#define	MC13783_REG_VRFREF	12
#define	MC13783_REG_VRFCP	13
#define	MC13783_REG_VSIM	14
#define	MC13783_REG_VESIM	15
#define	MC13783_REG_VCAM	16
#define	MC13783_REG_VRFBG	17
#define	MC13783_REG_VVIB	18
#define	MC13783_REG_VRF1	19
#define	MC13783_REG_VRF2	20
#define	MC13783_REG_VMMC1	21
#define	MC13783_REG_VMMC2	22
#define	MC13783_REG_GPO1	23
#define	MC13783_REG_GPO2	24
#define	MC13783_REG_GPO3	25
#define	MC13783_REG_GPO4	26
#define	MC13783_REG_V1		27
#define	MC13783_REG_V2		28
#define	MC13783_REG_V3		29
#define	MC13783_REG_V4		30
#define	MC13783_REG_PWGT1SPI	31
#define	MC13783_REG_PWGT2SPI	32

#define MC13783_IRQ_ADCDONE	MC13XXX_IRQ_ADCDONE
#define MC13783_IRQ_ADCBISDONE	MC13XXX_IRQ_ADCBISDONE
#define MC13783_IRQ_TS		MC13XXX_IRQ_TS
#define MC13783_IRQ_WHIGH	3
#define MC13783_IRQ_WLOW	4
#define MC13783_IRQ_CHGDET	MC13XXX_IRQ_CHGDET
#define MC13783_IRQ_CHGOV	7
#define MC13783_IRQ_CHGREV	MC13XXX_IRQ_CHGREV
#define MC13783_IRQ_CHGSHORT	MC13XXX_IRQ_CHGSHORT
#define MC13783_IRQ_CCCV	MC13XXX_IRQ_CCCV
#define MC13783_IRQ_CHGCURR	MC13XXX_IRQ_CHGCURR
#define MC13783_IRQ_BPON	MC13XXX_IRQ_BPON
#define MC13783_IRQ_LOBATL	MC13XXX_IRQ_LOBATL
#define MC13783_IRQ_LOBATH	MC13XXX_IRQ_LOBATH
#define MC13783_IRQ_UDP		15
#define MC13783_IRQ_USB		16
#define MC13783_IRQ_ID		19
#define MC13783_IRQ_SE1		21
#define MC13783_IRQ_CKDET	22
#define MC13783_IRQ_UDM		23
#define MC13783_IRQ_1HZ		MC13XXX_IRQ_1HZ
#define MC13783_IRQ_TODA	MC13XXX_IRQ_TODA
#define MC13783_IRQ_ONOFD1	27
#define MC13783_IRQ_ONOFD2	28
#define MC13783_IRQ_ONOFD3	29
#define MC13783_IRQ_SYSRST	MC13XXX_IRQ_SYSRST
#define MC13783_IRQ_RTCRST	MC13XXX_IRQ_RTCRST
#define MC13783_IRQ_PC		MC13XXX_IRQ_PC
#define MC13783_IRQ_WARM	MC13XXX_IRQ_WARM
#define MC13783_IRQ_MEMHLD	MC13XXX_IRQ_MEMHLD
#define MC13783_IRQ_PWRRDY	35
#define MC13783_IRQ_THWARNL	MC13XXX_IRQ_THWARNL
#define MC13783_IRQ_THWARNH	MC13XXX_IRQ_THWARNH
#define MC13783_IRQ_CLK		MC13XXX_IRQ_CLK
#define MC13783_IRQ_SEMAF	39
#define MC13783_IRQ_MC2B	41
#define MC13783_IRQ_HSDET	42
#define MC13783_IRQ_HSL		43
#define MC13783_IRQ_ALSPTH	44
#define MC13783_IRQ_AHSSHORT	45
#define MC13783_NUM_IRQ		MC13XXX_NUM_IRQ

#endif /* ifndef __LINUX_MFD_MC13783_H */
