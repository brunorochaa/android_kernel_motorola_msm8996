/*
 *	Macintosh interrupts
 *
 * General design:
 * In contrary to the Amiga and Atari platforms, the Mac hardware seems to
 * exclusively use the autovector interrupts (the 'generic level0-level7'
 * interrupts with exception vectors 0x19-0x1f). The following interrupt levels
 * are used:
 *	1	- VIA1
 *		  - slot 0: one second interrupt (CA2)
 *		  - slot 1: VBlank (CA1)
 *		  - slot 2: ADB data ready (SR full)
 *		  - slot 3: ADB data  (CB2)
 *		  - slot 4: ADB clock (CB1)
 *		  - slot 5: timer 2
 *		  - slot 6: timer 1
 *		  - slot 7: status of IRQ; signals 'any enabled int.'
 *
 *	2	- VIA2 or RBV
 *		  - slot 0: SCSI DRQ (CA2)
 *		  - slot 1: NUBUS IRQ (CA1) need to read port A to find which
 *		  - slot 2: /EXP IRQ (only on IIci)
 *		  - slot 3: SCSI IRQ (CB2)
 *		  - slot 4: ASC IRQ (CB1)
 *		  - slot 5: timer 2 (not on IIci)
 *		  - slot 6: timer 1 (not on IIci)
 *		  - slot 7: status of IRQ; signals 'any enabled int.'
 *
 *	2	- OSS (IIfx only?)
 *		  - slot 0: SCSI interrupt
 *		  - slot 1: Sound interrupt
 *
 * Levels 3-6 vary by machine type. For VIA or RBV Macintoshes:
 *
 *	3	- unused (?)
 *
 *	4	- SCC
 *
 *	5	- unused (?)
 *		  [serial errors or special conditions seem to raise level 6
 *		  interrupts on some models (LC4xx?)]
 *
 *	6	- off switch (?)
 *
 * For OSS Macintoshes (IIfx only at this point):
 *
 *	3	- Nubus interrupt
 *		  - slot 0: Slot $9
 *		  - slot 1: Slot $A
 *		  - slot 2: Slot $B
 *		  - slot 3: Slot $C
 *		  - slot 4: Slot $D
 *		  - slot 5: Slot $E
 *
 *	4	- SCC IOP
 *
 *	5	- ISM IOP (ADB?)
 *
 *	6	- unused
 *
 * For PSC Macintoshes (660AV, 840AV):
 *
 *	3	- PSC level 3
 *		  - slot 0: MACE
 *
 *	4	- PSC level 4
 *		  - slot 1: SCC channel A interrupt
 *		  - slot 2: SCC channel B interrupt
 *		  - slot 3: MACE DMA
 *
 *	5	- PSC level 5
 *
 *	6	- PSC level 6
 *
 * Finally we have good 'ole level 7, the non-maskable interrupt:
 *
 *	7	- NMI (programmer's switch on the back of some Macs)
 *		  Also RAM parity error on models which support it (IIc, IIfx?)
 *
 * The current interrupt logic looks something like this:
 *
 * - We install dispatchers for the autovector interrupts (1-7). These
 *   dispatchers are responsible for querying the hardware (the
 *   VIA/RBV/OSS/PSC chips) to determine the actual interrupt source. Using
 *   this information a machspec interrupt number is generated by placing the
 *   index of the interrupt hardware into the low three bits and the original
 *   autovector interrupt number in the upper 5 bits. The handlers for the
 *   resulting machspec interrupt are then called.
 *
 * - Nubus is a special case because its interrupts are hidden behind two
 *   layers of hardware. Nubus interrupts come in as index 1 on VIA #2,
 *   which translates to IRQ number 17. In this spot we install _another_
 *   dispatcher. This dispatcher finds the interrupting slot number (9-F) and
 *   then forms a new machspec interrupt number as above with the slot number
 *   minus 9 in the low three bits and the pseudo-level 7 in the upper five
 *   bits.  The handlers for this new machspec interrupt number are then
 *   called. This puts Nubus interrupts into the range 56-62.
 *
 * - The Baboon interrupts (used on some PowerBooks) are an even more special
 *   case. They're hidden behind the Nubus slot $C interrupt thus adding a
 *   third layer of indirection. Why oh why did the Apple engineers do that?
 *
 * - We support "fast" and "slow" handlers, just like the Amiga port. The
 *   fast handlers are called first and with all interrupts disabled. They
 *   are expected to execute quickly (hence the name). The slow handlers are
 *   called last with interrupts enabled and the interrupt level restored.
 *   They must therefore be reentrant.
 *
 *   TODO:
 *
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/interrupt.h> /* for intr_count */
#include <linux/delay.h>
#include <linux/seq_file.h>

