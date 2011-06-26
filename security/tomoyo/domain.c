/*
 * security/tomoyo/domain.c
 *
 * Domain transition functions for TOMOYO.
 *
 * Copyright (C) 2005-2010  NTT DATA CORPORATION
 */

#include "common.h"
#include <linux/binfmts.h>
#include <linux/slab.h>

/* Variables definitions.*/

/* The global ACL referred by "use_group" keyword. */
struct list_head tomoyo_acl_group[TOMOYO_MAX_ACL_GROUPS];

/* The initial domain. */
struct tomoyo_domain_info tomoyo_kernel_domain;

/**
 * tomoyo_update_policy - Update an entry for exception policy.
 *
 * @new_entry:       Pointer to "struct tomoyo_acl_info".
 * @size:            Size of @new_entry in bytes.
 * @param:           Pointer to "struct tomoyo_acl_param".
 * @check_duplicate: Callback function to find duplicated entry.
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
int tomoyo_update_policy(struct tomoyo_acl_head *new_entry, const int size,
			 struct tomoyo_acl_param *param,
			 bool (*check_duplicate) (const struct tomoyo_acl_head
						  *,
						  const struct tomoyo_acl_head
						  *))
{
	int error = param->is_delete ? -ENOENT : -ENOMEM;
	struct tomoyo_acl_head *entry;
	struct list_head *list = param->list;

	if (mutex_lock_interruptible(&tomoyo_policy_lock))
		return -ENOMEM;
	list_for_each_entry_rcu(entry, list, list) {
		if (!check_duplicate(entry, new_entry))
			continue;
		entry->is_deleted = param->is_delete;
		error = 0;
		break;
	}
	if (error && !param->is_delete) {
		entry = tomoyo_commit_ok(new_entry, size);
		if (entry) {
			list_add_tail_rcu(&entry->list, list);
			error = 0;
		}
	}
	mutex_unlock(&tomoyo_policy_lock);
	return error;
}

/**
 * tomoyo_same_acl_head - Check for duplicated "struct tomoyo_acl_info" entry.
 *
 * @a: Pointer to "struct tomoyo_acl_info".
 * @b: Pointer to "struct tomoyo_acl_info".
 *
 * Returns true if @a == @b, false otherwise.
 */
static inline bool tomoyo_same_acl_head(const struct tomoyo_acl_info *a,
					const struct tomoyo_acl_info *b)
{
	return a->type == b->type;
}

/**
 * tomoyo_update_domain - Update an entry for domain policy.
 *
 * @new_entry:       Pointer to "struct tomoyo_acl_info".
 * @size:            Size of @new_entry in bytes.
 * @param:           Pointer to "struct tomoyo_acl_param".
 * @check_duplicate: Callback function to find duplicated entry.
 * @merge_duplicate: Callback function to merge duplicated entry.
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
int tomoyo_update_domain(struct tomoyo_acl_info *new_entry, const int size,
			 struct tomoyo_acl_param *param,
			 bool (*check_duplicate) (const struct tomoyo_acl_info
						  *,
						  const struct tomoyo_acl_info
						  *),
			 bool (*merge_duplicate) (struct tomoyo_acl_info *,
						  struct tomoyo_acl_info *,
						  const bool))
{
	const bool is_delete = param->is_delete;
	int error = is_delete ? -ENOENT : -ENOMEM;
	struct tomoyo_acl_info *entry;
	struct list_head * const list = param->list;

	if (mutex_lock_interruptible(&tomoyo_policy_lock))
		return error;
	list_for_each_entry_rcu(entry, list, list) {
		if (!tomoyo_same_acl_head(entry, new_entry) ||
		    !check_duplicate(entry, new_entry))
			continue;
		if (merge_duplicate)
			entry->is_deleted = merge_duplicate(entry, new_entry,
							    is_delete);
		else
			entry->is_deleted = is_delete;
		error = 0;
		break;
	}
	if (error && !is_delete) {
		entry = tomoyo_commit_ok(new_entry, size);
		if (entry) {
			list_add_tail_rcu(&entry->list, list);
			error = 0;
		}
	}
	mutex_unlock(&tomoyo_policy_lock);
	return error;
}

/**
 * tomoyo_check_acl - Do permission check.
 *
 * @r:           Pointer to "struct tomoyo_request_info".
 * @check_entry: Callback function to check type specific parameters.
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
void tomoyo_check_acl(struct tomoyo_request_info *r,
		      bool (*check_entry) (struct tomoyo_request_info *,
					   const struct tomoyo_acl_info *))
{
	const struct tomoyo_domain_info *domain = r->domain;
	struct tomoyo_acl_info *ptr;
	bool retried = false;
	const struct list_head *list = &domain->acl_info_list;

retry:
	list_for_each_entry_rcu(ptr, list, list) {
		if (ptr->is_deleted || ptr->type != r->param_type)
			continue;
		if (check_entry(r, ptr)) {
			r->granted = true;
			return;
		}
	}
	if (!retried) {
		retried = true;
		list = &tomoyo_acl_group[domain->group];
		goto retry;
	}
	r->granted = false;
}

/* The list for "struct tomoyo_domain_info". */
LIST_HEAD(tomoyo_domain_list);

