/*
 * Copyright (C) 2008-2011 Freescale Semiconductor, Inc. All rights reserved.
 *
 * Author: Yu Liu, yu.liu@freescale.com
 *
 * Description:
 * This file is based on arch/powerpc/kvm/44x_tlb.c,
 * by Hollis Blanchard <hollisb@us.ibm.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kvm.h>
#include <linux/kvm_host.h>
#include <linux/highmem.h>
#include <asm/kvm_ppc.h>
#include <asm/kvm_e500.h>

#include "../mm/mmu_decl.h"
#include "e500_tlb.h"
#include "trace.h"
#include "timing.h"

#define to_htlb1_esel(esel) (tlb1_entry_num - (esel) - 1)

static unsigned int tlb1_entry_num;

void kvmppc_dump_tlbs(struct kvm_vcpu *vcpu)
{
	struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);
	struct tlbe *tlbe;
	int i, tlbsel;

	printk("| %8s | %8s | %8s | %8s | %8s |\n",
			"nr", "mas1", "mas2", "mas3", "mas7");

	for (tlbsel = 0; tlbsel < 2; tlbsel++) {
		printk("Guest TLB%d:\n", tlbsel);
		for (i = 0; i < vcpu_e500->guest_tlb_size[tlbsel]; i++) {
			tlbe = &vcpu_e500->guest_tlb[tlbsel][i];
			if (tlbe->mas1 & MAS1_VALID)
				printk(" G[%d][%3d] |  %08X | %08X | %08X | %08X |\n",
					tlbsel, i, tlbe->mas1, tlbe->mas2,
					tlbe->mas3, tlbe->mas7);
		}
	}

	for (tlbsel = 0; tlbsel < 2; tlbsel++) {
		printk("Shadow TLB%d:\n", tlbsel);
		for (i = 0; i < vcpu_e500->shadow_tlb_size[tlbsel]; i++) {
			tlbe = &vcpu_e500->shadow_tlb[tlbsel][i];
			if (tlbe->mas1 & MAS1_VALID)
				printk(" S[%d][%3d] |  %08X | %08X | %08X | %08X |\n",
					tlbsel, i, tlbe->mas1, tlbe->mas2,
					tlbe->mas3, tlbe->mas7);
		}
	}
}

static inline unsigned int tlb0_get_next_victim(
		struct kvmppc_vcpu_e500 *vcpu_e500)
{
	unsigned int victim;

	victim = vcpu_e500->guest_tlb_nv[0]++;
	if (unlikely(vcpu_e500->guest_tlb_nv[0] >= KVM_E500_TLB0_WAY_NUM))
		vcpu_e500->guest_tlb_nv[0] = 0;

	return victim;
}

static inline unsigned int tlb1_max_shadow_size(void)
{
	/* reserve one entry for magic page */
	return tlb1_entry_num - tlbcam_index - 1;
}

static inline int tlbe_is_writable(struct tlbe *tlbe)
{
	return tlbe->mas3 & (MAS3_SW|MAS3_UW);
}

static inline u32 e500_shadow_mas3_attrib(u32 mas3, int usermode)
{
	/* Mask off reserved bits. */
	mas3 &= MAS3_ATTRIB_MASK;

	if (!usermode) {
		/* Guest is in supervisor mode,
		 * so we need to translate guest
		 * supervisor permissions into user permissions. */
		mas3 &= ~E500_TLB_USER_PERM_MASK;
		mas3 |= (mas3 & E500_TLB_SUPER_PERM_MASK) << 1;
	}

	return mas3 | E500_TLB_SUPER_PERM_MASK;
}

static inline u32 e500_shadow_mas2_attrib(u32 mas2, int usermode)
{
#ifdef CONFIG_SMP
	return (mas2 & MAS2_ATTRIB_MASK) | MAS2_M;
#else
	return mas2 & MAS2_ATTRIB_MASK;
#endif
}

/*
 * writing shadow tlb entry to host TLB
 */