#include <asm/system.h>
#include <asm/irq.h>
#include <asm/traps.h>
#include <asm/bootinfo.h>
#include <asm/macintosh.h>
#include <asm/mac_via.h>
#include <asm/mac_psc.h>
#include <asm/hwtest.h>
#include <asm/errno.h>
#include <asm/macints.h>
#include <asm/irq_regs.h>
#include <asm/mac_oss.h>

#define SHUTUP_SONIC

/*
 * VIA/RBV hooks
 */

extern void via_register_interrupts(void);
extern void via_irq_enable(int);
extern void via_irq_disable(int);
extern void via_irq_clear(int);
extern int  via_irq_pending(int);

/*
 * OSS hooks
 */

extern void oss_register_interrupts(void);
extern void oss_irq_enable(int);
extern void oss_irq_disable(int);
extern void oss_irq_clear(int);
extern int  oss_irq_pending(int);

/*
 * PSC hooks
 */

extern void psc_register_interrupts(void);
extern void psc_irq_enable(int);
extern void psc_irq_disable(int);
extern void psc_irq_clear(int);
extern int  psc_irq_pending(int);

/*
 * IOP hooks
 */

extern void iop_register_interrupts(void);

/*
 * Baboon hooks
 */

extern int baboon_present;

extern void baboon_register_interrupts(void);
extern void baboon_irq_enable(int);
extern void baboon_irq_disable(int);
extern void baboon_irq_clear(int);

/*
 * console_loglevel determines NMI handler function
 */

irqreturn_t mac_nmi_handler(int, void *);
irqreturn_t mac_debug_handler(int, void *);

/* #define DEBUG_MACINTS */

void mac_enable_irq(unsigned int irq);
void mac_disable_irq(unsigned int irq);

static struct irq_controller mac_irq_controller = {
	.name		= "mac",
	.lock		= __SPIN_LOCK_UNLOCKED(mac_irq_controller.lock),
	.enable		= mac_enable_irq,
	.disable	= mac_disable_irq,
};

void __init mac_init_IRQ(void)
{
#ifdef DEBUG_MACINTS
	printk("mac_init_IRQ(): Setting things up...\n");
#endif
	m68k_setup_irq_controller(&mac_irq_controller, IRQ_USER,
				  NUM_MAC_SOURCES - IRQ_USER);
	/* Make sure the SONIC interrupt is cleared or things get ugly */
#ifdef SHUTUP_SONIC
	printk("Killing onboard sonic... ");
	/* This address should hopefully be mapped already */
	if (hwreg_present((void*)(0x50f0a000))) {
		*(long *)(0x50f0a014) = 0x7fffL;
		*(long *)(0x50f0a010) = 0L;
	}
	printk("Done.\n");
#endif /* SHUTUP_SONIC */

	/*
	 * Now register the handlers for the master IRQ handlers
	 * at levels 1-7. Most of the work is done elsewhere.
	 */

	if (oss_present)
		oss_register_interrupts();
	else
		via_register_interrupts();
	if (psc_present)
		psc_register_interrupts();
	if (baboon_present)
		baboon_register_interrupts();
	iop_register_interrupts();
	if (request_irq(IRQ_AUTO_7, mac_nmi_handler, 0, "NMI",
			mac_nmi_handler))
		pr_err("Couldn't register NMI\n");
#ifdef DEBUG_MACINTS
	printk("mac_init_IRQ(): Done!\n");
#endif
}

/*
 *  mac_enable_irq - enable an interrupt source
 * mac_disable_irq - disable an interrupt source
 *   mac_clear_irq - clears a pending interrupt
 * mac_pending_irq - Returns the pending status of an IRQ (nonzero = pending)
 *
 * These routines are just dispatchers to the VIA/OSS/PSC routines.
 */

