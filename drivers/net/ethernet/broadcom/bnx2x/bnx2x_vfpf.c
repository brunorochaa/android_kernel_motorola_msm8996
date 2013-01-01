/* bnx2x_vfpf.c: Broadcom Everest network driver.
 *
 * Copyright 2009-2012 Broadcom Corporation
 *
 * Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2, available
 * at http://www.gnu.org/licenses/old-licenses/gpl-2.0.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a
 * license other than the GPL, without Broadcom's express prior written
 * consent.
 *
 * Maintained by: Eilon Greenstein <eilong@broadcom.com>
 * Written by: Shmulik Ravid <shmulikr@broadcom.com>
 *	       Ariel Elior <ariele@broadcom.com>
 */

#include "bnx2x.h"
#include "bnx2x_sriov.h"
#include <linux/crc32.h>

/* place a given tlv on the tlv buffer at a given offset */
void bnx2x_add_tlv(struct bnx2x *bp, void *tlvs_list, u16 offset, u16 type,
		   u16 length)
{
	struct channel_tlv *tl =
		(struct channel_tlv *)(tlvs_list + offset);

	tl->type = type;
	tl->length = length;
}

/* Clear the mailbox and init the header of the first tlv */
void bnx2x_vfpf_prep(struct bnx2x *bp, struct vfpf_first_tlv *first_tlv,
		     u16 type, u16 length)
{
	DP(BNX2X_MSG_IOV, "preparing to send %d tlv over vf pf channel\n",
	   type);

	/* Clear mailbox */
	memset(bp->vf2pf_mbox, 0, sizeof(struct bnx2x_vf_mbx_msg));

	/* init type and length */
	bnx2x_add_tlv(bp, &first_tlv->tl, 0, type, length);

	/* init first tlv header */
	first_tlv->resp_msg_offset = sizeof(bp->vf2pf_mbox->req);
}

/* list the types and lengths of the tlvs on the buffer */
void bnx2x_dp_tlv_list(struct bnx2x *bp, void *tlvs_list)
{
	int i = 1;
	struct channel_tlv *tlv = (struct channel_tlv *)tlvs_list;

	while (tlv->type != CHANNEL_TLV_LIST_END) {
		/* output tlv */
		DP(BNX2X_MSG_IOV, "TLV number %d: type %d, length %d\n", i,
		   tlv->type, tlv->length);

		/* advance to next tlv */
		tlvs_list += tlv->length;

		/* cast general tlv list pointer to channel tlv header*/
		tlv = (struct channel_tlv *)tlvs_list;

		i++;

		/* break condition for this loop */
		if (i > MAX_TLVS_IN_LIST) {
			WARN(true, "corrupt tlvs");
			return;
		}
	}

	/* output last tlv */
	DP(BNX2X_MSG_IOV, "TLV number %d: type %d, length %d\n", i,
	   tlv->type, tlv->length);
}

/* test whether we support a tlv type */
bool bnx2x_tlv_supported(u16 tlvtype)
{
	return CHANNEL_TLV_NONE < tlvtype && tlvtype < CHANNEL_TLV_MAX;
}

static inline int bnx2x_pfvf_status_codes(int rc)
{
	switch (rc) {
	case 0:
		return PFVF_STATUS_SUCCESS;
	case -ENOMEM:
		return PFVF_STATUS_NO_RESOURCE;
	default:
		return PFVF_STATUS_FAILURE;
	}
}

/* General service functions */
static void storm_memset_vf_mbx_ack(struct bnx2x *bp, u16 abs_fid)
{
	u32 addr = BAR_CSTRORM_INTMEM +
		   CSTORM_VF_PF_CHANNEL_STATE_OFFSET(abs_fid);

	REG_WR8(bp, addr, VF_PF_CHANNEL_STATE_READY);
}

static void storm_memset_vf_mbx_valid(struct bnx2x *bp, u16 abs_fid)
{
	u32 addr = BAR_CSTRORM_INTMEM +
		   CSTORM_VF_PF_CHANNEL_VALID_OFFSET(abs_fid);

	REG_WR8(bp, addr, 1);
}

static inline void bnx2x_set_vf_mbxs_valid(struct bnx2x *bp)
{
	int i;

	for_each_vf(bp, i)
		storm_memset_vf_mbx_valid(bp, bnx2x_vf(bp, i, abs_vfid));
}

/* enable vf_pf mailbox (aka vf-pf-chanell) */
void bnx2x_vf_enable_mbx(struct bnx2x *bp, u8 abs_vfid)
{
	bnx2x_vf_flr_clnup_epilog(bp, abs_vfid);

	/* enable the mailbox in the FW */
	storm_memset_vf_mbx_ack(bp, abs_vfid);
	storm_memset_vf_mbx_valid(bp, abs_vfid);

	/* enable the VF access to the mailbox */
	bnx2x_vf_enable_access(bp, abs_vfid);
}

