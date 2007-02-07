/*
 *  include/asm-s390/sclp.h
 *
 *    Copyright IBM Corp. 2007
 *    Author(s): Heiko Carstens <heiko.carstens@de.ibm.com>
 */

#ifndef _ASM_S390_SCLP_H
#define _ASM_S390_SCLP_H

#include <linux/types.h>

struct sccb_header {
	u16	length;
	u8	function_code;
	u8	control_mask[3];
	u16	response_code;
} __attribute__((packed));

#define LOADPARM_LEN 8

struct sclp_readinfo_sccb {
	struct	sccb_header header;	/* 0-7 */
	u16	rnmax;			/* 8-9 */
	u8	rnsize;			/* 10 */
	u8	_reserved0[24 - 11];	/* 11-23 */
	u8	loadparm[LOADPARM_LEN];	/* 24-31 */
	u8	_reserved1[91 - 32];	/* 32-90 */
	u8	flags;			/* 91 */
	u8	_reserved2[100 - 92];	/* 92-99 */
	u32	rnsize2;		/* 100-103 */
	u64	rnmax2;			/* 104-111 */
	u8	_reserved3[4096 - 112];	/* 112-4095 */
} __attribute__((packed, aligned(4096)));

extern struct sclp_readinfo_sccb s390_readinfo_sccb;
extern void sclp_readinfo_early(void);

#endif /* _ASM_S390_SCLP_H */