void mac_enable_irq(unsigned int irq)
{
	int irq_src = IRQ_SRC(irq);

	switch(irq_src) {
	case 1:
		via_irq_enable(irq);
		break;
	case 2:
	case 7:
		if (oss_present)
			oss_irq_enable(irq);
		else
			via_irq_enable(irq);
		break;
	case 3:
	case 5:
	case 6:
		if (psc_present)
			psc_irq_enable(irq);
		else if (oss_present)
			oss_irq_enable(irq);
		break;
	case 4:
		if (psc_present)
			psc_irq_enable(irq);
		break;
	case 8:
		if (baboon_present)
			baboon_irq_enable(irq);
		break;
	}
}

void mac_disable_irq(unsigned int irq)
{
	int irq_src = IRQ_SRC(irq);

	switch(irq_src) {
	case 1:
		via_irq_disable(irq);
		break;
	case 2:
	case 7:
		if (oss_present)
			oss_irq_disable(irq);
		else
			via_irq_disable(irq);
		break;
	case 3:
	case 5:
	case 6:
		if (psc_present)
			psc_irq_disable(irq);
		else if (oss_present)
			oss_irq_disable(irq);
		break;
	case 4:
		if (psc_present)
			psc_irq_disable(irq);
		break;
	case 8:
		if (baboon_present)
			baboon_irq_disable(irq);
		break;
	}
}

void mac_clear_irq(unsigned int irq)
{
	switch(IRQ_SRC(irq)) {
	case 1:
		via_irq_clear(irq);
		break;
	case 2:
	case 7:
		if (oss_present)
			oss_irq_clear(irq);
		else
			via_irq_clear(irq);
		break;
	case 3:
	case 5:
	case 6:
		if (psc_present)
			psc_irq_clear(irq);
		else if (oss_present)
			oss_irq_clear(irq);
		break;
	case 4:
		if (psc_present)
			psc_irq_clear(irq);
		break;
	case 8:
		if (baboon_present)
			baboon_irq_clear(irq);
		break;
	}
}

int mac_irq_pending(unsigned int irq)
{
	switch(IRQ_SRC(irq)) {
	case 1:
		return via_irq_pending(irq);
	case 2:
	case 7:
		if (oss_present)
			return oss_irq_pending(irq);
		else
			return via_irq_pending(irq);
	case 3:
	case 5:
	case 6:
		if (psc_present)
			return psc_irq_pending(irq);
		else if (oss_present)
			return oss_irq_pending(irq);
		break;
	case 4:
		if (psc_present)
			psc_irq_pending(irq);
		break;
	}
	return 0;
}
EXPORT_SYMBOL(mac_irq_pending);

static int num_debug[8];

irqreturn_t mac_debug_handler(int irq, void *dev_id)
{
	if (num_debug[irq] < 10) {
		printk("DEBUG: Unexpected IRQ %d\n", irq);
		num_debug[irq]++;
	}
	return IRQ_HANDLED;
}

static int in_nmi;
static volatile int nmi_hold;

irqreturn_t mac_nmi_handler(int irq, void *dev_id)
{
	int i;
	/*
	 * generate debug output on NMI switch if 'debug' kernel option given
	 * (only works with Penguin!)
	 */

	in_nmi++;
	for (i=0; i<100; i++)
		udelay(1000);

	if (in_nmi == 1) {
		nmi_hold = 1;
		printk("... pausing, press NMI to resume ...");
	} else {
		printk(" ok!\n");
		nmi_hold = 0;
	}

	barrier();

	while (nmi_hold == 1)
		udelay(1000);

	if (console_loglevel >= 8) {
#if 0
		struct pt_regs *fp = get_irq_regs();
		show_state();
		printk("PC: %08lx\nSR: %04x  SP: %p\n", fp->pc, fp->sr, fp);
		printk("d0: %08lx    d1: %08lx    d2: %08lx    d3: %08lx\n",
		       fp->d0, fp->d1, fp->d2, fp->d3);
		printk("d4: %08lx    d5: %08lx    a0: %08lx    a1: %08lx\n",
		       fp->d4, fp->d5, fp->a0, fp->a1);

		if (STACK_MAGIC != *(unsigned long *)current->kernel_stack_page)
			printk("Corrupted stack page\n");
		printk("Process %s (pid: %d, stackpage=%08lx)\n",
			current->comm, current->pid, current->kernel_stack_page);
		if (intr_count == 1)
			dump_stack((struct frame *)fp);
#else
		/* printk("NMI "); */
#endif
	}
	in_nmi--;
	return IRQ_HANDLED;
}
