/* Copyright (C) 2007, Red Hat, Inc. */

#ifndef _LBS_CMD_H_
#define _LBS_CMD_H_

#include "host.h"
#include "dev.h"


/* Command & response transfer between host and card */

struct cmd_ctrl_node {
	struct list_head list;
	int result;
	/* command response */
	int (*callback)(struct lbs_private *,
			unsigned long,
			struct cmd_header *);
	unsigned long callback_arg;
	/* command data */
	struct cmd_header *cmdbuf;
	/* wait queue */
	u16 cmdwaitqwoken;
	wait_queue_head_t cmdwait_q;
};


/* lbs_cmd() infers the size of the buffer to copy data back into, from
   the size of the target of the pointer. Since the command to be sent
   may often be smaller, that size is set in cmd->size by the caller.*/
#define lbs_cmd(priv, cmdnr, cmd, cb, cb_arg)	({		\
	uint16_t __sz = le16_to_cpu((cmd)->hdr.size);		\
	(cmd)->hdr.size = cpu_to_le16(sizeof(*(cmd)));		\
	__lbs_cmd(priv, cmdnr, &(cmd)->hdr, __sz, cb, cb_arg);	\
})

#define lbs_cmd_with_response(priv, cmdnr, cmd)	\
	lbs_cmd(priv, cmdnr, cmd, lbs_cmd_copyback, (unsigned long) (cmd))

int lbs_prepare_and_send_command(struct lbs_private *priv,
	u16 cmd_no,
	u16 cmd_action,
	u16 wait_option, u32 cmd_oid, void *pdata_buf);

void lbs_cmd_async(struct lbs_private *priv, uint16_t command,
	struct cmd_header *in_cmd, int in_cmd_size);

int __lbs_cmd(struct lbs_private *priv, uint16_t command,
	      struct cmd_header *in_cmd, int in_cmd_size,
	      int (*callback)(struct lbs_private *, unsigned long, struct cmd_header *),
	      unsigned long callback_arg);

struct cmd_ctrl_node *__lbs_cmd_async(struct lbs_private *priv,
	uint16_t command, struct cmd_header *in_cmd, int in_cmd_size,
	int (*callback)(struct lbs_private *, unsigned long, struct cmd_header *),
	unsigned long callback_arg);

int lbs_cmd_copyback(struct lbs_private *priv, unsigned long extra,
		     struct cmd_header *resp);

int lbs_allocate_cmd_buffer(struct lbs_private *priv);
int lbs_free_cmd_buffer(struct lbs_private *priv);

int lbs_execute_next_command(struct lbs_private *priv);
void lbs_complete_command(struct lbs_private *priv, struct cmd_ctrl_node *cmd,
			  int result);
int lbs_process_command_response(struct lbs_private *priv, u8 *data, u32 len);


/* From cmdresp.c */

void lbs_mac_event_disconnected(struct lbs_private *priv);



/* Events */

int lbs_process_event(struct lbs_private *priv, u32 event);


/* Actual commands */

int lbs_update_hw_spec(struct lbs_private *priv);

int lbs_set_channel(struct lbs_private *priv, u8 channel);

int lbs_update_channel(struct lbs_private *priv);

int lbs_host_sleep_cfg(struct lbs_private *priv, uint32_t criteria,
		struct wol_config *p_wol_config);

int lbs_cmd_802_11_sleep_params(struct lbs_private *priv, uint16_t cmd_action,
				struct sleep_params *sp);

void lbs_ps_sleep(struct lbs_private *priv, int wait_option);

void lbs_ps_wakeup(struct lbs_private *priv, int wait_option);

void lbs_ps_confirm_sleep(struct lbs_private *priv);

int lbs_set_radio(struct lbs_private *priv, u8 preamble, u8 radio_on);

void lbs_set_mac_control(struct lbs_private *priv);

int lbs_get_tx_power(struct lbs_private *priv, s16 *curlevel, s16 *minlevel,
		     s16 *maxlevel);

int lbs_set_snmp_mib(struct lbs_private *priv, u32 oid, u16 val);

int lbs_get_snmp_mib(struct lbs_private *priv, u32 oid, u16 *out_val);


/* Mesh related */

int lbs_mesh_access(struct lbs_private *priv, uint16_t cmd_action,
		    struct cmd_ds_mesh_access *cmd);

int lbs_mesh_config_send(struct lbs_private *priv,
			 struct cmd_ds_mesh_config *cmd,
			 uint16_t action, uint16_t type);

int lbs_mesh_config(struct lbs_private *priv, uint16_t enable, uint16_t chan);


/* Commands only used in wext.c, assoc. and scan.c */

int lbs_set_power_adapt_cfg(struct lbs_private *priv, int enable, int8_t p0,
		int8_t p1, int8_t p2);

int lbs_set_tpc_cfg(struct lbs_private *priv, int enable, int8_t p0, int8_t p1,
		int8_t p2, int usesnr);

int lbs_set_data_rate(struct lbs_private *priv, u8 rate);

int lbs_cmd_802_11_rate_adapt_rateset(struct lbs_private *priv,
				      uint16_t cmd_action);

int lbs_cmd_802_11_set_wep(struct lbs_private *priv, uint16_t cmd_action,
			   struct assoc_request *assoc);

int lbs_cmd_802_11_enable_rsn(struct lbs_private *priv, uint16_t cmd_action,
			      uint16_t *enable);

int lbs_cmd_802_11_key_material(struct lbs_private *priv, uint16_t cmd_action,
				struct assoc_request *assoc);

int lbs_set_tx_power(struct lbs_private *priv, s16 dbm);

int lbs_set_deep_sleep(struct lbs_private *priv, int deep_sleep);

#endif /* _LBS_CMD_H */
