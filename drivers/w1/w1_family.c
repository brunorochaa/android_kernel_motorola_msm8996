/*
 *	w1_family.c
 *
 * Copyright (c) 2004 Evgeniy Polyakov <johnpol@2ka.mipt.ru>
 *
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/delay.h>

#include "w1_family.h"

DEFINE_SPINLOCK(w1_flock);
static LIST_HEAD(w1_families);

static int w1_check_family(struct w1_family *f)
{
	if (!f->fops->rname || !f->fops->rbin)
		return -EINVAL;

	return 0;
}

int w1_register_family(struct w1_family *newf)
{
	struct list_head *ent, *n;
	struct w1_family *f;
	int ret = 0;

	if (w1_check_family(newf))
		return -EINVAL;

	spin_lock(&w1_flock);
	list_for_each_safe(ent, n, &w1_families) {
		f = list_entry(ent, struct w1_family, family_entry);

		if (f->fid == newf->fid) {
			ret = -EEXIST;
			break;
		}
	}

	if (!ret) {
		atomic_set(&newf->refcnt, 0);
		newf->need_exit = 0;
		list_add_tail(&newf->family_entry, &w1_families);
	}
	spin_unlock(&w1_flock);

	w1_reconnect_slaves(newf);

	return ret;
}

void w1_unregister_family(struct w1_family *fent)
{
	struct list_head *ent, *n;
	struct w1_family *f;

	spin_lock(&w1_flock);
	list_for_each_safe(ent, n, &w1_families) {
		f = list_entry(ent, struct w1_family, family_entry);

		if (f->fid == fent->fid) {
			list_del(&fent->family_entry);
			break;
		}
	}

	fent->need_exit = 1;

	spin_unlock(&w1_flock);

	while (atomic_read(&fent->refcnt)) {
		printk(KERN_INFO "Waiting for family %u to become free: refcnt=%d.\n",
				fent->fid, atomic_read(&fent->refcnt));

		if (msleep_interruptible(1000))
			flush_signals(current);
	}
}

/*
 * Should be called under w1_flock held.
 */
struct w1_family * w1_family_registered(u8 fid)
{
	struct list_head *ent, *n;
	struct w1_family *f = NULL;
	int ret = 0;

	list_for_each_safe(ent, n, &w1_families) {
		f = list_entry(ent, struct w1_family, family_entry);

		if (f->fid == fid) {
			ret = 1;
			break;
		}
	}

	return (ret) ? f : NULL;
}

void w1_family_put(struct w1_family *f)
{
	spin_lock(&w1_flock);
	__w1_family_put(f);
	spin_unlock(&w1_flock);
}

void __w1_family_put(struct w1_family *f)
{
	if (atomic_dec_and_test(&f->refcnt))
		f->need_exit = 1;
}

void w1_family_get(struct w1_family *f)
{
	spin_lock(&w1_flock);
	__w1_family_get(f);
	spin_unlock(&w1_flock);

}

void __w1_family_get(struct w1_family *f)
{
	smp_mb__before_atomic_inc();
	atomic_inc(&f->refcnt);
	smp_mb__after_atomic_inc();
}

EXPORT_SYMBOL(w1_family_get);
EXPORT_SYMBOL(w1_family_put);
EXPORT_SYMBOL(w1_family_registered);
EXPORT_SYMBOL(w1_unregister_family);
EXPORT_SYMBOL(w1_register_family);