/* this works only on !E1h */
static int bnx2x_copy32_vf_dmae(struct bnx2x *bp, u8 from_vf,
				dma_addr_t pf_addr, u8 vfid, u32 vf_addr_hi,
				u32 vf_addr_lo, u32 len32)
{
	struct dmae_command dmae;

	if (CHIP_IS_E1x(bp)) {
		BNX2X_ERR("Chip revision does not support VFs\n");
		return DMAE_NOT_RDY;
	}

	if (!bp->dmae_ready) {
		BNX2X_ERR("DMAE is not ready, can not copy\n");
		return DMAE_NOT_RDY;
	}

	/* set opcode and fixed command fields */
	bnx2x_prep_dmae_with_comp(bp, &dmae, DMAE_SRC_PCI, DMAE_DST_PCI);

	if (from_vf) {
		dmae.opcode_iov = (vfid << DMAE_COMMAND_SRC_VFID_SHIFT) |
			(DMAE_SRC_VF << DMAE_COMMAND_SRC_VFPF_SHIFT) |
			(DMAE_DST_PF << DMAE_COMMAND_DST_VFPF_SHIFT);

		dmae.opcode |= (DMAE_C_DST << DMAE_COMMAND_C_FUNC_SHIFT);

		dmae.src_addr_lo = vf_addr_lo;
		dmae.src_addr_hi = vf_addr_hi;
		dmae.dst_addr_lo = U64_LO(pf_addr);
		dmae.dst_addr_hi = U64_HI(pf_addr);
	} else {
		dmae.opcode_iov = (vfid << DMAE_COMMAND_DST_VFID_SHIFT) |
			(DMAE_DST_VF << DMAE_COMMAND_DST_VFPF_SHIFT) |
			(DMAE_SRC_PF << DMAE_COMMAND_SRC_VFPF_SHIFT);

		dmae.opcode |= (DMAE_C_SRC << DMAE_COMMAND_C_FUNC_SHIFT);

		dmae.src_addr_lo = U64_LO(pf_addr);
		dmae.src_addr_hi = U64_HI(pf_addr);
		dmae.dst_addr_lo = vf_addr_lo;
		dmae.dst_addr_hi = vf_addr_hi;
	}
	dmae.len = len32;
	bnx2x_dp_dmae(bp, &dmae, BNX2X_MSG_DMAE);

	/* issue the command and wait for completion */
	return bnx2x_issue_dmae_with_comp(bp, &dmae);
}

static void bnx2x_vf_mbx_resp(struct bnx2x *bp, struct bnx2x_virtf *vf)
{
	struct bnx2x_vf_mbx *mbx = BP_VF_MBX(bp, vf->index);
	u64 vf_addr;
	dma_addr_t pf_addr;
	u16 length, type;
	int rc;
	struct pfvf_general_resp_tlv *resp = &mbx->msg->resp.general_resp;

	/* prepare response */
	type = mbx->first_tlv.tl.type;
	length = type == CHANNEL_TLV_ACQUIRE ?
		sizeof(struct pfvf_acquire_resp_tlv) :
		sizeof(struct pfvf_general_resp_tlv);
	bnx2x_add_tlv(bp, resp, 0, type, length);
	resp->hdr.status = bnx2x_pfvf_status_codes(vf->op_rc);
	bnx2x_add_tlv(bp, resp, length, CHANNEL_TLV_LIST_END,
		      sizeof(struct channel_list_end_tlv));
	bnx2x_dp_tlv_list(bp, resp);
	DP(BNX2X_MSG_IOV, "mailbox vf address hi 0x%x, lo 0x%x, offset 0x%x\n",
	   mbx->vf_addr_hi, mbx->vf_addr_lo, mbx->first_tlv.resp_msg_offset);

	/* send response */
	vf_addr = HILO_U64(mbx->vf_addr_hi, mbx->vf_addr_lo) +
		  mbx->first_tlv.resp_msg_offset;
	pf_addr = mbx->msg_mapping +
		  offsetof(struct bnx2x_vf_mbx_msg, resp);

	/* copy the response body, if there is one, before the header, as the vf
	 * is sensitive to the header being written
	 */
	if (resp->hdr.tl.length > sizeof(u64)) {
		length = resp->hdr.tl.length - sizeof(u64);
		vf_addr += sizeof(u64);
		pf_addr += sizeof(u64);
		rc = bnx2x_copy32_vf_dmae(bp, false, pf_addr, vf->abs_vfid,
					  U64_HI(vf_addr),
					  U64_LO(vf_addr),
					  length/4);
		if (rc) {
			BNX2X_ERR("Failed to copy response body to VF %d\n",
				  vf->abs_vfid);
			return;
		}
		vf_addr -= sizeof(u64);
		pf_addr -= sizeof(u64);
	}

	/* ack the FW */
	storm_memset_vf_mbx_ack(bp, vf->abs_vfid);
	mmiowb();

	/* initiate dmae to send the response */
	mbx->flags &= ~VF_MSG_INPROCESS;

	/* copy the response header including status-done field,
	 * must be last dmae, must be after FW is acked
	 */
	rc = bnx2x_copy32_vf_dmae(bp, false, pf_addr, vf->abs_vfid,
				  U64_HI(vf_addr),
				  U64_LO(vf_addr),
				  sizeof(u64)/4);

	/* unlock channel mutex */
	bnx2x_unlock_vf_pf_channel(bp, vf, mbx->first_tlv.tl.type);

	if (rc) {
		BNX2X_ERR("Failed to copy response status to VF %d\n",
			  vf->abs_vfid);
	}
	return;
}

