/*
 * Copyright (C) Freescale Semicondutor, Inc. 2006. All rights reserved.
 *
 * Author: Shlomi Gridish <gridish@freescale.com>
 *
 * Description:
 * QE UCC Gigabit Ethernet Driver
 *
 * Changelog:
 * Jul 6, 2006 Li Yang <LeoLi@freescale.com>
 * - Rearrange code and style fixes
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/ethtool.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/fsl_devices.h>
#include <linux/ethtool.h>
#include <linux/platform_device.h>
#include <linux/mii.h>

#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/immap_qe.h>
#include <asm/qe.h>
#include <asm/ucc.h>
#include <asm/ucc_fast.h>

#include "ucc_geth.h"
#include "ucc_geth_phy.h"

#undef DEBUG

#define DRV_DESC "QE UCC Gigabit Ethernet Controller version:Sept 11, 2006"
#define DRV_NAME "ucc_geth"

#define ugeth_printk(level, format, arg...)  \
        printk(level format "\n", ## arg)

#define ugeth_dbg(format, arg...)            \
        ugeth_printk(KERN_DEBUG , format , ## arg)
#define ugeth_err(format, arg...)            \
        ugeth_printk(KERN_ERR , format , ## arg)
#define ugeth_info(format, arg...)           \
        ugeth_printk(KERN_INFO , format , ## arg)
#define ugeth_warn(format, arg...)           \
        ugeth_printk(KERN_WARNING , format , ## arg)

#ifdef UGETH_VERBOSE_DEBUG
#define ugeth_vdbg ugeth_dbg
#else
#define ugeth_vdbg(fmt, args...) do { } while (0)
#endif				/* UGETH_VERBOSE_DEBUG */

static DEFINE_SPINLOCK(ugeth_lock);

static ucc_geth_info_t ugeth_primary_info = {
	.uf_info = {
		    .bd_mem_part = MEM_PART_SYSTEM,
		    .rtsm = UCC_FAST_SEND_IDLES_BETWEEN_FRAMES,
		    .max_rx_buf_length = 1536,
/* FIXME: should be changed in run time for 1G and 100M */
#ifdef CONFIG_UGETH_HAS_GIGA
		    .urfs = UCC_GETH_URFS_GIGA_INIT,
		    .urfet = UCC_GETH_URFET_GIGA_INIT,
		    .urfset = UCC_GETH_URFSET_GIGA_INIT,
		    .utfs = UCC_GETH_UTFS_GIGA_INIT,
		    .utfet = UCC_GETH_UTFET_GIGA_INIT,
		    .utftt = UCC_GETH_UTFTT_GIGA_INIT,
#else
		    .urfs = UCC_GETH_URFS_INIT,
		    .urfet = UCC_GETH_URFET_INIT,
		    .urfset = UCC_GETH_URFSET_INIT,
		    .utfs = UCC_GETH_UTFS_INIT,
		    .utfet = UCC_GETH_UTFET_INIT,
		    .utftt = UCC_GETH_UTFTT_INIT,
#endif
		    .ufpt = 256,
		    .mode = UCC_FAST_PROTOCOL_MODE_ETHERNET,
		    .ttx_trx = UCC_FAST_GUMR_TRANSPARENT_TTX_TRX_NORMAL,
		    .tenc = UCC_FAST_TX_ENCODING_NRZ,
		    .renc = UCC_FAST_RX_ENCODING_NRZ,
		    .tcrc = UCC_FAST_16_BIT_CRC,
		    .synl = UCC_FAST_SYNC_LEN_NOT_USED,
		    },
	.numQueuesTx = 1,
	.numQueuesRx = 1,
	.extendedFilteringChainPointer = ((uint32_t) NULL),
	.typeorlen = 3072 /*1536 */ ,
	.nonBackToBackIfgPart1 = 0x40,
	.nonBackToBackIfgPart2 = 0x60,
	.miminumInterFrameGapEnforcement = 0x50,
	.backToBackInterFrameGap = 0x60,
	.mblinterval = 128,
	.nortsrbytetime = 5,
	.fracsiz = 1,
	.strictpriorityq = 0xff,
	.altBebTruncation = 0xa,
	.excessDefer = 1,
	.maxRetransmission = 0xf,
	.collisionWindow = 0x37,
	.receiveFlowControl = 1,
	.maxGroupAddrInHash = 4,
	.maxIndAddrInHash = 4,
	.prel = 7,
	.maxFrameLength = 1518,
	.minFrameLength = 64,
	.maxD1Length = 1520,
	.maxD2Length = 1520,
	.vlantype = 0x8100,
	.ecamptr = ((uint32_t) NULL),
	.eventRegMask = UCCE_OTHER,
	.pausePeriod = 0xf000,
	.interruptcoalescingmaxvalue = {1, 1, 1, 1, 1, 1, 1, 1},
	.bdRingLenTx = {
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN,
			TX_BD_RING_LEN},

	.bdRingLenRx = {
			RX_BD_RING_LEN,
			RX_BD_RING_LEN,
			RX_BD_RING_LEN,
			RX_BD_RING_LEN,
			RX_BD_RING_LEN,
			RX_BD_RING_LEN,
			RX_BD_RING_LEN,
			RX_BD_RING_LEN},

	.numStationAddresses = UCC_GETH_NUM_OF_STATION_ADDRESSES_1,
	.largestexternallookupkeysize =
	    QE_FLTR_LARGEST_EXTERNAL_TABLE_LOOKUP_KEY_SIZE_NONE,
	.statisticsMode = UCC_GETH_STATISTICS_GATHERING_MODE_NONE,
	.vlanOperationTagged = UCC_GETH_VLAN_OPERATION_TAGGED_NOP,
	.vlanOperationNonTagged = UCC_GETH_VLAN_OPERATION_NON_TAGGED_NOP,
	.rxQoSMode = UCC_GETH_QOS_MODE_DEFAULT,
	.aufc = UPSMR_AUTOMATIC_FLOW_CONTROL_MODE_NONE,
	.padAndCrc = MACCFG2_PAD_AND_CRC_MODE_PAD_AND_CRC,
	.numThreadsTx = UCC_GETH_NUM_OF_THREADS_4,
	.numThreadsRx = UCC_GETH_NUM_OF_THREADS_4,
	.riscTx = QE_RISC_ALLOCATION_RISC1_AND_RISC2,
	.riscRx = QE_RISC_ALLOCATION_RISC1_AND_RISC2,
};

static ucc_geth_info_t ugeth_info[8];

#ifdef DEBUG
static void mem_disp(u8 *addr, int size)
{
	u8 *i;
	int size16Aling = (size >> 4) << 4;
	int size4Aling = (size >> 2) << 2;
	int notAlign = 0;
	if (size % 16)
		notAlign = 1;

	for (i = addr; (u32) i < (u32) addr + size16Aling; i += 16)
		printk("0x%08x: %08x %08x %08x %08x\r\n",
		       (u32) i,
		       *((u32 *) (i)),
		       *((u32 *) (i + 4)),
		       *((u32 *) (i + 8)), *((u32 *) (i + 12)));
	if (notAlign == 1)
		printk("0x%08x: ", (u32) i);
	for (; (u32) i < (u32) addr + size4Aling; i += 4)
		printk("%08x ", *((u32 *) (i)));
	for (; (u32) i < (u32) addr + size; i++)
		printk("%02x", *((u8 *) (i)));
	if (notAlign == 1)
		printk("\r\n");
}
#endif /* DEBUG */

#ifdef CONFIG_UGETH_FILTERING
static void enqueue(struct list_head *node, struct list_head *lh)
{
	unsigned long flags;

	spin_lock_irqsave(ugeth_lock, flags);
	list_add_tail(node, lh);
	spin_unlock_irqrestore(ugeth_lock, flags);
}
#endif /* CONFIG_UGETH_FILTERING */

static struct list_head *dequeue(struct list_head *lh)
{
	unsigned long flags;

	spin_lock_irqsave(ugeth_lock, flags);
	if (!list_empty(lh)) {
		struct list_head *node = lh->next;
		list_del(node);
		spin_unlock_irqrestore(ugeth_lock, flags);
		return node;
	} else {
		spin_unlock_irqrestore(ugeth_lock, flags);
		return NULL;
	}
}

static int get_interface_details(enet_interface_e enet_interface,
				 enet_speed_e *speed,
				 int *r10m,
				 int *rmm,
				 int *rpm,
				 int *tbi, int *limited_to_full_duplex)
{
	/* Analyze enet_interface according to Interface Mode
	Configuration table */
	switch (enet_interface) {
	case ENET_10_MII:
		*speed = ENET_SPEED_10BT;
		break;
	case ENET_10_RMII:
		*speed = ENET_SPEED_10BT;
		*r10m = 1;
		*rmm = 1;
		break;
	case ENET_10_RGMII:
		*speed = ENET_SPEED_10BT;
		*rpm = 1;
		*r10m = 1;
		*limited_to_full_duplex = 1;
		break;
	case ENET_100_MII:
		*speed = ENET_SPEED_100BT;
		break;
	case ENET_100_RMII:
		*speed = ENET_SPEED_100BT;
		*rmm = 1;
		break;
	case ENET_100_RGMII:
		*speed = ENET_SPEED_100BT;
		*rpm = 1;
		*limited_to_full_duplex = 1;
		break;
	case ENET_1000_GMII:
		*speed = ENET_SPEED_1000BT;
		*limited_to_full_duplex = 1;
		break;
	case ENET_1000_RGMII:
		*speed = ENET_SPEED_1000BT;
		*rpm = 1;
		*limited_to_full_duplex = 1;
		break;
	case ENET_1000_TBI:
		*speed = ENET_SPEED_1000BT;
		*tbi = 1;
		*limited_to_full_duplex = 1;
		break;
	case ENET_1000_RTBI:
		*speed = ENET_SPEED_1000BT;
		*rpm = 1;
		*tbi = 1;
		*limited_to_full_duplex = 1;
		break;
	default:
		return -EINVAL;
		break;
	}

	return 0;
}

static struct sk_buff *get_new_skb(ucc_geth_private_t *ugeth, u8 *bd)
{
	struct sk_buff *skb = NULL;

	skb = dev_alloc_skb(ugeth->ug_info->uf_info.max_rx_buf_length +
				  UCC_GETH_RX_DATA_BUF_ALIGNMENT);

	if (skb == NULL)
		return NULL;

	/* We need the data buffer to be aligned properly.  We will reserve
	 * as many bytes as needed to align the data properly
	 */
	skb_reserve(skb,
		    UCC_GETH_RX_DATA_BUF_ALIGNMENT -
		    (((unsigned)skb->data) & (UCC_GETH_RX_DATA_BUF_ALIGNMENT -
					      1)));

	skb->dev = ugeth->dev;

	BD_BUFFER_SET(bd,
		      dma_map_single(NULL,
				     skb->data,
				     ugeth->ug_info->uf_info.max_rx_buf_length +
				     UCC_GETH_RX_DATA_BUF_ALIGNMENT,
				     DMA_FROM_DEVICE));

	BD_STATUS_AND_LENGTH_SET(bd,
				 (R_E | R_I |
				  (BD_STATUS_AND_LENGTH(bd) & R_W)));

	return skb;
}

static int rx_bd_buffer_set(ucc_geth_private_t *ugeth, u8 rxQ)
{
	u8 *bd;
	u32 bd_status;
	struct sk_buff *skb;
	int i;

	bd = ugeth->p_rx_bd_ring[rxQ];
	i = 0;

	do {
		bd_status = BD_STATUS_AND_LENGTH(bd);
		skb = get_new_skb(ugeth, bd);

		if (!skb)	/* If can not allocate data buffer,
				abort. Cleanup will be elsewhere */
			return -ENOMEM;

		ugeth->rx_skbuff[rxQ][i] = skb;

		/* advance the BD pointer */
		bd += UCC_GETH_SIZE_OF_BD;
		i++;
	} while (!(bd_status & R_W));

	return 0;
}

static int fill_init_enet_entries(ucc_geth_private_t *ugeth,
				  volatile u32 *p_start,
				  u8 num_entries,
				  u32 thread_size,
				  u32 thread_alignment,
				  qe_risc_allocation_e risc,
				  int skip_page_for_first_entry)
{
	u32 init_enet_offset;
	u8 i;
	int snum;

	for (i = 0; i < num_entries; i++) {
		if ((snum = qe_get_snum()) < 0) {
			ugeth_err("fill_init_enet_entries: Can not get SNUM.");
			return snum;
		}
		if ((i == 0) && skip_page_for_first_entry)
		/* First entry of Rx does not have page */
			init_enet_offset = 0;
		else {
			init_enet_offset =
			    qe_muram_alloc(thread_size, thread_alignment);
			if (IS_MURAM_ERR(init_enet_offset)) {
				ugeth_err
		("fill_init_enet_entries: Can not allocate DPRAM memory.");
				qe_put_snum((u8) snum);
				return -ENOMEM;
			}
		}
		*(p_start++) =
		    ((u8) snum << ENET_INIT_PARAM_SNUM_SHIFT) | init_enet_offset
		    | risc;
	}

	return 0;
}

static int return_init_enet_entries(ucc_geth_private_t *ugeth,
				    volatile u32 *p_start,
				    u8 num_entries,
				    qe_risc_allocation_e risc,
				    int skip_page_for_first_entry)
{
	u32 init_enet_offset;
	u8 i;
	int snum;

	for (i = 0; i < num_entries; i++) {
		/* Check that this entry was actually valid --
		needed in case failed in allocations */
		if ((*p_start & ENET_INIT_PARAM_RISC_MASK) == risc) {
			snum =
			    (u32) (*p_start & ENET_INIT_PARAM_SNUM_MASK) >>
			    ENET_INIT_PARAM_SNUM_SHIFT;
			qe_put_snum((u8) snum);
			if (!((i == 0) && skip_page_for_first_entry)) {
			/* First entry of Rx does not have page */
				init_enet_offset =
				    (in_be32(p_start) &
				     ENET_INIT_PARAM_PTR_MASK);
				qe_muram_free(init_enet_offset);
			}
			*(p_start++) = 0;	/* Just for cosmetics */
		}
	}

	return 0;
}

#ifdef DEBUG
static int dump_init_enet_entries(ucc_geth_private_t *ugeth,
				  volatile u32 *p_start,
				  u8 num_entries,
				  u32 thread_size,
				  qe_risc_allocation_e risc,
				  int skip_page_for_first_entry)
{
	u32 init_enet_offset;
	u8 i;
	int snum;

	for (i = 0; i < num_entries; i++) {
		/* Check that this entry was actually valid --
		needed in case failed in allocations */
		if ((*p_start & ENET_INIT_PARAM_RISC_MASK) == risc) {
			snum =
			    (u32) (*p_start & ENET_INIT_PARAM_SNUM_MASK) >>
			    ENET_INIT_PARAM_SNUM_SHIFT;
			qe_put_snum((u8) snum);
			if (!((i == 0) && skip_page_for_first_entry)) {
			/* First entry of Rx does not have page */
				init_enet_offset =
				    (in_be32(p_start) &
				     ENET_INIT_PARAM_PTR_MASK);
				ugeth_info("Init enet entry %d:", i);
				ugeth_info("Base address: 0x%08x",
					   (u32)
					   qe_muram_addr(init_enet_offset));
				mem_disp(qe_muram_addr(init_enet_offset),
					 thread_size);
			}
			p_start++;
		}
	}

	return 0;
}
#endif

#ifdef CONFIG_UGETH_FILTERING
static enet_addr_container_t *get_enet_addr_container(void)
{
	enet_addr_container_t *enet_addr_cont;

	/* allocate memory */
	enet_addr_cont = kmalloc(sizeof(enet_addr_container_t), GFP_KERNEL);
	if (!enet_addr_cont) {
		ugeth_err("%s: No memory for enet_addr_container_t object.",
			  __FUNCTION__);
		return NULL;
	}

	return enet_addr_cont;
}
#endif /* CONFIG_UGETH_FILTERING */

static void put_enet_addr_container(enet_addr_container_t *enet_addr_cont)
{
	kfree(enet_addr_cont);
}

#ifdef CONFIG_UGETH_FILTERING
static int hw_add_addr_in_paddr(ucc_geth_private_t *ugeth,
				enet_addr_t *p_enet_addr, u8 paddr_num)
{
	ucc_geth_82xx_address_filtering_pram_t *p_82xx_addr_filt;

	if (!(paddr_num < NUM_OF_PADDRS)) {
		ugeth_warn("%s: Illagel paddr_num.", __FUNCTION__);
		return -EINVAL;
	}

	p_82xx_addr_filt =
	    (ucc_geth_82xx_address_filtering_pram_t *) ugeth->p_rx_glbl_pram->
	    addressfiltering;

	/* Ethernet frames are defined in Little Endian mode,    */
	/* therefore to insert the address we reverse the bytes. */
	out_be16(&p_82xx_addr_filt->paddr[paddr_num].h,
		 (u16) (((u16) (((u16) ((*p_enet_addr)[5])) << 8)) |
			(u16) (*p_enet_addr)[4]));
	out_be16(&p_82xx_addr_filt->paddr[paddr_num].m,
		 (u16) (((u16) (((u16) ((*p_enet_addr)[3])) << 8)) |
			(u16) (*p_enet_addr)[2]));
	out_be16(&p_82xx_addr_filt->paddr[paddr_num].l,
		 (u16) (((u16) (((u16) ((*p_enet_addr)[1])) << 8)) |
			(u16) (*p_enet_addr)[0]));

	return 0;
}
#endif /* CONFIG_UGETH_FILTERING */

static int hw_clear_addr_in_paddr(ucc_geth_private_t *ugeth, u8 paddr_num)
{
	ucc_geth_82xx_address_filtering_pram_t *p_82xx_addr_filt;

	if (!(paddr_num < NUM_OF_PADDRS)) {
		ugeth_warn("%s: Illagel paddr_num.", __FUNCTION__);
		return -EINVAL;
	}

	p_82xx_addr_filt =
	    (ucc_geth_82xx_address_filtering_pram_t *) ugeth->p_rx_glbl_pram->
	    addressfiltering;

	/* Writing address ff.ff.ff.ff.ff.ff disables address
	recognition for this register */
	out_be16(&p_82xx_addr_filt->paddr[paddr_num].h, 0xffff);
	out_be16(&p_82xx_addr_filt->paddr[paddr_num].m, 0xffff);
	out_be16(&p_82xx_addr_filt->paddr[paddr_num].l, 0xffff);

	return 0;
}

static void hw_add_addr_in_hash(ucc_geth_private_t *ugeth,
				enet_addr_t *p_enet_addr)
{
	ucc_geth_82xx_address_filtering_pram_t *p_82xx_addr_filt;
	u32 cecr_subblock;

	p_82xx_addr_filt =
	    (ucc_geth_82xx_address_filtering_pram_t *) ugeth->p_rx_glbl_pram->
	    addressfiltering;

	cecr_subblock =
	    ucc_fast_get_qe_cr_subblock(ugeth->ug_info->uf_info.ucc_num);

	/* Ethernet frames are defined in Little Endian mode,
	therefor to insert */
	/* the address to the hash (Big Endian mode), we reverse the bytes.*/
	out_be16(&p_82xx_addr_filt->taddr.h,
		 (u16) (((u16) (((u16) ((*p_enet_addr)[5])) << 8)) |
			(u16) (*p_enet_addr)[4]));
	out_be16(&p_82xx_addr_filt->taddr.m,
		 (u16) (((u16) (((u16) ((*p_enet_addr)[3])) << 8)) |
			(u16) (*p_enet_addr)[2]));
	out_be16(&p_82xx_addr_filt->taddr.l,
		 (u16) (((u16) (((u16) ((*p_enet_addr)[1])) << 8)) |
			(u16) (*p_enet_addr)[0]));

	qe_issue_cmd(QE_SET_GROUP_ADDRESS, cecr_subblock,
		     (u8) QE_CR_PROTOCOL_ETHERNET, 0);
}

#ifdef CONFIG_UGETH_MAGIC_PACKET
static void magic_packet_detection_enable(ucc_geth_private_t *ugeth)
{
	ucc_fast_private_t *uccf;
	ucc_geth_t *ug_regs;
	u32 maccfg2, uccm;

	uccf = ugeth->uccf;
	ug_regs = ugeth->ug_regs;

	/* Enable interrupts for magic packet detection */
	uccm = in_be32(uccf->p_uccm);
	uccm |= UCCE_MPD;
	out_be32(uccf->p_uccm, uccm);

	/* Enable magic packet detection */
	maccfg2 = in_be32(&ug_regs->maccfg2);
	maccfg2 |= MACCFG2_MPE;
	out_be32(&ug_regs->maccfg2, maccfg2);
}

static void magic_packet_detection_disable(ucc_geth_private_t *ugeth)
{
	ucc_fast_private_t *uccf;
	ucc_geth_t *ug_regs;
	u32 maccfg2, uccm;

	uccf = ugeth->uccf;
	ug_regs = ugeth->ug_regs;

	/* Disable interrupts for magic packet detection */
	uccm = in_be32(uccf->p_uccm);
	uccm &= ~UCCE_MPD;
	out_be32(uccf->p_uccm, uccm);

	/* Disable magic packet detection */
	maccfg2 = in_be32(&ug_regs->maccfg2);
	maccfg2 &= ~MACCFG2_MPE;
	out_be32(&ug_regs->maccfg2, maccfg2);
}
#endif /* MAGIC_PACKET */

static inline int compare_addr(enet_addr_t *addr1, enet_addr_t *addr2)
{
	return memcmp(addr1, addr2, ENET_NUM_OCTETS_PER_ADDRESS);
}