struct list_head tomoyo_policy_list[TOMOYO_MAX_POLICY];
struct list_head tomoyo_group_list[TOMOYO_MAX_GROUP];

/**
 * tomoyo_last_word - Get last component of a domainname.
 *
 * @domainname: Domainname to check.
 *
 * Returns the last word of @domainname.
 */
static const char *tomoyo_last_word(const char *name)
{
        const char *cp = strrchr(name, ' ');
        if (cp)
                return cp + 1;
        return name;
}

/**
 * tomoyo_same_transition_control - Check for duplicated "struct tomoyo_transition_control" entry.
 *
 * @a: Pointer to "struct tomoyo_acl_head".
 * @b: Pointer to "struct tomoyo_acl_head".
 *
 * Returns true if @a == @b, false otherwise.
 */
static bool tomoyo_same_transition_control(const struct tomoyo_acl_head *a,
					   const struct tomoyo_acl_head *b)
{
	const struct tomoyo_transition_control *p1 = container_of(a,
								  typeof(*p1),
								  head);
	const struct tomoyo_transition_control *p2 = container_of(b,
								  typeof(*p2),
								  head);
	return p1->type == p2->type && p1->is_last_name == p2->is_last_name
		&& p1->domainname == p2->domainname
		&& p1->program == p2->program;
}

/**
 * tomoyo_write_transition_control - Write "struct tomoyo_transition_control" list.
 *
 * @param: Pointer to "struct tomoyo_acl_param".
 * @type:  Type of this entry.
 *
 * Returns 0 on success, negative value otherwise.
 */
int tomoyo_write_transition_control(struct tomoyo_acl_param *param,
				    const u8 type)
{
	struct tomoyo_transition_control e = { .type = type };
	int error = param->is_delete ? -ENOENT : -ENOMEM;
	char *program = param->data;
	char *domainname = strstr(program, " from ");
	if (domainname) {
		*domainname = '\0';
		domainname += 6;
	} else if (type == TOMOYO_TRANSITION_CONTROL_NO_KEEP ||
		   type == TOMOYO_TRANSITION_CONTROL_KEEP) {
		domainname = program;
		program = NULL;
	}
	if (program && strcmp(program, "any")) {
		if (!tomoyo_correct_path(program))
			return -EINVAL;
		e.program = tomoyo_get_name(program);
		if (!e.program)
			goto out;
	}
	if (domainname && strcmp(domainname, "any")) {
		if (!tomoyo_correct_domain(domainname)) {
			if (!tomoyo_correct_path(domainname))
				goto out;
			e.is_last_name = true;
		}
		e.domainname = tomoyo_get_name(domainname);
		if (!e.domainname)
			goto out;
	}
	param->list = &tomoyo_policy_list[TOMOYO_ID_TRANSITION_CONTROL];
	error = tomoyo_update_policy(&e.head, sizeof(e), param,
				     tomoyo_same_transition_control);
out:
	tomoyo_put_name(e.domainname);
	tomoyo_put_name(e.program);
	return error;
}