static inline void __write_host_tlbe(struct tlbe *stlbe, uint32_t mas0)
{
	unsigned long flags;

	local_irq_save(flags);
	mtspr(SPRN_MAS0, mas0);
	mtspr(SPRN_MAS1, stlbe->mas1);
	mtspr(SPRN_MAS2, stlbe->mas2);
	mtspr(SPRN_MAS3, stlbe->mas3);
	mtspr(SPRN_MAS7, stlbe->mas7);
	asm volatile("isync; tlbwe" : : : "memory");
	local_irq_restore(flags);
}

static inline void write_host_tlbe(struct kvmppc_vcpu_e500 *vcpu_e500,
		int tlbsel, int esel)
{
	struct tlbe *stlbe = &vcpu_e500->shadow_tlb[tlbsel][esel];

	if (tlbsel == 0) {
		__write_host_tlbe(stlbe,
				  MAS0_TLBSEL(0) |
				  MAS0_ESEL(esel & (KVM_E500_TLB0_WAY_NUM - 1)));
	} else {
		__write_host_tlbe(stlbe,
				  MAS0_TLBSEL(1) |
				  MAS0_ESEL(to_htlb1_esel(esel)));
	}
}

void kvmppc_map_magic(struct kvm_vcpu *vcpu)
{
	struct tlbe magic;
	ulong shared_page = ((ulong)vcpu->arch.shared) & PAGE_MASK;
	pfn_t pfn;

	pfn = (pfn_t)virt_to_phys((void *)shared_page) >> PAGE_SHIFT;
	get_page(pfn_to_page(pfn));

	magic.mas1 = MAS1_VALID | MAS1_TS |
		     MAS1_TSIZE(BOOK3E_PAGESZ_4K);
	magic.mas2 = vcpu->arch.magic_page_ea | MAS2_M;
	magic.mas3 = (pfn << PAGE_SHIFT) |
		     MAS3_SW | MAS3_SR | MAS3_UW | MAS3_UR;
	magic.mas7 = pfn >> (32 - PAGE_SHIFT);

	__write_host_tlbe(&magic, MAS0_TLBSEL(1) | MAS0_ESEL(tlbcam_index));
}

void kvmppc_e500_tlb_load(struct kvm_vcpu *vcpu, int cpu)
{
}

void kvmppc_e500_tlb_put(struct kvm_vcpu *vcpu)
{
	_tlbil_all();
}

/* Search the guest TLB for a matching entry. */
static int kvmppc_e500_tlb_index(struct kvmppc_vcpu_e500 *vcpu_e500,
		gva_t eaddr, int tlbsel, unsigned int pid, int as)
{
	int i;

	/* XXX Replace loop with fancy data structures. */
	for (i = 0; i < vcpu_e500->guest_tlb_size[tlbsel]; i++) {
		struct tlbe *tlbe = &vcpu_e500->guest_tlb[tlbsel][i];
		unsigned int tid;

		if (eaddr < get_tlb_eaddr(tlbe))
			continue;

		if (eaddr > get_tlb_end(tlbe))
			continue;

		tid = get_tlb_tid(tlbe);
		if (tid && (tid != pid))
			continue;

		if (!get_tlb_v(tlbe))
			continue;

		if (get_tlb_ts(tlbe) != as && as != -1)
			continue;

		return i;
	}

	return -1;
}

static void kvmppc_e500_shadow_release(struct kvmppc_vcpu_e500 *vcpu_e500,
		int tlbsel, int esel)
{
	struct tlbe *stlbe = &vcpu_e500->shadow_tlb[tlbsel][esel];
	unsigned long pfn;

	pfn = stlbe->mas3 >> PAGE_SHIFT;
	pfn |= stlbe->mas7 << (32 - PAGE_SHIFT);

	if (get_tlb_v(stlbe)) {
		if (tlbe_is_writable(stlbe))
			kvm_release_pfn_dirty(pfn);
		else
			kvm_release_pfn_clean(pfn);
	}
}

static void kvmppc_e500_stlbe_invalidate(struct kvmppc_vcpu_e500 *vcpu_e500,
		int tlbsel, int esel)
{
	struct tlbe *stlbe = &vcpu_e500->shadow_tlb[tlbsel][esel];

	kvmppc_e500_shadow_release(vcpu_e500, tlbsel, esel);
	stlbe->mas1 = 0;
	trace_kvm_stlb_inval(index_of(tlbsel, esel));
}