#ifdef DEBUG
static void get_statistics(ucc_geth_private_t *ugeth,
			   ucc_geth_tx_firmware_statistics_t *
			   tx_firmware_statistics,
			   ucc_geth_rx_firmware_statistics_t *
			   rx_firmware_statistics,
			   ucc_geth_hardware_statistics_t *hardware_statistics)
{
	ucc_fast_t *uf_regs;
	ucc_geth_t *ug_regs;
	ucc_geth_tx_firmware_statistics_pram_t *p_tx_fw_statistics_pram;
	ucc_geth_rx_firmware_statistics_pram_t *p_rx_fw_statistics_pram;

	ug_regs = ugeth->ug_regs;
	uf_regs = (ucc_fast_t *) ug_regs;
	p_tx_fw_statistics_pram = ugeth->p_tx_fw_statistics_pram;
	p_rx_fw_statistics_pram = ugeth->p_rx_fw_statistics_pram;

	/* Tx firmware only if user handed pointer and driver actually
	gathers Tx firmware statistics */
	if (tx_firmware_statistics && p_tx_fw_statistics_pram) {
		tx_firmware_statistics->sicoltx =
		    in_be32(&p_tx_fw_statistics_pram->sicoltx);
		tx_firmware_statistics->mulcoltx =
		    in_be32(&p_tx_fw_statistics_pram->mulcoltx);
		tx_firmware_statistics->latecoltxfr =
		    in_be32(&p_tx_fw_statistics_pram->latecoltxfr);
		tx_firmware_statistics->frabortduecol =
		    in_be32(&p_tx_fw_statistics_pram->frabortduecol);
		tx_firmware_statistics->frlostinmactxer =
		    in_be32(&p_tx_fw_statistics_pram->frlostinmactxer);
		tx_firmware_statistics->carriersenseertx =
		    in_be32(&p_tx_fw_statistics_pram->carriersenseertx);
		tx_firmware_statistics->frtxok =
		    in_be32(&p_tx_fw_statistics_pram->frtxok);
		tx_firmware_statistics->txfrexcessivedefer =
		    in_be32(&p_tx_fw_statistics_pram->txfrexcessivedefer);
		tx_firmware_statistics->txpkts256 =
		    in_be32(&p_tx_fw_statistics_pram->txpkts256);
		tx_firmware_statistics->txpkts512 =
		    in_be32(&p_tx_fw_statistics_pram->txpkts512);
		tx_firmware_statistics->txpkts1024 =
		    in_be32(&p_tx_fw_statistics_pram->txpkts1024);
		tx_firmware_statistics->txpktsjumbo =
		    in_be32(&p_tx_fw_statistics_pram->txpktsjumbo);
	}

	/* Rx firmware only if user handed pointer and driver actually
	 * gathers Rx firmware statistics */
	if (rx_firmware_statistics && p_rx_fw_statistics_pram) {
		int i;
		rx_firmware_statistics->frrxfcser =
		    in_be32(&p_rx_fw_statistics_pram->frrxfcser);
		rx_firmware_statistics->fraligner =
		    in_be32(&p_rx_fw_statistics_pram->fraligner);
		rx_firmware_statistics->inrangelenrxer =
		    in_be32(&p_rx_fw_statistics_pram->inrangelenrxer);
		rx_firmware_statistics->outrangelenrxer =
		    in_be32(&p_rx_fw_statistics_pram->outrangelenrxer);
		rx_firmware_statistics->frtoolong =
		    in_be32(&p_rx_fw_statistics_pram->frtoolong);
		rx_firmware_statistics->runt =
		    in_be32(&p_rx_fw_statistics_pram->runt);
		rx_firmware_statistics->verylongevent =
		    in_be32(&p_rx_fw_statistics_pram->verylongevent);
		rx_firmware_statistics->symbolerror =
		    in_be32(&p_rx_fw_statistics_pram->symbolerror);
		rx_firmware_statistics->dropbsy =
		    in_be32(&p_rx_fw_statistics_pram->dropbsy);
		for (i = 0; i < 0x8; i++)
			rx_firmware_statistics->res0[i] =
			    p_rx_fw_statistics_pram->res0[i];
		rx_firmware_statistics->mismatchdrop =
		    in_be32(&p_rx_fw_statistics_pram->mismatchdrop);
		rx_firmware_statistics->underpkts =
		    in_be32(&p_rx_fw_statistics_pram->underpkts);
		rx_firmware_statistics->pkts256 =
		    in_be32(&p_rx_fw_statistics_pram->pkts256);
		rx_firmware_statistics->pkts512 =
		    in_be32(&p_rx_fw_statistics_pram->pkts512);
		rx_firmware_statistics->pkts1024 =
		    in_be32(&p_rx_fw_statistics_pram->pkts1024);
		rx_firmware_statistics->pktsjumbo =
		    in_be32(&p_rx_fw_statistics_pram->pktsjumbo);
		rx_firmware_statistics->frlossinmacer =
		    in_be32(&p_rx_fw_statistics_pram->frlossinmacer);
		rx_firmware_statistics->pausefr =
		    in_be32(&p_rx_fw_statistics_pram->pausefr);
		for (i = 0; i < 0x4; i++)
			rx_firmware_statistics->res1[i] =
			    p_rx_fw_statistics_pram->res1[i];
		rx_firmware_statistics->removevlan =
		    in_be32(&p_rx_fw_statistics_pram->removevlan);
		rx_firmware_statistics->replacevlan =
		    in_be32(&p_rx_fw_statistics_pram->replacevlan);
		rx_firmware_statistics->insertvlan =
		    in_be32(&p_rx_fw_statistics_pram->insertvlan);
	}

	/* Hardware only if user handed pointer and driver actually
	gathers hardware statistics */
	if (hardware_statistics && (in_be32(&uf_regs->upsmr) & UPSMR_HSE)) {
		hardware_statistics->tx64 = in_be32(&ug_regs->tx64);
		hardware_statistics->tx127 = in_be32(&ug_regs->tx127);
		hardware_statistics->tx255 = in_be32(&ug_regs->tx255);
		hardware_statistics->rx64 = in_be32(&ug_regs->rx64);
		hardware_statistics->rx127 = in_be32(&ug_regs->rx127);
		hardware_statistics->rx255 = in_be32(&ug_regs->rx255);
		hardware_statistics->txok = in_be32(&ug_regs->txok);
		hardware_statistics->txcf = in_be16(&ug_regs->txcf);
		hardware_statistics->tmca = in_be32(&ug_regs->tmca);
		hardware_statistics->tbca = in_be32(&ug_regs->tbca);
		hardware_statistics->rxfok = in_be32(&ug_regs->rxfok);
		hardware_statistics->rxbok = in_be32(&ug_regs->rxbok);
		hardware_statistics->rbyt = in_be32(&ug_regs->rbyt);
		hardware_statistics->rmca = in_be32(&ug_regs->rmca);
		hardware_statistics->rbca = in_be32(&ug_regs->rbca);
	}
}

static void dump_bds(ucc_geth_private_t *ugeth)
{
	int i;
	int length;

	for (i = 0; i < ugeth->ug_info->numQueuesTx; i++) {
		if (ugeth->p_tx_bd_ring[i]) {
			length =
			    (ugeth->ug_info->bdRingLenTx[i] *
			     UCC_GETH_SIZE_OF_BD);
			ugeth_info("TX BDs[%d]", i);
			mem_disp(ugeth->p_tx_bd_ring[i], length);
		}
	}
	for (i = 0; i < ugeth->ug_info->numQueuesRx; i++) {
		if (ugeth->p_rx_bd_ring[i]) {
			length =
			    (ugeth->ug_info->bdRingLenRx[i] *
			     UCC_GETH_SIZE_OF_BD);
			ugeth_info("RX BDs[%d]", i);
			mem_disp(ugeth->p_rx_bd_ring[i], length);
		}
	}
}

static void dump_regs(ucc_geth_private_t *ugeth)
{
	int i;

	ugeth_info("UCC%d Geth registers:", ugeth->ug_info->uf_info.ucc_num);
	ugeth_info("Base address: 0x%08x", (u32) ugeth->ug_regs);

	ugeth_info("maccfg1    : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->maccfg1,
		   in_be32(&ugeth->ug_regs->maccfg1));
	ugeth_info("maccfg2    : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->maccfg2,
		   in_be32(&ugeth->ug_regs->maccfg2));
	ugeth_info("ipgifg     : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->ipgifg,
		   in_be32(&ugeth->ug_regs->ipgifg));
	ugeth_info("hafdup     : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->hafdup,
		   in_be32(&ugeth->ug_regs->hafdup));
	ugeth_info("miimcfg    : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->miimng.miimcfg,
		   in_be32(&ugeth->ug_regs->miimng.miimcfg));
	ugeth_info("miimcom    : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->miimng.miimcom,
		   in_be32(&ugeth->ug_regs->miimng.miimcom));
	ugeth_info("miimadd    : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->miimng.miimadd,
		   in_be32(&ugeth->ug_regs->miimng.miimadd));
	ugeth_info("miimcon    : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->miimng.miimcon,
		   in_be32(&ugeth->ug_regs->miimng.miimcon));
	ugeth_info("miimstat   : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->miimng.miimstat,
		   in_be32(&ugeth->ug_regs->miimng.miimstat));
	ugeth_info("miimmind   : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->miimng.miimind,
		   in_be32(&ugeth->ug_regs->miimng.miimind));
	ugeth_info("ifctl      : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->ifctl,
		   in_be32(&ugeth->ug_regs->ifctl));
	ugeth_info("ifstat     : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->ifstat,
		   in_be32(&ugeth->ug_regs->ifstat));
	ugeth_info("macstnaddr1: addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->macstnaddr1,
		   in_be32(&ugeth->ug_regs->macstnaddr1));
	ugeth_info("macstnaddr2: addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->macstnaddr2,
		   in_be32(&ugeth->ug_regs->macstnaddr2));
	ugeth_info("uempr      : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->uempr,
		   in_be32(&ugeth->ug_regs->uempr));
	ugeth_info("utbipar    : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->utbipar,
		   in_be32(&ugeth->ug_regs->utbipar));
	ugeth_info("uescr      : addr - 0x%08x, val - 0x%04x",
		   (u32) & ugeth->ug_regs->uescr,
		   in_be16(&ugeth->ug_regs->uescr));
	ugeth_info("tx64       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->tx64,
		   in_be32(&ugeth->ug_regs->tx64));
	ugeth_info("tx127      : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->tx127,
		   in_be32(&ugeth->ug_regs->tx127));
	ugeth_info("tx255      : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->tx255,
		   in_be32(&ugeth->ug_regs->tx255));
	ugeth_info("rx64       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->rx64,
		   in_be32(&ugeth->ug_regs->rx64));
	ugeth_info("rx127      : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->rx127,
		   in_be32(&ugeth->ug_regs->rx127));
	ugeth_info("rx255      : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->rx255,
		   in_be32(&ugeth->ug_regs->rx255));
	ugeth_info("txok       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->txok,
		   in_be32(&ugeth->ug_regs->txok));
	ugeth_info("txcf       : addr - 0x%08x, val - 0x%04x",
		   (u32) & ugeth->ug_regs->txcf,
		   in_be16(&ugeth->ug_regs->txcf));
	ugeth_info("tmca       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->tmca,
		   in_be32(&ugeth->ug_regs->tmca));
	ugeth_info("tbca       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->tbca,
		   in_be32(&ugeth->ug_regs->tbca));
	ugeth_info("rxfok      : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->rxfok,
		   in_be32(&ugeth->ug_regs->rxfok));
	ugeth_info("rxbok      : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->rxbok,
		   in_be32(&ugeth->ug_regs->rxbok));
	ugeth_info("rbyt       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->rbyt,
		   in_be32(&ugeth->ug_regs->rbyt));
	ugeth_info("rmca       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->rmca,
		   in_be32(&ugeth->ug_regs->rmca));
	ugeth_info("rbca       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->rbca,
		   in_be32(&ugeth->ug_regs->rbca));
	ugeth_info("scar       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->scar,
		   in_be32(&ugeth->ug_regs->scar));
	ugeth_info("scam       : addr - 0x%08x, val - 0x%08x",
		   (u32) & ugeth->ug_regs->scam,
		   in_be32(&ugeth->ug_regs->scam));

	if (ugeth->p_thread_data_tx) {
		int numThreadsTxNumerical;
		switch (ugeth->ug_info->numThreadsTx) {
		case UCC_GETH_NUM_OF_THREADS_1:
			numThreadsTxNumerical = 1;
			break;
		case UCC_GETH_NUM_OF_THREADS_2:
			numThreadsTxNumerical = 2;
			break;
		case UCC_GETH_NUM_OF_THREADS_4:
			numThreadsTxNumerical = 4;
			break;
		case UCC_GETH_NUM_OF_THREADS_6:
			numThreadsTxNumerical = 6;
			break;
		case UCC_GETH_NUM_OF_THREADS_8:
			numThreadsTxNumerical = 8;
			break;
		default:
			numThreadsTxNumerical = 0;
			break;
		}

		ugeth_info("Thread data TXs:");
		ugeth_info("Base address: 0x%08x",
			   (u32) ugeth->p_thread_data_tx);
		for (i = 0; i < numThreadsTxNumerical; i++) {
			ugeth_info("Thread data TX[%d]:", i);
			ugeth_info("Base address: 0x%08x",
				   (u32) & ugeth->p_thread_data_tx[i]);
			mem_disp((u8 *) & ugeth->p_thread_data_tx[i],
				 sizeof(ucc_geth_thread_data_tx_t));
		}
	}
	if (ugeth->p_thread_data_rx) {
		int numThreadsRxNumerical;
		switch (ugeth->ug_info->numThreadsRx) {
		case UCC_GETH_NUM_OF_THREADS_1:
			numThreadsRxNumerical = 1;
			break;
		case UCC_GETH_NUM_OF_THREADS_2:
			numThreadsRxNumerical = 2;
			break;
		case UCC_GETH_NUM_OF_THREADS_4:
			numThreadsRxNumerical = 4;
			break;
		case UCC_GETH_NUM_OF_THREADS_6:
			numThreadsRxNumerical = 6;
			break;
		case UCC_GETH_NUM_OF_THREADS_8:
			numThreadsRxNumerical = 8;
			break;
		default:
			numThreadsRxNumerical = 0;
			break;
		}

		ugeth_info("Thread data RX:");
		ugeth_info("Base address: 0x%08x",
			   (u32) ugeth->p_thread_data_rx);
		for (i = 0; i < numThreadsRxNumerical; i++) {
			ugeth_info("Thread data RX[%d]:", i);
			ugeth_info("Base address: 0x%08x",
				   (u32) & ugeth->p_thread_data_rx[i]);
			mem_disp((u8 *) & ugeth->p_thread_data_rx[i],
				 sizeof(ucc_geth_thread_data_rx_t));
		}
	}
	if (ugeth->p_exf_glbl_param) {
		ugeth_info("EXF global param:");
		ugeth_info("Base address: 0x%08x",
			   (u32) ugeth->p_exf_glbl_param);
		mem_disp((u8 *) ugeth->p_exf_glbl_param,
			 sizeof(*ugeth->p_exf_glbl_param));
	}
	if (ugeth->p_tx_glbl_pram) {
		ugeth_info("TX global param:");
		ugeth_info("Base address: 0x%08x", (u32) ugeth->p_tx_glbl_pram);
		ugeth_info("temoder      : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_tx_glbl_pram->temoder,
			   in_be16(&ugeth->p_tx_glbl_pram->temoder));
		ugeth_info("sqptr        : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->sqptr,
			   in_be32(&ugeth->p_tx_glbl_pram->sqptr));
		ugeth_info("schedulerbasepointer: addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->schedulerbasepointer,
			   in_be32(&ugeth->p_tx_glbl_pram->
				   schedulerbasepointer));
		ugeth_info("txrmonbaseptr: addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->txrmonbaseptr,
			   in_be32(&ugeth->p_tx_glbl_pram->txrmonbaseptr));
		ugeth_info("tstate       : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->tstate,
			   in_be32(&ugeth->p_tx_glbl_pram->tstate));
		ugeth_info("iphoffset[0] : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_tx_glbl_pram->iphoffset[0],
			   ugeth->p_tx_glbl_pram->iphoffset[0]);
		ugeth_info("iphoffset[1] : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_tx_glbl_pram->iphoffset[1],
			   ugeth->p_tx_glbl_pram->iphoffset[1]);
		ugeth_info("iphoffset[2] : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_tx_glbl_pram->iphoffset[2],
			   ugeth->p_tx_glbl_pram->iphoffset[2]);
		ugeth_info("iphoffset[3] : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_tx_glbl_pram->iphoffset[3],
			   ugeth->p_tx_glbl_pram->iphoffset[3]);
		ugeth_info("iphoffset[4] : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_tx_glbl_pram->iphoffset[4],
			   ugeth->p_tx_glbl_pram->iphoffset[4]);
		ugeth_info("iphoffset[5] : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_tx_glbl_pram->iphoffset[5],
			   ugeth->p_tx_glbl_pram->iphoffset[5]);
		ugeth_info("iphoffset[6] : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_tx_glbl_pram->iphoffset[6],
			   ugeth->p_tx_glbl_pram->iphoffset[6]);
		ugeth_info("iphoffset[7] : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_tx_glbl_pram->iphoffset[7],
			   ugeth->p_tx_glbl_pram->iphoffset[7]);
		ugeth_info("vtagtable[0] : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->vtagtable[0],
			   in_be32(&ugeth->p_tx_glbl_pram->vtagtable[0]));
		ugeth_info("vtagtable[1] : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->vtagtable[1],
			   in_be32(&ugeth->p_tx_glbl_pram->vtagtable[1]));
		ugeth_info("vtagtable[2] : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->vtagtable[2],
			   in_be32(&ugeth->p_tx_glbl_pram->vtagtable[2]));
		ugeth_info("vtagtable[3] : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->vtagtable[3],
			   in_be32(&ugeth->p_tx_glbl_pram->vtagtable[3]));
		ugeth_info("vtagtable[4] : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->vtagtable[4],
			   in_be32(&ugeth->p_tx_glbl_pram->vtagtable[4]));
		ugeth_info("vtagtable[5] : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->vtagtable[5],
			   in_be32(&ugeth->p_tx_glbl_pram->vtagtable[5]));
		ugeth_info("vtagtable[6] : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->vtagtable[6],
			   in_be32(&ugeth->p_tx_glbl_pram->vtagtable[6]));
		ugeth_info("vtagtable[7] : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->vtagtable[7],
			   in_be32(&ugeth->p_tx_glbl_pram->vtagtable[7]));
		ugeth_info("tqptr        : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_tx_glbl_pram->tqptr,
			   in_be32(&ugeth->p_tx_glbl_pram->tqptr));
	}
	if (ugeth->p_rx_glbl_pram) {
		ugeth_info("RX global param:");
		ugeth_info("Base address: 0x%08x", (u32) ugeth->p_rx_glbl_pram);
		ugeth_info("remoder         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->remoder,
			   in_be32(&ugeth->p_rx_glbl_pram->remoder));
		ugeth_info("rqptr           : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->rqptr,
			   in_be32(&ugeth->p_rx_glbl_pram->rqptr));
		ugeth_info("typeorlen       : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_rx_glbl_pram->typeorlen,
			   in_be16(&ugeth->p_rx_glbl_pram->typeorlen));
		ugeth_info("rxgstpack       : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_rx_glbl_pram->rxgstpack,
			   ugeth->p_rx_glbl_pram->rxgstpack);
		ugeth_info("rxrmonbaseptr   : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->rxrmonbaseptr,
			   in_be32(&ugeth->p_rx_glbl_pram->rxrmonbaseptr));
		ugeth_info("intcoalescingptr: addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->intcoalescingptr,
			   in_be32(&ugeth->p_rx_glbl_pram->intcoalescingptr));
		ugeth_info("rstate          : addr - 0x%08x, val - 0x%02x",
			   (u32) & ugeth->p_rx_glbl_pram->rstate,
			   ugeth->p_rx_glbl_pram->rstate);
		ugeth_info("mrblr           : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_rx_glbl_pram->mrblr,
			   in_be16(&ugeth->p_rx_glbl_pram->mrblr));
		ugeth_info("rbdqptr         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->rbdqptr,
			   in_be32(&ugeth->p_rx_glbl_pram->rbdqptr));
		ugeth_info("mflr            : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_rx_glbl_pram->mflr,
			   in_be16(&ugeth->p_rx_glbl_pram->mflr));
		ugeth_info("minflr          : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_rx_glbl_pram->minflr,
			   in_be16(&ugeth->p_rx_glbl_pram->minflr));
		ugeth_info("maxd1           : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_rx_glbl_pram->maxd1,
			   in_be16(&ugeth->p_rx_glbl_pram->maxd1));
		ugeth_info("maxd2           : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_rx_glbl_pram->maxd2,
			   in_be16(&ugeth->p_rx_glbl_pram->maxd2));
		ugeth_info("ecamptr         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->ecamptr,
			   in_be32(&ugeth->p_rx_glbl_pram->ecamptr));
		ugeth_info("l2qt            : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l2qt,
			   in_be32(&ugeth->p_rx_glbl_pram->l2qt));
		ugeth_info("l3qt[0]         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l3qt[0],
			   in_be32(&ugeth->p_rx_glbl_pram->l3qt[0]));
		ugeth_info("l3qt[1]         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l3qt[1],
			   in_be32(&ugeth->p_rx_glbl_pram->l3qt[1]));
		ugeth_info("l3qt[2]         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l3qt[2],
			   in_be32(&ugeth->p_rx_glbl_pram->l3qt[2]));
		ugeth_info("l3qt[3]         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l3qt[3],
			   in_be32(&ugeth->p_rx_glbl_pram->l3qt[3]));
		ugeth_info("l3qt[4]         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l3qt[4],
			   in_be32(&ugeth->p_rx_glbl_pram->l3qt[4]));
		ugeth_info("l3qt[5]         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l3qt[5],
			   in_be32(&ugeth->p_rx_glbl_pram->l3qt[5]));
		ugeth_info("l3qt[6]         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l3qt[6],
			   in_be32(&ugeth->p_rx_glbl_pram->l3qt[6]));
		ugeth_info("l3qt[7]         : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->l3qt[7],
			   in_be32(&ugeth->p_rx_glbl_pram->l3qt[7]));
		ugeth_info("vlantype        : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_rx_glbl_pram->vlantype,
			   in_be16(&ugeth->p_rx_glbl_pram->vlantype));
		ugeth_info("vlantci         : addr - 0x%08x, val - 0x%04x",
			   (u32) & ugeth->p_rx_glbl_pram->vlantci,
			   in_be16(&ugeth->p_rx_glbl_pram->vlantci));
		for (i = 0; i < 64; i++)
			ugeth_info
		    ("addressfiltering[%d]: addr - 0x%08x, val - 0x%02x",
			     i,
			     (u32) & ugeth->p_rx_glbl_pram->addressfiltering[i],
			     ugeth->p_rx_glbl_pram->addressfiltering[i]);
		ugeth_info("exfGlobalParam  : addr - 0x%08x, val - 0x%08x",
			   (u32) & ugeth->p_rx_glbl_pram->exfGlobalParam,
			   in_be32(&ugeth->p_rx_glbl_pram->exfGlobalParam));
	}
	if (ugeth->p_send_q_mem_reg) {
		ugeth_info("Send Q memory registers:");
		ugeth_info("Base address: 0x%08x",
			   (u32) ugeth->p_send_q_mem_reg);
		for (i = 0; i < ugeth->ug_info->numQueuesTx; i++) {
			ugeth_info("SQQD[%d]:", i);
			ugeth_info("Base address: 0x%08x",
				   (u32) & ugeth->p_send_q_mem_reg->sqqd[i]);
			mem_disp((u8 *) & ugeth->p_send_q_mem_reg->sqqd[i],
				 sizeof(ucc_geth_send_queue_qd_t));
		}
	}
	if (ugeth->p_scheduler) {
		ugeth_info("Scheduler:");
		ugeth_info("Base address: 0x%08x", (u32) ugeth->p_scheduler);
		mem_disp((u8 *) ugeth->p_scheduler,
			 sizeof(*ugeth->p_scheduler));
	}
	if (ugeth->p_tx_fw_statistics_pram) {
		ugeth_info("TX FW statistics pram:");
		ugeth_info("Base address: 0x%08x",
			   (u32) ugeth->p_tx_fw_statistics_pram);
		mem_disp((u8 *) ugeth->p_tx_fw_statistics_pram,
			 sizeof(*ugeth->p_tx_fw_statistics_pram));
	}
	if (ugeth->p_rx_fw_statistics_pram) {
		ugeth_info("RX FW statistics pram:");
		ugeth_info("Base address: 0x%08x",
			   (u32) ugeth->p_rx_fw_statistics_pram);
		mem_disp((u8 *) ugeth->p_rx_fw_statistics_pram,
			 sizeof(*ugeth->p_rx_fw_statistics_pram));
	}
	if (ugeth->p_rx_irq_coalescing_tbl) {
		ugeth_info("RX IRQ coalescing tables:");
		ugeth_info("Base address: 0x%08x",
			   (u32) ugeth->p_rx_irq_coalescing_tbl);
		for (i = 0; i < ugeth->ug_info->numQueuesRx; i++) {
			ugeth_info("RX IRQ coalescing table entry[%d]:", i);
			ugeth_info("Base address: 0x%08x",
				   (u32) & ugeth->p_rx_irq_coalescing_tbl->
				   coalescingentry[i]);
			ugeth_info
		("interruptcoalescingmaxvalue: addr - 0x%08x, val - 0x%08x",
			     (u32) & ugeth->p_rx_irq_coalescing_tbl->
			     coalescingentry[i].interruptcoalescingmaxvalue,
			     in_be32(&ugeth->p_rx_irq_coalescing_tbl->
				     coalescingentry[i].
				     interruptcoalescingmaxvalue));
			ugeth_info
		("interruptcoalescingcounter : addr - 0x%08x, val - 0x%08x",
			     (u32) & ugeth->p_rx_irq_coalescing_tbl->
			     coalescingentry[i].interruptcoalescingcounter,
			     in_be32(&ugeth->p_rx_irq_coalescing_tbl->
				     coalescingentry[i].
				     interruptcoalescingcounter));
		}
	}
	if (ugeth->p_rx_bd_qs_tbl) {
		ugeth_info("RX BD QS tables:");
		ugeth_info("Base address: 0x%08x", (u32) ugeth->p_rx_bd_qs_tbl);
		for (i = 0; i < ugeth->ug_info->numQueuesRx; i++) {
			ugeth_info("RX BD QS table[%d]:", i);
			ugeth_info("Base address: 0x%08x",
				   (u32) & ugeth->p_rx_bd_qs_tbl[i]);
			ugeth_info
			    ("bdbaseptr        : addr - 0x%08x, val - 0x%08x",
			     (u32) & ugeth->p_rx_bd_qs_tbl[i].bdbaseptr,
			     in_be32(&ugeth->p_rx_bd_qs_tbl[i].bdbaseptr));
			ugeth_info
			    ("bdptr            : addr - 0x%08x, val - 0x%08x",
			     (u32) & ugeth->p_rx_bd_qs_tbl[i].bdptr,
			     in_be32(&ugeth->p_rx_bd_qs_tbl[i].bdptr));
			ugeth_info
			    ("externalbdbaseptr: addr - 0x%08x, val - 0x%08x",
			     (u32) & ugeth->p_rx_bd_qs_tbl[i].externalbdbaseptr,
			     in_be32(&ugeth->p_rx_bd_qs_tbl[i].
				     externalbdbaseptr));
			ugeth_info
			    ("externalbdptr    : addr - 0x%08x, val - 0x%08x",
			     (u32) & ugeth->p_rx_bd_qs_tbl[i].externalbdptr,
			     in_be32(&ugeth->p_rx_bd_qs_tbl[i].externalbdptr));
			ugeth_info("ucode RX Prefetched BDs:");
			ugeth_info("Base address: 0x%08x",
				   (u32)
				   qe_muram_addr(in_be32
						 (&ugeth->p_rx_bd_qs_tbl[i].
						  bdbaseptr)));
			mem_disp((u8 *)
				 qe_muram_addr(in_be32
					       (&ugeth->p_rx_bd_qs_tbl[i].
						bdbaseptr)),
				 sizeof(ucc_geth_rx_prefetched_bds_t));
		}
	}
	if (ugeth->p_init_enet_param_shadow) {
		int size;
		ugeth_info("Init enet param shadow:");
		ugeth_info("Base address: 0x%08x",
			   (u32) ugeth->p_init_enet_param_shadow);
		mem_disp((u8 *) ugeth->p_init_enet_param_shadow,
			 sizeof(*ugeth->p_init_enet_param_shadow));

		size = sizeof(ucc_geth_thread_rx_pram_t);
		if (ugeth->ug_info->rxExtendedFiltering) {
			size +=
			    THREAD_RX_PRAM_ADDITIONAL_FOR_EXTENDED_FILTERING;
			if (ugeth->ug_info->largestexternallookupkeysize ==
			    QE_FLTR_TABLE_LOOKUP_KEY_SIZE_8_BYTES)
				size +=
			THREAD_RX_PRAM_ADDITIONAL_FOR_EXTENDED_FILTERING_8;
			if (ugeth->ug_info->largestexternallookupkeysize ==
			    QE_FLTR_TABLE_LOOKUP_KEY_SIZE_16_BYTES)
				size +=
			THREAD_RX_PRAM_ADDITIONAL_FOR_EXTENDED_FILTERING_16;
		}

		dump_init_enet_entries(ugeth,
				       &(ugeth->p_init_enet_param_shadow->
					 txthread[0]),
				       ENET_INIT_PARAM_MAX_ENTRIES_TX,
				       sizeof(ucc_geth_thread_tx_pram_t),
				       ugeth->ug_info->riscTx, 0);
		dump_init_enet_entries(ugeth,
				       &(ugeth->p_init_enet_param_shadow->
					 rxthread[0]),
				       ENET_INIT_PARAM_MAX_ENTRIES_RX, size,
				       ugeth->ug_info->riscRx, 1);
	}
}
#endif /* DEBUG */