/**
 * tomoyo_transition_type - Get domain transition type.
 *
 * @domainname: The name of domain.
 * @program:    The name of program.
 *
 * Returns TOMOYO_TRANSITION_CONTROL_INITIALIZE if executing @program
 * reinitializes domain transition, TOMOYO_TRANSITION_CONTROL_KEEP if executing
 * @program suppresses domain transition, others otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
static u8 tomoyo_transition_type(const struct tomoyo_path_info *domainname,
				 const struct tomoyo_path_info *program)
{
	const struct tomoyo_transition_control *ptr;
	const char *last_name = tomoyo_last_word(domainname->name);
	u8 type;
	for (type = 0; type < TOMOYO_MAX_TRANSITION_TYPE; type++) {
 next:
		list_for_each_entry_rcu(ptr, &tomoyo_policy_list
					[TOMOYO_ID_TRANSITION_CONTROL],
					head.list) {
			if (ptr->head.is_deleted || ptr->type != type)
				continue;
			if (ptr->domainname) {
				if (!ptr->is_last_name) {
					if (ptr->domainname != domainname)
						continue;
				} else {
					/*
					 * Use direct strcmp() since this is
					 * unlikely used.
					 */
					if (strcmp(ptr->domainname->name,
						   last_name))
						continue;
				}
			}
			if (ptr->program &&
			    tomoyo_pathcmp(ptr->program, program))
				continue;
			if (type == TOMOYO_TRANSITION_CONTROL_NO_INITIALIZE) {
				/*
				 * Do not check for initialize_domain if
				 * no_initialize_domain matched.
				 */
				type = TOMOYO_TRANSITION_CONTROL_NO_KEEP;
				goto next;
			}
			goto done;
		}
	}
 done:
	return type;
}

/**
 * tomoyo_same_aggregator - Check for duplicated "struct tomoyo_aggregator" entry.
 *
 * @a: Pointer to "struct tomoyo_acl_head".
 * @b: Pointer to "struct tomoyo_acl_head".
 *
 * Returns true if @a == @b, false otherwise.
 */
static bool tomoyo_same_aggregator(const struct tomoyo_acl_head *a,
				   const struct tomoyo_acl_head *b)
{
	const struct tomoyo_aggregator *p1 = container_of(a, typeof(*p1),
							  head);
	const struct tomoyo_aggregator *p2 = container_of(b, typeof(*p2),
							  head);
	return p1->original_name == p2->original_name &&
		p1->aggregated_name == p2->aggregated_name;
}

/**
 * tomoyo_write_aggregator - Write "struct tomoyo_aggregator" list.
 *
 * @param: Pointer to "struct tomoyo_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
int tomoyo_write_aggregator(struct tomoyo_acl_param *param)
{
	struct tomoyo_aggregator e = { };
	int error = param->is_delete ? -ENOENT : -ENOMEM;
	const char *original_name = tomoyo_read_token(param);
	const char *aggregated_name = tomoyo_read_token(param);
	if (!tomoyo_correct_word(original_name) ||
	    !tomoyo_correct_path(aggregated_name))
		return -EINVAL;
	e.original_name = tomoyo_get_name(original_name);
	e.aggregated_name = tomoyo_get_name(aggregated_name);
	if (!e.original_name || !e.aggregated_name ||
	    e.aggregated_name->is_patterned) /* No patterns allowed. */
		goto out;
	param->list = &tomoyo_policy_list[TOMOYO_ID_AGGREGATOR];
	error = tomoyo_update_policy(&e.head, sizeof(e), param,
				     tomoyo_same_aggregator);
out:
	tomoyo_put_name(e.original_name);
	tomoyo_put_name(e.aggregated_name);
	return error;
}