static void kvmppc_e500_tlb1_invalidate(struct kvmppc_vcpu_e500 *vcpu_e500,
		gva_t eaddr, gva_t eend, u32 tid)
{
	unsigned int pid = tid & 0xff;
	unsigned int i;

	/* XXX Replace loop with fancy data structures. */
	for (i = 0; i < vcpu_e500->guest_tlb_size[1]; i++) {
		struct tlbe *stlbe = &vcpu_e500->shadow_tlb[1][i];
		unsigned int tid;

		if (!get_tlb_v(stlbe))
			continue;

		if (eend < get_tlb_eaddr(stlbe))
			continue;

		if (eaddr > get_tlb_end(stlbe))
			continue;

		tid = get_tlb_tid(stlbe);
		if (tid && (tid != pid))
			continue;

		kvmppc_e500_stlbe_invalidate(vcpu_e500, 1, i);
		write_host_tlbe(vcpu_e500, 1, i);
	}
}

static inline void kvmppc_e500_deliver_tlb_miss(struct kvm_vcpu *vcpu,
		unsigned int eaddr, int as)
{
	struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);
	unsigned int victim, pidsel, tsized;
	int tlbsel;

	/* since we only have two TLBs, only lower bit is used. */
	tlbsel = (vcpu_e500->mas4 >> 28) & 0x1;
	victim = (tlbsel == 0) ? tlb0_get_next_victim(vcpu_e500) : 0;
	pidsel = (vcpu_e500->mas4 >> 16) & 0xf;
	tsized = (vcpu_e500->mas4 >> 7) & 0x1f;

	vcpu_e500->mas0 = MAS0_TLBSEL(tlbsel) | MAS0_ESEL(victim)
		| MAS0_NV(vcpu_e500->guest_tlb_nv[tlbsel]);
	vcpu_e500->mas1 = MAS1_VALID | (as ? MAS1_TS : 0)
		| MAS1_TID(vcpu_e500->pid[pidsel])
		| MAS1_TSIZE(tsized);
	vcpu_e500->mas2 = (eaddr & MAS2_EPN)
		| (vcpu_e500->mas4 & MAS2_ATTRIB_MASK);
	vcpu_e500->mas3 &= MAS3_U0 | MAS3_U1 | MAS3_U2 | MAS3_U3;
	vcpu_e500->mas6 = (vcpu_e500->mas6 & MAS6_SPID1)
		| (get_cur_pid(vcpu) << 16)
		| (as ? MAS6_SAS : 0);
	vcpu_e500->mas7 = 0;
}