static void init_default_reg_vals(volatile u32 *upsmr_register,
				  volatile u32 *maccfg1_register,
				  volatile u32 *maccfg2_register)
{
	out_be32(upsmr_register, UCC_GETH_UPSMR_INIT);
	out_be32(maccfg1_register, UCC_GETH_MACCFG1_INIT);
	out_be32(maccfg2_register, UCC_GETH_MACCFG2_INIT);
}

static int init_half_duplex_params(int alt_beb,
				   int back_pressure_no_backoff,
				   int no_backoff,
				   int excess_defer,
				   u8 alt_beb_truncation,
				   u8 max_retransmissions,
				   u8 collision_window,
				   volatile u32 *hafdup_register)
{
	u32 value = 0;

	if ((alt_beb_truncation > HALFDUP_ALT_BEB_TRUNCATION_MAX) ||
	    (max_retransmissions > HALFDUP_MAX_RETRANSMISSION_MAX) ||
	    (collision_window > HALFDUP_COLLISION_WINDOW_MAX))
		return -EINVAL;

	value = (u32) (alt_beb_truncation << HALFDUP_ALT_BEB_TRUNCATION_SHIFT);

	if (alt_beb)
		value |= HALFDUP_ALT_BEB;
	if (back_pressure_no_backoff)
		value |= HALFDUP_BACK_PRESSURE_NO_BACKOFF;
	if (no_backoff)
		value |= HALFDUP_NO_BACKOFF;
	if (excess_defer)
		value |= HALFDUP_EXCESSIVE_DEFER;

	value |= (max_retransmissions << HALFDUP_MAX_RETRANSMISSION_SHIFT);

	value |= collision_window;

	out_be32(hafdup_register, value);
	return 0;
}

static int init_inter_frame_gap_params(u8 non_btb_cs_ipg,
				       u8 non_btb_ipg,
				       u8 min_ifg,
				       u8 btb_ipg,
				       volatile u32 *ipgifg_register)
{
	u32 value = 0;

	/* Non-Back-to-back IPG part 1 should be <= Non-Back-to-back
	IPG part 2 */
	if (non_btb_cs_ipg > non_btb_ipg)
		return -EINVAL;

	if ((non_btb_cs_ipg > IPGIFG_NON_BACK_TO_BACK_IFG_PART1_MAX) ||
	    (non_btb_ipg > IPGIFG_NON_BACK_TO_BACK_IFG_PART2_MAX) ||
	    /*(min_ifg        > IPGIFG_MINIMUM_IFG_ENFORCEMENT_MAX) || */
	    (btb_ipg > IPGIFG_BACK_TO_BACK_IFG_MAX))
		return -EINVAL;

	value |=
	    ((non_btb_cs_ipg << IPGIFG_NON_BACK_TO_BACK_IFG_PART1_SHIFT) &
	     IPGIFG_NBTB_CS_IPG_MASK);
	value |=
	    ((non_btb_ipg << IPGIFG_NON_BACK_TO_BACK_IFG_PART2_SHIFT) &
	     IPGIFG_NBTB_IPG_MASK);
	value |=
	    ((min_ifg << IPGIFG_MINIMUM_IFG_ENFORCEMENT_SHIFT) &
	     IPGIFG_MIN_IFG_MASK);
	value |= (btb_ipg & IPGIFG_BTB_IPG_MASK);

	out_be32(ipgifg_register, value);
	return 0;
}

static int init_flow_control_params(u32 automatic_flow_control_mode,
				    int rx_flow_control_enable,
				    int tx_flow_control_enable,
				    u16 pause_period,
				    u16 extension_field,
				    volatile u32 *upsmr_register,
				    volatile u32 *uempr_register,
				    volatile u32 *maccfg1_register)
{
	u32 value = 0;

	/* Set UEMPR register */
	value = (u32) pause_period << UEMPR_PAUSE_TIME_VALUE_SHIFT;
	value |= (u32) extension_field << UEMPR_EXTENDED_PAUSE_TIME_VALUE_SHIFT;
	out_be32(uempr_register, value);

	/* Set UPSMR register */
	value = in_be32(upsmr_register);
	value |= automatic_flow_control_mode;
	out_be32(upsmr_register, value);

	value = in_be32(maccfg1_register);
	if (rx_flow_control_enable)
		value |= MACCFG1_FLOW_RX;
	if (tx_flow_control_enable)
		value |= MACCFG1_FLOW_TX;
	out_be32(maccfg1_register, value);

	return 0;
}

static int init_hw_statistics_gathering_mode(int enable_hardware_statistics,
					     int auto_zero_hardware_statistics,
					     volatile u32 *upsmr_register,
					     volatile u16 *uescr_register)
{
	u32 upsmr_value = 0;
	u16 uescr_value = 0;
	/* Enable hardware statistics gathering if requested */
	if (enable_hardware_statistics) {
		upsmr_value = in_be32(upsmr_register);
		upsmr_value |= UPSMR_HSE;
		out_be32(upsmr_register, upsmr_value);
	}

	/* Clear hardware statistics counters */
	uescr_value = in_be16(uescr_register);
	uescr_value |= UESCR_CLRCNT;
	/* Automatically zero hardware statistics counters on read,
	if requested */
	if (auto_zero_hardware_statistics)
		uescr_value |= UESCR_AUTOZ;
	out_be16(uescr_register, uescr_value);

	return 0;
}

static int init_firmware_statistics_gathering_mode(int
		enable_tx_firmware_statistics,
		int enable_rx_firmware_statistics,
		volatile u32 *tx_rmon_base_ptr,
		u32 tx_firmware_statistics_structure_address,
		volatile u32 *rx_rmon_base_ptr,
		u32 rx_firmware_statistics_structure_address,
		volatile u16 *temoder_register,
		volatile u32 *remoder_register)
{
	/* Note: this function does not check if */
	/* the parameters it receives are NULL   */
	u16 temoder_value;
	u32 remoder_value;

	if (enable_tx_firmware_statistics) {
		out_be32(tx_rmon_base_ptr,
			 tx_firmware_statistics_structure_address);
		temoder_value = in_be16(temoder_register);
		temoder_value |= TEMODER_TX_RMON_STATISTICS_ENABLE;
		out_be16(temoder_register, temoder_value);
	}

	if (enable_rx_firmware_statistics) {
		out_be32(rx_rmon_base_ptr,
			 rx_firmware_statistics_structure_address);
		remoder_value = in_be32(remoder_register);
		remoder_value |= REMODER_RX_RMON_STATISTICS_ENABLE;
		out_be32(remoder_register, remoder_value);
	}

	return 0;
}

static int init_mac_station_addr_regs(u8 address_byte_0,
				      u8 address_byte_1,
				      u8 address_byte_2,
				      u8 address_byte_3,
				      u8 address_byte_4,
				      u8 address_byte_5,
				      volatile u32 *macstnaddr1_register,
				      volatile u32 *macstnaddr2_register)
{
	u32 value = 0;

	/* Example: for a station address of 0x12345678ABCD, */
	/* 0x12 is byte 0, 0x34 is byte 1 and so on and 0xCD is byte 5 */

	/* MACSTNADDR1 Register: */

	/* 0                      7   8                      15  */
	/* station address byte 5     station address byte 4     */
	/* 16                     23  24                     31  */
	/* station address byte 3     station address byte 2     */
	value |= (u32) ((address_byte_2 << 0) & 0x000000FF);
	value |= (u32) ((address_byte_3 << 8) & 0x0000FF00);
	value |= (u32) ((address_byte_4 << 16) & 0x00FF0000);
	value |= (u32) ((address_byte_5 << 24) & 0xFF000000);

	out_be32(macstnaddr1_register, value);

	/* MACSTNADDR2 Register: */

	/* 0                      7   8                      15  */
	/* station address byte 1     station address byte 0     */
	/* 16                     23  24                     31  */
	/*         reserved                   reserved           */
	value = 0;
	value |= (u32) ((address_byte_0 << 16) & 0x00FF0000);
	value |= (u32) ((address_byte_1 << 24) & 0xFF000000);

	out_be32(macstnaddr2_register, value);

	return 0;
}

static int init_mac_duplex_mode(int full_duplex,
				int limited_to_full_duplex,
				volatile u32 *maccfg2_register)
{
	u32 value = 0;

	/* some interfaces must work in full duplex mode */
	if ((full_duplex == 0) && (limited_to_full_duplex == 1))
		return -EINVAL;

	value = in_be32(maccfg2_register);

	if (full_duplex)
		value |= MACCFG2_FDX;
	else
		value &= ~MACCFG2_FDX;

	out_be32(maccfg2_register, value);
	return 0;
}

static int init_check_frame_length_mode(int length_check,
					volatile u32 *maccfg2_register)
{
	u32 value = 0;

	value = in_be32(maccfg2_register);

	if (length_check)
		value |= MACCFG2_LC;
	else
		value &= ~MACCFG2_LC;

	out_be32(maccfg2_register, value);
	return 0;
}

static int init_preamble_length(u8 preamble_length,
				volatile u32 *maccfg2_register)
{
	u32 value = 0;

	if ((preamble_length < 3) || (preamble_length > 7))
		return -EINVAL;

	value = in_be32(maccfg2_register);
	value &= ~MACCFG2_PREL_MASK;
	value |= (preamble_length << MACCFG2_PREL_SHIFT);
	out_be32(maccfg2_register, value);
	return 0;
}

static int init_mii_management_configuration(int reset_mgmt,
					     int preamble_supress,
					     volatile u32 *miimcfg_register,
					     volatile u32 *miimind_register)
{
	unsigned int timeout = PHY_INIT_TIMEOUT;
	u32 value = 0;

	value = in_be32(miimcfg_register);
	if (reset_mgmt) {
		value |= MIIMCFG_RESET_MANAGEMENT;
		out_be32(miimcfg_register, value);
	}

	value = 0;

	if (preamble_supress)
		value |= MIIMCFG_NO_PREAMBLE;

	value |= UCC_GETH_MIIMCFG_MNGMNT_CLC_DIV_INIT;
	out_be32(miimcfg_register, value);

	/* Wait until the bus is free */
	while ((in_be32(miimind_register) & MIIMIND_BUSY) && timeout--)
		cpu_relax();

	if (timeout <= 0) {
		ugeth_err("%s: The MII Bus is stuck!", __FUNCTION__);
		return -ETIMEDOUT;
	}

	return 0;
}

static int init_rx_parameters(int reject_broadcast,
			      int receive_short_frames,
			      int promiscuous, volatile u32 *upsmr_register)
{
	u32 value = 0;

	value = in_be32(upsmr_register);

	if (reject_broadcast)
		value |= UPSMR_BRO;
	else
		value &= ~UPSMR_BRO;

	if (receive_short_frames)
		value |= UPSMR_RSH;
	else
		value &= ~UPSMR_RSH;

	if (promiscuous)
		value |= UPSMR_PRO;
	else
		value &= ~UPSMR_PRO;

	out_be32(upsmr_register, value);

	return 0;
}

static int init_max_rx_buff_len(u16 max_rx_buf_len,
				volatile u16 *mrblr_register)
{
	/* max_rx_buf_len value must be a multiple of 128 */
	if ((max_rx_buf_len == 0)
	    || (max_rx_buf_len % UCC_GETH_MRBLR_ALIGNMENT))
		return -EINVAL;

	out_be16(mrblr_register, max_rx_buf_len);
	return 0;
}

static int init_min_frame_len(u16 min_frame_length,
			      volatile u16 *minflr_register,
			      volatile u16 *mrblr_register)
{
	u16 mrblr_value = 0;

	mrblr_value = in_be16(mrblr_register);
	if (min_frame_length >= (mrblr_value - 4))
		return -EINVAL;

	out_be16(minflr_register, min_frame_length);
	return 0;
}

