/*
 * Copyright (C) 2009-2010 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __MACH_MX23_H__
#define __MACH_MX23_H__

#include <mach/mxs.h>

/*
 * OCRAM
 */
#define MX23_OCRAM_BASE_ADDR		0x00000000
#define MX23_OCRAM_SIZE			SZ_32K

/*
 * IO
 */
#define MX23_IO_BASE_ADDR		0x80000000
#define MX23_IO_SIZE			SZ_1M

#define MX23_ICOLL_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x000000)
#define MX23_APBH_DMA_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x004000)
#define MX23_BCH_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x00a000)
#define MX23_GPMI_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x00c000)
#define MX23_SSP1_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x010000)
#define MX23_PINCTRL_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x018000)
#define MX23_DIGCTL_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x01c000)
#define MX23_ETM_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x020000)
#define MX23_APBX_DMA_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x024000)
#define MX23_DCP_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x028000)
#define MX23_PXP_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x02a000)
#define MX23_OCOTP_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x02c000)
#define MX23_AXI_AHB0_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x02e000)
#define MX23_LCDIF_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x030000)
#define MX23_SSP2_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x034000)
#define MX23_TVENC_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x038000)
#define MX23_CLKCTRL_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x040000)
#define MX23_SAIF0_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x042000)
#define MX23_POWER_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x044000)
#define MX23_SAIF1_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x046000)
#define MX23_AUDIOOUT_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x048000)
#define MX23_AUDIOIN_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x04c000)
#define MX23_LRADC_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x050000)
#define MX23_SPDIF_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x054000)
#define MX23_I2C0_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x058000)
#define MX23_RTC_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x05c000)
#define MX23_PWM_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x064000)
#define MX23_TIMROT_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x068000)
#define MX23_AUART1_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x06c000)
#define MX23_AUART2_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x06e000)
#define MX23_DUART_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x070000)
#define MX23_USBPHY_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x07c000)
#define MX23_USBCTRL_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x080000)
#define MX23_DRAM_BASE_ADDR		(MX23_IO_BASE_ADDR + 0x0e0000)

#define MX23_IO_P2V(x)			MXS_IO_P2V(x)
#define MX23_IO_ADDRESS(x)		IOMEM(MX23_IO_P2V(x))

/*
 * IRQ
 */
#define MX23_INT_DUART			0
#define MX23_INT_COMMS_RX		1
#define MX23_INT_COMMS_TX		1
#define MX23_INT_SSP2_ERROR		2
#define MX23_INT_VDD5V			3
#define MX23_INT_HEADPHONE_SHORT	4
#define MX23_INT_DAC_DMA		5
#define MX23_INT_DAC_ERROR		6
#define MX23_INT_ADC_DMA		7
#define MX23_INT_ADC_ERROR		8
#define MX23_INT_SPDIF_DMA		9
#define MX23_INT_SAIF2_DMA		9
#define MX23_INT_SPDIF_ERROR		10
#define MX23_INT_SAIF1_IRQ		10
#define MX23_INT_SAIF2_IRQ		10
#define MX23_INT_USB_CTRL		11
#define MX23_INT_USB_WAKEUP		12
#define MX23_INT_GPMI_DMA		13
#define MX23_INT_SSP1_DMA		14
#define MX23_INT_SSP_ERROR		15
#define MX23_INT_GPIO0			16
#define MX23_INT_GPIO1			17
#define MX23_INT_GPIO2			18
#define MX23_INT_SAIF1_DMA		19
#define MX23_INT_SSP2_DMA		20
#define MX23_INT_ECC8_IRQ		21
#define MX23_INT_RTC_ALARM		22
#define MX23_INT_UARTAPP_TX_DMA		23
#define MX23_INT_UARTAPP_INTERNAL	24
#define MX23_INT_UARTAPP_RX_DMA		25
#define MX23_INT_I2C_DMA		26
#define MX23_INT_I2C_ERROR		27
#define MX23_INT_TIMER0			28
#define MX23_INT_TIMER1			29
#define MX23_INT_TIMER2			30
#define MX23_INT_TIMER3			31
#define MX23_INT_BATT_BRNOUT		32
#define MX23_INT_VDDD_BRNOUT		33
#define MX23_INT_VDDIO_BRNOUT		34
#define MX23_INT_VDD18_BRNOUT		35
#define MX23_INT_TOUCH_DETECT		36
#define MX23_INT_LRADC_CH0		37
#define MX23_INT_LRADC_CH1		38
#define MX23_INT_LRADC_CH2		39
#define MX23_INT_LRADC_CH3		40
#define MX23_INT_LRADC_CH4		41
#define MX23_INT_LRADC_CH5		42
#define MX23_INT_LRADC_CH6		43
#define MX23_INT_LRADC_CH7		44
#define MX23_INT_LCDIF_DMA		45
#define MX23_INT_LCDIF_ERROR		46
#define MX23_INT_DIGCTL_DEBUG_TRAP	47
#define MX23_INT_RTC_1MSEC		48
#define MX23_INT_DRI_DMA		49
#define MX23_INT_DRI_ATTENTION		50
#define MX23_INT_GPMI_ATTENTION		51
#define MX23_INT_IR			52
#define MX23_INT_DCP_VMI		53
#define MX23_INT_DCP			54
#define MX23_INT_BCH			56
#define MX23_INT_PXP			57
#define MX23_INT_UARTAPP2_TX_DMA	58
#define MX23_INT_UARTAPP2_INTERNAL	59
#define MX23_INT_UARTAPP2_RX_DMA	60
#define MX23_INT_VDAC_DETECT		61
#define MX23_INT_VDD5V_DROOP		64
#define MX23_INT_DCDC4P2_BO		65

#endif /* __MACH_MX23_H__ */