static inline void kvmppc_e500_shadow_map(struct kvmppc_vcpu_e500 *vcpu_e500,
	u64 gvaddr, gfn_t gfn, struct tlbe *gtlbe, int tlbsel, int esel)
{
	struct kvm_memory_slot *slot;
	struct tlbe *stlbe;
	unsigned long pfn, hva;
	int pfnmap = 0;
	int tsize = BOOK3E_PAGESZ_4K;

	stlbe = &vcpu_e500->shadow_tlb[tlbsel][esel];

	/*
	 * Translate guest physical to true physical, acquiring
	 * a page reference if it is normal, non-reserved memory.
	 *
	 * gfn_to_memslot() must succeed because otherwise we wouldn't
	 * have gotten this far.  Eventually we should just pass the slot
	 * pointer through from the first lookup.
	 */
	slot = gfn_to_memslot(vcpu_e500->vcpu.kvm, gfn);
	hva = gfn_to_hva_memslot(slot, gfn);

	if (tlbsel == 1) {
		struct vm_area_struct *vma;
		down_read(&current->mm->mmap_sem);

		vma = find_vma(current->mm, hva);
		if (vma && hva >= vma->vm_start &&
		    (vma->vm_flags & VM_PFNMAP)) {
			/*
			 * This VMA is a physically contiguous region (e.g.
			 * /dev/mem) that bypasses normal Linux page
			 * management.  Find the overlap between the
			 * vma and the memslot.
			 */

			unsigned long start, end;
			unsigned long slot_start, slot_end;

			pfnmap = 1;

			start = vma->vm_pgoff;
			end = start +
			      ((vma->vm_end - vma->vm_start) >> PAGE_SHIFT);

			pfn = start + ((hva - vma->vm_start) >> PAGE_SHIFT);

			slot_start = pfn - (gfn - slot->base_gfn);
			slot_end = slot_start + slot->npages;

			if (start < slot_start)
				start = slot_start;
			if (end > slot_end)
				end = slot_end;

			tsize = (gtlbe->mas1 & MAS1_TSIZE_MASK) >>
				MAS1_TSIZE_SHIFT;

			/*
			 * e500 doesn't implement the lowest tsize bit,
			 * or 1K pages.
			 */
			tsize = max(BOOK3E_PAGESZ_4K, tsize & ~1);

			/*
			 * Now find the largest tsize (up to what the guest
			 * requested) that will cover gfn, stay within the
			 * range, and for which gfn and pfn are mutually
			 * aligned.
			 */

			for (; tsize > BOOK3E_PAGESZ_4K; tsize -= 2) {
				unsigned long gfn_start, gfn_end, tsize_pages;
				tsize_pages = 1 << (tsize - 2);

				gfn_start = gfn & ~(tsize_pages - 1);
				gfn_end = gfn_start + tsize_pages;

				if (gfn_start + pfn - gfn < start)
					continue;
				if (gfn_end + pfn - gfn > end)
					continue;
				if ((gfn & (tsize_pages - 1)) !=
				    (pfn & (tsize_pages - 1)))
					continue;

				gvaddr &= ~((tsize_pages << PAGE_SHIFT) - 1);
				pfn &= ~(tsize_pages - 1);
				break;
			}
		}

		up_read(&current->mm->mmap_sem);
	}

	if (likely(!pfnmap)) {
		pfn = gfn_to_pfn_memslot(vcpu_e500->vcpu.kvm, slot, gfn);
		if (is_error_pfn(pfn)) {
			printk(KERN_ERR "Couldn't get real page for gfn %lx!\n",
					(long)gfn);
			kvm_release_pfn_clean(pfn);
			return;
		}
	}

	/* Drop reference to old page. */
	kvmppc_e500_shadow_release(vcpu_e500, tlbsel, esel);

	/* Force TS=1 IPROT=0 for all guest mappings. */
	stlbe->mas1 = MAS1_TSIZE(tsize)
		| MAS1_TID(get_tlb_tid(gtlbe)) | MAS1_TS | MAS1_VALID;
	stlbe->mas2 = (gvaddr & MAS2_EPN)
		| e500_shadow_mas2_attrib(gtlbe->mas2,
				vcpu_e500->vcpu.arch.shared->msr & MSR_PR);
	stlbe->mas3 = ((pfn << PAGE_SHIFT) & MAS3_RPN)
		| e500_shadow_mas3_attrib(gtlbe->mas3,
				vcpu_e500->vcpu.arch.shared->msr & MSR_PR);
	stlbe->mas7 = (pfn >> (32 - PAGE_SHIFT)) & MAS7_RPN;

	trace_kvm_stlb_write(index_of(tlbsel, esel), stlbe->mas1, stlbe->mas2,
			     stlbe->mas3, stlbe->mas7);
}

/* XXX only map the one-one case, for now use TLB0 */
static int kvmppc_e500_stlbe_map(struct kvmppc_vcpu_e500 *vcpu_e500,
		int tlbsel, int esel)
{
	struct tlbe *gtlbe;

	gtlbe = &vcpu_e500->guest_tlb[tlbsel][esel];

	kvmppc_e500_shadow_map(vcpu_e500, get_tlb_eaddr(gtlbe),
			get_tlb_raddr(gtlbe) >> PAGE_SHIFT,
			gtlbe, tlbsel, esel);

	return esel;
}

/* Caller must ensure that the specified guest TLB entry is safe to insert into
 * the shadow TLB. */
/* XXX for both one-one and one-to-many , for now use TLB1 */
static int kvmppc_e500_tlb1_map(struct kvmppc_vcpu_e500 *vcpu_e500,
		u64 gvaddr, gfn_t gfn, struct tlbe *gtlbe)
{
	unsigned int victim;

	victim = vcpu_e500->guest_tlb_nv[1]++;

	if (unlikely(vcpu_e500->guest_tlb_nv[1] >= tlb1_max_shadow_size()))
		vcpu_e500->guest_tlb_nv[1] = 0;

	kvmppc_e500_shadow_map(vcpu_e500, gvaddr, gfn, gtlbe, 1, victim);

	return victim;
}