static int adjust_enet_interface(ucc_geth_private_t *ugeth)
{
	ucc_geth_info_t *ug_info;
	ucc_geth_t *ug_regs;
	ucc_fast_t *uf_regs;
	enet_speed_e speed;
	int ret_val, rpm = 0, tbi = 0, r10m = 0, rmm =
	    0, limited_to_full_duplex = 0;
	u32 upsmr, maccfg2, utbipar, tbiBaseAddress;
	u16 value;

	ugeth_vdbg("%s: IN", __FUNCTION__);

	ug_info = ugeth->ug_info;
	ug_regs = ugeth->ug_regs;
	uf_regs = ugeth->uccf->uf_regs;

	/* Analyze enet_interface according to Interface Mode Configuration
	table */
	ret_val =
	    get_interface_details(ug_info->enet_interface, &speed, &r10m, &rmm,
				  &rpm, &tbi, &limited_to_full_duplex);
	if (ret_val != 0) {
		ugeth_err
		  ("%s: half duplex not supported in requested configuration.",
		     __FUNCTION__);
		return ret_val;
	}

	/*                    Set MACCFG2                    */
	maccfg2 = in_be32(&ug_regs->maccfg2);
	maccfg2 &= ~MACCFG2_INTERFACE_MODE_MASK;
	if ((speed == ENET_SPEED_10BT) || (speed == ENET_SPEED_100BT))
		maccfg2 |= MACCFG2_INTERFACE_MODE_NIBBLE;
	else if (speed == ENET_SPEED_1000BT)
		maccfg2 |= MACCFG2_INTERFACE_MODE_BYTE;
	maccfg2 |= ug_info->padAndCrc;
	out_be32(&ug_regs->maccfg2, maccfg2);

	/*                    Set UPSMR                      */
	upsmr = in_be32(&uf_regs->upsmr);
	upsmr &= ~(UPSMR_RPM | UPSMR_R10M | UPSMR_TBIM | UPSMR_RMM);
	if (rpm)
		upsmr |= UPSMR_RPM;
	if (r10m)
		upsmr |= UPSMR_R10M;
	if (tbi)
		upsmr |= UPSMR_TBIM;
	if (rmm)
		upsmr |= UPSMR_RMM;
	out_be32(&uf_regs->upsmr, upsmr);

	/*                    Set UTBIPAR                    */
	utbipar = in_be32(&ug_regs->utbipar);
	utbipar &= ~UTBIPAR_PHY_ADDRESS_MASK;
	if (tbi)
		utbipar |=
		    (ug_info->phy_address +
		     ugeth->ug_info->uf_info.
		     ucc_num) << UTBIPAR_PHY_ADDRESS_SHIFT;
	else
		utbipar |=
		    (0x10 +
		     ugeth->ug_info->uf_info.
		     ucc_num) << UTBIPAR_PHY_ADDRESS_SHIFT;
	out_be32(&ug_regs->utbipar, utbipar);

	/* Disable autonegotiation in tbi mode, because by default it
	comes up in autonegotiation mode. */
	/* Note that this depends on proper setting in utbipar register. */
	if (tbi) {
		tbiBaseAddress = in_be32(&ug_regs->utbipar);
		tbiBaseAddress &= UTBIPAR_PHY_ADDRESS_MASK;
		tbiBaseAddress >>= UTBIPAR_PHY_ADDRESS_SHIFT;
		value =
		    ugeth->mii_info->mdio_read(ugeth->dev, (u8) tbiBaseAddress,
					       ENET_TBI_MII_CR);
		value &= ~0x1000;	/* Turn off autonegotiation */
		ugeth->mii_info->mdio_write(ugeth->dev, (u8) tbiBaseAddress,
					    ENET_TBI_MII_CR, value);
	}

	ret_val = init_mac_duplex_mode(1,
				       limited_to_full_duplex,
				       &ug_regs->maccfg2);
	if (ret_val != 0) {
		ugeth_err
		("%s: half duplex not supported in requested configuration.",
		     __FUNCTION__);
		return ret_val;
	}

	init_check_frame_length_mode(ug_info->lengthCheckRx, &ug_regs->maccfg2);

	ret_val = init_preamble_length(ug_info->prel, &ug_regs->maccfg2);
	if (ret_val != 0) {
		ugeth_err
		    ("%s: Preamble length must be between 3 and 7 inclusive.",
		     __FUNCTION__);
		return ret_val;
	}

	return 0;
}

/* Called every time the controller might need to be made
 * aware of new link state.  The PHY code conveys this
 * information through variables in the ugeth structure, and this
 * function converts those variables into the appropriate
 * register values, and can bring down the device if needed.
 */
static void adjust_link(struct net_device *dev)
{
	ucc_geth_private_t *ugeth = netdev_priv(dev);
	ucc_geth_t *ug_regs;
	u32 tempval;
	struct ugeth_mii_info *mii_info = ugeth->mii_info;

	ug_regs = ugeth->ug_regs;

	if (mii_info->link) {
		/* Now we make sure that we can be in full duplex mode.
		 * If not, we operate in half-duplex mode. */
		if (mii_info->duplex != ugeth->oldduplex) {
			if (!(mii_info->duplex)) {
				tempval = in_be32(&ug_regs->maccfg2);
				tempval &= ~(MACCFG2_FDX);
				out_be32(&ug_regs->maccfg2, tempval);

				ugeth_info("%s: Half Duplex", dev->name);
			} else {
				tempval = in_be32(&ug_regs->maccfg2);
				tempval |= MACCFG2_FDX;
				out_be32(&ug_regs->maccfg2, tempval);

				ugeth_info("%s: Full Duplex", dev->name);
			}

			ugeth->oldduplex = mii_info->duplex;
		}

		if (mii_info->speed != ugeth->oldspeed) {
			switch (mii_info->speed) {
			case 1000:
#ifdef CONFIG_MPC836x
/* FIXME: This code is for 100Mbs BUG fixing,
remove this when it is fixed!!! */
				if (ugeth->ug_info->enet_interface ==
				    ENET_1000_GMII)
				/* Run the commands which initialize the PHY */
				{
					tempval =
					    (u32) mii_info->mdio_read(ugeth->
						dev, mii_info->mii_id, 0x1b);
					tempval |= 0x000f;
					mii_info->mdio_write(ugeth->dev,
						mii_info->mii_id, 0x1b,
						(u16) tempval);
					tempval =
					    (u32) mii_info->mdio_read(ugeth->
						dev, mii_info->mii_id,
						MII_BMCR);
					mii_info->mdio_write(ugeth->dev,
						mii_info->mii_id, MII_BMCR,
						(u16) (tempval | BMCR_RESET));
				} else if (ugeth->ug_info->enet_interface ==
					   ENET_1000_RGMII)
				/* Run the commands which initialize the PHY */
				{
					tempval =
					    (u32) mii_info->mdio_read(ugeth->
						dev, mii_info->mii_id, 0x1b);
					tempval = (tempval & ~0x000f) | 0x000b;
					mii_info->mdio_write(ugeth->dev,
						mii_info->mii_id, 0x1b,
						(u16) tempval);
					tempval =
					    (u32) mii_info->mdio_read(ugeth->
						dev, mii_info->mii_id,
						MII_BMCR);
					mii_info->mdio_write(ugeth->dev,
						mii_info->mii_id, MII_BMCR,
						(u16) (tempval | BMCR_RESET));
				}
				msleep(4000);
#endif				/* CONFIG_MPC8360 */
				adjust_enet_interface(ugeth);
				break;
			case 100:
			case 10:
#ifdef CONFIG_MPC836x
/* FIXME: This code is for 100Mbs BUG fixing,
remove this lines when it will be fixed!!! */
				ugeth->ug_info->enet_interface = ENET_100_RGMII;
				tempval =
				    (u32) mii_info->mdio_read(ugeth->dev,
							      mii_info->mii_id,
							      0x1b);
				tempval = (tempval & ~0x000f) | 0x000b;
				mii_info->mdio_write(ugeth->dev,
						     mii_info->mii_id, 0x1b,
						     (u16) tempval);
				tempval =
				    (u32) mii_info->mdio_read(ugeth->dev,
							      mii_info->mii_id,
							      MII_BMCR);
				mii_info->mdio_write(ugeth->dev,
						     mii_info->mii_id, MII_BMCR,
						     (u16) (tempval |
							    BMCR_RESET));
				msleep(4000);
#endif				/* CONFIG_MPC8360 */
				adjust_enet_interface(ugeth);
				break;
			default:
				ugeth_warn
				    ("%s: Ack!  Speed (%d) is not 10/100/1000!",
				     dev->name, mii_info->speed);
				break;
			}

			ugeth_info("%s: Speed %dBT", dev->name,
				   mii_info->speed);

			ugeth->oldspeed = mii_info->speed;
		}

		if (!ugeth->oldlink) {
			ugeth_info("%s: Link is up", dev->name);
			ugeth->oldlink = 1;
			netif_carrier_on(dev);
			netif_schedule(dev);
		}
	} else {
		if (ugeth->oldlink) {
			ugeth_info("%s: Link is down", dev->name);
			ugeth->oldlink = 0;
			ugeth->oldspeed = 0;
			ugeth->oldduplex = -1;
			netif_carrier_off(dev);
		}
	}
}

/* Configure the PHY for dev.
 * returns 0 if success.  -1 if failure
 */
static int init_phy(struct net_device *dev)
{
	ucc_geth_private_t *ugeth = netdev_priv(dev);
	struct phy_info *curphy;
	ucc_mii_mng_t *mii_regs;
	struct ugeth_mii_info *mii_info;
	int err;

	mii_regs = &ugeth->ug_regs->miimng;

	ugeth->oldlink = 0;
	ugeth->oldspeed = 0;
	ugeth->oldduplex = -1;

	mii_info = kmalloc(sizeof(struct ugeth_mii_info), GFP_KERNEL);

	if (NULL == mii_info) {
		ugeth_err("%s: Could not allocate mii_info", dev->name);
		return -ENOMEM;
	}

	mii_info->mii_regs = mii_regs;
	mii_info->speed = SPEED_1000;
	mii_info->duplex = DUPLEX_FULL;
	mii_info->pause = 0;
	mii_info->link = 0;

	mii_info->advertising = (ADVERTISED_10baseT_Half |
				 ADVERTISED_10baseT_Full |
				 ADVERTISED_100baseT_Half |
				 ADVERTISED_100baseT_Full |
				 ADVERTISED_1000baseT_Full);
	mii_info->autoneg = 1;

	mii_info->mii_id = ugeth->ug_info->phy_address;

	mii_info->dev = dev;

	mii_info->mdio_read = &read_phy_reg;
	mii_info->mdio_write = &write_phy_reg;

	ugeth->mii_info = mii_info;

	spin_lock_irq(&ugeth->lock);

	/* Set this UCC to be the master of the MII managment */
	ucc_set_qe_mux_mii_mng(ugeth->ug_info->uf_info.ucc_num);

	if (init_mii_management_configuration(1,
					      ugeth->ug_info->
					      miiPreambleSupress,
					      &mii_regs->miimcfg,
					      &mii_regs->miimind)) {
		ugeth_err("%s: The MII Bus is stuck!", dev->name);
		err = -1;
		goto bus_fail;
	}

	spin_unlock_irq(&ugeth->lock);

	/* get info for this PHY */
	curphy = get_phy_info(ugeth->mii_info);

	if (curphy == NULL) {
		ugeth_err("%s: No PHY found", dev->name);
		err = -1;
		goto no_phy;
	}

	mii_info->phyinfo = curphy;

	/* Run the commands which initialize the PHY */
	if (curphy->init) {
		err = curphy->init(ugeth->mii_info);
		if (err)
			goto phy_init_fail;
	}

	return 0;

      phy_init_fail:
      no_phy:
      bus_fail:
	kfree(mii_info);

	return err;
}

#ifdef CONFIG_UGETH_TX_ON_DEMOND
static int ugeth_transmit_on_demand(ucc_geth_private_t *ugeth)
{
	ucc_fast_transmit_on_demand(ugeth->uccf);

	return 0;
}
#endif

static int ugeth_graceful_stop_tx(ucc_geth_private_t *ugeth)
{
	ucc_fast_private_t *uccf;
	u32 cecr_subblock;
	u32 temp;

	uccf = ugeth->uccf;

	/* Mask GRACEFUL STOP TX interrupt bit and clear it */
	temp = in_be32(uccf->p_uccm);
	temp &= ~UCCE_GRA;
	out_be32(uccf->p_uccm, temp);
	out_be32(uccf->p_ucce, UCCE_GRA);	/* clear by writing 1 */

	/* Issue host command */
	cecr_subblock =
	    ucc_fast_get_qe_cr_subblock(ugeth->ug_info->uf_info.ucc_num);
	qe_issue_cmd(QE_GRACEFUL_STOP_TX, cecr_subblock,
		     (u8) QE_CR_PROTOCOL_ETHERNET, 0);

	/* Wait for command to complete */
	do {
		temp = in_be32(uccf->p_ucce);
	} while (!(temp & UCCE_GRA));

	uccf->stopped_tx = 1;

	return 0;
}

static int ugeth_graceful_stop_rx(ucc_geth_private_t * ugeth)
{
	ucc_fast_private_t *uccf;
	u32 cecr_subblock;
	u8 temp;

	uccf = ugeth->uccf;

	/* Clear acknowledge bit */
	temp = ugeth->p_rx_glbl_pram->rxgstpack;
	temp &= ~GRACEFUL_STOP_ACKNOWLEDGE_RX;
	ugeth->p_rx_glbl_pram->rxgstpack = temp;

	/* Keep issuing command and checking acknowledge bit until
	it is asserted, according to spec */
	do {
		/* Issue host command */
		cecr_subblock =
		    ucc_fast_get_qe_cr_subblock(ugeth->ug_info->uf_info.
						ucc_num);
		qe_issue_cmd(QE_GRACEFUL_STOP_RX, cecr_subblock,
			     (u8) QE_CR_PROTOCOL_ETHERNET, 0);

		temp = ugeth->p_rx_glbl_pram->rxgstpack;
	} while (!(temp & GRACEFUL_STOP_ACKNOWLEDGE_RX));

	uccf->stopped_rx = 1;

	return 0;
}

static int ugeth_restart_tx(ucc_geth_private_t *ugeth)
{
	ucc_fast_private_t *uccf;
	u32 cecr_subblock;

	uccf = ugeth->uccf;

	cecr_subblock =
	    ucc_fast_get_qe_cr_subblock(ugeth->ug_info->uf_info.ucc_num);
	qe_issue_cmd(QE_RESTART_TX, cecr_subblock, (u8) QE_CR_PROTOCOL_ETHERNET,
		     0);
	uccf->stopped_tx = 0;

	return 0;
}

static int ugeth_restart_rx(ucc_geth_private_t *ugeth)
{
	ucc_fast_private_t *uccf;
	u32 cecr_subblock;

	uccf = ugeth->uccf;

	cecr_subblock =
	    ucc_fast_get_qe_cr_subblock(ugeth->ug_info->uf_info.ucc_num);
	qe_issue_cmd(QE_RESTART_RX, cecr_subblock, (u8) QE_CR_PROTOCOL_ETHERNET,
		     0);
	uccf->stopped_rx = 0;

	return 0;
}

static int ugeth_enable(ucc_geth_private_t *ugeth, comm_dir_e mode)
{
	ucc_fast_private_t *uccf;
	int enabled_tx, enabled_rx;

	uccf = ugeth->uccf;

	/* check if the UCC number is in range. */
	if (ugeth->ug_info->uf_info.ucc_num >= UCC_MAX_NUM) {
		ugeth_err("%s: ucc_num out of range.", __FUNCTION__);
		return -EINVAL;
	}

	enabled_tx = uccf->enabled_tx;
	enabled_rx = uccf->enabled_rx;

	/* Get Tx and Rx going again, in case this channel was actively
	disabled. */
	if ((mode & COMM_DIR_TX) && (!enabled_tx) && uccf->stopped_tx)
		ugeth_restart_tx(ugeth);
	if ((mode & COMM_DIR_RX) && (!enabled_rx) && uccf->stopped_rx)
		ugeth_restart_rx(ugeth);

	ucc_fast_enable(uccf, mode);	/* OK to do even if not disabled */

	return 0;

}

static int ugeth_disable(ucc_geth_private_t * ugeth, comm_dir_e mode)
{
	ucc_fast_private_t *uccf;

	uccf = ugeth->uccf;

	/* check if the UCC number is in range. */
	if (ugeth->ug_info->uf_info.ucc_num >= UCC_MAX_NUM) {
		ugeth_err("%s: ucc_num out of range.", __FUNCTION__);
		return -EINVAL;
	}

	/* Stop any transmissions */
	if ((mode & COMM_DIR_TX) && uccf->enabled_tx && !uccf->stopped_tx)
		ugeth_graceful_stop_tx(ugeth);

	/* Stop any receptions */
	if ((mode & COMM_DIR_RX) && uccf->enabled_rx && !uccf->stopped_rx)
		ugeth_graceful_stop_rx(ugeth);

	ucc_fast_disable(ugeth->uccf, mode); /* OK to do even if not enabled */

	return 0;
}

static void ugeth_dump_regs(ucc_geth_private_t *ugeth)
{
#ifdef DEBUG
	ucc_fast_dump_regs(ugeth->uccf);
	dump_regs(ugeth);
	dump_bds(ugeth);
#endif
}

#ifdef CONFIG_UGETH_FILTERING
static int ugeth_ext_filtering_serialize_tad(ucc_geth_tad_params_t *
					     p_UccGethTadParams,
					     qe_fltr_tad_t *qe_fltr_tad)
{
	u16 temp;

	/* Zero serialized TAD */
	memset(qe_fltr_tad, 0, QE_FLTR_TAD_SIZE);

	qe_fltr_tad->serialized[0] |= UCC_GETH_TAD_V;	/* Must have this */
	if (p_UccGethTadParams->rx_non_dynamic_extended_features_mode ||
	    (p_UccGethTadParams->vtag_op != UCC_GETH_VLAN_OPERATION_TAGGED_NOP)
	    || (p_UccGethTadParams->vnontag_op !=
		UCC_GETH_VLAN_OPERATION_NON_TAGGED_NOP)
	    )
		qe_fltr_tad->serialized[0] |= UCC_GETH_TAD_EF;
	if (p_UccGethTadParams->reject_frame)
		qe_fltr_tad->serialized[0] |= UCC_GETH_TAD_REJ;
	temp =
	    (u16) (((u16) p_UccGethTadParams->
		    vtag_op) << UCC_GETH_TAD_VTAG_OP_SHIFT);
	qe_fltr_tad->serialized[0] |= (u8) (temp >> 8);	/* upper bits */

	qe_fltr_tad->serialized[1] |= (u8) (temp & 0x00ff);	/* lower bits */
	if (p_UccGethTadParams->vnontag_op ==
	    UCC_GETH_VLAN_OPERATION_NON_TAGGED_Q_TAG_INSERT)
		qe_fltr_tad->serialized[1] |= UCC_GETH_TAD_V_NON_VTAG_OP;
	qe_fltr_tad->serialized[1] |=
	    p_UccGethTadParams->rqos << UCC_GETH_TAD_RQOS_SHIFT;

	qe_fltr_tad->serialized[2] |=
	    p_UccGethTadParams->vpri << UCC_GETH_TAD_V_PRIORITY_SHIFT;
	/* upper bits */
	qe_fltr_tad->serialized[2] |= (u8) (p_UccGethTadParams->vid >> 8);
	/* lower bits */
	qe_fltr_tad->serialized[3] |= (u8) (p_UccGethTadParams->vid & 0x00ff);

	return 0;
}

static enet_addr_container_t
    *ugeth_82xx_filtering_get_match_addr_in_hash(ucc_geth_private_t *ugeth,
						 enet_addr_t *p_enet_addr)
{
	enet_addr_container_t *enet_addr_cont;
	struct list_head *p_lh;
	u16 i, num;
	int32_t j;
	u8 *p_counter;

	if ((*p_enet_addr)[0] & ENET_GROUP_ADDR) {
		p_lh = &ugeth->group_hash_q;
		p_counter = &(ugeth->numGroupAddrInHash);
	} else {
		p_lh = &ugeth->ind_hash_q;
		p_counter = &(ugeth->numIndAddrInHash);
	}

	if (!p_lh)
		return NULL;

	num = *p_counter;

	for (i = 0; i < num; i++) {
		enet_addr_cont =
		    (enet_addr_container_t *)
		    ENET_ADDR_CONT_ENTRY(dequeue(p_lh));
		for (j = ENET_NUM_OCTETS_PER_ADDRESS - 1; j >= 0; j--) {
			if ((*p_enet_addr)[j] != (enet_addr_cont->address)[j])
				break;
			if (j == 0)
				return enet_addr_cont;	/* Found */
		}
		enqueue(p_lh, &enet_addr_cont->node);	/* Put it back */
	}
	return NULL;
}

static int ugeth_82xx_filtering_add_addr_in_hash(ucc_geth_private_t *ugeth,
						 enet_addr_t *p_enet_addr)
{
	ucc_geth_enet_address_recognition_location_e location;
	enet_addr_container_t *enet_addr_cont;
	struct list_head *p_lh;
	u8 i;
	u32 limit;
	u8 *p_counter;

	if ((*p_enet_addr)[0] & ENET_GROUP_ADDR) {
		p_lh = &ugeth->group_hash_q;
		limit = ugeth->ug_info->maxGroupAddrInHash;
		location =
		    UCC_GETH_ENET_ADDRESS_RECOGNITION_LOCATION_GROUP_HASH;
		p_counter = &(ugeth->numGroupAddrInHash);
	} else {
		p_lh = &ugeth->ind_hash_q;
		limit = ugeth->ug_info->maxIndAddrInHash;
		location =
		    UCC_GETH_ENET_ADDRESS_RECOGNITION_LOCATION_INDIVIDUAL_HASH;
		p_counter = &(ugeth->numIndAddrInHash);
	}

	if ((enet_addr_cont =
	     ugeth_82xx_filtering_get_match_addr_in_hash(ugeth, p_enet_addr))) {
		list_add(p_lh, &enet_addr_cont->node);	/* Put it back */
		return 0;
	}
	if ((!p_lh) || (!(*p_counter < limit)))
		return -EBUSY;
	if (!(enet_addr_cont = get_enet_addr_container()))
		return -ENOMEM;
	for (i = 0; i < ENET_NUM_OCTETS_PER_ADDRESS; i++)
		(enet_addr_cont->address)[i] = (*p_enet_addr)[i];
	enet_addr_cont->location = location;
	enqueue(p_lh, &enet_addr_cont->node);	/* Put it back */
	++(*p_counter);

	hw_add_addr_in_hash(ugeth, &(enet_addr_cont->address));

	return 0;
}

