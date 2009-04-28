/*
 * trace_events_filter - generic event filtering
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) 2009 Tom Zanussi <tzanussi@gmail.com>
 */

#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/mutex.h>

#include "trace.h"
#include "trace_output.h"

static DEFINE_MUTEX(filter_mutex);

static int filter_pred_64(struct filter_pred *pred, void *event)
{
	u64 *addr = (u64 *)(event + pred->offset);
	u64 val = (u64)pred->val;
	int match;

	match = (val == *addr) ^ pred->not;

	return match;
}

static int filter_pred_32(struct filter_pred *pred, void *event)
{
	u32 *addr = (u32 *)(event + pred->offset);
	u32 val = (u32)pred->val;
	int match;

	match = (val == *addr) ^ pred->not;

	return match;
}

static int filter_pred_16(struct filter_pred *pred, void *event)
{
	u16 *addr = (u16 *)(event + pred->offset);
	u16 val = (u16)pred->val;
	int match;

	match = (val == *addr) ^ pred->not;

	return match;
}

static int filter_pred_8(struct filter_pred *pred, void *event)
{
	u8 *addr = (u8 *)(event + pred->offset);
	u8 val = (u8)pred->val;
	int match;

	match = (val == *addr) ^ pred->not;

	return match;
}

static int filter_pred_string(struct filter_pred *pred, void *event)
{
	char *addr = (char *)(event + pred->offset);
	int cmp, match;

	cmp = strncmp(addr, pred->str_val, pred->str_len);

	match = (!cmp) ^ pred->not;

	return match;
}

static int filter_pred_none(struct filter_pred *pred, void *event)
{
	return 0;
}

/* return 1 if event matches, 0 otherwise (discard) */
int filter_match_preds(struct ftrace_event_call *call, void *rec)
{
	struct event_filter *filter = call->filter;
	int i, matched, and_failed = 0;
	struct filter_pred *pred;

	for (i = 0; i < filter->n_preds; i++) {
		pred = filter->preds[i];
		if (and_failed && !pred->or)
			continue;
		matched = pred->fn(pred, rec);
		if (!matched && !pred->or) {
			and_failed = 1;
			continue;
		} else if (matched && pred->or)
			return 1;
	}

	if (and_failed)
		return 0;

	return 1;
}
EXPORT_SYMBOL_GPL(filter_match_preds);

static void __filter_print_preds(struct event_filter *filter,
				 struct trace_seq *s)
{
	struct filter_pred *pred;
	char *field_name;
	int i;

	if (!filter || !filter->n_preds) {
		trace_seq_printf(s, "none\n");
		return;
	}

	for (i = 0; i < filter->n_preds; i++) {
		pred = filter->preds[i];
		field_name = pred->field_name;
		if (i)
			trace_seq_printf(s, pred->or ? "|| " : "&& ");
		trace_seq_printf(s, "%s ", field_name);
		trace_seq_printf(s, pred->not ? "!= " : "== ");
		if (pred->str_len)
			trace_seq_printf(s, "%s\n", pred->str_val);
		else
			trace_seq_printf(s, "%llu\n", pred->val);
	}
}

void filter_print_preds(struct ftrace_event_call *call, struct trace_seq *s)
{
	mutex_lock(&filter_mutex);
	__filter_print_preds(call->filter, s);
	mutex_unlock(&filter_mutex);
}

void filter_print_subsystem_preds(struct event_subsystem *system,
				  struct trace_seq *s)
{
	mutex_lock(&filter_mutex);
	__filter_print_preds(system->filter, s);
	mutex_unlock(&filter_mutex);
}

static struct ftrace_event_field *
find_event_field(struct ftrace_event_call *call, char *name)
{
	struct ftrace_event_field *field;

	list_for_each_entry(field, &call->fields, link) {
		if (!strcmp(field->name, name))
			return field;
	}

	return NULL;
}

void filter_free_pred(struct filter_pred *pred)
{
	if (!pred)
		return;

	kfree(pred->field_name);
	kfree(pred);
}

static void filter_clear_pred(struct filter_pred *pred)
{
	kfree(pred->field_name);
	pred->field_name = NULL;
	pred->str_len = 0;
}

