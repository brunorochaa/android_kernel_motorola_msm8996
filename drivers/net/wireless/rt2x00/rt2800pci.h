/*
	Copyright (C) 2004 - 2009 rt2x00 SourceForge Project
	<http://rt2x00.serialmonkey.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the
	Free Software Foundation, Inc.,
	59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
	Module: rt2800pci
	Abstract: Data structures and registers for the rt2800pci module.
	Supported chipsets: RT2800E & RT2800ED.
 */

#ifndef RT2800PCI_H
#define RT2800PCI_H

/*
 * PCI registers.
 */

/*
 * E2PROM_CSR: EEPROM control register.
 * RELOAD: Write 1 to reload eeprom content.
 * TYPE: 0: 93c46, 1:93c66.
 * LOAD_STATUS: 1:loading, 0:done.
 */
#define E2PROM_CSR			0x0004
#define E2PROM_CSR_DATA_CLOCK		FIELD32(0x00000001)
#define E2PROM_CSR_CHIP_SELECT		FIELD32(0x00000002)
#define E2PROM_CSR_DATA_IN		FIELD32(0x00000004)
#define E2PROM_CSR_DATA_OUT		FIELD32(0x00000008)
#define E2PROM_CSR_TYPE			FIELD32(0x00000030)
#define E2PROM_CSR_LOAD_STATUS		FIELD32(0x00000040)
#define E2PROM_CSR_RELOAD		FIELD32(0x00000080)

/*
 * Queue register offset macros
 */
#define TX_QUEUE_REG_OFFSET		0x10
#define TX_BASE_PTR(__x)		TX_BASE_PTR0 + ((__x) * TX_QUEUE_REG_OFFSET)
#define TX_MAX_CNT(__x)			TX_MAX_CNT0 + ((__x) * TX_QUEUE_REG_OFFSET)
#define TX_CTX_IDX(__x)			TX_CTX_IDX0 + ((__x) * TX_QUEUE_REG_OFFSET)
#define TX_DTX_IDX(__x)			TX_DTX_IDX0 + ((__x) * TX_QUEUE_REG_OFFSET)

/*
 * EFUSE_CSR: RT3090 EEPROM
 */
#define EFUSE_CTRL			0x0580
#define EFUSE_CTRL_ADDRESS_IN		FIELD32(0x03fe0000)
#define EFUSE_CTRL_MODE			FIELD32(0x000000c0)
#define EFUSE_CTRL_KICK			FIELD32(0x40000000)
#define EFUSE_CTRL_PRESENT		FIELD32(0x80000000)

/*
 * EFUSE_DATA0
 */
#define EFUSE_DATA0			0x0590

/*
 * EFUSE_DATA1
 */
#define EFUSE_DATA1			0x0594

/*
 * EFUSE_DATA2
 */
#define EFUSE_DATA2			0x0598

/*
 * EFUSE_DATA3
 */
#define EFUSE_DATA3			0x059c

/*
 * 8051 firmware image.
 */
#define FIRMWARE_RT2860			"rt2860.bin"
#define FIRMWARE_IMAGE_BASE		0x2000

/*
 * DMA descriptor defines.
 */
#define TXD_DESC_SIZE			( 4 * sizeof(__le32) )
#define RXD_DESC_SIZE			( 4 * sizeof(__le32) )

/*
 * TX descriptor format for TX, PRIO and Beacon Ring.
 */

/*
 * Word0
 */
#define TXD_W0_SD_PTR0			FIELD32(0xffffffff)

/*
 * Word1
 */
#define TXD_W1_SD_LEN1			FIELD32(0x00003fff)
#define TXD_W1_LAST_SEC1		FIELD32(0x00004000)
#define TXD_W1_BURST			FIELD32(0x00008000)
#define TXD_W1_SD_LEN0			FIELD32(0x3fff0000)
#define TXD_W1_LAST_SEC0		FIELD32(0x40000000)
#define TXD_W1_DMA_DONE			FIELD32(0x80000000)

/*
 * Word2
 */
#define TXD_W2_SD_PTR1			FIELD32(0xffffffff)

/*
 * Word3
 * WIV: Wireless Info Valid. 1: Driver filled WI, 0: DMA needs to copy WI
 * QSEL: Select on-chip FIFO ID for 2nd-stage output scheduler.
 *       0:MGMT, 1:HCCA 2:EDCA
 */
#define TXD_W3_WIV			FIELD32(0x01000000)
#define TXD_W3_QSEL			FIELD32(0x06000000)
#define TXD_W3_TCO			FIELD32(0x20000000)
#define TXD_W3_UCO			FIELD32(0x40000000)
#define TXD_W3_ICO			FIELD32(0x80000000)

/*
 * RX descriptor format for RX Ring.
 */

/*
 * Word0
 */
#define RXD_W0_SDP0			FIELD32(0xffffffff)

/*
 * Word1
 */
#define RXD_W1_SDL1			FIELD32(0x00003fff)
#define RXD_W1_SDL0			FIELD32(0x3fff0000)
#define RXD_W1_LS0			FIELD32(0x40000000)
#define RXD_W1_DMA_DONE			FIELD32(0x80000000)

/*
 * Word2
 */
#define RXD_W2_SDP1			FIELD32(0xffffffff)

/*
 * Word3
 * AMSDU: RX with 802.3 header, not 802.11 header.
 * DECRYPTED: This frame is being decrypted.
 */
#define RXD_W3_BA			FIELD32(0x00000001)
#define RXD_W3_DATA			FIELD32(0x00000002)
#define RXD_W3_NULLDATA			FIELD32(0x00000004)
#define RXD_W3_FRAG			FIELD32(0x00000008)
#define RXD_W3_UNICAST_TO_ME		FIELD32(0x00000010)
#define RXD_W3_MULTICAST		FIELD32(0x00000020)
#define RXD_W3_BROADCAST		FIELD32(0x00000040)
#define RXD_W3_MY_BSS			FIELD32(0x00000080)
#define RXD_W3_CRC_ERROR		FIELD32(0x00000100)
#define RXD_W3_CIPHER_ERROR		FIELD32(0x00000600)
#define RXD_W3_AMSDU			FIELD32(0x00000800)
#define RXD_W3_HTC			FIELD32(0x00001000)
#define RXD_W3_RSSI			FIELD32(0x00002000)
#define RXD_W3_L2PAD			FIELD32(0x00004000)
#define RXD_W3_AMPDU			FIELD32(0x00008000)
#define RXD_W3_DECRYPTED		FIELD32(0x00010000)
#define RXD_W3_PLCP_SIGNAL		FIELD32(0x00020000)
#define RXD_W3_PLCP_RSSI		FIELD32(0x00040000)

#endif /* RT2800PCI_H */