static int ugeth_82xx_filtering_clear_addr_in_hash(ucc_geth_private_t *ugeth,
						   enet_addr_t *p_enet_addr)
{
	ucc_geth_82xx_address_filtering_pram_t *p_82xx_addr_filt;
	enet_addr_container_t *enet_addr_cont;
	ucc_fast_private_t *uccf;
	comm_dir_e comm_dir;
	u16 i, num;
	struct list_head *p_lh;
	u32 *addr_h, *addr_l;
	u8 *p_counter;

	uccf = ugeth->uccf;

	p_82xx_addr_filt =
	    (ucc_geth_82xx_address_filtering_pram_t *) ugeth->p_rx_glbl_pram->
	    addressfiltering;

	if (!
	    (enet_addr_cont =
	     ugeth_82xx_filtering_get_match_addr_in_hash(ugeth, p_enet_addr)))
		return -ENOENT;

	/* It's been found and removed from the CQ. */
	/* Now destroy its container */
	put_enet_addr_container(enet_addr_cont);

	if ((*p_enet_addr)[0] & ENET_GROUP_ADDR) {
		addr_h = &(p_82xx_addr_filt->gaddr_h);
		addr_l = &(p_82xx_addr_filt->gaddr_l);
		p_lh = &ugeth->group_hash_q;
		p_counter = &(ugeth->numGroupAddrInHash);
	} else {
		addr_h = &(p_82xx_addr_filt->iaddr_h);
		addr_l = &(p_82xx_addr_filt->iaddr_l);
		p_lh = &ugeth->ind_hash_q;
		p_counter = &(ugeth->numIndAddrInHash);
	}

	comm_dir = 0;
	if (uccf->enabled_tx)
		comm_dir |= COMM_DIR_TX;
	if (uccf->enabled_rx)
		comm_dir |= COMM_DIR_RX;
	if (comm_dir)
		ugeth_disable(ugeth, comm_dir);

	/* Clear the hash table. */
	out_be32(addr_h, 0x00000000);
	out_be32(addr_l, 0x00000000);

	/* Add all remaining CQ elements back into hash */
	num = --(*p_counter);
	for (i = 0; i < num; i++) {
		enet_addr_cont =
		    (enet_addr_container_t *)
		    ENET_ADDR_CONT_ENTRY(dequeue(p_lh));
		hw_add_addr_in_hash(ugeth, &(enet_addr_cont->address));
		enqueue(p_lh, &enet_addr_cont->node);	/* Put it back */
	}

	if (comm_dir)
		ugeth_enable(ugeth, comm_dir);

	return 0;
}
#endif /* CONFIG_UGETH_FILTERING */

static int ugeth_82xx_filtering_clear_all_addr_in_hash(ucc_geth_private_t *
						       ugeth,
						       enet_addr_type_e
						       enet_addr_type)
{
	ucc_geth_82xx_address_filtering_pram_t *p_82xx_addr_filt;
	ucc_fast_private_t *uccf;
	comm_dir_e comm_dir;
	struct list_head *p_lh;
	u16 i, num;
	u32 *addr_h, *addr_l;
	u8 *p_counter;

	uccf = ugeth->uccf;

	p_82xx_addr_filt =
	    (ucc_geth_82xx_address_filtering_pram_t *) ugeth->p_rx_glbl_pram->
	    addressfiltering;

	if (enet_addr_type == ENET_ADDR_TYPE_GROUP) {
		addr_h = &(p_82xx_addr_filt->gaddr_h);
		addr_l = &(p_82xx_addr_filt->gaddr_l);
		p_lh = &ugeth->group_hash_q;
		p_counter = &(ugeth->numGroupAddrInHash);
	} else if (enet_addr_type == ENET_ADDR_TYPE_INDIVIDUAL) {
		addr_h = &(p_82xx_addr_filt->iaddr_h);
		addr_l = &(p_82xx_addr_filt->iaddr_l);
		p_lh = &ugeth->ind_hash_q;
		p_counter = &(ugeth->numIndAddrInHash);
	} else
		return -EINVAL;

	comm_dir = 0;
	if (uccf->enabled_tx)
		comm_dir |= COMM_DIR_TX;
	if (uccf->enabled_rx)
		comm_dir |= COMM_DIR_RX;
	if (comm_dir)
		ugeth_disable(ugeth, comm_dir);

	/* Clear the hash table. */
	out_be32(addr_h, 0x00000000);
	out_be32(addr_l, 0x00000000);

	if (!p_lh)
		return 0;

	num = *p_counter;

	/* Delete all remaining CQ elements */
	for (i = 0; i < num; i++)
		put_enet_addr_container(ENET_ADDR_CONT_ENTRY(dequeue(p_lh)));

	*p_counter = 0;

	if (comm_dir)
		ugeth_enable(ugeth, comm_dir);

	return 0;
}

#ifdef CONFIG_UGETH_FILTERING
static int ugeth_82xx_filtering_add_addr_in_paddr(ucc_geth_private_t *ugeth,
						  enet_addr_t *p_enet_addr,
						  u8 paddr_num)
{
	int i;

	if ((*p_enet_addr)[0] & ENET_GROUP_ADDR)
		ugeth_warn
		    ("%s: multicast address added to paddr will have no "
		     "effect - is this what you wanted?",
		     __FUNCTION__);

	ugeth->indAddrRegUsed[paddr_num] = 1;	/* mark this paddr as used */
	/* store address in our database */
	for (i = 0; i < ENET_NUM_OCTETS_PER_ADDRESS; i++)
		ugeth->paddr[paddr_num][i] = (*p_enet_addr)[i];
	/* put in hardware */
	return hw_add_addr_in_paddr(ugeth, p_enet_addr, paddr_num);
}
#endif /* CONFIG_UGETH_FILTERING */

static int ugeth_82xx_filtering_clear_addr_in_paddr(ucc_geth_private_t *ugeth,
						    u8 paddr_num)
{
	ugeth->indAddrRegUsed[paddr_num] = 0; /* mark this paddr as not used */
	return hw_clear_addr_in_paddr(ugeth, paddr_num);/* clear in hardware */
}

static void ucc_geth_memclean(ucc_geth_private_t *ugeth)
{
	u16 i, j;
	u8 *bd;

	if (!ugeth)
		return;

	if (ugeth->uccf)
		ucc_fast_free(ugeth->uccf);

	if (ugeth->p_thread_data_tx) {
		qe_muram_free(ugeth->thread_dat_tx_offset);
		ugeth->p_thread_data_tx = NULL;
	}
	if (ugeth->p_thread_data_rx) {
		qe_muram_free(ugeth->thread_dat_rx_offset);
		ugeth->p_thread_data_rx = NULL;
	}
	if (ugeth->p_exf_glbl_param) {
		qe_muram_free(ugeth->exf_glbl_param_offset);
		ugeth->p_exf_glbl_param = NULL;
	}
	if (ugeth->p_rx_glbl_pram) {
		qe_muram_free(ugeth->rx_glbl_pram_offset);
		ugeth->p_rx_glbl_pram = NULL;
	}
	if (ugeth->p_tx_glbl_pram) {
		qe_muram_free(ugeth->tx_glbl_pram_offset);
		ugeth->p_tx_glbl_pram = NULL;
	}
	if (ugeth->p_send_q_mem_reg) {
		qe_muram_free(ugeth->send_q_mem_reg_offset);
		ugeth->p_send_q_mem_reg = NULL;
	}
	if (ugeth->p_scheduler) {
		qe_muram_free(ugeth->scheduler_offset);
		ugeth->p_scheduler = NULL;
	}
	if (ugeth->p_tx_fw_statistics_pram) {
		qe_muram_free(ugeth->tx_fw_statistics_pram_offset);
		ugeth->p_tx_fw_statistics_pram = NULL;
	}
	if (ugeth->p_rx_fw_statistics_pram) {
		qe_muram_free(ugeth->rx_fw_statistics_pram_offset);
		ugeth->p_rx_fw_statistics_pram = NULL;
	}
	if (ugeth->p_rx_irq_coalescing_tbl) {
		qe_muram_free(ugeth->rx_irq_coalescing_tbl_offset);
		ugeth->p_rx_irq_coalescing_tbl = NULL;
	}
	if (ugeth->p_rx_bd_qs_tbl) {
		qe_muram_free(ugeth->rx_bd_qs_tbl_offset);
		ugeth->p_rx_bd_qs_tbl = NULL;
	}
	if (ugeth->p_init_enet_param_shadow) {
		return_init_enet_entries(ugeth,
					 &(ugeth->p_init_enet_param_shadow->
					   rxthread[0]),
					 ENET_INIT_PARAM_MAX_ENTRIES_RX,
					 ugeth->ug_info->riscRx, 1);
		return_init_enet_entries(ugeth,
					 &(ugeth->p_init_enet_param_shadow->
					   txthread[0]),
					 ENET_INIT_PARAM_MAX_ENTRIES_TX,
					 ugeth->ug_info->riscTx, 0);
		kfree(ugeth->p_init_enet_param_shadow);
		ugeth->p_init_enet_param_shadow = NULL;
	}
	for (i = 0; i < ugeth->ug_info->numQueuesTx; i++) {
		bd = ugeth->p_tx_bd_ring[i];
		for (j = 0; j < ugeth->ug_info->bdRingLenTx[i]; j++) {
			if (ugeth->tx_skbuff[i][j]) {
				dma_unmap_single(NULL,
						 BD_BUFFER_ARG(bd),
						 (BD_STATUS_AND_LENGTH(bd) &
						  BD_LENGTH_MASK),
						 DMA_TO_DEVICE);
				dev_kfree_skb_any(ugeth->tx_skbuff[i][j]);
				ugeth->tx_skbuff[i][j] = NULL;
			}
		}

		kfree(ugeth->tx_skbuff[i]);

		if (ugeth->p_tx_bd_ring[i]) {
			if (ugeth->ug_info->uf_info.bd_mem_part ==
			    MEM_PART_SYSTEM)
				kfree((void *)ugeth->tx_bd_ring_offset[i]);
			else if (ugeth->ug_info->uf_info.bd_mem_part ==
				 MEM_PART_MURAM)
				qe_muram_free(ugeth->tx_bd_ring_offset[i]);
			ugeth->p_tx_bd_ring[i] = NULL;
		}
	}
	for (i = 0; i < ugeth->ug_info->numQueuesRx; i++) {
		if (ugeth->p_rx_bd_ring[i]) {
			/* Return existing data buffers in ring */
			bd = ugeth->p_rx_bd_ring[i];
			for (j = 0; j < ugeth->ug_info->bdRingLenRx[i]; j++) {
				if (ugeth->rx_skbuff[i][j]) {
					dma_unmap_single(NULL, BD_BUFFER(bd),
						 ugeth->ug_info->
						 uf_info.
						 max_rx_buf_length +
						 UCC_GETH_RX_DATA_BUF_ALIGNMENT,
						 DMA_FROM_DEVICE);

					dev_kfree_skb_any(ugeth->
							  rx_skbuff[i][j]);
					ugeth->rx_skbuff[i][j] = NULL;
				}
				bd += UCC_GETH_SIZE_OF_BD;
			}

			kfree(ugeth->rx_skbuff[i]);

			if (ugeth->ug_info->uf_info.bd_mem_part ==
			    MEM_PART_SYSTEM)
				kfree((void *)ugeth->rx_bd_ring_offset[i]);
			else if (ugeth->ug_info->uf_info.bd_mem_part ==
				 MEM_PART_MURAM)
				qe_muram_free(ugeth->rx_bd_ring_offset[i]);
			ugeth->p_rx_bd_ring[i] = NULL;
		}
	}
	while (!list_empty(&ugeth->group_hash_q))
		put_enet_addr_container(ENET_ADDR_CONT_ENTRY
					(dequeue(&ugeth->group_hash_q)));
	while (!list_empty(&ugeth->ind_hash_q))
		put_enet_addr_container(ENET_ADDR_CONT_ENTRY
					(dequeue(&ugeth->ind_hash_q)));

}

static void ucc_geth_set_multi(struct net_device *dev)
{
	ucc_geth_private_t *ugeth;
	struct dev_mc_list *dmi;
	ucc_fast_t *uf_regs;
	ucc_geth_82xx_address_filtering_pram_t *p_82xx_addr_filt;
	enet_addr_t tempaddr;
	u8 *mcptr, *tdptr;
	int i, j;

	ugeth = netdev_priv(dev);

	uf_regs = ugeth->uccf->uf_regs;

	if (dev->flags & IFF_PROMISC) {

		uf_regs->upsmr |= UPSMR_PRO;

	} else {

		uf_regs->upsmr &= ~UPSMR_PRO;

		p_82xx_addr_filt =
		    (ucc_geth_82xx_address_filtering_pram_t *) ugeth->
		    p_rx_glbl_pram->addressfiltering;

		if (dev->flags & IFF_ALLMULTI) {
			/* Catch all multicast addresses, so set the
			 * filter to all 1's.
			 */
			out_be32(&p_82xx_addr_filt->gaddr_h, 0xffffffff);
			out_be32(&p_82xx_addr_filt->gaddr_l, 0xffffffff);
		} else {
			/* Clear filter and add the addresses in the list.
			 */
			out_be32(&p_82xx_addr_filt->gaddr_h, 0x0);
			out_be32(&p_82xx_addr_filt->gaddr_l, 0x0);

			dmi = dev->mc_list;

			for (i = 0; i < dev->mc_count; i++, dmi = dmi->next) {

				/* Only support group multicast for now.
				 */
				if (!(dmi->dmi_addr[0] & 1))
					continue;

				/* The address in dmi_addr is LSB first,
				 * and taddr is MSB first.  We have to
				 * copy bytes MSB first from dmi_addr.
				 */
				mcptr = (u8 *) dmi->dmi_addr + 5;
				tdptr = (u8 *) & tempaddr;
				for (j = 0; j < 6; j++)
					*tdptr++ = *mcptr--;

				/* Ask CPM to run CRC and set bit in
				 * filter mask.
				 */
				hw_add_addr_in_hash(ugeth, &tempaddr);

			}
		}
	}
}

static void ucc_geth_stop(ucc_geth_private_t *ugeth)
{
	ucc_geth_t *ug_regs = ugeth->ug_regs;
	u32 tempval;

	ugeth_vdbg("%s: IN", __FUNCTION__);

	/* Disable the controller */
	ugeth_disable(ugeth, COMM_DIR_RX_AND_TX);

	/* Tell the kernel the link is down */
	ugeth->mii_info->link = 0;
	adjust_link(ugeth->dev);

	/* Mask all interrupts */
	out_be32(ugeth->uccf->p_ucce, 0x00000000);

	/* Clear all interrupts */
	out_be32(ugeth->uccf->p_ucce, 0xffffffff);

	/* Disable Rx and Tx */
	tempval = in_be32(&ug_regs->maccfg1);
	tempval &= ~(MACCFG1_ENABLE_RX | MACCFG1_ENABLE_TX);
	out_be32(&ug_regs->maccfg1, tempval);

	if (ugeth->ug_info->board_flags & FSL_UGETH_BRD_HAS_PHY_INTR) {
		/* Clear any pending interrupts */
		mii_clear_phy_interrupt(ugeth->mii_info);

		/* Disable PHY Interrupts */
		mii_configure_phy_interrupt(ugeth->mii_info,
					    MII_INTERRUPT_DISABLED);
	}

	free_irq(ugeth->ug_info->uf_info.irq, ugeth->dev);

	if (ugeth->ug_info->board_flags & FSL_UGETH_BRD_HAS_PHY_INTR) {
		free_irq(ugeth->ug_info->phy_interrupt, ugeth->dev);
	} else {
		del_timer_sync(&ugeth->phy_info_timer);
	}

	ucc_geth_memclean(ugeth);
}