static int filter_set_pred(struct filter_pred *dest,
			   struct filter_pred *src,
			   filter_pred_fn_t fn)
{
	*dest = *src;
	dest->field_name = kstrdup(src->field_name, GFP_KERNEL);
	if (!dest->field_name)
		return -ENOMEM;
	dest->fn = fn;

	return 0;
}

static void __filter_disable_preds(struct ftrace_event_call *call)
{
	struct event_filter *filter = call->filter;
	int i;

	call->filter_active = 0;
	filter->n_preds = 0;

	for (i = 0; i < MAX_FILTER_PRED; i++)
		filter->preds[i]->fn = filter_pred_none;
}

void filter_disable_preds(struct ftrace_event_call *call)
{
	mutex_lock(&filter_mutex);
	__filter_disable_preds(call);
	mutex_unlock(&filter_mutex);
}

int init_preds(struct ftrace_event_call *call)
{
	struct event_filter *filter;
	struct filter_pred *pred;
	int i;

	filter = call->filter = kzalloc(sizeof(*filter), GFP_KERNEL);
	if (!call->filter)
		return -ENOMEM;

	call->filter_active = 0;
	filter->n_preds = 0;

	filter->preds = kzalloc(MAX_FILTER_PRED * sizeof(pred), GFP_KERNEL);
	if (!filter->preds)
		goto oom;

	for (i = 0; i < MAX_FILTER_PRED; i++) {
		pred = kzalloc(sizeof(*pred), GFP_KERNEL);
		if (!pred)
			goto oom;
		pred->fn = filter_pred_none;
		filter->preds[i] = pred;
	}

	return 0;

oom:
	for (i = 0; i < MAX_FILTER_PRED; i++) {
		if (filter->preds[i])
			filter_free_pred(filter->preds[i]);
	}
	kfree(filter->preds);
	kfree(call->filter);
	call->filter = NULL;

	return -ENOMEM;
}
EXPORT_SYMBOL_GPL(init_preds);

static void __filter_free_subsystem_preds(struct event_subsystem *system)
{
	struct event_filter *filter = system->filter;
	struct ftrace_event_call *call;
	int i;

	if (filter && filter->n_preds) {
		for (i = 0; i < filter->n_preds; i++)
			filter_free_pred(filter->preds[i]);
		kfree(filter->preds);
		kfree(filter);
		system->filter = NULL;
	}

	list_for_each_entry(call, &ftrace_events, list) {
		if (!call->define_fields)
			continue;

		if (!strcmp(call->system, system->name))
			__filter_disable_preds(call);
	}
}

void filter_free_subsystem_preds(struct event_subsystem *system)
{
	mutex_lock(&filter_mutex);
	__filter_free_subsystem_preds(system);
	mutex_unlock(&filter_mutex);
}

static int filter_add_pred_fn(struct ftrace_event_call *call,
			      struct filter_pred *pred,
			      filter_pred_fn_t fn)
{
	struct event_filter *filter = call->filter;
	int idx, err;

	if (filter->n_preds && !pred->compound)
		__filter_disable_preds(call);

	if (filter->n_preds == MAX_FILTER_PRED)
		return -ENOSPC;

	idx = filter->n_preds;
	filter_clear_pred(filter->preds[idx]);
	err = filter_set_pred(filter->preds[idx], pred, fn);
	if (err)
		return err;

	filter->n_preds++;
	call->filter_active = 1;

	return 0;
}

static int is_string_field(const char *type)
{
	if (strchr(type, '[') && strstr(type, "char"))
		return 1;

	return 0;
}

static int __filter_add_pred(struct ftrace_event_call *call,
			     struct filter_pred *pred)
{
	struct ftrace_event_field *field;
	filter_pred_fn_t fn;
	unsigned long long val;

	field = find_event_field(call, pred->field_name);
	if (!field)
		return -EINVAL;

	pred->fn = filter_pred_none;
	pred->offset = field->offset;

	if (is_string_field(field->type)) {
		fn = filter_pred_string;
		pred->str_len = field->size;
		return filter_add_pred_fn(call, pred, fn);
	} else {
		if (strict_strtoull(pred->str_val, 0, &val))
			return -EINVAL;
		pred->val = val;
	}

