/*
 * resource cgroups
 *
 * Copyright 2007 OpenVZ SWsoft Inc
 *
 * Author: Pavel Emelianov <xemul@openvz.org>
 *
 */

#include <linux/types.h>
#include <linux/parser.h>
#include <linux/fs.h>
#include <linux/res_counter.h>
#include <linux/uaccess.h>

void res_counter_init(struct res_counter *counter)
{
	spin_lock_init(&counter->lock);
	counter->limit = (unsigned long)LONG_MAX;
}

int res_counter_charge_locked(struct res_counter *counter, unsigned long val)
{
	if (counter->usage + val > counter->limit) {
		counter->failcnt++;
		return -ENOMEM;
	}

	counter->usage += val;
	return 0;
}

int res_counter_charge(struct res_counter *counter, unsigned long val)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&counter->lock, flags);
	ret = res_counter_charge_locked(counter, val);
	spin_unlock_irqrestore(&counter->lock, flags);
	return ret;
}

void res_counter_uncharge_locked(struct res_counter *counter, unsigned long val)
{
	if (WARN_ON(counter->usage < val))
		val = counter->usage;

	counter->usage -= val;
}

void res_counter_uncharge(struct res_counter *counter, unsigned long val)
{
	unsigned long flags;

	spin_lock_irqsave(&counter->lock, flags);
	res_counter_uncharge_locked(counter, val);
	spin_unlock_irqrestore(&counter->lock, flags);
}


static inline unsigned long *res_counter_member(struct res_counter *counter,
						int member)
{
	switch (member) {
	case RES_USAGE:
		return &counter->usage;
	case RES_LIMIT:
		return &counter->limit;
	case RES_FAILCNT:
		return &counter->failcnt;
	};

	BUG();
	return NULL;
}

ssize_t res_counter_read(struct res_counter *counter, int member,
		const char __user *userbuf, size_t nbytes, loff_t *pos)
{
	unsigned long *val;
	char buf[64], *s;

	s = buf;
	val = res_counter_member(counter, member);
	s += sprintf(s, "%lu\n", *val);
	return simple_read_from_buffer((void __user *)userbuf, nbytes,
			pos, buf, s - buf);
}

ssize_t res_counter_write(struct res_counter *counter, int member,
		const char __user *userbuf, size_t nbytes, loff_t *pos)
{
	int ret;
	char *buf, *end;
	unsigned long tmp, *val;

	buf = kmalloc(nbytes + 1, GFP_KERNEL);
	ret = -ENOMEM;
	if (buf == NULL)
		goto out;

	buf[nbytes] = '\0';
	ret = -EFAULT;
	if (copy_from_user(buf, userbuf, nbytes))
		goto out_free;

	ret = -EINVAL;
	tmp = simple_strtoul(buf, &end, 10);
	if (*end != '\0')
		goto out_free;

	val = res_counter_member(counter, member);
	*val = tmp;
	ret = nbytes;
out_free:
	kfree(buf);
out:
	return ret;
}