static int ucc_geth_startup(ucc_geth_private_t *ugeth)
{
	ucc_geth_82xx_address_filtering_pram_t *p_82xx_addr_filt;
	ucc_geth_init_pram_t *p_init_enet_pram;
	ucc_fast_private_t *uccf;
	ucc_geth_info_t *ug_info;
	ucc_fast_info_t *uf_info;
	ucc_fast_t *uf_regs;
	ucc_geth_t *ug_regs;
	int ret_val = -EINVAL;
	u32 remoder = UCC_GETH_REMODER_INIT;
	u32 init_enet_pram_offset, cecr_subblock, command, maccfg1;
	u32 ifstat, i, j, size, l2qt, l3qt, length;
	u16 temoder = UCC_GETH_TEMODER_INIT;
	u16 test;
	u8 function_code = 0;
	u8 *bd, *endOfRing;
	u8 numThreadsRxNumerical, numThreadsTxNumerical;

	ugeth_vdbg("%s: IN", __FUNCTION__);

	ug_info = ugeth->ug_info;
	uf_info = &ug_info->uf_info;

	if (!((uf_info->bd_mem_part == MEM_PART_SYSTEM) ||
	      (uf_info->bd_mem_part == MEM_PART_MURAM))) {
		ugeth_err("%s: Bad memory partition value.", __FUNCTION__);
		return -EINVAL;
	}

	/* Rx BD lengths */
	for (i = 0; i < ug_info->numQueuesRx; i++) {
		if ((ug_info->bdRingLenRx[i] < UCC_GETH_RX_BD_RING_SIZE_MIN) ||
		    (ug_info->bdRingLenRx[i] %
		     UCC_GETH_RX_BD_RING_SIZE_ALIGNMENT)) {
			ugeth_err
			    ("%s: Rx BD ring length must be multiple of 4,"
				" no smaller than 8.", __FUNCTION__);
			return -EINVAL;
		}
	}

	/* Tx BD lengths */
	for (i = 0; i < ug_info->numQueuesTx; i++) {
		if (ug_info->bdRingLenTx[i] < UCC_GETH_TX_BD_RING_SIZE_MIN) {
			ugeth_err
			    ("%s: Tx BD ring length must be no smaller than 2.",
			     __FUNCTION__);
			return -EINVAL;
		}
	}

	/* mrblr */
	if ((uf_info->max_rx_buf_length == 0) ||
	    (uf_info->max_rx_buf_length % UCC_GETH_MRBLR_ALIGNMENT)) {
		ugeth_err
		    ("%s: max_rx_buf_length must be non-zero multiple of 128.",
		     __FUNCTION__);
		return -EINVAL;
	}

	/* num Tx queues */
	if (ug_info->numQueuesTx > NUM_TX_QUEUES) {
		ugeth_err("%s: number of tx queues too large.", __FUNCTION__);
		return -EINVAL;
	}

	/* num Rx queues */
	if (ug_info->numQueuesRx > NUM_RX_QUEUES) {
		ugeth_err("%s: number of rx queues too large.", __FUNCTION__);
		return -EINVAL;
	}

	/* l2qt */
	for (i = 0; i < UCC_GETH_VLAN_PRIORITY_MAX; i++) {
		if (ug_info->l2qt[i] >= ug_info->numQueuesRx) {
			ugeth_err
			    ("%s: VLAN priority table entry must not be"
				" larger than number of Rx queues.",
			     __FUNCTION__);
			return -EINVAL;
		}
	}

	/* l3qt */
	for (i = 0; i < UCC_GETH_IP_PRIORITY_MAX; i++) {
		if (ug_info->l3qt[i] >= ug_info->numQueuesRx) {
			ugeth_err
			    ("%s: IP priority table entry must not be"
				" larger than number of Rx queues.",
			     __FUNCTION__);
			return -EINVAL;
		}
	}

	if (ug_info->cam && !ug_info->ecamptr) {
		ugeth_err("%s: If cam mode is chosen, must supply cam ptr.",
			  __FUNCTION__);
		return -EINVAL;
	}

	if ((ug_info->numStationAddresses !=
	     UCC_GETH_NUM_OF_STATION_ADDRESSES_1)
	    && ug_info->rxExtendedFiltering) {
		ugeth_err("%s: Number of station addresses greater than 1 "
			  "not allowed in extended parsing mode.",
			  __FUNCTION__);
		return -EINVAL;
	}

	/* Generate uccm_mask for receive */
	uf_info->uccm_mask = ug_info->eventRegMask & UCCE_OTHER;/* Errors */
	for (i = 0; i < ug_info->numQueuesRx; i++)
		uf_info->uccm_mask |= (UCCE_RXBF_SINGLE_MASK << i);

	for (i = 0; i < ug_info->numQueuesTx; i++)
		uf_info->uccm_mask |= (UCCE_TXBF_SINGLE_MASK << i);
	/* Initialize the general fast UCC block. */
	if (ucc_fast_init(uf_info, &uccf)) {
		ugeth_err("%s: Failed to init uccf.", __FUNCTION__);
		ucc_geth_memclean(ugeth);
		return -ENOMEM;
	}
	ugeth->uccf = uccf;

	switch (ug_info->numThreadsRx) {
	case UCC_GETH_NUM_OF_THREADS_1:
		numThreadsRxNumerical = 1;
		break;
	case UCC_GETH_NUM_OF_THREADS_2:
		numThreadsRxNumerical = 2;
		break;
	case UCC_GETH_NUM_OF_THREADS_4:
		numThreadsRxNumerical = 4;
		break;
	case UCC_GETH_NUM_OF_THREADS_6:
		numThreadsRxNumerical = 6;
		break;
	case UCC_GETH_NUM_OF_THREADS_8:
		numThreadsRxNumerical = 8;
		break;
	default:
		ugeth_err("%s: Bad number of Rx threads value.", __FUNCTION__);
		ucc_geth_memclean(ugeth);
		return -EINVAL;
		break;
	}

	switch (ug_info->numThreadsTx) {
	case UCC_GETH_NUM_OF_THREADS_1:
		numThreadsTxNumerical = 1;
		break;
	case UCC_GETH_NUM_OF_THREADS_2:
		numThreadsTxNumerical = 2;
		break;
	case UCC_GETH_NUM_OF_THREADS_4:
		numThreadsTxNumerical = 4;
		break;
	case UCC_GETH_NUM_OF_THREADS_6:
		numThreadsTxNumerical = 6;
		break;
	case UCC_GETH_NUM_OF_THREADS_8:
		numThreadsTxNumerical = 8;
		break;
	default:
		ugeth_err("%s: Bad number of Tx threads value.", __FUNCTION__);
		ucc_geth_memclean(ugeth);
		return -EINVAL;
		break;
	}

	/* Calculate rx_extended_features */
	ugeth->rx_non_dynamic_extended_features = ug_info->ipCheckSumCheck ||
	    ug_info->ipAddressAlignment ||
	    (ug_info->numStationAddresses !=
	     UCC_GETH_NUM_OF_STATION_ADDRESSES_1);

	ugeth->rx_extended_features = ugeth->rx_non_dynamic_extended_features ||
	    (ug_info->vlanOperationTagged != UCC_GETH_VLAN_OPERATION_TAGGED_NOP)
	    || (ug_info->vlanOperationNonTagged !=
		UCC_GETH_VLAN_OPERATION_NON_TAGGED_NOP);

	uf_regs = uccf->uf_regs;
	ug_regs = (ucc_geth_t *) (uccf->uf_regs);
	ugeth->ug_regs = ug_regs;

	init_default_reg_vals(&uf_regs->upsmr,
			      &ug_regs->maccfg1, &ug_regs->maccfg2);

	/*                    Set UPSMR                      */
	/* For more details see the hardware spec.           */
	init_rx_parameters(ug_info->bro,
			   ug_info->rsh, ug_info->pro, &uf_regs->upsmr);

	/* We're going to ignore other registers for now, */
	/* except as needed to get up and running         */

	/*                    Set MACCFG1                    */
	/* For more details see the hardware spec.           */
	init_flow_control_params(ug_info->aufc,
				 ug_info->receiveFlowControl,
				 1,
				 ug_info->pausePeriod,
				 ug_info->extensionField,
				 &uf_regs->upsmr,
				 &ug_regs->uempr, &ug_regs->maccfg1);

	maccfg1 = in_be32(&ug_regs->maccfg1);
	maccfg1 |= MACCFG1_ENABLE_RX;
	maccfg1 |= MACCFG1_ENABLE_TX;
	out_be32(&ug_regs->maccfg1, maccfg1);

	/*                    Set IPGIFG                     */
	/* For more details see the hardware spec.           */
	ret_val = init_inter_frame_gap_params(ug_info->nonBackToBackIfgPart1,
					      ug_info->nonBackToBackIfgPart2,
					      ug_info->
					      miminumInterFrameGapEnforcement,
					      ug_info->backToBackInterFrameGap,
					      &ug_regs->ipgifg);
	if (ret_val != 0) {
		ugeth_err("%s: IPGIFG initialization parameter too large.",
			  __FUNCTION__);
		ucc_geth_memclean(ugeth);
		return ret_val;
	}

	/*                    Set HAFDUP                     */
	/* For more details see the hardware spec.           */
	ret_val = init_half_duplex_params(ug_info->altBeb,
					  ug_info->backPressureNoBackoff,
					  ug_info->noBackoff,
					  ug_info->excessDefer,
					  ug_info->altBebTruncation,
					  ug_info->maxRetransmission,
					  ug_info->collisionWindow,
					  &ug_regs->hafdup);
	if (ret_val != 0) {
		ugeth_err("%s: Half Duplex initialization parameter too large.",
			  __FUNCTION__);
		ucc_geth_memclean(ugeth);
		return ret_val;
	}

	/*                    Set IFSTAT                     */
	/* For more details see the hardware spec.           */
	/* Read only - resets upon read                      */
	ifstat = in_be32(&ug_regs->ifstat);

	/*                    Clear UEMPR                    */
	/* For more details see the hardware spec.           */
	out_be32(&ug_regs->uempr, 0);

	/*                    Set UESCR                      */
	/* For more details see the hardware spec.           */
	init_hw_statistics_gathering_mode((ug_info->statisticsMode &
				UCC_GETH_STATISTICS_GATHERING_MODE_HARDWARE),
				0, &uf_regs->upsmr, &ug_regs->uescr);

	/* Allocate Tx bds */
	for (j = 0; j < ug_info->numQueuesTx; j++) {
		/* Allocate in multiple of
		   UCC_GETH_TX_BD_RING_SIZE_MEMORY_ALIGNMENT,
		   according to spec */
		length = ((ug_info->bdRingLenTx[j] * UCC_GETH_SIZE_OF_BD)
			  / UCC_GETH_TX_BD_RING_SIZE_MEMORY_ALIGNMENT)
		    * UCC_GETH_TX_BD_RING_SIZE_MEMORY_ALIGNMENT;
		if ((ug_info->bdRingLenTx[j] * UCC_GETH_SIZE_OF_BD) %
		    UCC_GETH_TX_BD_RING_SIZE_MEMORY_ALIGNMENT)
			length += UCC_GETH_TX_BD_RING_SIZE_MEMORY_ALIGNMENT;
		if (uf_info->bd_mem_part == MEM_PART_SYSTEM) {
			u32 align = 4;
			if (UCC_GETH_TX_BD_RING_ALIGNMENT > 4)
				align = UCC_GETH_TX_BD_RING_ALIGNMENT;
			ugeth->tx_bd_ring_offset[j] =
				(u32) (kmalloc((u32) (length + align),
				GFP_KERNEL));
			if (ugeth->tx_bd_ring_offset[j] != 0)
				ugeth->p_tx_bd_ring[j] =
					(void*)((ugeth->tx_bd_ring_offset[j] +
					align) & ~(align - 1));
		} else if (uf_info->bd_mem_part == MEM_PART_MURAM) {
			ugeth->tx_bd_ring_offset[j] =
			    qe_muram_alloc(length,
					   UCC_GETH_TX_BD_RING_ALIGNMENT);
			if (!IS_MURAM_ERR(ugeth->tx_bd_ring_offset[j]))
				ugeth->p_tx_bd_ring[j] =
				    (u8 *) qe_muram_addr(ugeth->
							 tx_bd_ring_offset[j]);
		}
		if (!ugeth->p_tx_bd_ring[j]) {
			ugeth_err
			    ("%s: Can not allocate memory for Tx bd rings.",
			     __FUNCTION__);
			ucc_geth_memclean(ugeth);
			return -ENOMEM;
		}
		/* Zero unused end of bd ring, according to spec */
		memset(ugeth->p_tx_bd_ring[j] +
		       ug_info->bdRingLenTx[j] * UCC_GETH_SIZE_OF_BD, 0,
		       length - ug_info->bdRingLenTx[j] * UCC_GETH_SIZE_OF_BD);
	}

	/* Allocate Rx bds */
	for (j = 0; j < ug_info->numQueuesRx; j++) {
		length = ug_info->bdRingLenRx[j] * UCC_GETH_SIZE_OF_BD;
		if (uf_info->bd_mem_part == MEM_PART_SYSTEM) {
			u32 align = 4;
			if (UCC_GETH_RX_BD_RING_ALIGNMENT > 4)
				align = UCC_GETH_RX_BD_RING_ALIGNMENT;
			ugeth->rx_bd_ring_offset[j] =
			    (u32) (kmalloc((u32) (length + align), GFP_KERNEL));
			if (ugeth->rx_bd_ring_offset[j] != 0)
				ugeth->p_rx_bd_ring[j] =
					(void*)((ugeth->rx_bd_ring_offset[j] +
					align) & ~(align - 1));
		} else if (uf_info->bd_mem_part == MEM_PART_MURAM) {
			ugeth->rx_bd_ring_offset[j] =
			    qe_muram_alloc(length,
					   UCC_GETH_RX_BD_RING_ALIGNMENT);
			if (!IS_MURAM_ERR(ugeth->rx_bd_ring_offset[j]))
				ugeth->p_rx_bd_ring[j] =
				    (u8 *) qe_muram_addr(ugeth->
							 rx_bd_ring_offset[j]);
		}
		if (!ugeth->p_rx_bd_ring[j]) {
			ugeth_err
			    ("%s: Can not allocate memory for Rx bd rings.",
			     __FUNCTION__);
			ucc_geth_memclean(ugeth);
			return -ENOMEM;
		}
	}

	/* Init Tx bds */
	for (j = 0; j < ug_info->numQueuesTx; j++) {
		/* Setup the skbuff rings */
		ugeth->tx_skbuff[j] =
		    (struct sk_buff **)kmalloc(sizeof(struct sk_buff *) *
					       ugeth->ug_info->bdRingLenTx[j],
					       GFP_KERNEL);

		if (ugeth->tx_skbuff[j] == NULL) {
			ugeth_err("%s: Could not allocate tx_skbuff",
				  __FUNCTION__);
			ucc_geth_memclean(ugeth);
			return -ENOMEM;
		}

		for (i = 0; i < ugeth->ug_info->bdRingLenTx[j]; i++)
			ugeth->tx_skbuff[j][i] = NULL;

		ugeth->skb_curtx[j] = ugeth->skb_dirtytx[j] = 0;
		bd = ugeth->confBd[j] = ugeth->txBd[j] = ugeth->p_tx_bd_ring[j];
		for (i = 0; i < ug_info->bdRingLenTx[j]; i++) {
			BD_BUFFER_CLEAR(bd);
			BD_STATUS_AND_LENGTH_SET(bd, 0);
			bd += UCC_GETH_SIZE_OF_BD;
		}
		bd -= UCC_GETH_SIZE_OF_BD;
		BD_STATUS_AND_LENGTH_SET(bd, T_W);/* for last BD set Wrap bit */
	}

	/* Init Rx bds */
	for (j = 0; j < ug_info->numQueuesRx; j++) {
		/* Setup the skbuff rings */
		ugeth->rx_skbuff[j] =
		    (struct sk_buff **)kmalloc(sizeof(struct sk_buff *) *
					       ugeth->ug_info->bdRingLenRx[j],
					       GFP_KERNEL);

		if (ugeth->rx_skbuff[j] == NULL) {
			ugeth_err("%s: Could not allocate rx_skbuff",
				  __FUNCTION__);
			ucc_geth_memclean(ugeth);
			return -ENOMEM;
		}

		for (i = 0; i < ugeth->ug_info->bdRingLenRx[j]; i++)
			ugeth->rx_skbuff[j][i] = NULL;

		ugeth->skb_currx[j] = 0;
		bd = ugeth->rxBd[j] = ugeth->p_rx_bd_ring[j];
		for (i = 0; i < ug_info->bdRingLenRx[j]; i++) {
			BD_STATUS_AND_LENGTH_SET(bd, R_I);
			BD_BUFFER_CLEAR(bd);
			bd += UCC_GETH_SIZE_OF_BD;
		}
		bd -= UCC_GETH_SIZE_OF_BD;
		BD_STATUS_AND_LENGTH_SET(bd, R_W);/* for last BD set Wrap bit */
	}

	/*
	 * Global PRAM
	 */
	/* Tx global PRAM */
	/* Allocate global tx parameter RAM page */
	ugeth->tx_glbl_pram_offset =
	    qe_muram_alloc(sizeof(ucc_geth_tx_global_pram_t),
			   UCC_GETH_TX_GLOBAL_PRAM_ALIGNMENT);
	if (IS_MURAM_ERR(ugeth->tx_glbl_pram_offset)) {
		ugeth_err
		    ("%s: Can not allocate DPRAM memory for p_tx_glbl_pram.",
		     __FUNCTION__);
		ucc_geth_memclean(ugeth);
		return -ENOMEM;
	}
	ugeth->p_tx_glbl_pram =
	    (ucc_geth_tx_global_pram_t *) qe_muram_addr(ugeth->
							tx_glbl_pram_offset);
	/* Zero out p_tx_glbl_pram */
	memset(ugeth->p_tx_glbl_pram, 0, sizeof(ucc_geth_tx_global_pram_t));

	/* Fill global PRAM */

	/* TQPTR */
	/* Size varies with number of Tx threads */
	ugeth->thread_dat_tx_offset =
	    qe_muram_alloc(numThreadsTxNumerical *
			   sizeof(ucc_geth_thread_data_tx_t) +
			   32 * (numThreadsTxNumerical == 1),
			   UCC_GETH_THREAD_DATA_ALIGNMENT);
	if (IS_MURAM_ERR(ugeth->thread_dat_tx_offset)) {
		ugeth_err
		    ("%s: Can not allocate DPRAM memory for p_thread_data_tx.",
		     __FUNCTION__);
		ucc_geth_memclean(ugeth);
		return -ENOMEM;
	}

	ugeth->p_thread_data_tx =
	    (ucc_geth_thread_data_tx_t *) qe_muram_addr(ugeth->
							thread_dat_tx_offset);
	out_be32(&ugeth->p_tx_glbl_pram->tqptr, ugeth->thread_dat_tx_offset);

	/* vtagtable */
	for (i = 0; i < UCC_GETH_TX_VTAG_TABLE_ENTRY_MAX; i++)
		out_be32(&ugeth->p_tx_glbl_pram->vtagtable[i],
			 ug_info->vtagtable[i]);

	/* iphoffset */
	for (i = 0; i < TX_IP_OFFSET_ENTRY_MAX; i++)
		ugeth->p_tx_glbl_pram->iphoffset[i] = ug_info->iphoffset[i];

	/* SQPTR */
	/* Size varies with number of Tx queues */
	ugeth->send_q_mem_reg_offset =
	    qe_muram_alloc(ug_info->numQueuesTx *
			   sizeof(ucc_geth_send_queue_qd_t),
			   UCC_GETH_SEND_QUEUE_QUEUE_DESCRIPTOR_ALIGNMENT);
	if (IS_MURAM_ERR(ugeth->send_q_mem_reg_offset)) {
		ugeth_err
		    ("%s: Can not allocate DPRAM memory for p_send_q_mem_reg.",
		     __FUNCTION__);
		ucc_geth_memclean(ugeth);
		return -ENOMEM;
	}

	ugeth->p_send_q_mem_reg =
	    (ucc_geth_send_queue_mem_region_t *) qe_muram_addr(ugeth->
			send_q_mem_reg_offset);
	out_be32(&ugeth->p_tx_glbl_pram->sqptr, ugeth->send_q_mem_reg_offset);

	/* Setup the table */
	/* Assume BD rings are already established */
	for (i = 0; i < ug_info->numQueuesTx; i++) {
		endOfRing =
		    ugeth->p_tx_bd_ring[i] + (ug_info->bdRingLenTx[i] -
					      1) * UCC_GETH_SIZE_OF_BD;
		if (ugeth->ug_info->uf_info.bd_mem_part == MEM_PART_SYSTEM) {
			out_be32(&ugeth->p_send_q_mem_reg->sqqd[i].bd_ring_base,
				 (u32) virt_to_phys(ugeth->p_tx_bd_ring[i]));
			out_be32(&ugeth->p_send_q_mem_reg->sqqd[i].
				 last_bd_completed_address,
				 (u32) virt_to_phys(endOfRing));
		} else if (ugeth->ug_info->uf_info.bd_mem_part ==
			   MEM_PART_MURAM) {
			out_be32(&ugeth->p_send_q_mem_reg->sqqd[i].bd_ring_base,
				 (u32) immrbar_virt_to_phys(ugeth->
							    p_tx_bd_ring[i]));
			out_be32(&ugeth->p_send_q_mem_reg->sqqd[i].
				 last_bd_completed_address,
				 (u32) immrbar_virt_to_phys(endOfRing));
		}
	}

	/* schedulerbasepointer */

	if (ug_info->numQueuesTx > 1) {
	/* scheduler exists only if more than 1 tx queue */
		ugeth->scheduler_offset =
		    qe_muram_alloc(sizeof(ucc_geth_scheduler_t),
				   UCC_GETH_SCHEDULER_ALIGNMENT);
		if (IS_MURAM_ERR(ugeth->scheduler_offset)) {
			ugeth_err
			 ("%s: Can not allocate DPRAM memory for p_scheduler.",
			     __FUNCTION__);
			ucc_geth_memclean(ugeth);
			return -ENOMEM;
		}

		ugeth->p_scheduler =
		    (ucc_geth_scheduler_t *) qe_muram_addr(ugeth->
							   scheduler_offset);
		out_be32(&ugeth->p_tx_glbl_pram->schedulerbasepointer,
			 ugeth->scheduler_offset);
		/* Zero out p_scheduler */
		memset(ugeth->p_scheduler, 0, sizeof(ucc_geth_scheduler_t));

		/* Set values in scheduler */
		out_be32(&ugeth->p_scheduler->mblinterval,
			 ug_info->mblinterval);
		out_be16(&ugeth->p_scheduler->nortsrbytetime,
			 ug_info->nortsrbytetime);
		ugeth->p_scheduler->fracsiz = ug_info->fracsiz;
		ugeth->p_scheduler->strictpriorityq = ug_info->strictpriorityq;
		ugeth->p_scheduler->txasap = ug_info->txasap;
		ugeth->p_scheduler->extrabw = ug_info->extrabw;
		for (i = 0; i < NUM_TX_QUEUES; i++)
			ugeth->p_scheduler->weightfactor[i] =
			    ug_info->weightfactor[i];

		/* Set pointers to cpucount registers in scheduler */
		ugeth->p_cpucount[0] = &(ugeth->p_scheduler->cpucount0);
		ugeth->p_cpucount[1] = &(ugeth->p_scheduler->cpucount1);
		ugeth->p_cpucount[2] = &(ugeth->p_scheduler->cpucount2);
		ugeth->p_cpucount[3] = &(ugeth->p_scheduler->cpucount3);
		ugeth->p_cpucount[4] = &(ugeth->p_scheduler->cpucount4);
		ugeth->p_cpucount[5] = &(ugeth->p_scheduler->cpucount5);
		ugeth->p_cpucount[6] = &(ugeth->p_scheduler->cpucount6);
		ugeth->p_cpucount[7] = &(ugeth->p_scheduler->cpucount7);
	}

	/* schedulerbasepointer */
	/* TxRMON_PTR (statistics) */
	if (ug_info->
	    statisticsMode & UCC_GETH_STATISTICS_GATHERING_MODE_FIRMWARE_TX) {
		ugeth->tx_fw_statistics_pram_offset =
		    qe_muram_alloc(sizeof
				   (ucc_geth_tx_firmware_statistics_pram_t),
				   UCC_GETH_TX_STATISTICS_ALIGNMENT);
		if (IS_MURAM_ERR(ugeth->tx_fw_statistics_pram_offset)) {
			ugeth_err
			    ("%s: Can not allocate DPRAM memory for"
				" p_tx_fw_statistics_pram.", __FUNCTION__);
			ucc_geth_memclean(ugeth);
			return -ENOMEM;
		}
		ugeth->p_tx_fw_statistics_pram =
		    (ucc_geth_tx_firmware_statistics_pram_t *)
		    qe_muram_addr(ugeth->tx_fw_statistics_pram_offset);
		/* Zero out p_tx_fw_statistics_pram */
		memset(ugeth->p_tx_fw_statistics_pram,
		       0, sizeof(ucc_geth_tx_firmware_statistics_pram_t));
	}

	/* temoder */
	/* Already has speed set */

	if (ug_info->numQueuesTx > 1)
		temoder |= TEMODER_SCHEDULER_ENABLE;
	if (ug_info->ipCheckSumGenerate)
		temoder |= TEMODER_IP_CHECKSUM_GENERATE;
	temoder |= ((ug_info->numQueuesTx - 1) << TEMODER_NUM_OF_QUEUES_SHIFT);
	out_be16(&ugeth->p_tx_glbl_pram->temoder, temoder);

	test = in_be16(&ugeth->p_tx_glbl_pram->temoder);

	/* Function code register value to be used later */
	function_code = QE_BMR_BYTE_ORDER_BO_MOT | UCC_FAST_FUNCTION_CODE_GBL;
	/* Required for QE */

	/* function code register */
	out_be32(&ugeth->p_tx_glbl_pram->tstate, ((u32) function_code) << 24);

	/* Rx global PRAM */
	/* Allocate global rx parameter RAM page */
	ugeth->rx_glbl_pram_offset =
	    qe_muram_alloc(sizeof(ucc_geth_rx_global_pram_t),
			   UCC_GETH_RX_GLOBAL_PRAM_ALIGNMENT);
	if (IS_MURAM_ERR(ugeth->rx_glbl_pram_offset)) {
		ugeth_err
		    ("%s: Can not allocate DPRAM memory for p_rx_glbl_pram.",
		     __FUNCTION__);
		ucc_geth_memclean(ugeth);
		return -ENOMEM;
	}
	ugeth->p_rx_glbl_pram =
	    (ucc_geth_rx_global_pram_t *) qe_muram_addr(ugeth->
							rx_glbl_pram_offset);
	/* Zero out p_rx_glbl_pram */
	memset(ugeth->p_rx_glbl_pram, 0, sizeof(ucc_geth_rx_global_pram_t));

	/* Fill global PRAM */

	/* RQPTR */
	/* Size varies with number of Rx threads */
	ugeth->thread_dat_rx_offset =
	    qe_muram_alloc(numThreadsRxNumerical *
			   sizeof(ucc_geth_thread_data_rx_t),
			   UCC_GETH_THREAD_DATA_ALIGNMENT);
	if (IS_MURAM_ERR(ugeth->thread_dat_rx_offset)) {
		ugeth_err
		    ("%s: Can not allocate DPRAM memory for p_thread_data_rx.",
		     __FUNCTION__);
		ucc_geth_memclean(ugeth);
		return -ENOMEM;
	}

	ugeth->p_thread_data_rx =
	    (ucc_geth_thread_data_rx_t *) qe_muram_addr(ugeth->
							thread_dat_rx_offset);
	out_be32(&ugeth->p_rx_glbl_pram->rqptr, ugeth->thread_dat_rx_offset);

	/* typeorlen */
	out_be16(&ugeth->p_rx_glbl_pram->typeorlen, ug_info->typeorlen);

	/* rxrmonbaseptr (statistics) */
	if (ug_info->
	    statisticsMode & UCC_GETH_STATISTICS_GATHERING_MODE_FIRMWARE_RX) {
		ugeth->rx_fw_statistics_pram_offset =
		    qe_muram_alloc(sizeof
				   (ucc_geth_rx_firmware_statistics_pram_t),
				   UCC_GETH_RX_STATISTICS_ALIGNMENT);
		if (IS_MURAM_ERR(ugeth->rx_fw_statistics_pram_offset)) {
			ugeth_err
				("%s: Can not allocate DPRAM memory for"
				" p_rx_fw_statistics_pram.", __FUNCTION__);
			ucc_geth_memclean(ugeth);
			return -ENOMEM;
		}
		ugeth->p_rx_fw_statistics_pram =
		    (ucc_geth_rx_firmware_statistics_pram_t *)
		    qe_muram_addr(ugeth->rx_fw_statistics_pram_offset);
		/* Zero out p_rx_fw_statistics_pram */
		memset(ugeth->p_rx_fw_statistics_pram, 0,
		       sizeof(ucc_geth_rx_firmware_statistics_pram_t));
	}

	/* intCoalescingPtr */

	/* Size varies with number of Rx queues */
	ugeth->rx_irq_coalescing_tbl_offset =
	    qe_muram_alloc(ug_info->numQueuesRx *
			   sizeof(ucc_geth_rx_interrupt_coalescing_entry_t),
			   UCC_GETH_RX_INTERRUPT_COALESCING_ALIGNMENT);
	if (IS_MURAM_ERR(ugeth->rx_irq_coalescing_tbl_offset)) {
		ugeth_err
		    ("%s: Can not allocate DPRAM memory for"
			" p_rx_irq_coalescing_tbl.", __FUNCTION__);
		ucc_geth_memclean(ugeth);
		return -ENOMEM;
	}

	ugeth->p_rx_irq_coalescing_tbl =
	    (ucc_geth_rx_interrupt_coalescing_table_t *)
	    qe_muram_addr(ugeth->rx_irq_coalescing_tbl_offset);
	out_be32(&ugeth->p_rx_glbl_pram->intcoalescingptr,
		 ugeth->rx_irq_coalescing_tbl_offset);

	/* Fill interrupt coalescing table */
	for (i = 0; i < ug_info->numQueuesRx; i++) {
		out_be32(&ugeth->p_rx_irq_coalescing_tbl->coalescingentry[i].
			 interruptcoalescingmaxvalue,
			 ug_info->interruptcoalescingmaxvalue[i]);
		out_be32(&ugeth->p_rx_irq_coalescing_tbl->coalescingentry[i].
			 interruptcoalescingcounter,
			 ug_info->interruptcoalescingmaxvalue[i]);
	}

	/* MRBLR */
	init_max_rx_buff_len(uf_info->max_rx_buf_length,
			     &ugeth->p_rx_glbl_pram->mrblr);
	/* MFLR */
	out_be16(&ugeth->p_rx_glbl_pram->mflr, ug_info->maxFrameLength);
	/* MINFLR */
	init_min_frame_len(ug_info->minFrameLength,
			   &ugeth->p_rx_glbl_pram->minflr,
			   &ugeth->p_rx_glbl_pram->mrblr);
	/* MAXD1 */
	out_be16(&ugeth->p_rx_glbl_pram->maxd1, ug_info->maxD1Length);
	/* MAXD2 */
	out_be16(&ugeth->p_rx_glbl_pram->maxd2, ug_info->maxD2Length);

	/* l2qt */
	l2qt = 0;
	for (i = 0; i < UCC_GETH_VLAN_PRIORITY_MAX; i++)
		l2qt |= (ug_info->l2qt[i] << (28 - 4 * i));
	out_be32(&ugeth->p_rx_glbl_pram->l2qt, l2qt);

	/* l3qt */
	for (j = 0; j < UCC_GETH_IP_PRIORITY_MAX; j += 8) {
		l3qt = 0;
		for (i = 0; i < 8; i++)
			l3qt |= (ug_info->l3qt[j + i] << (28 - 4 * i));
		out_be32(&ugeth->p_rx_glbl_pram->l3qt[j], l3qt);
	}

	/* vlantype */
	out_be16(&ugeth->p_rx_glbl_pram->vlantype, ug_info->vlantype);

	/* vlantci */
	out_be16(&ugeth->p_rx_glbl_pram->vlantci, ug_info->vlantci);

	/* ecamptr */
	out_be32(&ugeth->p_rx_glbl_pram->ecamptr, ug_info->ecamptr);

	/* RBDQPTR */
	/* Size varies with number of Rx queues */
	ugeth->rx_bd_qs_tbl_offset =
	    qe_muram_alloc(ug_info->numQueuesRx *
			   (sizeof(ucc_geth_rx_bd_queues_entry_t) +
			    sizeof(ucc_geth_rx_prefetched_bds_t)),
			   UCC_GETH_RX_BD_QUEUES_ALIGNMENT);
	if (IS_MURAM_ERR(ugeth->rx_bd_qs_tbl_offset)) {
		ugeth_err
		    ("%s: Can not allocate DPRAM memory for p_rx_bd_qs_tbl.",
		     __FUNCTION__);
		ucc_geth_memclean(ugeth);
		return -ENOMEM;
	}

	ugeth->p_rx_bd_qs_tbl =
	    (ucc_geth_rx_bd_queues_entry_t *) qe_muram_addr(ugeth->
				    rx_bd_qs_tbl_offset);
	out_be32(&ugeth->p_rx_glbl_pram->rbdqptr, ugeth->rx_bd_qs_tbl_offset);
	/* Zero out p_rx_bd_qs_tbl */
	memset(ugeth->p_rx_bd_qs_tbl,
	       0,
	       ug_info->numQueuesRx * (sizeof(ucc_geth_rx_bd_queues_entry_t) +
				       sizeof(ucc_geth_rx_prefetched_bds_t)));

	/* Setup the table */
	/* Assume BD rings are already established */
	for (i = 0; i < ug_info->numQueuesRx; i++) {
		if (ugeth->ug_info->uf_info.bd_mem_part == MEM_PART_SYSTEM) {
			out_be32(&ugeth->p_rx_bd_qs_tbl[i].externalbdbaseptr,
				 (u32) virt_to_phys(ugeth->p_rx_bd_ring[i]));
		} else if (ugeth->ug_info->uf_info.bd_mem_part ==
			   MEM_PART_MURAM) {
			out_be32(&ugeth->p_rx_bd_qs_tbl[i].externalbdbaseptr,
				 (u32) immrbar_virt_to_phys(ugeth->
							    p_rx_bd_ring[i]));
		}
		/* rest of fields handled by QE */
	}

	/* remoder */
	/* Already has speed set */

	if (ugeth->rx_extended_features)
		remoder |= REMODER_RX_EXTENDED_FEATURES;
	if (ug_info->rxExtendedFiltering)
		remoder |= REMODER_RX_EXTENDED_FILTERING;
	if (ug_info->dynamicMaxFrameLength)
		remoder |= REMODER_DYNAMIC_MAX_FRAME_LENGTH;
	if (ug_info->dynamicMinFrameLength)
		remoder |= REMODER_DYNAMIC_MIN_FRAME_LENGTH;
	remoder |=
	    ug_info->vlanOperationTagged << REMODER_VLAN_OPERATION_TAGGED_SHIFT;
	remoder |=
	    ug_info->
	    vlanOperationNonTagged << REMODER_VLAN_OPERATION_NON_TAGGED_SHIFT;
	remoder |= ug_info->rxQoSMode << REMODER_RX_QOS_MODE_SHIFT;
	remoder |= ((ug_info->numQueuesRx - 1) << REMODER_NUM_OF_QUEUES_SHIFT);
	if (ug_info->ipCheckSumCheck)
		remoder |= REMODER_IP_CHECKSUM_CHECK;
	if (ug_info->ipAddressAlignment)
		remoder |= REMODER_IP_ADDRESS_ALIGNMENT;
	out_be32(&ugeth->p_rx_glbl_pram->remoder, remoder);

	/* Note that this function must be called */
	/* ONLY AFTER p_tx_fw_statistics_pram */
	/* andp_UccGethRxFirmwareStatisticsPram are allocated ! */
	init_firmware_statistics_gathering_mode((ug_info->
		statisticsMode &
		UCC_GETH_STATISTICS_GATHERING_MODE_FIRMWARE_TX),
		(ug_info->statisticsMode &
		UCC_GETH_STATISTICS_GATHERING_MODE_FIRMWARE_RX),
		&ugeth->p_tx_glbl_pram->txrmonbaseptr,
		ugeth->tx_fw_statistics_pram_offset,
		&ugeth->p_rx_glbl_pram->rxrmonbaseptr,
		ugeth->rx_fw_statistics_pram_offset,
		&ugeth->p_tx_glbl_pram->temoder,
		&ugeth->p_rx_glbl_pram->remoder);

	/* function code register */
	ugeth->p_rx_glbl_pram->rstate = function_code;

	/* initialize extended filtering */
	if (ug_info->rxExtendedFiltering) {
		if (!ug_info->extendedFilteringChainPointer) {
			ugeth_err("%s: Null Extended Filtering Chain Pointer.",
				  __FUNCTION__);
			ucc_geth_memclean(ugeth);
			return -EINVAL;
		}

		/* Allocate memory for extended filtering Mode Global
		Parameters */
		ugeth->exf_glbl_param_offset =
		    qe_muram_alloc(sizeof(ucc_geth_exf_global_pram_t),
		UCC_GETH_RX_EXTENDED_FILTERING_GLOBAL_PARAMETERS_ALIGNMENT);
		if (IS_MURAM_ERR(ugeth->exf_glbl_param_offset)) {
			ugeth_err
				("%s: Can not allocate DPRAM memory for"
				" p_exf_glbl_param.", __FUNCTION__);
			ucc_geth_memclean(ugeth);
			return -ENOMEM;
		}

		ugeth->p_exf_glbl_param =
		    (ucc_geth_exf_global_pram_t *) qe_muram_addr(ugeth->
				 exf_glbl_param_offset);
		out_be32(&ugeth->p_rx_glbl_pram->exfGlobalParam,
			 ugeth->exf_glbl_param_offset);
		out_be32(&ugeth->p_exf_glbl_param->l2pcdptr,
			 (u32) ug_info->extendedFilteringChainPointer);

	} else {		/* initialize 82xx style address filtering */

		/* Init individual address recognition registers to disabled */

		for (j = 0; j < NUM_OF_PADDRS; j++)
			ugeth_82xx_filtering_clear_addr_in_paddr(ugeth, (u8) j);

		/* Create CQs for hash tables */
		if (ug_info->maxGroupAddrInHash > 0) {
			INIT_LIST_HEAD(&ugeth->group_hash_q);
		}
		if (ug_info->maxIndAddrInHash > 0) {
			INIT_LIST_HEAD(&ugeth->ind_hash_q);
		}
		p_82xx_addr_filt =
		    (ucc_geth_82xx_address_filtering_pram_t *) ugeth->
		    p_rx_glbl_pram->addressfiltering;

		ugeth_82xx_filtering_clear_all_addr_in_hash(ugeth,
			ENET_ADDR_TYPE_GROUP);
		ugeth_82xx_filtering_clear_all_addr_in_hash(ugeth,
			ENET_ADDR_TYPE_INDIVIDUAL);
	}

	/*
	 * Initialize UCC at QE level
	 */

	command = QE_INIT_TX_RX;

	/* Allocate shadow InitEnet command parameter structure.
	 * This is needed because after the InitEnet command is executed,
	 * the structure in DPRAM is released, because DPRAM is a premium
	 * resource.
	 * This shadow structure keeps a copy of what was done so that the
	 * allocated resources can be released when the channel is freed.
	 */
	if (!(ugeth->p_init_enet_param_shadow =
	     (ucc_geth_init_pram_t *) kmalloc(sizeof(ucc_geth_init_pram_t),
					      GFP_KERNEL))) {
		ugeth_err
		    ("%s: Can not allocate memory for"
			" p_UccInitEnetParamShadows.", __FUNCTION__);
		ucc_geth_memclean(ugeth);
		return -ENOMEM;
	}
	/* Zero out *p_init_enet_param_shadow */
	memset((char *)ugeth->p_init_enet_param_shadow,
	       0, sizeof(ucc_geth_init_pram_t));

	/* Fill shadow InitEnet command parameter structure */

	ugeth->p_init_enet_param_shadow->resinit1 =
	    ENET_INIT_PARAM_MAGIC_RES_INIT1;
	ugeth->p_init_enet_param_shadow->resinit2 =
	    ENET_INIT_PARAM_MAGIC_RES_INIT2;
	ugeth->p_init_enet_param_shadow->resinit3 =
	    ENET_INIT_PARAM_MAGIC_RES_INIT3;
	ugeth->p_init_enet_param_shadow->resinit4 =
	    ENET_INIT_PARAM_MAGIC_RES_INIT4;
	ugeth->p_init_enet_param_shadow->resinit5 =
	    ENET_INIT_PARAM_MAGIC_RES_INIT5;
	ugeth->p_init_enet_param_shadow->rgftgfrxglobal |=
	    ((u32) ug_info->numThreadsRx) << ENET_INIT_PARAM_RGF_SHIFT;
	ugeth->p_init_enet_param_shadow->rgftgfrxglobal |=
	    ((u32) ug_info->numThreadsTx) << ENET_INIT_PARAM_TGF_SHIFT;

	ugeth->p_init_enet_param_shadow->rgftgfrxglobal |=
	    ugeth->rx_glbl_pram_offset | ug_info->riscRx;
	if ((ug_info->largestexternallookupkeysize !=
	     QE_FLTR_LARGEST_EXTERNAL_TABLE_LOOKUP_KEY_SIZE_NONE)
	    && (ug_info->largestexternallookupkeysize !=
		QE_FLTR_LARGEST_EXTERNAL_TABLE_LOOKUP_KEY_SIZE_8_BYTES)
	    && (ug_info->largestexternallookupkeysize !=
		QE_FLTR_LARGEST_EXTERNAL_TABLE_LOOKUP_KEY_SIZE_16_BYTES)) {
		ugeth_err("%s: Invalid largest External Lookup Key Size.",
			  __FUNCTION__);
		ucc_geth_memclean(ugeth);
		return -EINVAL;
	}
	ugeth->p_init_enet_param_shadow->largestexternallookupkeysize =
	    ug_info->largestexternallookupkeysize;
	size = sizeof(ucc_geth_thread_rx_pram_t);
	if (ug_info->rxExtendedFiltering) {
		size += THREAD_RX_PRAM_ADDITIONAL_FOR_EXTENDED_FILTERING;
		if (ug_info->largestexternallookupkeysize ==
		    QE_FLTR_TABLE_LOOKUP_KEY_SIZE_8_BYTES)
			size +=
			    THREAD_RX_PRAM_ADDITIONAL_FOR_EXTENDED_FILTERING_8;
		if (ug_info->largestexternallookupkeysize ==
		    QE_FLTR_TABLE_LOOKUP_KEY_SIZE_16_BYTES)
			size +=
			    THREAD_RX_PRAM_ADDITIONAL_FOR_EXTENDED_FILTERING_16;
	}

	if ((ret_val = fill_init_enet_entries(ugeth, &(ugeth->
		p_init_enet_param_shadow->rxthread[0]),
		(u8) (numThreadsRxNumerical + 1)
		/* Rx needs one extra for terminator */
		, size, UCC_GETH_THREAD_RX_PRAM_ALIGNMENT,
		ug_info->riscRx, 1)) != 0) {
			ugeth_err("%s: Can not fill p_init_enet_param_shadow.",
				__FUNCTION__);
		ucc_geth_memclean(ugeth);
		return ret_val;
	}

	ugeth->p_init_enet_param_shadow->txglobal =
	    ugeth->tx_glbl_pram_offset | ug_info->riscTx;
	if ((ret_val =
	     fill_init_enet_entries(ugeth,
				    &(ugeth->p_init_enet_param_shadow->
				      txthread[0]), numThreadsTxNumerical,
				    sizeof(ucc_geth_thread_tx_pram_t),
				    UCC_GETH_THREAD_TX_PRAM_ALIGNMENT,
				    ug_info->riscTx, 0)) != 0) {
		ugeth_err("%s: Can not fill p_init_enet_param_shadow.",
			  __FUNCTION__);
		ucc_geth_memclean(ugeth);
		return ret_val;
	}

	/* Load Rx bds with buffers */
	for (i = 0; i < ug_info->numQueuesRx; i++) {
		if ((ret_val = rx_bd_buffer_set(ugeth, (u8) i)) != 0) {
			ugeth_err("%s: Can not fill Rx bds with buffers.",
				  __FUNCTION__);
			ucc_geth_memclean(ugeth);
			return ret_val;
		}
	}

	/* Allocate InitEnet command parameter structure */
	init_enet_pram_offset = qe_muram_alloc(sizeof(ucc_geth_init_pram_t), 4);
	if (IS_MURAM_ERR(init_enet_pram_offset)) {
		ugeth_err
		    ("%s: Can not allocate DPRAM memory for p_init_enet_pram.",
		     __FUNCTION__);
		ucc_geth_memclean(ugeth);
		return -ENOMEM;
	}
	p_init_enet_pram =
	    (ucc_geth_init_pram_t *) qe_muram_addr(init_enet_pram_offset);

	/* Copy shadow InitEnet command parameter structure into PRAM */
	p_init_enet_pram->resinit1 = ugeth->p_init_enet_param_shadow->resinit1;
	p_init_enet_pram->resinit2 = ugeth->p_init_enet_param_shadow->resinit2;
	p_init_enet_pram->resinit3 = ugeth->p_init_enet_param_shadow->resinit3;
	p_init_enet_pram->resinit4 = ugeth->p_init_enet_param_shadow->resinit4;
	out_be16(&p_init_enet_pram->resinit5,
		 ugeth->p_init_enet_param_shadow->resinit5);
	p_init_enet_pram->largestexternallookupkeysize =
	    ugeth->p_init_enet_param_shadow->largestexternallookupkeysize;
	out_be32(&p_init_enet_pram->rgftgfrxglobal,
		 ugeth->p_init_enet_param_shadow->rgftgfrxglobal);
	for (i = 0; i < ENET_INIT_PARAM_MAX_ENTRIES_RX; i++)
		out_be32(&p_init_enet_pram->rxthread[i],
			 ugeth->p_init_enet_param_shadow->rxthread[i]);
	out_be32(&p_init_enet_pram->txglobal,
		 ugeth->p_init_enet_param_shadow->txglobal);
	for (i = 0; i < ENET_INIT_PARAM_MAX_ENTRIES_TX; i++)
		out_be32(&p_init_enet_pram->txthread[i],
			 ugeth->p_init_enet_param_shadow->txthread[i]);

	/* Issue QE command */
	cecr_subblock =
	    ucc_fast_get_qe_cr_subblock(ugeth->ug_info->uf_info.ucc_num);
	qe_issue_cmd(command, cecr_subblock, (u8) QE_CR_PROTOCOL_ETHERNET,
		     init_enet_pram_offset);

	/* Free InitEnet command parameter */
	qe_muram_free(init_enet_pram_offset);

	return 0;
}