/* Invalidate all guest kernel mappings when enter usermode,
 * so that when they fault back in they will get the
 * proper permission bits. */
void kvmppc_mmu_priv_switch(struct kvm_vcpu *vcpu, int usermode)
{
	if (usermode) {
		struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);
		int i;

		/* XXX Replace loop with fancy data structures. */
		for (i = 0; i < tlb1_max_shadow_size(); i++)
			kvmppc_e500_stlbe_invalidate(vcpu_e500, 1, i);

		_tlbil_all();
	}
}

static int kvmppc_e500_gtlbe_invalidate(struct kvmppc_vcpu_e500 *vcpu_e500,
		int tlbsel, int esel)
{
	struct tlbe *gtlbe = &vcpu_e500->guest_tlb[tlbsel][esel];

	if (unlikely(get_tlb_iprot(gtlbe)))
		return -1;

	if (tlbsel == 1) {
		kvmppc_e500_tlb1_invalidate(vcpu_e500, get_tlb_eaddr(gtlbe),
				get_tlb_end(gtlbe),
				get_tlb_tid(gtlbe));
	} else {
		kvmppc_e500_stlbe_invalidate(vcpu_e500, tlbsel, esel);
	}

	gtlbe->mas1 = 0;

	return 0;
}

int kvmppc_e500_emul_mt_mmucsr0(struct kvmppc_vcpu_e500 *vcpu_e500, ulong value)
{
	int esel;

	if (value & MMUCSR0_TLB0FI)
		for (esel = 0; esel < vcpu_e500->guest_tlb_size[0]; esel++)
			kvmppc_e500_gtlbe_invalidate(vcpu_e500, 0, esel);
	if (value & MMUCSR0_TLB1FI)
		for (esel = 0; esel < vcpu_e500->guest_tlb_size[1]; esel++)
			kvmppc_e500_gtlbe_invalidate(vcpu_e500, 1, esel);

	_tlbil_all();

	return EMULATE_DONE;
}

int kvmppc_e500_emul_tlbivax(struct kvm_vcpu *vcpu, int ra, int rb)
{
	struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);
	unsigned int ia;
	int esel, tlbsel;
	gva_t ea;

	ea = ((ra) ? kvmppc_get_gpr(vcpu, ra) : 0) + kvmppc_get_gpr(vcpu, rb);

	ia = (ea >> 2) & 0x1;

	/* since we only have two TLBs, only lower bit is used. */
	tlbsel = (ea >> 3) & 0x1;

	if (ia) {
		/* invalidate all entries */
		for (esel = 0; esel < vcpu_e500->guest_tlb_size[tlbsel]; esel++)
			kvmppc_e500_gtlbe_invalidate(vcpu_e500, tlbsel, esel);
	} else {
		ea &= 0xfffff000;
		esel = kvmppc_e500_tlb_index(vcpu_e500, ea, tlbsel,
				get_cur_pid(vcpu), -1);
		if (esel >= 0)
			kvmppc_e500_gtlbe_invalidate(vcpu_e500, tlbsel, esel);
	}

	_tlbil_all();

	return EMULATE_DONE;
}

int kvmppc_e500_emul_tlbre(struct kvm_vcpu *vcpu)
{
	struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);
	int tlbsel, esel;
	struct tlbe *gtlbe;

	tlbsel = get_tlb_tlbsel(vcpu_e500);
	esel = get_tlb_esel(vcpu_e500, tlbsel);

	gtlbe = &vcpu_e500->guest_tlb[tlbsel][esel];
	vcpu_e500->mas0 &= ~MAS0_NV(~0);
	vcpu_e500->mas0 |= MAS0_NV(vcpu_e500->guest_tlb_nv[tlbsel]);
	vcpu_e500->mas1 = gtlbe->mas1;
	vcpu_e500->mas2 = gtlbe->mas2;
	vcpu_e500->mas3 = gtlbe->mas3;
	vcpu_e500->mas7 = gtlbe->mas7;

	return EMULATE_DONE;
}