static void bnx2x_vf_mbx_acquire_resp(struct bnx2x *bp, struct bnx2x_virtf *vf,
				      struct bnx2x_vf_mbx *mbx, int vfop_status)
{
	int i;
	struct pfvf_acquire_resp_tlv *resp = &mbx->msg->resp.acquire_resp;
	struct pf_vf_resc *resc = &resp->resc;
	u8 status = bnx2x_pfvf_status_codes(vfop_status);

	memset(resp, 0, sizeof(*resp));

	/* fill in pfdev info */
	resp->pfdev_info.chip_num = bp->common.chip_id;
	resp->pfdev_info.db_size = (1 << BNX2X_DB_SHIFT);
	resp->pfdev_info.indices_per_sb = HC_SB_MAX_INDICES_E2;
	resp->pfdev_info.pf_cap = (PFVF_CAP_RSS |
				   /* PFVF_CAP_DHC |*/ PFVF_CAP_TPA);
	bnx2x_fill_fw_str(bp, resp->pfdev_info.fw_ver,
			  sizeof(resp->pfdev_info.fw_ver));

	if (status == PFVF_STATUS_NO_RESOURCE ||
	    status == PFVF_STATUS_SUCCESS) {
		/* set resources numbers, if status equals NO_RESOURCE these
		 * are max possible numbers
		 */
		resc->num_rxqs = vf_rxq_count(vf) ? :
			bnx2x_vf_max_queue_cnt(bp, vf);
		resc->num_txqs = vf_txq_count(vf) ? :
			bnx2x_vf_max_queue_cnt(bp, vf);
		resc->num_sbs = vf_sb_count(vf);
		resc->num_mac_filters = vf_mac_rules_cnt(vf);
		resc->num_vlan_filters = vf_vlan_rules_cnt(vf);
		resc->num_mc_filters = 0;

		if (status == PFVF_STATUS_SUCCESS) {
			for_each_vfq(vf, i)
				resc->hw_qid[i] =
					vfq_qzone_id(vf, vfq_get(vf, i));

			for_each_vf_sb(vf, i) {
				resc->hw_sbs[i].hw_sb_id = vf_igu_sb(vf, i);
				resc->hw_sbs[i].sb_qid = vf_hc_qzone(vf, i);
			}
		}
	}

	DP(BNX2X_MSG_IOV, "VF[%d] ACQUIRE_RESPONSE: pfdev_info- chip_num=0x%x, db_size=%d, idx_per_sb=%d, pf_cap=0x%x\n"
	   "resources- n_rxq-%d, n_txq-%d, n_sbs-%d, n_macs-%d, n_vlans-%d, n_mcs-%d, fw_ver: '%s'\n",
	   vf->abs_vfid,
	   resp->pfdev_info.chip_num,
	   resp->pfdev_info.db_size,
	   resp->pfdev_info.indices_per_sb,
	   resp->pfdev_info.pf_cap,
	   resc->num_rxqs,
	   resc->num_txqs,
	   resc->num_sbs,
	   resc->num_mac_filters,
	   resc->num_vlan_filters,
	   resc->num_mc_filters,
	   resp->pfdev_info.fw_ver);

	DP_CONT(BNX2X_MSG_IOV, "hw_qids- [ ");
	for (i = 0; i < vf_rxq_count(vf); i++)
		DP_CONT(BNX2X_MSG_IOV, "%d ", resc->hw_qid[i]);
	DP_CONT(BNX2X_MSG_IOV, "], sb_info- [ ");
	for (i = 0; i < vf_sb_count(vf); i++)
		DP_CONT(BNX2X_MSG_IOV, "%d:%d ",
			resc->hw_sbs[i].hw_sb_id,
			resc->hw_sbs[i].sb_qid);
	DP_CONT(BNX2X_MSG_IOV, "]\n");

	/* send the response */
	vf->op_rc = vfop_status;
	bnx2x_vf_mbx_resp(bp, vf);
}

static void bnx2x_vf_mbx_acquire(struct bnx2x *bp, struct bnx2x_virtf *vf,
				 struct bnx2x_vf_mbx *mbx)
{
	int rc;
	struct vfpf_acquire_tlv *acquire = &mbx->msg->req.acquire;