/* returns a net_device_stats structure pointer */
static struct net_device_stats *ucc_geth_get_stats(struct net_device *dev)
{
	ucc_geth_private_t *ugeth = netdev_priv(dev);

	return &(ugeth->stats);
}

/* ucc_geth_timeout gets called when a packet has not been
 * transmitted after a set amount of time.
 * For now, assume that clearing out all the structures, and
 * starting over will fix the problem. */
static void ucc_geth_timeout(struct net_device *dev)
{
	ucc_geth_private_t *ugeth = netdev_priv(dev);

	ugeth_vdbg("%s: IN", __FUNCTION__);

	ugeth->stats.tx_errors++;

	ugeth_dump_regs(ugeth);

	if (dev->flags & IFF_UP) {
		ucc_geth_stop(ugeth);
		ucc_geth_startup(ugeth);
	}

	netif_schedule(dev);
}

/* This is called by the kernel when a frame is ready for transmission. */
/* It is pointed to by the dev->hard_start_xmit function pointer */
static int ucc_geth_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	ucc_geth_private_t *ugeth = netdev_priv(dev);
	u8 *bd;			/* BD pointer */
	u32 bd_status;
	u8 txQ = 0;

	ugeth_vdbg("%s: IN", __FUNCTION__);

	spin_lock_irq(&ugeth->lock);

	ugeth->stats.tx_bytes += skb->len;

	/* Start from the next BD that should be filled */
	bd = ugeth->txBd[txQ];
	bd_status = BD_STATUS_AND_LENGTH(bd);
	/* Save the skb pointer so we can free it later */
	ugeth->tx_skbuff[txQ][ugeth->skb_curtx[txQ]] = skb;

	/* Update the current skb pointer (wrapping if this was the last) */
	ugeth->skb_curtx[txQ] =
	    (ugeth->skb_curtx[txQ] +
	     1) & TX_RING_MOD_MASK(ugeth->ug_info->bdRingLenTx[txQ]);

	/* set up the buffer descriptor */
	BD_BUFFER_SET(bd,
		      dma_map_single(NULL, skb->data, skb->len, DMA_TO_DEVICE));

	//printk(KERN_DEBUG"skb->data is 0x%x\n",skb->data);

	bd_status = (bd_status & T_W) | T_R | T_I | T_L | skb->len;

	BD_STATUS_AND_LENGTH_SET(bd, bd_status);

	dev->trans_start = jiffies;

	/* Move to next BD in the ring */
	if (!(bd_status & T_W))
		ugeth->txBd[txQ] = bd + UCC_GETH_SIZE_OF_BD;
	else
		ugeth->txBd[txQ] = ugeth->p_tx_bd_ring[txQ];

	/* If the next BD still needs to be cleaned up, then the bds
	   are full.  We need to tell the kernel to stop sending us stuff. */
	if (bd == ugeth->confBd[txQ]) {
		if (!netif_queue_stopped(dev))
			netif_stop_queue(dev);
	}

	if (ugeth->p_scheduler) {
		ugeth->cpucount[txQ]++;
		/* Indicate to QE that there are more Tx bds ready for
		transmission */
		/* This is done by writing a running counter of the bd
		count to the scheduler PRAM. */
		out_be16(ugeth->p_cpucount[txQ], ugeth->cpucount[txQ]);
	}

	spin_unlock_irq(&ugeth->lock);

	return 0;
}