int kvmppc_e500_emul_tlbsx(struct kvm_vcpu *vcpu, int rb)
{
	struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);
	int as = !!get_cur_sas(vcpu_e500);
	unsigned int pid = get_cur_spid(vcpu_e500);
	int esel, tlbsel;
	struct tlbe *gtlbe = NULL;
	gva_t ea;

	ea = kvmppc_get_gpr(vcpu, rb);

	for (tlbsel = 0; tlbsel < 2; tlbsel++) {
		esel = kvmppc_e500_tlb_index(vcpu_e500, ea, tlbsel, pid, as);
		if (esel >= 0) {
			gtlbe = &vcpu_e500->guest_tlb[tlbsel][esel];
			break;
		}
	}

	if (gtlbe) {
		vcpu_e500->mas0 = MAS0_TLBSEL(tlbsel) | MAS0_ESEL(esel)
			| MAS0_NV(vcpu_e500->guest_tlb_nv[tlbsel]);
		vcpu_e500->mas1 = gtlbe->mas1;
		vcpu_e500->mas2 = gtlbe->mas2;
		vcpu_e500->mas3 = gtlbe->mas3;
		vcpu_e500->mas7 = gtlbe->mas7;
	} else {
		int victim;

		/* since we only have two TLBs, only lower bit is used. */
		tlbsel = vcpu_e500->mas4 >> 28 & 0x1;
		victim = (tlbsel == 0) ? tlb0_get_next_victim(vcpu_e500) : 0;

		vcpu_e500->mas0 = MAS0_TLBSEL(tlbsel) | MAS0_ESEL(victim)
			| MAS0_NV(vcpu_e500->guest_tlb_nv[tlbsel]);
		vcpu_e500->mas1 = (vcpu_e500->mas6 & MAS6_SPID0)
			| (vcpu_e500->mas6 & (MAS6_SAS ? MAS1_TS : 0))
			| (vcpu_e500->mas4 & MAS4_TSIZED(~0));
		vcpu_e500->mas2 &= MAS2_EPN;
		vcpu_e500->mas2 |= vcpu_e500->mas4 & MAS2_ATTRIB_MASK;
		vcpu_e500->mas3 &= MAS3_U0 | MAS3_U1 | MAS3_U2 | MAS3_U3;
		vcpu_e500->mas7 = 0;
	}

	kvmppc_set_exit_type(vcpu, EMULATED_TLBSX_EXITS);
	return EMULATE_DONE;
}

int kvmppc_e500_emul_tlbwe(struct kvm_vcpu *vcpu)
{
	struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);
	u64 eaddr;
	u64 raddr;
	u32 tid;
	struct tlbe *gtlbe;
	int tlbsel, esel, stlbsel, sesel;

	tlbsel = get_tlb_tlbsel(vcpu_e500);
	esel = get_tlb_esel(vcpu_e500, tlbsel);

	gtlbe = &vcpu_e500->guest_tlb[tlbsel][esel];

	if (get_tlb_v(gtlbe) && tlbsel == 1) {
		eaddr = get_tlb_eaddr(gtlbe);
		tid = get_tlb_tid(gtlbe);
		kvmppc_e500_tlb1_invalidate(vcpu_e500, eaddr,
				get_tlb_end(gtlbe), tid);
	}

	gtlbe->mas1 = vcpu_e500->mas1;
	gtlbe->mas2 = vcpu_e500->mas2;
	gtlbe->mas3 = vcpu_e500->mas3;
	gtlbe->mas7 = vcpu_e500->mas7;

	trace_kvm_gtlb_write(vcpu_e500->mas0, gtlbe->mas1, gtlbe->mas2,
			     gtlbe->mas3, gtlbe->mas7);

	/* Invalidate shadow mappings for the about-to-be-clobbered TLBE. */
	if (tlbe_is_host_safe(vcpu, gtlbe)) {
		switch (tlbsel) {
		case 0:
			/* TLB0 */
			gtlbe->mas1 &= ~MAS1_TSIZE(~0);
			gtlbe->mas1 |= MAS1_TSIZE(BOOK3E_PAGESZ_4K);

			stlbsel = 0;
			sesel = kvmppc_e500_stlbe_map(vcpu_e500, 0, esel);

			break;

		case 1:
			/* TLB1 */
			eaddr = get_tlb_eaddr(gtlbe);
			raddr = get_tlb_raddr(gtlbe);

			/* Create a 4KB mapping on the host.
			 * If the guest wanted a large page,
			 * only the first 4KB is mapped here and the rest
			 * are mapped on the fly. */
			stlbsel = 1;
			sesel = kvmppc_e500_tlb1_map(vcpu_e500, eaddr,
					raddr >> PAGE_SHIFT, gtlbe);
			break;

		default:
			BUG();
		}
		write_host_tlbe(vcpu_e500, stlbsel, sesel);
	}

	kvmppc_set_exit_type(vcpu, EMULATED_TLBWE_EXITS);
	return EMULATE_DONE;
}