	switch (field->size) {
	case 8:
		fn = filter_pred_64;
		break;
	case 4:
		fn = filter_pred_32;
		break;
	case 2:
		fn = filter_pred_16;
		break;
	case 1:
		fn = filter_pred_8;
		break;
	default:
		return -EINVAL;
	}

	return filter_add_pred_fn(call, pred, fn);
}

int filter_add_pred(struct ftrace_event_call *call, struct filter_pred *pred)
{
	int err;

	mutex_lock(&filter_mutex);
	err = __filter_add_pred(call, pred);
	mutex_unlock(&filter_mutex);

	return err;
}

int filter_add_subsystem_pred(struct event_subsystem *system,
			      struct filter_pred *pred)
{
	struct event_filter *filter = system->filter;
	struct ftrace_event_call *call;

	mutex_lock(&filter_mutex);

	if (filter && filter->n_preds && !pred->compound) {
		__filter_free_subsystem_preds(system);
		filter = NULL;
	}

	if (!filter) {
		system->filter = kzalloc(sizeof(*filter), GFP_KERNEL);
		if (!system->filter) {
			mutex_unlock(&filter_mutex);
			return -ENOMEM;
		}
		filter = system->filter;
		filter->preds = kzalloc(MAX_FILTER_PRED * sizeof(pred),
					GFP_KERNEL);

		if (!filter->preds) {
			kfree(system->filter);
			system->filter = NULL;
			mutex_unlock(&filter_mutex);
			return -ENOMEM;
		}
	}

	if (filter->n_preds == MAX_FILTER_PRED) {
		mutex_unlock(&filter_mutex);
		return -ENOSPC;
	}

	filter->preds[filter->n_preds] = pred;
	filter->n_preds++;

	list_for_each_entry(call, &ftrace_events, list) {
		int err;

		if (!call->define_fields)
			continue;

		if (strcmp(call->system, system->name))
			continue;

		err = __filter_add_pred(call, pred);
		if (err == -ENOMEM) {
			filter->preds[filter->n_preds] = NULL;
			filter->n_preds--;
			mutex_unlock(&filter_mutex);
			return err;
		}
	}

	mutex_unlock(&filter_mutex);

	return 0;
}

/*
 * The filter format can be
 *   - 0, which means remove all filter preds
 *   - [||/&&] <field> ==/!= <val>
 */
int filter_parse(char **pbuf, struct filter_pred *pred)
{
	char *tok, *val_str = NULL;
	int tok_n = 0;

	while ((tok = strsep(pbuf, " \n"))) {
		if (tok_n == 0) {
			if (!strcmp(tok, "0")) {
				pred->clear = 1;
				return 0;
			} else if (!strcmp(tok, "&&")) {
				pred->or = 0;
				pred->compound = 1;
			} else if (!strcmp(tok, "||")) {
				pred->or = 1;
				pred->compound = 1;
			} else
				pred->field_name = tok;
			tok_n = 1;
			continue;
		}
		if (tok_n == 1) {
			if (!pred->field_name)
				pred->field_name = tok;
			else if (!strcmp(tok, "!="))
				pred->not = 1;
			else if (!strcmp(tok, "=="))
				pred->not = 0;
			else {
				pred->field_name = NULL;
				return -EINVAL;
			}
			tok_n = 2;
			continue;
		}
		if (tok_n == 2) {
			if (pred->compound) {
				if (!strcmp(tok, "!="))
					pred->not = 1;
				else if (!strcmp(tok, "=="))
					pred->not = 0;
				else {
					pred->field_name = NULL;
					return -EINVAL;
				}
			} else {
				val_str = tok;
				break; /* done */
			}
			tok_n = 3;
			continue;
		}
		if (tok_n == 3) {
			val_str = tok;
			break; /* done */
		}
	}

	if (!val_str || !strlen(val_str)
	    || strlen(val_str) >= MAX_FILTER_STR_VAL) {
		pred->field_name = NULL;
		return -EINVAL;
	}

	strcpy(pred->str_val, val_str);
	pred->str_len = strlen(val_str);

	pred->field_name = kstrdup(pred->field_name, GFP_KERNEL);
	if (!pred->field_name)
		return -ENOMEM;

	return 0;
}


