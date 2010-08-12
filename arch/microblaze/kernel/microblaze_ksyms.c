/*
 * Copyright (C) 2008-2009 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2008-2009 PetaLogix
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/cryptohash.h>
#include <linux/delay.h>
#include <linux/in6.h>
#include <linux/syscalls.h>

#include <asm/checksum.h>
#include <linux/io.h>
#include <asm/page.h>
#include <asm/system.h>
#include <linux/ftrace.h>
#include <linux/uaccess.h>

extern char *_ebss;
EXPORT_SYMBOL_GPL(_ebss);
#ifdef CONFIG_FUNCTION_TRACER
extern void _mcount(void);
EXPORT_SYMBOL(_mcount);
#endif

/*
 * Assembly functions that may be used (directly or indirectly) by modules
 */
EXPORT_SYMBOL(__copy_tofrom_user);
EXPORT_SYMBOL(__strncpy_user);

#ifdef CONFIG_OPT_LIB_ASM
EXPORT_SYMBOL(memcpy);
EXPORT_SYMBOL(memmove);
#endif