/**
 * tomoyo_assign_domain - Create a domain.
 *
 * @domainname: The name of domain.
 * @profile:    Profile number to assign if the domain was newly created.
 *
 * Returns pointer to "struct tomoyo_domain_info" on success, NULL otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
struct tomoyo_domain_info *tomoyo_assign_domain(const char *domainname,
						const u8 profile)
{
	struct tomoyo_domain_info *entry;
	struct tomoyo_domain_info *domain = NULL;
	const struct tomoyo_path_info *saved_domainname;
	bool found = false;

	if (!tomoyo_correct_domain(domainname))
		return NULL;
	saved_domainname = tomoyo_get_name(domainname);
	if (!saved_domainname)
		return NULL;
	entry = kzalloc(sizeof(*entry), GFP_NOFS);
	if (mutex_lock_interruptible(&tomoyo_policy_lock))
		goto out;
	list_for_each_entry_rcu(domain, &tomoyo_domain_list, list) {
		if (domain->is_deleted ||
		    tomoyo_pathcmp(saved_domainname, domain->domainname))
			continue;
		found = true;
		break;
	}
	if (!found && tomoyo_memory_ok(entry)) {
		INIT_LIST_HEAD(&entry->acl_info_list);
		entry->domainname = saved_domainname;
		saved_domainname = NULL;
		entry->profile = profile;
		list_add_tail_rcu(&entry->list, &tomoyo_domain_list);
		domain = entry;
		entry = NULL;
		found = true;
	}
	mutex_unlock(&tomoyo_policy_lock);
 out:
	tomoyo_put_name(saved_domainname);
	kfree(entry);
	return found ? domain : NULL;
}

/**
 * tomoyo_find_next_domain - Find a domain.
 *
 * @bprm: Pointer to "struct linux_binprm".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
int tomoyo_find_next_domain(struct linux_binprm *bprm)
{
	struct tomoyo_request_info r;
	char *tmp = kzalloc(TOMOYO_EXEC_TMPSIZE, GFP_NOFS);
	struct tomoyo_domain_info *old_domain = tomoyo_domain();
	struct tomoyo_domain_info *domain = NULL;
	const char *original_name = bprm->filename;
	u8 mode;
	bool is_enforce;
	int retval = -ENOMEM;
	bool need_kfree = false;
	struct tomoyo_path_info rn = { }; /* real name */

	mode = tomoyo_init_request_info(&r, NULL, TOMOYO_MAC_FILE_EXECUTE);
	is_enforce = (mode == TOMOYO_CONFIG_ENFORCING);
	if (!tmp)
		goto out;

 retry:
	if (need_kfree) {
		kfree(rn.name);
		need_kfree = false;
	}
	/* Get symlink's pathname of program. */
	retval = -ENOENT;
	rn.name = tomoyo_realpath_nofollow(original_name);
	if (!rn.name)
		goto out;
	tomoyo_fill_path_info(&rn);
	need_kfree = true;

	/* Check 'aggregator' directive. */
	{
		struct tomoyo_aggregator *ptr;
		list_for_each_entry_rcu(ptr, &tomoyo_policy_list
					[TOMOYO_ID_AGGREGATOR], head.list) {
			if (ptr->head.is_deleted ||
			    !tomoyo_path_matches_pattern(&rn,
							 ptr->original_name))
				continue;
			kfree(rn.name);
			need_kfree = false;
			/* This is OK because it is read only. */
			rn = *ptr->aggregated_name;
			break;
		}
	}

	/* Check execute permission. */
	retval = tomoyo_path_permission(&r, TOMOYO_TYPE_EXECUTE, &rn);
	if (retval == TOMOYO_RETRY_REQUEST)
		goto retry;
	if (retval < 0)
		goto out;
	/*
	 * To be able to specify domainnames with wildcards, use the
	 * pathname specified in the policy (which may contain
	 * wildcard) rather than the pathname passed to execve()
	 * (which never contains wildcard).
	 */
	if (r.param.path.matched_path) {
		if (need_kfree)
			kfree(rn.name);
		need_kfree = false;
		/* This is OK because it is read only. */
		rn = *r.param.path.matched_path;
	}

	/* Calculate domain to transit to. */
	switch (tomoyo_transition_type(old_domain->domainname, &rn)) {
	case TOMOYO_TRANSITION_CONTROL_INITIALIZE:
		/* Transit to the child of tomoyo_kernel_domain domain. */
		snprintf(tmp, TOMOYO_EXEC_TMPSIZE - 1, TOMOYO_ROOT_NAME " "
			 "%s", rn.name);
		break;
	case TOMOYO_TRANSITION_CONTROL_KEEP:
		/* Keep current domain. */
		domain = old_domain;
		break;
	default:
		if (old_domain == &tomoyo_kernel_domain &&
		    !tomoyo_policy_loaded) {
			/*
			 * Needn't to transit from kernel domain before
			 * starting /sbin/init. But transit from kernel domain
			 * if executing initializers because they might start
			 * before /sbin/init.
			 */
			domain = old_domain;
		} else {
			/* Normal domain transition. */
			snprintf(tmp, TOMOYO_EXEC_TMPSIZE - 1, "%s %s",
				 old_domain->domainname->name, rn.name);
		}
		break;
	}
	if (domain || strlen(tmp) >= TOMOYO_EXEC_TMPSIZE - 10)
		goto done;
	domain = tomoyo_find_domain(tmp);
	if (!domain)
		domain = tomoyo_assign_domain(tmp, old_domain->profile);
 done:
	if (domain)
		goto out;
	printk(KERN_WARNING "TOMOYO-ERROR: Domain '%s' not defined.\n", tmp);
	if (is_enforce)
		retval = -EPERM;
	else
		old_domain->transition_failed = true;
 out:
	if (!domain)
		domain = old_domain;
	/* Update reference count on "struct tomoyo_domain_info". */
	atomic_inc(&domain->users);
	bprm->cred->security = domain;
	if (need_kfree)
		kfree(rn.name);
	kfree(tmp);
	return retval;
}
