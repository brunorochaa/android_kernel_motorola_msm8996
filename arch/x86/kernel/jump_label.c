/*
 * jump label x86 support
 *
 * Copyright (C) 2009 Jason Baron <jbaron@redhat.com>
 *
 */
#include <linux/jump_label.h>
#include <linux/memory.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/jhash.h>
#include <linux/cpu.h>
#include <asm/kprobes.h>
#include <asm/alternative.h>

#ifdef HAVE_JUMP_LABEL

union jump_code_union {
	char code[JUMP_LABEL_NOP_SIZE];
	struct {
		char jump;
		int offset;
	} __attribute__((packed));
};

static void __jump_label_transform(struct jump_entry *entry,
				   enum jump_label_type type,
				   void *(*poker)(void *, const void *, size_t))
{
	union jump_code_union code;

	if (type == JUMP_LABEL_ENABLE) {
		code.jump = 0xe9;
		code.offset = entry->target -
				(entry->code + JUMP_LABEL_NOP_SIZE);
	} else
		memcpy(&code, ideal_nops[NOP_ATOMIC5], JUMP_LABEL_NOP_SIZE);

	(*poker)((void *)entry->code, &code, JUMP_LABEL_NOP_SIZE);
}

void arch_jump_label_transform(struct jump_entry *entry,
			       enum jump_label_type type)
{
	get_online_cpus();
	mutex_lock(&text_mutex);
	__jump_label_transform(entry, type, text_poke_smp);
	mutex_unlock(&text_mutex);
	put_online_cpus();
}

static enum {
	JL_STATE_START,
	JL_STATE_NO_UPDATE,
	JL_STATE_UPDATE,
} jlstate __initdata_or_module = JL_STATE_START;

__init_or_module void arch_jump_label_transform_static(struct jump_entry *entry,
				      enum jump_label_type type)
{
	/*
	 * This function is called at boot up and when modules are
	 * first loaded. Check if the default nop, the one that is
	 * inserted at compile time, is the ideal nop. If it is, then
	 * we do not need to update the nop, and we can leave it as is.
	 * If it is not, then we need to update the nop to the ideal nop.
	 */
	if (jlstate == JL_STATE_START) {
		const unsigned char default_nop[] = { STATIC_KEY_INIT_NOP };
		const unsigned char *ideal_nop = ideal_nops[NOP_ATOMIC5];

		if (memcmp(ideal_nop, default_nop, 5) != 0)
			jlstate = JL_STATE_UPDATE;
		else
			jlstate = JL_STATE_NO_UPDATE;
	}
	if (jlstate == JL_STATE_UPDATE)
		__jump_label_transform(entry, type, text_poke_early);
}

#endif