	/* log vfdef info */
	DP(BNX2X_MSG_IOV,
	   "VF[%d] ACQUIRE: vfdev_info- vf_id %d, vf_os %d resources- n_rxq-%d, n_txq-%d, n_sbs-%d, n_macs-%d, n_vlans-%d, n_mcs-%d\n",
	   vf->abs_vfid, acquire->vfdev_info.vf_id, acquire->vfdev_info.vf_os,
	   acquire->resc_request.num_rxqs, acquire->resc_request.num_txqs,
	   acquire->resc_request.num_sbs, acquire->resc_request.num_mac_filters,
	   acquire->resc_request.num_vlan_filters,
	   acquire->resc_request.num_mc_filters);

	/* acquire the resources */
	rc = bnx2x_vf_acquire(bp, vf, &acquire->resc_request);

	/* response */
	bnx2x_vf_mbx_acquire_resp(bp, vf, mbx, rc);
}

static void bnx2x_vf_mbx_init_vf(struct bnx2x *bp, struct bnx2x_virtf *vf,
			      struct bnx2x_vf_mbx *mbx)
{
	struct vfpf_init_tlv *init = &mbx->msg->req.init;

	/* record ghost addresses from vf message */
	vf->spq_map = init->spq_addr;
	vf->fw_stat_map = init->stats_addr;
	vf->op_rc = bnx2x_vf_init(bp, vf, (dma_addr_t *)init->sb_addr);

	/* response */
	bnx2x_vf_mbx_resp(bp, vf);
}

/* convert MBX queue-flags to standard SP queue-flags */
static void bnx2x_vf_mbx_set_q_flags(u32 mbx_q_flags,
				     unsigned long *sp_q_flags)
{
	if (mbx_q_flags & VFPF_QUEUE_FLG_TPA)
		__set_bit(BNX2X_Q_FLG_TPA, sp_q_flags);
	if (mbx_q_flags & VFPF_QUEUE_FLG_TPA_IPV6)
		__set_bit(BNX2X_Q_FLG_TPA_IPV6, sp_q_flags);
	if (mbx_q_flags & VFPF_QUEUE_FLG_TPA_GRO)
		__set_bit(BNX2X_Q_FLG_TPA_GRO, sp_q_flags);
	if (mbx_q_flags & VFPF_QUEUE_FLG_STATS)
		__set_bit(BNX2X_Q_FLG_STATS, sp_q_flags);
	if (mbx_q_flags & VFPF_QUEUE_FLG_OV)
		__set_bit(BNX2X_Q_FLG_OV, sp_q_flags);
	if (mbx_q_flags & VFPF_QUEUE_FLG_VLAN)
		__set_bit(BNX2X_Q_FLG_VLAN, sp_q_flags);
	if (mbx_q_flags & VFPF_QUEUE_FLG_COS)
		__set_bit(BNX2X_Q_FLG_COS, sp_q_flags);
	if (mbx_q_flags & VFPF_QUEUE_FLG_HC)
		__set_bit(BNX2X_Q_FLG_HC, sp_q_flags);
	if (mbx_q_flags & VFPF_QUEUE_FLG_DHC)
		__set_bit(BNX2X_Q_FLG_DHC, sp_q_flags);
}

static void bnx2x_vf_mbx_setup_q(struct bnx2x *bp, struct bnx2x_virtf *vf,
				 struct bnx2x_vf_mbx *mbx)
{
	struct vfpf_setup_q_tlv *setup_q = &mbx->msg->req.setup_q;
	struct bnx2x_vfop_cmd cmd = {
		.done = bnx2x_vf_mbx_resp,
		.block = false,
	};

	/* verify vf_qid */
	if (setup_q->vf_qid >= vf_rxq_count(vf)) {
		BNX2X_ERR("vf_qid %d invalid, max queue count is %d\n",
			  setup_q->vf_qid, vf_rxq_count(vf));
		vf->op_rc = -EINVAL;
		goto response;
	}