static int ucc_geth_rx(ucc_geth_private_t *ugeth, u8 rxQ, int rx_work_limit)
{
	struct sk_buff *skb;
	u8 *bd;
	u16 length, howmany = 0;
	u32 bd_status;
	u8 *bdBuffer;

	ugeth_vdbg("%s: IN", __FUNCTION__);

	spin_lock(&ugeth->lock);
	/* collect received buffers */
	bd = ugeth->rxBd[rxQ];

	bd_status = BD_STATUS_AND_LENGTH(bd);

	/* while there are received buffers and BD is full (~R_E) */
	while (!((bd_status & (R_E)) || (--rx_work_limit < 0))) {
		bdBuffer = (u8 *) BD_BUFFER(bd);
		length = (u16) ((bd_status & BD_LENGTH_MASK) - 4);
		skb = ugeth->rx_skbuff[rxQ][ugeth->skb_currx[rxQ]];

		/* determine whether buffer is first, last, first and last
		(single buffer frame) or middle (not first and not last) */
		if (!skb ||
		    (!(bd_status & (R_F | R_L))) ||
		    (bd_status & R_ERRORS_FATAL)) {
			ugeth_vdbg("%s, %d: ERROR!!! skb - 0x%08x",
				   __FUNCTION__, __LINE__, (u32) skb);
			if (skb)
				dev_kfree_skb_any(skb);

			ugeth->rx_skbuff[rxQ][ugeth->skb_currx[rxQ]] = NULL;
			ugeth->stats.rx_dropped++;
		} else {
			ugeth->stats.rx_packets++;
			howmany++;

			/* Prep the skb for the packet */
			skb_put(skb, length);

			/* Tell the skb what kind of packet this is */
			skb->protocol = eth_type_trans(skb, ugeth->dev);

			ugeth->stats.rx_bytes += length;
			/* Send the packet up the stack */
#ifdef CONFIG_UGETH_NAPI
			netif_receive_skb(skb);
#else
			netif_rx(skb);
#endif				/* CONFIG_UGETH_NAPI */
		}

		ugeth->dev->last_rx = jiffies;

		skb = get_new_skb(ugeth, bd);
		if (!skb) {
			ugeth_warn("%s: No Rx Data Buffer", __FUNCTION__);
			spin_unlock(&ugeth->lock);
			ugeth->stats.rx_dropped++;
			break;
		}

		ugeth->rx_skbuff[rxQ][ugeth->skb_currx[rxQ]] = skb;

		/* update to point at the next skb */
		ugeth->skb_currx[rxQ] =
		    (ugeth->skb_currx[rxQ] +
		     1) & RX_RING_MOD_MASK(ugeth->ug_info->bdRingLenRx[rxQ]);

		if (bd_status & R_W)
			bd = ugeth->p_rx_bd_ring[rxQ];
		else
			bd += UCC_GETH_SIZE_OF_BD;

		bd_status = BD_STATUS_AND_LENGTH(bd);
	}

	ugeth->rxBd[rxQ] = bd;
	spin_unlock(&ugeth->lock);
	return howmany;
}

static int ucc_geth_tx(struct net_device *dev, u8 txQ)
{
	/* Start from the next BD that should be filled */
	ucc_geth_private_t *ugeth = netdev_priv(dev);
	u8 *bd;			/* BD pointer */
	u32 bd_status;

	bd = ugeth->confBd[txQ];
	bd_status = BD_STATUS_AND_LENGTH(bd);

	/* Normal processing. */
	while ((bd_status & T_R) == 0) {
		/* BD contains already transmitted buffer.   */
		/* Handle the transmitted buffer and release */
		/* the BD to be used with the current frame  */

		if ((bd = ugeth->txBd[txQ]) && (netif_queue_stopped(dev) == 0))
			break;

		ugeth->stats.tx_packets++;

		/* Free the sk buffer associated with this TxBD */
		dev_kfree_skb_irq(ugeth->
				  tx_skbuff[txQ][ugeth->skb_dirtytx[txQ]]);
		ugeth->tx_skbuff[txQ][ugeth->skb_dirtytx[txQ]] = NULL;
		ugeth->skb_dirtytx[txQ] =
		    (ugeth->skb_dirtytx[txQ] +
		     1) & TX_RING_MOD_MASK(ugeth->ug_info->bdRingLenTx[txQ]);

		/* We freed a buffer, so now we can restart transmission */
		if (netif_queue_stopped(dev))
			netif_wake_queue(dev);

		/* Advance the confirmation BD pointer */
		if (!(bd_status & T_W))
			ugeth->confBd[txQ] += UCC_GETH_SIZE_OF_BD;
		else
			ugeth->confBd[txQ] = ugeth->p_tx_bd_ring[txQ];
	}
	return 0;
}

#ifdef CONFIG_UGETH_NAPI
static int ucc_geth_poll(struct net_device *dev, int *budget)
{
	ucc_geth_private_t *ugeth = netdev_priv(dev);
	int howmany;
	int rx_work_limit = *budget;
	u8 rxQ = 0;

	if (rx_work_limit > dev->quota)
		rx_work_limit = dev->quota;

	howmany = ucc_geth_rx(ugeth, rxQ, rx_work_limit);

	dev->quota -= howmany;
	rx_work_limit -= howmany;
	*budget -= howmany;

	if (rx_work_limit >= 0)
		netif_rx_complete(dev);

	return (rx_work_limit < 0) ? 1 : 0;
}
#endif				/* CONFIG_UGETH_NAPI */

static irqreturn_t ucc_geth_irq_handler(int irq, void *info,
					struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *)info;
	ucc_geth_private_t *ugeth = netdev_priv(dev);
	ucc_fast_private_t *uccf;
	ucc_geth_info_t *ug_info;
	register u32 ucce = 0;
	register u32 bit_mask = UCCE_RXBF_SINGLE_MASK;
	register u32 tx_mask = UCCE_TXBF_SINGLE_MASK;
	register u8 i;

	ugeth_vdbg("%s: IN", __FUNCTION__);

	if (!ugeth)
		return IRQ_NONE;

	uccf = ugeth->uccf;
	ug_info = ugeth->ug_info;

	do {
		ucce |= (u32) (in_be32(uccf->p_ucce) & in_be32(uccf->p_uccm));

		/* clear event bits for next time */
		/* Side effect here is to mask ucce variable
		for future processing below. */
		out_be32(uccf->p_ucce, ucce);	/* Clear with ones,
						but only bits in UCCM */

		/* We ignore Tx interrupts because Tx confirmation is
		done inside Tx routine */

		for (i = 0; i < ug_info->numQueuesRx; i++) {
			if (ucce & bit_mask)
				ucc_geth_rx(ugeth, i,
					    (int)ugeth->ug_info->
					    bdRingLenRx[i]);
			ucce &= ~bit_mask;
			bit_mask <<= 1;
		}

		for (i = 0; i < ug_info->numQueuesTx; i++) {
			if (ucce & tx_mask)
				ucc_geth_tx(dev, i);
			ucce &= ~tx_mask;
			tx_mask <<= 1;
		}

		/* Exceptions */
		if (ucce & UCCE_BSY) {
			ugeth_vdbg("Got BUSY irq!!!!");
			ugeth->stats.rx_errors++;
			ucce &= ~UCCE_BSY;
		}
		if (ucce & UCCE_OTHER) {
			ugeth_vdbg("Got frame with error (ucce - 0x%08x)!!!!",
				   ucce);
			ugeth->stats.rx_errors++;
			ucce &= ~ucce;
		}
	}
	while (ucce);

	return IRQ_HANDLED;
}

static irqreturn_t phy_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *)dev_id;
	ucc_geth_private_t *ugeth = netdev_priv(dev);

	ugeth_vdbg("%s: IN", __FUNCTION__);

	/* Clear the interrupt */
	mii_clear_phy_interrupt(ugeth->mii_info);

	/* Disable PHY interrupts */
	mii_configure_phy_interrupt(ugeth->mii_info, MII_INTERRUPT_DISABLED);

	/* Schedule the phy change */
	schedule_work(&ugeth->tq);

	return IRQ_HANDLED;
}

/* Scheduled by the phy_interrupt/timer to handle PHY changes */
static void ugeth_phy_change(void *data)
{
	struct net_device *dev = (struct net_device *)data;
	ucc_geth_private_t *ugeth = netdev_priv(dev);
	ucc_geth_t *ug_regs;
	int result = 0;

	ugeth_vdbg("%s: IN", __FUNCTION__);

	ug_regs = ugeth->ug_regs;

	/* Delay to give the PHY a chance to change the
	 * register state */
	msleep(1);

	/* Update the link, speed, duplex */
	result = ugeth->mii_info->phyinfo->read_status(ugeth->mii_info);

	/* Adjust the known status as long as the link
	 * isn't still coming up */
	if ((0 == result) || (ugeth->mii_info->link == 0))
		adjust_link(dev);

	/* Reenable interrupts, if needed */
	if (ugeth->ug_info->board_flags & FSL_UGETH_BRD_HAS_PHY_INTR)
		mii_configure_phy_interrupt(ugeth->mii_info,
					    MII_INTERRUPT_ENABLED);
}

/* Called every so often on systems that don't interrupt
 * the core for PHY changes */
static void ugeth_phy_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	ucc_geth_private_t *ugeth = netdev_priv(dev);

	schedule_work(&ugeth->tq);

	mod_timer(&ugeth->phy_info_timer, jiffies + PHY_CHANGE_TIME * HZ);
}

/* Keep trying aneg for some time
 * If, after GFAR_AN_TIMEOUT seconds, it has not
 * finished, we switch to forced.
 * Either way, once the process has completed, we either
 * request the interrupt, or switch the timer over to
 * using ugeth_phy_timer to check status */
static void ugeth_phy_startup_timer(unsigned long data)
{
	struct ugeth_mii_info *mii_info = (struct ugeth_mii_info *)data;
	ucc_geth_private_t *ugeth = netdev_priv(mii_info->dev);
	static int secondary = UGETH_AN_TIMEOUT;
	int result;

	/* Configure the Auto-negotiation */
	result = mii_info->phyinfo->config_aneg(mii_info);

	/* If autonegotiation failed to start, and
	 * we haven't timed out, reset the timer, and return */
	if (result && secondary--) {
		mod_timer(&ugeth->phy_info_timer, jiffies + HZ);
		return;
	} else if (result) {
		/* Couldn't start autonegotiation.
		 * Try switching to forced */
		mii_info->autoneg = 0;
		result = mii_info->phyinfo->config_aneg(mii_info);

		/* Forcing failed!  Give up */
		if (result) {
			ugeth_err("%s: Forcing failed!", mii_info->dev->name);
			return;
		}
	}

	/* Kill the timer so it can be restarted */
	del_timer_sync(&ugeth->phy_info_timer);

	/* Grab the PHY interrupt, if necessary/possible */
	if (ugeth->ug_info->board_flags & FSL_UGETH_BRD_HAS_PHY_INTR) {
		if (request_irq(ugeth->ug_info->phy_interrupt,
				phy_interrupt,
				SA_SHIRQ, "phy_interrupt", mii_info->dev) < 0) {
			ugeth_err("%s: Can't get IRQ %d (PHY)",
				  mii_info->dev->name,
				  ugeth->ug_info->phy_interrupt);
		} else {
			mii_configure_phy_interrupt(ugeth->mii_info,
						    MII_INTERRUPT_ENABLED);
			return;
		}
	}

	/* Start the timer again, this time in order to
	 * handle a change in status */
	init_timer(&ugeth->phy_info_timer);
	ugeth->phy_info_timer.function = &ugeth_phy_timer;
	ugeth->phy_info_timer.data = (unsigned long)mii_info->dev;
	mod_timer(&ugeth->phy_info_timer, jiffies + PHY_CHANGE_TIME * HZ);
}

/* Called when something needs to use the ethernet device */
/* Returns 0 for success. */
static int ucc_geth_open(struct net_device *dev)
{
	ucc_geth_private_t *ugeth = netdev_priv(dev);
	int err;

	ugeth_vdbg("%s: IN", __FUNCTION__);

	/* Test station address */
	if (dev->dev_addr[0] & ENET_GROUP_ADDR) {
		ugeth_err("%s: Multicast address used for station address"
			  " - is this what you wanted?", __FUNCTION__);
		return -EINVAL;
	}

	err = ucc_geth_startup(ugeth);
	if (err) {
		ugeth_err("%s: Cannot configure net device, aborting.",
			  dev->name);
		return err;
	}

	err = adjust_enet_interface(ugeth);
	if (err) {
		ugeth_err("%s: Cannot configure net device, aborting.",
			  dev->name);
		return err;
	}

	/*       Set MACSTNADDR1, MACSTNADDR2                */
	/* For more details see the hardware spec.           */
	init_mac_station_addr_regs(dev->dev_addr[0],
				   dev->dev_addr[1],
				   dev->dev_addr[2],
				   dev->dev_addr[3],
				   dev->dev_addr[4],
				   dev->dev_addr[5],
				   &ugeth->ug_regs->macstnaddr1,
				   &ugeth->ug_regs->macstnaddr2);

	err = init_phy(dev);
	if (err) {
		ugeth_err("%s: Cannot initialzie PHY, aborting.", dev->name);
		return err;
	}
#ifndef CONFIG_UGETH_NAPI
	err =
	    request_irq(ugeth->ug_info->uf_info.irq, ucc_geth_irq_handler, 0,
			"UCC Geth", dev);
	if (err) {
		ugeth_err("%s: Cannot get IRQ for net device, aborting.",
			  dev->name);
		ucc_geth_stop(ugeth);
		return err;
	}
#endif				/* CONFIG_UGETH_NAPI */

	/* Set up the PHY change work queue */
	INIT_WORK(&ugeth->tq, ugeth_phy_change, dev);

	init_timer(&ugeth->phy_info_timer);
	ugeth->phy_info_timer.function = &ugeth_phy_startup_timer;
	ugeth->phy_info_timer.data = (unsigned long)ugeth->mii_info;
	mod_timer(&ugeth->phy_info_timer, jiffies + HZ);

	err = ugeth_enable(ugeth, COMM_DIR_RX_AND_TX);
	if (err) {
		ugeth_err("%s: Cannot enable net device, aborting.", dev->name);
		ucc_geth_stop(ugeth);
		return err;
	}

	netif_start_queue(dev);

	return err;
}

/* Stops the kernel queue, and halts the controller */
static int ucc_geth_close(struct net_device *dev)
{
	ucc_geth_private_t *ugeth = netdev_priv(dev);

	ugeth_vdbg("%s: IN", __FUNCTION__);

	ucc_geth_stop(ugeth);

	/* Shutdown the PHY */
	if (ugeth->mii_info->phyinfo->close)
		ugeth->mii_info->phyinfo->close(ugeth->mii_info);

	kfree(ugeth->mii_info);

	netif_stop_queue(dev);

	return 0;
}

struct ethtool_ops ucc_geth_ethtool_ops = {
	.get_settings = NULL,
	.get_drvinfo = NULL,
	.get_regs_len = NULL,
	.get_regs = NULL,
	.get_link = NULL,
	.get_coalesce = NULL,
	.set_coalesce = NULL,
	.get_ringparam = NULL,
	.set_ringparam = NULL,
	.get_strings = NULL,
	.get_stats_count = NULL,
	.get_ethtool_stats = NULL,
};

static int ucc_geth_probe(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);
	struct ucc_geth_platform_data *ugeth_pdata;
	struct net_device *dev = NULL;
	struct ucc_geth_private *ugeth = NULL;
	struct ucc_geth_info *ug_info;
	int err;
	static int mii_mng_configured = 0;

	ugeth_vdbg("%s: IN", __FUNCTION__);

	ugeth_pdata = (struct ucc_geth_platform_data *)pdev->dev.platform_data;

	ug_info = &ugeth_info[pdev->id];
	ug_info->uf_info.ucc_num = pdev->id;
	ug_info->uf_info.rx_clock = ugeth_pdata->rx_clock;
	ug_info->uf_info.tx_clock = ugeth_pdata->tx_clock;
	ug_info->uf_info.regs = ugeth_pdata->phy_reg_addr;
	ug_info->uf_info.irq = platform_get_irq(pdev, 0);
	ug_info->phy_address = ugeth_pdata->phy_id;
	ug_info->enet_interface = ugeth_pdata->phy_interface;
	ug_info->board_flags = ugeth_pdata->board_flags;
	ug_info->phy_interrupt = ugeth_pdata->phy_interrupt;

	printk(KERN_INFO "ucc_geth: UCC%1d at 0x%8x (irq = %d) \n",
		ug_info->uf_info.ucc_num + 1, ug_info->uf_info.regs,
		ug_info->uf_info.irq);

	if (ug_info == NULL) {
		ugeth_err("%s: [%d] Missing additional data!", __FUNCTION__,
			  pdev->id);
		return -ENODEV;
	}

	if (!mii_mng_configured) {
		ucc_set_qe_mux_mii_mng(ug_info->uf_info.ucc_num);
		mii_mng_configured = 1;
	}

	/* Create an ethernet device instance */
	dev = alloc_etherdev(sizeof(*ugeth));

	if (dev == NULL)
		return -ENOMEM;

	ugeth = netdev_priv(dev);
	spin_lock_init(&ugeth->lock);

	dev_set_drvdata(device, dev);

	/* Set the dev->base_addr to the gfar reg region */
	dev->base_addr = (unsigned long)(ug_info->uf_info.regs);

	SET_MODULE_OWNER(dev);
	SET_NETDEV_DEV(dev, device);

	/* Fill in the dev structure */
	dev->open = ucc_geth_open;
	dev->hard_start_xmit = ucc_geth_start_xmit;
	dev->tx_timeout = ucc_geth_timeout;
	dev->watchdog_timeo = TX_TIMEOUT;
#ifdef CONFIG_UGETH_NAPI
	dev->poll = ucc_geth_poll;
	dev->weight = UCC_GETH_DEV_WEIGHT;
#endif				/* CONFIG_UGETH_NAPI */
	dev->stop = ucc_geth_close;
	dev->get_stats = ucc_geth_get_stats;
//    dev->change_mtu = ucc_geth_change_mtu;
	dev->mtu = 1500;
	dev->set_multicast_list = ucc_geth_set_multi;
	dev->ethtool_ops = &ucc_geth_ethtool_ops;

	err = register_netdev(dev);
	if (err) {
		ugeth_err("%s: Cannot register net device, aborting.",
			  dev->name);
		free_netdev(dev);
		return err;
	}

	ugeth->ug_info = ug_info;
	ugeth->dev = dev;
	memcpy(dev->dev_addr, ugeth_pdata->mac_addr, 6);

	return 0;
}

static int ucc_geth_remove(struct device *device)
{
	struct net_device *dev = dev_get_drvdata(device);
	struct ucc_geth_private *ugeth = netdev_priv(dev);

	dev_set_drvdata(device, NULL);
	ucc_geth_memclean(ugeth);
	free_netdev(dev);

	return 0;
}

/* Structure for a device driver */
static struct device_driver ucc_geth_driver = {
	.name = DRV_NAME,
	.bus = &platform_bus_type,
	.probe = ucc_geth_probe,
	.remove = ucc_geth_remove,
};

static int __init ucc_geth_init(void)
{
	int i;
	printk(KERN_INFO "ucc_geth: " DRV_DESC "\n");
	for (i = 0; i < 8; i++)
		memcpy(&(ugeth_info[i]), &ugeth_primary_info,
		       sizeof(ugeth_primary_info));

	return driver_register(&ucc_geth_driver);
}

static void __exit ucc_geth_exit(void)
{
	driver_unregister(&ucc_geth_driver);
}

module_init(ucc_geth_init);
module_exit(ucc_geth_exit);

MODULE_AUTHOR("Freescale Semiconductor, Inc");
MODULE_DESCRIPTION(DRV_DESC);
MODULE_LICENSE("GPL");