int kvmppc_mmu_itlb_index(struct kvm_vcpu *vcpu, gva_t eaddr)
{
	unsigned int as = !!(vcpu->arch.shared->msr & MSR_IS);

	return kvmppc_e500_tlb_search(vcpu, eaddr, get_cur_pid(vcpu), as);
}

int kvmppc_mmu_dtlb_index(struct kvm_vcpu *vcpu, gva_t eaddr)
{
	unsigned int as = !!(vcpu->arch.shared->msr & MSR_DS);

	return kvmppc_e500_tlb_search(vcpu, eaddr, get_cur_pid(vcpu), as);
}

void kvmppc_mmu_itlb_miss(struct kvm_vcpu *vcpu)
{
	unsigned int as = !!(vcpu->arch.shared->msr & MSR_IS);

	kvmppc_e500_deliver_tlb_miss(vcpu, vcpu->arch.pc, as);
}

void kvmppc_mmu_dtlb_miss(struct kvm_vcpu *vcpu)
{
	unsigned int as = !!(vcpu->arch.shared->msr & MSR_DS);

	kvmppc_e500_deliver_tlb_miss(vcpu, vcpu->arch.fault_dear, as);
}

gpa_t kvmppc_mmu_xlate(struct kvm_vcpu *vcpu, unsigned int index,
			gva_t eaddr)
{
	struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);
	struct tlbe *gtlbe =
		&vcpu_e500->guest_tlb[tlbsel_of(index)][esel_of(index)];
	u64 pgmask = get_tlb_bytes(gtlbe) - 1;

	return get_tlb_raddr(gtlbe) | (eaddr & pgmask);
}

void kvmppc_mmu_destroy(struct kvm_vcpu *vcpu)
{
	struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);
	int tlbsel, i;

	for (tlbsel = 0; tlbsel < 2; tlbsel++)
		for (i = 0; i < vcpu_e500->guest_tlb_size[tlbsel]; i++)
			kvmppc_e500_shadow_release(vcpu_e500, tlbsel, i);

	/* discard all guest mapping */
	_tlbil_all();
}

void kvmppc_mmu_map(struct kvm_vcpu *vcpu, u64 eaddr, gpa_t gpaddr,
			unsigned int index)
{
	struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);
	int tlbsel = tlbsel_of(index);
	int esel = esel_of(index);
	int stlbsel, sesel;

	switch (tlbsel) {
	case 0:
		stlbsel = 0;
		sesel = esel;
		break;

	case 1: {
		gfn_t gfn = gpaddr >> PAGE_SHIFT;
		struct tlbe *gtlbe
			= &vcpu_e500->guest_tlb[tlbsel][esel];

		stlbsel = 1;
		sesel = kvmppc_e500_tlb1_map(vcpu_e500, eaddr, gfn, gtlbe);
		break;
	}

	default:
		BUG();
		break;
	}
	write_host_tlbe(vcpu_e500, stlbsel, sesel);
}

int kvmppc_e500_tlb_search(struct kvm_vcpu *vcpu,
				gva_t eaddr, unsigned int pid, int as)
{
	struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);
	int esel, tlbsel;

	for (tlbsel = 0; tlbsel < 2; tlbsel++) {
		esel = kvmppc_e500_tlb_index(vcpu_e500, eaddr, tlbsel, pid, as);
		if (esel >= 0)
			return index_of(tlbsel, esel);
	}

	return -1;
}