	/* tx queues must be setup alongside rx queues thus if the rx queue
	 * is not marked as valid there's nothing to do.
	 */
	if (setup_q->param_valid & (VFPF_RXQ_VALID|VFPF_TXQ_VALID)) {
		struct bnx2x_vf_queue *q = vfq_get(vf, setup_q->vf_qid);
		unsigned long q_type = 0;

		struct bnx2x_queue_init_params *init_p;
		struct bnx2x_queue_setup_params *setup_p;

		/* reinit the VF operation context */
		memset(&vf->op_params.qctor, 0 , sizeof(vf->op_params.qctor));
		setup_p = &vf->op_params.qctor.prep_qsetup;
		init_p =  &vf->op_params.qctor.qstate.params.init;

		/* activate immediately */
		__set_bit(BNX2X_Q_FLG_ACTIVE, &setup_p->flags);

		if (setup_q->param_valid & VFPF_TXQ_VALID) {
			struct bnx2x_txq_setup_params *txq_params =
				&setup_p->txq_params;

			__set_bit(BNX2X_Q_TYPE_HAS_TX, &q_type);

			/* save sb resource index */
			q->sb_idx = setup_q->txq.vf_sb;

			/* tx init */
			init_p->tx.hc_rate = setup_q->txq.hc_rate;
			init_p->tx.sb_cq_index = setup_q->txq.sb_index;

			bnx2x_vf_mbx_set_q_flags(setup_q->txq.flags,
						 &init_p->tx.flags);

			/* tx setup - flags */
			bnx2x_vf_mbx_set_q_flags(setup_q->txq.flags,
						 &setup_p->flags);

			/* tx setup - general, nothing */

			/* tx setup - tx */
			txq_params->dscr_map = setup_q->txq.txq_addr;
			txq_params->sb_cq_index = setup_q->txq.sb_index;
			txq_params->traffic_type = setup_q->txq.traffic_type;

			bnx2x_vfop_qctor_dump_tx(bp, vf, init_p, setup_p,
						 q->index, q->sb_idx);
		}

		if (setup_q->param_valid & VFPF_RXQ_VALID) {
			struct bnx2x_rxq_setup_params *rxq_params =
							&setup_p->rxq_params;

			__set_bit(BNX2X_Q_TYPE_HAS_RX, &q_type);

			/* Note: there is no support for different SBs
			 * for TX and RX
			 */
			q->sb_idx = setup_q->rxq.vf_sb;

			/* rx init */
			init_p->rx.hc_rate = setup_q->rxq.hc_rate;
			init_p->rx.sb_cq_index = setup_q->rxq.sb_index;
			bnx2x_vf_mbx_set_q_flags(setup_q->rxq.flags,
						 &init_p->rx.flags);

			/* rx setup - flags */
			bnx2x_vf_mbx_set_q_flags(setup_q->rxq.flags,
						 &setup_p->flags);

			/* rx setup - general */
			setup_p->gen_params.mtu = setup_q->rxq.mtu;

			/* rx setup - rx */
			rxq_params->drop_flags = setup_q->rxq.drop_flags;
			rxq_params->dscr_map = setup_q->rxq.rxq_addr;
			rxq_params->sge_map = setup_q->rxq.sge_addr;
			rxq_params->rcq_map = setup_q->rxq.rcq_addr;
			rxq_params->rcq_np_map = setup_q->rxq.rcq_np_addr;
			rxq_params->buf_sz = setup_q->rxq.buf_sz;
			rxq_params->tpa_agg_sz = setup_q->rxq.tpa_agg_sz;
			rxq_params->max_sges_pkt = setup_q->rxq.max_sge_pkt;
			rxq_params->sge_buf_sz = setup_q->rxq.sge_buf_sz;
			rxq_params->cache_line_log =
				setup_q->rxq.cache_line_log;
			rxq_params->sb_cq_index = setup_q->rxq.sb_index;

			bnx2x_vfop_qctor_dump_rx(bp, vf, init_p, setup_p,
						 q->index, q->sb_idx);
		}
		/* complete the preparations */
		bnx2x_vfop_qctor_prep(bp, vf, q, &vf->op_params.qctor, q_type);

		vf->op_rc = bnx2x_vfop_qsetup_cmd(bp, vf, &cmd, q->index);
		if (vf->op_rc)
			goto response;
		return;
	}
response:
	bnx2x_vf_mbx_resp(bp, vf);
}

enum bnx2x_vfop_filters_state {
	   BNX2X_VFOP_MBX_Q_FILTERS_MACS,
	   BNX2X_VFOP_MBX_Q_FILTERS_VLANS,
	   BNX2X_VFOP_MBX_Q_FILTERS_RXMODE,
	   BNX2X_VFOP_MBX_Q_FILTERS_MCAST,
	   BNX2X_VFOP_MBX_Q_FILTERS_DONE
};

static int bnx2x_vf_mbx_macvlan_list(struct bnx2x *bp,
				     struct bnx2x_virtf *vf,
				     struct vfpf_set_q_filters_tlv *tlv,
				     struct bnx2x_vfop_filters **pfl,
				     u32 type_flag)
{
	int i, j;
	struct bnx2x_vfop_filters *fl = NULL;
	size_t fsz;

	fsz = tlv->n_mac_vlan_filters * sizeof(struct bnx2x_vfop_filter) +
		sizeof(struct bnx2x_vfop_filters);

	fl = kzalloc(fsz, GFP_KERNEL);
	if (!fl)
		return -ENOMEM;

	INIT_LIST_HEAD(&fl->head);

	for (i = 0, j = 0; i < tlv->n_mac_vlan_filters; i++) {
		struct vfpf_q_mac_vlan_filter *msg_filter = &tlv->filters[i];

		if ((msg_filter->flags & type_flag) != type_flag)
			continue;
		if (type_flag == VFPF_Q_FILTER_DEST_MAC_VALID) {
			fl->filters[j].mac = msg_filter->mac;
			fl->filters[j].type = BNX2X_VFOP_FILTER_MAC;
		} else {
			fl->filters[j].vid = msg_filter->vlan_tag;
			fl->filters[j].type = BNX2X_VFOP_FILTER_VLAN;
		}
		fl->filters[j].add =
			(msg_filter->flags & VFPF_Q_FILTER_SET_MAC) ?
			true : false;
		list_add_tail(&fl->filters[j++].link, &fl->head);
	}
	if (list_empty(&fl->head))
		kfree(fl);
	else
		*pfl = fl;

	return 0;
}

