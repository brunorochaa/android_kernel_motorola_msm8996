#ifndef __NETNS_XFRM_H
#define __NETNS_XFRM_H

#include <linux/list.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

struct netns_xfrm {
	struct list_head	state_all;
	/*
	 * Hash table to find appropriate SA towards given target (endpoint of
	 * tunnel or destination of transport mode) allowed by selector.
	 *
	 * Main use is finding SA after policy selected tunnel or transport
	 * mode. Also, it can be used by ah/esp icmp error handler to find
	 * offending SA.
	 */
	struct hlist_head	*state_bydst;
	struct hlist_head	*state_bysrc;
	struct hlist_head	*state_byspi;
	unsigned int		state_hmask;
	unsigned int		state_num;
	struct work_struct	state_hash_work;
	struct hlist_head	state_gc_list;
	struct work_struct	state_gc_work;

	wait_queue_head_t	km_waitq;
};

#endif