void kvmppc_set_pid(struct kvm_vcpu *vcpu, u32 pid)
{
	struct kvmppc_vcpu_e500 *vcpu_e500 = to_e500(vcpu);

	vcpu_e500->pid[0] = vcpu->arch.shadow_pid =
		vcpu->arch.pid = pid;
}

void kvmppc_e500_tlb_setup(struct kvmppc_vcpu_e500 *vcpu_e500)
{
	struct tlbe *tlbe;

	/* Insert large initial mapping for guest. */
	tlbe = &vcpu_e500->guest_tlb[1][0];
	tlbe->mas1 = MAS1_VALID | MAS1_TSIZE(BOOK3E_PAGESZ_256M);
	tlbe->mas2 = 0;
	tlbe->mas3 = E500_TLB_SUPER_PERM_MASK;
	tlbe->mas7 = 0;

	/* 4K map for serial output. Used by kernel wrapper. */
	tlbe = &vcpu_e500->guest_tlb[1][1];
	tlbe->mas1 = MAS1_VALID | MAS1_TSIZE(BOOK3E_PAGESZ_4K);
	tlbe->mas2 = (0xe0004500 & 0xFFFFF000) | MAS2_I | MAS2_G;
	tlbe->mas3 = (0xe0004500 & 0xFFFFF000) | E500_TLB_SUPER_PERM_MASK;
	tlbe->mas7 = 0;
}

int kvmppc_e500_tlb_init(struct kvmppc_vcpu_e500 *vcpu_e500)
{
	tlb1_entry_num = mfspr(SPRN_TLB1CFG) & 0xFFF;

	vcpu_e500->guest_tlb_size[0] = KVM_E500_TLB0_SIZE;
	vcpu_e500->guest_tlb[0] =
		kzalloc(sizeof(struct tlbe) * KVM_E500_TLB0_SIZE, GFP_KERNEL);
	if (vcpu_e500->guest_tlb[0] == NULL)
		goto err_out;

	vcpu_e500->shadow_tlb_size[0] = KVM_E500_TLB0_SIZE;
	vcpu_e500->shadow_tlb[0] =
		kzalloc(sizeof(struct tlbe) * KVM_E500_TLB0_SIZE, GFP_KERNEL);
	if (vcpu_e500->shadow_tlb[0] == NULL)
		goto err_out_guest0;

	vcpu_e500->guest_tlb_size[1] = KVM_E500_TLB1_SIZE;
	vcpu_e500->guest_tlb[1] =
		kzalloc(sizeof(struct tlbe) * KVM_E500_TLB1_SIZE, GFP_KERNEL);
	if (vcpu_e500->guest_tlb[1] == NULL)
		goto err_out_shadow0;

	vcpu_e500->shadow_tlb_size[1] = tlb1_entry_num;
	vcpu_e500->shadow_tlb[1] =
		kzalloc(sizeof(struct tlbe) * tlb1_entry_num, GFP_KERNEL);
	if (vcpu_e500->shadow_tlb[1] == NULL)
		goto err_out_guest1;

	/* Init TLB configuration register */
	vcpu_e500->tlb0cfg = mfspr(SPRN_TLB0CFG) & ~0xfffUL;
	vcpu_e500->tlb0cfg |= vcpu_e500->guest_tlb_size[0];
	vcpu_e500->tlb1cfg = mfspr(SPRN_TLB1CFG) & ~0xfffUL;
	vcpu_e500->tlb1cfg |= vcpu_e500->guest_tlb_size[1];

	return 0;

err_out_guest1:
	kfree(vcpu_e500->guest_tlb[1]);
err_out_shadow0:
	kfree(vcpu_e500->shadow_tlb[0]);
err_out_guest0:
	kfree(vcpu_e500->guest_tlb[0]);
err_out:
	return -1;
}

void kvmppc_e500_tlb_uninit(struct kvmppc_vcpu_e500 *vcpu_e500)
{
	kfree(vcpu_e500->shadow_tlb[1]);
	kfree(vcpu_e500->guest_tlb[1]);
	kfree(vcpu_e500->shadow_tlb[0]);
	kfree(vcpu_e500->guest_tlb[0]);
}