static void bnx2x_vf_mbx_dp_q_filter(struct bnx2x *bp, int msglvl, int idx,
				       struct vfpf_q_mac_vlan_filter *filter)
{
	DP(msglvl, "MAC-VLAN[%d] -- flags=0x%x\n", idx, filter->flags);
	if (filter->flags & VFPF_Q_FILTER_VLAN_TAG_VALID)
		DP_CONT(msglvl, ", vlan=%d", filter->vlan_tag);
	if (filter->flags & VFPF_Q_FILTER_DEST_MAC_VALID)
		DP_CONT(msglvl, ", MAC=%pM", filter->mac);
	DP_CONT(msglvl, "\n");
}

static void bnx2x_vf_mbx_dp_q_filters(struct bnx2x *bp, int msglvl,
				       struct vfpf_set_q_filters_tlv *filters)
{
	int i;

	if (filters->flags & VFPF_SET_Q_FILTERS_MAC_VLAN_CHANGED)
		for (i = 0; i < filters->n_mac_vlan_filters; i++)
			bnx2x_vf_mbx_dp_q_filter(bp, msglvl, i,
						 &filters->filters[i]);

	if (filters->flags & VFPF_SET_Q_FILTERS_RX_MASK_CHANGED)
		DP(msglvl, "RX-MASK=0x%x\n", filters->rx_mask);

	if (filters->flags & VFPF_SET_Q_FILTERS_MULTICAST_CHANGED)
		for (i = 0; i < filters->n_multicast; i++)
			DP(msglvl, "MULTICAST=%pM\n", filters->multicast[i]);
}

#define VFPF_MAC_FILTER		VFPF_Q_FILTER_DEST_MAC_VALID
#define VFPF_VLAN_FILTER	VFPF_Q_FILTER_VLAN_TAG_VALID

static void bnx2x_vfop_mbx_qfilters(struct bnx2x *bp, struct bnx2x_virtf *vf)
{
	int rc;

	struct vfpf_set_q_filters_tlv *msg =
		&BP_VF_MBX(bp, vf->index)->msg->req.set_q_filters;

	struct bnx2x_vfop *vfop = bnx2x_vfop_cur(bp, vf);
	enum bnx2x_vfop_filters_state state = vfop->state;

	struct bnx2x_vfop_cmd cmd = {
		.done = bnx2x_vfop_mbx_qfilters,
		.block = false,
	};

	DP(BNX2X_MSG_IOV, "STATE: %d\n", state);

	if (vfop->rc < 0)
		goto op_err;

	switch (state) {
	case BNX2X_VFOP_MBX_Q_FILTERS_MACS:
		/* next state */
		vfop->state = BNX2X_VFOP_MBX_Q_FILTERS_VLANS;

		/* check for any vlan/mac changes */
		if (msg->flags & VFPF_SET_Q_FILTERS_MAC_VLAN_CHANGED) {
			/* build mac list */
			struct bnx2x_vfop_filters *fl = NULL;

			vfop->rc = bnx2x_vf_mbx_macvlan_list(bp, vf, msg, &fl,
							     VFPF_MAC_FILTER);
			if (vfop->rc)
				goto op_err;

			if (fl) {
				/* set mac list */
				rc = bnx2x_vfop_mac_list_cmd(bp, vf, &cmd, fl,
							     msg->vf_qid,
							     false);
				if (rc) {
					vfop->rc = rc;
					goto op_err;
				}
				return;
			}
		}
		/* fall through */

	case BNX2X_VFOP_MBX_Q_FILTERS_VLANS:
		/* next state */
		vfop->state = BNX2X_VFOP_MBX_Q_FILTERS_RXMODE;

		/* check for any vlan/mac changes */
		if (msg->flags & VFPF_SET_Q_FILTERS_MAC_VLAN_CHANGED) {
			/* build vlan list */
			struct bnx2x_vfop_filters *fl = NULL;

			vfop->rc = bnx2x_vf_mbx_macvlan_list(bp, vf, msg, &fl,
							     VFPF_VLAN_FILTER);
			if (vfop->rc)
				goto op_err;

			if (fl) {
				/* set vlan list */
				rc = bnx2x_vfop_vlan_list_cmd(bp, vf, &cmd, fl,
							      msg->vf_qid,
							      false);
				if (rc) {
					vfop->rc = rc;
					goto op_err;
				}
				return;
			}
		}
		/* fall through */

	case BNX2X_VFOP_MBX_Q_FILTERS_RXMODE:
		/* next state */
		vfop->state = BNX2X_VFOP_MBX_Q_FILTERS_MCAST;

		if (msg->flags & VFPF_SET_Q_FILTERS_RX_MASK_CHANGED) {
			unsigned long accept = 0;

			/* covert VF-PF if mask to bnx2x accept flags */
			if (msg->rx_mask & VFPF_RX_MASK_ACCEPT_MATCHED_UNICAST)
				__set_bit(BNX2X_ACCEPT_UNICAST, &accept);

			if (msg->rx_mask &
					VFPF_RX_MASK_ACCEPT_MATCHED_MULTICAST)
				__set_bit(BNX2X_ACCEPT_MULTICAST, &accept);

			if (msg->rx_mask & VFPF_RX_MASK_ACCEPT_ALL_UNICAST)
				__set_bit(BNX2X_ACCEPT_ALL_UNICAST, &accept);

			if (msg->rx_mask & VFPF_RX_MASK_ACCEPT_ALL_MULTICAST)
				__set_bit(BNX2X_ACCEPT_ALL_MULTICAST, &accept);

			if (msg->rx_mask & VFPF_RX_MASK_ACCEPT_BROADCAST)
				__set_bit(BNX2X_ACCEPT_BROADCAST, &accept);

			/* A packet arriving the vf's mac should be accepted
			 * with any vlan
			 */
			__set_bit(BNX2X_ACCEPT_ANY_VLAN, &accept);

			/* set rx-mode */
			rc = bnx2x_vfop_rxmode_cmd(bp, vf, &cmd,
						   msg->vf_qid, accept);
			if (rc) {
				vfop->rc = rc;
				goto op_err;
			}
			return;
		}
		/* fall through */

	case BNX2X_VFOP_MBX_Q_FILTERS_MCAST:
		/* next state */
		vfop->state = BNX2X_VFOP_MBX_Q_FILTERS_DONE;

		if (msg->flags & VFPF_SET_Q_FILTERS_MULTICAST_CHANGED) {
			/* set mcasts */
			rc = bnx2x_vfop_mcast_cmd(bp, vf, &cmd, msg->multicast,
						  msg->n_multicast, false);
			if (rc) {
				vfop->rc = rc;
				goto op_err;
			}
			return;
		}
		/* fall through */
op_done:
	case BNX2X_VFOP_MBX_Q_FILTERS_DONE:
		bnx2x_vfop_end(bp, vf, vfop);
		return;
op_err:
	BNX2X_ERR("QFILTERS[%d:%d] error: rc %d\n",
		  vf->abs_vfid, msg->vf_qid, vfop->rc);
	goto op_done;

	default:
		bnx2x_vfop_default(state);
	}
}

static int bnx2x_vfop_mbx_qfilters_cmd(struct bnx2x *bp,
					struct bnx2x_virtf *vf,
					struct bnx2x_vfop_cmd *cmd)
{
	struct bnx2x_vfop *vfop = bnx2x_vfop_add(bp, vf);
	if (vfop) {
		bnx2x_vfop_opset(BNX2X_VFOP_MBX_Q_FILTERS_MACS,
				 bnx2x_vfop_mbx_qfilters, cmd->done);
		return bnx2x_vfop_transition(bp, vf, bnx2x_vfop_mbx_qfilters,
					     cmd->block);
	}
	return -ENOMEM;
}

static void bnx2x_vf_mbx_set_q_filters(struct bnx2x *bp,
				       struct bnx2x_virtf *vf,
				       struct bnx2x_vf_mbx *mbx)
{
	struct vfpf_set_q_filters_tlv *filters = &mbx->msg->req.set_q_filters;
	struct bnx2x_vfop_cmd cmd = {
		.done = bnx2x_vf_mbx_resp,
		.block = false,
	};

	/* verify vf_qid */
	if (filters->vf_qid > vf_rxq_count(vf))
		goto response;

	DP(BNX2X_MSG_IOV, "VF[%d] Q_FILTERS: queue[%d]\n",
	   vf->abs_vfid,
	   filters->vf_qid);

	/* print q_filter message */
	bnx2x_vf_mbx_dp_q_filters(bp, BNX2X_MSG_IOV, filters);

	vf->op_rc = bnx2x_vfop_mbx_qfilters_cmd(bp, vf, &cmd);
	if (vf->op_rc)
		goto response;
	return;

response:
	bnx2x_vf_mbx_resp(bp, vf);
}

static void bnx2x_vf_mbx_teardown_q(struct bnx2x *bp, struct bnx2x_virtf *vf,
				    struct bnx2x_vf_mbx *mbx)
{
	int qid = mbx->msg->req.q_op.vf_qid;
	struct bnx2x_vfop_cmd cmd = {
		.done = bnx2x_vf_mbx_resp,
		.block = false,
	};

	DP(BNX2X_MSG_IOV, "VF[%d] Q_TEARDOWN: vf_qid=%d\n",
	   vf->abs_vfid, qid);

	vf->op_rc = bnx2x_vfop_qdown_cmd(bp, vf, &cmd, qid);
	if (vf->op_rc)
		bnx2x_vf_mbx_resp(bp, vf);
}

/* dispatch request */
static void bnx2x_vf_mbx_request(struct bnx2x *bp, struct bnx2x_virtf *vf,
				  struct bnx2x_vf_mbx *mbx)
{
	int i;

	/* check if tlv type is known */
	if (bnx2x_tlv_supported(mbx->first_tlv.tl.type)) {
		/* Lock the per vf op mutex and note the locker's identity.
		 * The unlock will take place in mbx response.
		 */
		bnx2x_lock_vf_pf_channel(bp, vf, mbx->first_tlv.tl.type);

		/* switch on the opcode */
		switch (mbx->first_tlv.tl.type) {
		case CHANNEL_TLV_ACQUIRE:
			bnx2x_vf_mbx_acquire(bp, vf, mbx);
			break;
		case CHANNEL_TLV_INIT:
			bnx2x_vf_mbx_init_vf(bp, vf, mbx);
			break;
		case CHANNEL_TLV_SETUP_Q:
			bnx2x_vf_mbx_setup_q(bp, vf, mbx);
			break;
		case CHANNEL_TLV_SET_Q_FILTERS:
			bnx2x_vf_mbx_set_q_filters(bp, vf, mbx);
			break;
		case CHANNEL_TLV_TEARDOWN_Q:
			bnx2x_vf_mbx_teardown_q(bp, vf, mbx);
			break;
		}

	} else {
		/* unknown TLV - this may belong to a VF driver from the future
		 * - a version written after this PF driver was written, which
		 * supports features unknown as of yet. Too bad since we don't
		 * support them. Or this may be because someone wrote a crappy
		 * VF driver and is sending garbage over the channel.
		 */
		BNX2X_ERR("unknown TLV. type %d length %d. first 20 bytes of mailbox buffer:\n",
			  mbx->first_tlv.tl.type, mbx->first_tlv.tl.length);
		for (i = 0; i < 20; i++)
			DP_CONT(BNX2X_MSG_IOV, "%x ",
				mbx->msg->req.tlv_buf_size.tlv_buffer[i]);

		/* test whether we can respond to the VF (do we have an address
		 * for it?)
		 */
		if (vf->state == VF_ACQUIRED) {
			/* mbx_resp uses the op_rc of the VF */
			vf->op_rc = PFVF_STATUS_NOT_SUPPORTED;

			/* notify the VF that we do not support this request */
			bnx2x_vf_mbx_resp(bp, vf);
		} else {
			/* can't send a response since this VF is unknown to us
			 * just unlock the channel and be done with.
			 */
			bnx2x_unlock_vf_pf_channel(bp, vf,
						   mbx->first_tlv.tl.type);
		}
	}
}

/* handle new vf-pf message */
void bnx2x_vf_mbx(struct bnx2x *bp, struct vf_pf_event_data *vfpf_event)
{
	struct bnx2x_virtf *vf;
	struct bnx2x_vf_mbx *mbx;
	u8 vf_idx;
	int rc;

	DP(BNX2X_MSG_IOV,
	   "vf pf event received: vfid %d, address_hi %x, address lo %x",
	   vfpf_event->vf_id, vfpf_event->msg_addr_hi, vfpf_event->msg_addr_lo);
	/* Sanity checks consider removing later */

	/* check if the vf_id is valid */
	if (vfpf_event->vf_id - BP_VFDB(bp)->sriov.first_vf_in_pf >
	    BNX2X_NR_VIRTFN(bp)) {
		BNX2X_ERR("Illegal vf_id %d max allowed: %d\n",
			  vfpf_event->vf_id, BNX2X_NR_VIRTFN(bp));
		goto mbx_done;
	}
	vf_idx = bnx2x_vf_idx_by_abs_fid(bp, vfpf_event->vf_id);
	mbx = BP_VF_MBX(bp, vf_idx);

	/* verify an event is not currently being processed -
	 * debug failsafe only
	 */
	if (mbx->flags & VF_MSG_INPROCESS) {
		BNX2X_ERR("Previous message is still being processed, vf_id %d\n",
			  vfpf_event->vf_id);
		goto mbx_done;
	}
	vf = BP_VF(bp, vf_idx);

	/* save the VF message address */
	mbx->vf_addr_hi = vfpf_event->msg_addr_hi;
	mbx->vf_addr_lo = vfpf_event->msg_addr_lo;
	DP(BNX2X_MSG_IOV, "mailbox vf address hi 0x%x, lo 0x%x, offset 0x%x\n",
	   mbx->vf_addr_hi, mbx->vf_addr_lo, mbx->first_tlv.resp_msg_offset);

	/* dmae to get the VF request */
	rc = bnx2x_copy32_vf_dmae(bp, true, mbx->msg_mapping, vf->abs_vfid,
				  mbx->vf_addr_hi, mbx->vf_addr_lo,
				  sizeof(union vfpf_tlvs)/4);
	if (rc) {
		BNX2X_ERR("Failed to copy request VF %d\n", vf->abs_vfid);
		goto mbx_error;
	}

	/* process the VF message header */
	mbx->first_tlv = mbx->msg->req.first_tlv;

	/* dispatch the request (will prepare the response) */
	bnx2x_vf_mbx_request(bp, vf, mbx);
	goto mbx_done;

mbx_error:
mbx_done:
	return;
}
