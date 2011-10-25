/*
 * Copyright (c) 2004-2011 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "core.h"
#include "cfg80211.h"
#include "debug.h"
#include "hif-ops.h"
#include "testmode.h"

static unsigned int ath6kl_p2p;

module_param(ath6kl_p2p, uint, 0644);

#define RATETAB_ENT(_rate, _rateid, _flags) {   \
	.bitrate    = (_rate),                  \
	.flags      = (_flags),                 \
	.hw_value   = (_rateid),                \
}

#define CHAN2G(_channel, _freq, _flags) {   \
	.band           = IEEE80211_BAND_2GHZ,  \
	.hw_value       = (_channel),           \
	.center_freq    = (_freq),              \
	.flags          = (_flags),             \
	.max_antenna_gain   = 0,                \
	.max_power      = 30,                   \
}

#define CHAN5G(_channel, _flags) {		    \
	.band           = IEEE80211_BAND_5GHZ,      \
	.hw_value       = (_channel),               \
	.center_freq    = 5000 + (5 * (_channel)),  \
	.flags          = (_flags),                 \
	.max_antenna_gain   = 0,                    \
	.max_power      = 30,                       \
}

static struct ieee80211_rate ath6kl_rates[] = {
	RATETAB_ENT(10, 0x1, 0),
	RATETAB_ENT(20, 0x2, 0),
	RATETAB_ENT(55, 0x4, 0),
	RATETAB_ENT(110, 0x8, 0),
	RATETAB_ENT(60, 0x10, 0),
	RATETAB_ENT(90, 0x20, 0),
	RATETAB_ENT(120, 0x40, 0),
	RATETAB_ENT(180, 0x80, 0),
	RATETAB_ENT(240, 0x100, 0),
	RATETAB_ENT(360, 0x200, 0),
	RATETAB_ENT(480, 0x400, 0),
	RATETAB_ENT(540, 0x800, 0),
};

#define ath6kl_a_rates     (ath6kl_rates + 4)
#define ath6kl_a_rates_size    8
#define ath6kl_g_rates     (ath6kl_rates + 0)
#define ath6kl_g_rates_size    12

static struct ieee80211_channel ath6kl_2ghz_channels[] = {
	CHAN2G(1, 2412, 0),
	CHAN2G(2, 2417, 0),
	CHAN2G(3, 2422, 0),
	CHAN2G(4, 2427, 0),
	CHAN2G(5, 2432, 0),
	CHAN2G(6, 2437, 0),
	CHAN2G(7, 2442, 0),
	CHAN2G(8, 2447, 0),
	CHAN2G(9, 2452, 0),
	CHAN2G(10, 2457, 0),
	CHAN2G(11, 2462, 0),
	CHAN2G(12, 2467, 0),
	CHAN2G(13, 2472, 0),
	CHAN2G(14, 2484, 0),
};

static struct ieee80211_channel ath6kl_5ghz_a_channels[] = {
	CHAN5G(34, 0), CHAN5G(36, 0),
	CHAN5G(38, 0), CHAN5G(40, 0),
	CHAN5G(42, 0), CHAN5G(44, 0),
	CHAN5G(46, 0), CHAN5G(48, 0),
	CHAN5G(52, 0), CHAN5G(56, 0),
	CHAN5G(60, 0), CHAN5G(64, 0),
	CHAN5G(100, 0), CHAN5G(104, 0),
	CHAN5G(108, 0), CHAN5G(112, 0),
	CHAN5G(116, 0), CHAN5G(120, 0),
	CHAN5G(124, 0), CHAN5G(128, 0),
	CHAN5G(132, 0), CHAN5G(136, 0),
	CHAN5G(140, 0), CHAN5G(149, 0),
	CHAN5G(153, 0), CHAN5G(157, 0),
	CHAN5G(161, 0), CHAN5G(165, 0),
	CHAN5G(184, 0), CHAN5G(188, 0),
	CHAN5G(192, 0), CHAN5G(196, 0),
	CHAN5G(200, 0), CHAN5G(204, 0),
	CHAN5G(208, 0), CHAN5G(212, 0),
	CHAN5G(216, 0),
};

static struct ieee80211_supported_band ath6kl_band_2ghz = {
	.n_channels = ARRAY_SIZE(ath6kl_2ghz_channels),
	.channels = ath6kl_2ghz_channels,
	.n_bitrates = ath6kl_g_rates_size,
	.bitrates = ath6kl_g_rates,
};

static struct ieee80211_supported_band ath6kl_band_5ghz = {
	.n_channels = ARRAY_SIZE(ath6kl_5ghz_a_channels),
	.channels = ath6kl_5ghz_a_channels,
	.n_bitrates = ath6kl_a_rates_size,
	.bitrates = ath6kl_a_rates,
};

#define CCKM_KRK_CIPHER_SUITE 0x004096ff /* use for KRK */

static int ath6kl_set_wpa_version(struct ath6kl *ar,
				  enum nl80211_wpa_versions wpa_version)
{
	/* TODO: Findout vif */
	struct ath6kl_vif *vif = ar->vif;

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: %u\n", __func__, wpa_version);

	if (!wpa_version) {
		vif->auth_mode = NONE_AUTH;
	} else if (wpa_version & NL80211_WPA_VERSION_2) {
		vif->auth_mode = WPA2_AUTH;
	} else if (wpa_version & NL80211_WPA_VERSION_1) {
		vif->auth_mode = WPA_AUTH;
	} else {
		ath6kl_err("%s: %u not supported\n", __func__, wpa_version);
		return -ENOTSUPP;
	}

	return 0;
}

static int ath6kl_set_auth_type(struct ath6kl *ar,
				enum nl80211_auth_type auth_type)
{
	/* TODO: Findout vif */
	struct ath6kl_vif *vif = ar->vif;

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: 0x%x\n", __func__, auth_type);

	switch (auth_type) {
	case NL80211_AUTHTYPE_OPEN_SYSTEM:
		vif->dot11_auth_mode = OPEN_AUTH;
		break;
	case NL80211_AUTHTYPE_SHARED_KEY:
		vif->dot11_auth_mode = SHARED_AUTH;
		break;
	case NL80211_AUTHTYPE_NETWORK_EAP:
		vif->dot11_auth_mode = LEAP_AUTH;
		break;

	case NL80211_AUTHTYPE_AUTOMATIC:
		vif->dot11_auth_mode = OPEN_AUTH | SHARED_AUTH;
		break;

	default:
		ath6kl_err("%s: 0x%x not spported\n", __func__, auth_type);
		return -ENOTSUPP;
	}

	return 0;
}

static int ath6kl_set_cipher(struct ath6kl *ar, u32 cipher, bool ucast)
{
	/* TODO: Findout vif */
	struct ath6kl_vif *vif = ar->vif;

	u8 *ar_cipher = ucast ? &vif->prwise_crypto : &vif->grp_crypto;
	u8 *ar_cipher_len = ucast ? &vif->prwise_crypto_len :
		&vif->grp_crypto_len;

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: cipher 0x%x, ucast %u\n",
		   __func__, cipher, ucast);

	switch (cipher) {
	case 0:
		/* our own hack to use value 0 as no crypto used */
		*ar_cipher = NONE_CRYPT;
		*ar_cipher_len = 0;
		break;
	case WLAN_CIPHER_SUITE_WEP40:
		*ar_cipher = WEP_CRYPT;
		*ar_cipher_len = 5;
		break;
	case WLAN_CIPHER_SUITE_WEP104:
		*ar_cipher = WEP_CRYPT;
		*ar_cipher_len = 13;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		*ar_cipher = TKIP_CRYPT;
		*ar_cipher_len = 0;
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		*ar_cipher = AES_CRYPT;
		*ar_cipher_len = 0;
		break;
	default:
		ath6kl_err("cipher 0x%x not supported\n", cipher);
		return -ENOTSUPP;
	}

	return 0;
}

static void ath6kl_set_key_mgmt(struct ath6kl *ar, u32 key_mgmt)
{
	/* TODO: Findout vif */
	struct ath6kl_vif *vif = ar->vif;

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: 0x%x\n", __func__, key_mgmt);

	if (key_mgmt == WLAN_AKM_SUITE_PSK) {
		if (vif->auth_mode == WPA_AUTH)
			vif->auth_mode = WPA_PSK_AUTH;
		else if (vif->auth_mode == WPA2_AUTH)
			vif->auth_mode = WPA2_PSK_AUTH;
	} else if (key_mgmt == 0x00409600) {
		if (vif->auth_mode == WPA_AUTH)
			vif->auth_mode = WPA_AUTH_CCKM;
		else if (vif->auth_mode == WPA2_AUTH)
			vif->auth_mode = WPA2_AUTH_CCKM;
	} else if (key_mgmt != WLAN_AKM_SUITE_8021X) {
		vif->auth_mode = NONE_AUTH;
	}
}

static bool ath6kl_cfg80211_ready(struct ath6kl *ar)
{
	struct ath6kl_vif *vif = ar->vif;

	if (!test_bit(WMI_READY, &ar->flag)) {
		ath6kl_err("wmi is not ready\n");
		return false;
	}

	if (!test_bit(WLAN_ENABLED, &vif->flags)) {
		ath6kl_err("wlan disabled\n");
		return false;
	}

	return true;
}

static bool ath6kl_is_wpa_ie(const u8 *pos)
{
	return pos[0] == WLAN_EID_WPA && pos[1] >= 4 &&
		pos[2] == 0x00 && pos[3] == 0x50 &&
		pos[4] == 0xf2 && pos[5] == 0x01;
}

static bool ath6kl_is_rsn_ie(const u8 *pos)
{
	return pos[0] == WLAN_EID_RSN;
}

static int ath6kl_set_assoc_req_ies(struct ath6kl_vif *vif, const u8 *ies,
				    size_t ies_len)
{
	struct ath6kl *ar = vif->ar;
	const u8 *pos;
	u8 *buf = NULL;
	size_t len = 0;
	int ret;

	/*
	 * Filter out RSN/WPA IE(s)
	 */

	if (ies && ies_len) {
		buf = kmalloc(ies_len, GFP_KERNEL);
		if (buf == NULL)
			return -ENOMEM;
		pos = ies;

		while (pos + 1 < ies + ies_len) {
			if (pos + 2 + pos[1] > ies + ies_len)
				break;
			if (!(ath6kl_is_wpa_ie(pos) || ath6kl_is_rsn_ie(pos))) {
				memcpy(buf + len, pos, 2 + pos[1]);
				len += 2 + pos[1];
			}
			pos += 2 + pos[1];
		}
	}

	ret = ath6kl_wmi_set_appie_cmd(ar->wmi, vif->fw_vif_idx,
				       WMI_FRAME_ASSOC_REQ, buf, len);
	kfree(buf);
	return ret;
}

static int ath6kl_cfg80211_connect(struct wiphy *wiphy, struct net_device *dev,
				   struct cfg80211_connect_params *sme)
{
	struct ath6kl *ar = ath6kl_priv(dev);
	struct ath6kl_vif *vif = netdev_priv(dev);
	int status;

	vif->sme_state = SME_CONNECTING;

	if (!ath6kl_cfg80211_ready(ar))
		return -EIO;

	if (test_bit(DESTROY_IN_PROGRESS, &ar->flag)) {
		ath6kl_err("destroy in progress\n");
		return -EBUSY;
	}

	if (test_bit(SKIP_SCAN, &ar->flag) &&
	    ((sme->channel && sme->channel->center_freq == 0) ||
	     (sme->bssid && is_zero_ether_addr(sme->bssid)))) {
		ath6kl_err("SkipScan: channel or bssid invalid\n");
		return -EINVAL;
	}

	if (down_interruptible(&ar->sem)) {
		ath6kl_err("busy, couldn't get access\n");
		return -ERESTARTSYS;
	}

	if (test_bit(DESTROY_IN_PROGRESS, &ar->flag)) {
		ath6kl_err("busy, destroy in progress\n");
		up(&ar->sem);
		return -EBUSY;
	}

	if (ar->tx_pending[ath6kl_wmi_get_control_ep(ar->wmi)]) {
		/*
		 * sleep until the command queue drains
		 */
		wait_event_interruptible_timeout(ar->event_wq,
			ar->tx_pending[ath6kl_wmi_get_control_ep(ar->wmi)] == 0,
			WMI_TIMEOUT);
		if (signal_pending(current)) {
			ath6kl_err("cmd queue drain timeout\n");
			up(&ar->sem);
			return -EINTR;
		}
	}

	if (sme->ie && (sme->ie_len > 0)) {
		status = ath6kl_set_assoc_req_ies(vif, sme->ie, sme->ie_len);
		if (status)
			return status;
	}

	if (test_bit(CONNECTED, &vif->flags) &&
	    vif->ssid_len == sme->ssid_len &&
	    !memcmp(vif->ssid, sme->ssid, vif->ssid_len)) {
		vif->reconnect_flag = true;
		status = ath6kl_wmi_reconnect_cmd(ar->wmi, vif->fw_vif_idx,
						  vif->req_bssid,
						  vif->ch_hint);

		up(&ar->sem);
		if (status) {
			ath6kl_err("wmi_reconnect_cmd failed\n");
			return -EIO;
		}
		return 0;
	} else if (vif->ssid_len == sme->ssid_len &&
		   !memcmp(vif->ssid, sme->ssid, vif->ssid_len)) {
		ath6kl_disconnect(ar, vif->fw_vif_idx);
	}

	memset(vif->ssid, 0, sizeof(vif->ssid));
	vif->ssid_len = sme->ssid_len;
	memcpy(vif->ssid, sme->ssid, sme->ssid_len);

	if (sme->channel)
		vif->ch_hint = sme->channel->center_freq;

	memset(vif->req_bssid, 0, sizeof(vif->req_bssid));
	if (sme->bssid && !is_broadcast_ether_addr(sme->bssid))
		memcpy(vif->req_bssid, sme->bssid, sizeof(vif->req_bssid));

	ath6kl_set_wpa_version(ar, sme->crypto.wpa_versions);

	status = ath6kl_set_auth_type(ar, sme->auth_type);
	if (status) {
		up(&ar->sem);
		return status;
	}

	if (sme->crypto.n_ciphers_pairwise)
		ath6kl_set_cipher(ar, sme->crypto.ciphers_pairwise[0], true);
	else
		ath6kl_set_cipher(ar, 0, true);

	ath6kl_set_cipher(ar, sme->crypto.cipher_group, false);

	if (sme->crypto.n_akm_suites)
		ath6kl_set_key_mgmt(ar, sme->crypto.akm_suites[0]);

	if ((sme->key_len) &&
	    (vif->auth_mode == NONE_AUTH) &&
	    (vif->prwise_crypto == WEP_CRYPT)) {
		struct ath6kl_key *key = NULL;

		if (sme->key_idx < WMI_MIN_KEY_INDEX ||
		    sme->key_idx > WMI_MAX_KEY_INDEX) {
			ath6kl_err("key index %d out of bounds\n",
				   sme->key_idx);
			up(&ar->sem);
			return -ENOENT;
		}

		key = &vif->keys[sme->key_idx];
		key->key_len = sme->key_len;
		memcpy(key->key, sme->key, key->key_len);
		key->cipher = vif->prwise_crypto;
		vif->def_txkey_index = sme->key_idx;

		ath6kl_wmi_addkey_cmd(ar->wmi, vif->fw_vif_idx, sme->key_idx,
				      vif->prwise_crypto,
				      GROUP_USAGE | TX_USAGE,
				      key->key_len,
				      NULL,
				      key->key, KEY_OP_INIT_VAL, NULL,
				      NO_SYNC_WMIFLAG);
	}

	if (!ar->usr_bss_filter) {
		clear_bit(CLEAR_BSSFILTER_ON_BEACON, &vif->flags);
		if (ath6kl_wmi_bssfilter_cmd(ar->wmi, ALL_BSS_FILTER, 0) != 0) {
			ath6kl_err("couldn't set bss filtering\n");
			up(&ar->sem);
			return -EIO;
		}
	}

	vif->nw_type = vif->next_mode;

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG,
		   "%s: connect called with authmode %d dot11 auth %d"
		   " PW crypto %d PW crypto len %d GRP crypto %d"
		   " GRP crypto len %d channel hint %u\n",
		   __func__,
		   vif->auth_mode, vif->dot11_auth_mode, vif->prwise_crypto,
		   vif->prwise_crypto_len, vif->grp_crypto,
		   vif->grp_crypto_len, vif->ch_hint);

	vif->reconnect_flag = 0;
	status = ath6kl_wmi_connect_cmd(ar->wmi, vif->fw_vif_idx, vif->nw_type,
					vif->dot11_auth_mode, vif->auth_mode,
					vif->prwise_crypto,
					vif->prwise_crypto_len,
					vif->grp_crypto, vif->grp_crypto_len,
					vif->ssid_len, vif->ssid,
					vif->req_bssid, vif->ch_hint,
					ar->connect_ctrl_flags);

	up(&ar->sem);

	if (status == -EINVAL) {
		memset(vif->ssid, 0, sizeof(vif->ssid));
		vif->ssid_len = 0;
		ath6kl_err("invalid request\n");
		return -ENOENT;
	} else if (status) {
		ath6kl_err("ath6kl_wmi_connect_cmd failed\n");
		return -EIO;
	}

	if ((!(ar->connect_ctrl_flags & CONNECT_DO_WPA_OFFLOAD)) &&
	    ((vif->auth_mode == WPA_PSK_AUTH)
	     || (vif->auth_mode == WPA2_PSK_AUTH))) {
		mod_timer(&vif->disconnect_timer,
			  jiffies + msecs_to_jiffies(DISCON_TIMER_INTVAL));
	}

	ar->connect_ctrl_flags &= ~CONNECT_DO_WPA_OFFLOAD;
	set_bit(CONNECT_PEND, &vif->flags);

	return 0;
}

static int ath6kl_add_bss_if_needed(struct ath6kl *ar, const u8 *bssid,
				    struct ieee80211_channel *chan,
				    const u8 *beacon_ie, size_t beacon_ie_len)
{
	/* TODO: Findout vif */
	struct ath6kl_vif *vif = ar->vif;
	struct cfg80211_bss *bss;
	u8 *ie;

	bss = cfg80211_get_bss(ar->wiphy, chan, bssid,
			       vif->ssid, vif->ssid_len, WLAN_CAPABILITY_ESS,
			       WLAN_CAPABILITY_ESS);
	if (bss == NULL) {
		/*
		 * Since cfg80211 may not yet know about the BSS,
		 * generate a partial entry until the first BSS info
		 * event becomes available.
		 *
		 * Prepend SSID element since it is not included in the Beacon
		 * IEs from the target.
		 */
		ie = kmalloc(2 + vif->ssid_len + beacon_ie_len, GFP_KERNEL);
		if (ie == NULL)
			return -ENOMEM;
		ie[0] = WLAN_EID_SSID;
		ie[1] = vif->ssid_len;
		memcpy(ie + 2, vif->ssid, vif->ssid_len);
		memcpy(ie + 2 + vif->ssid_len, beacon_ie, beacon_ie_len);
		bss = cfg80211_inform_bss(ar->wiphy, chan,
					  bssid, 0, WLAN_CAPABILITY_ESS, 100,
					  ie, 2 + vif->ssid_len + beacon_ie_len,
					  0, GFP_KERNEL);
		if (bss)
			ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "added dummy bss for "
				   "%pM prior to indicating connect/roamed "
				   "event\n", bssid);
		kfree(ie);
	} else
		ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "cfg80211 already has a bss "
			   "entry\n");

	if (bss == NULL)
		return -ENOMEM;

	cfg80211_put_bss(bss);

	return 0;
}

void ath6kl_cfg80211_connect_event(struct ath6kl *ar, u16 channel,
				   u8 *bssid, u16 listen_intvl,
				   u16 beacon_intvl,
				   enum network_type nw_type,
				   u8 beacon_ie_len, u8 assoc_req_len,
				   u8 assoc_resp_len, u8 *assoc_info)
{
	struct ieee80211_channel *chan;
	/* TODO: Findout vif */
	struct ath6kl_vif *vif = ar->vif;

	/* capinfo + listen interval */
	u8 assoc_req_ie_offset = sizeof(u16) + sizeof(u16);

	/* capinfo + status code +  associd */
	u8 assoc_resp_ie_offset = sizeof(u16) + sizeof(u16) + sizeof(u16);

	u8 *assoc_req_ie = assoc_info + beacon_ie_len + assoc_req_ie_offset;
	u8 *assoc_resp_ie = assoc_info + beacon_ie_len + assoc_req_len +
	    assoc_resp_ie_offset;

	assoc_req_len -= assoc_req_ie_offset;
	assoc_resp_len -= assoc_resp_ie_offset;

	/*
	 * Store Beacon interval here; DTIM period will be available only once
	 * a Beacon frame from the AP is seen.
	 */
	vif->assoc_bss_beacon_int = beacon_intvl;
	clear_bit(DTIM_PERIOD_AVAIL, &vif->flags);

	if (nw_type & ADHOC_NETWORK) {
		if (ar->wdev->iftype != NL80211_IFTYPE_ADHOC) {
			ath6kl_dbg(ATH6KL_DBG_WLAN_CFG,
				   "%s: ath6k not in ibss mode\n", __func__);
			return;
		}
	}

	if (nw_type & INFRA_NETWORK) {
		if (ar->wdev->iftype != NL80211_IFTYPE_STATION &&
		    ar->wdev->iftype != NL80211_IFTYPE_P2P_CLIENT) {
			ath6kl_dbg(ATH6KL_DBG_WLAN_CFG,
				   "%s: ath6k not in station mode\n", __func__);
			return;
		}
	}

	chan = ieee80211_get_channel(ar->wiphy, (int) channel);


	if (nw_type & ADHOC_NETWORK) {
		cfg80211_ibss_joined(ar->net_dev, bssid, GFP_KERNEL);
		return;
	}

	if (ath6kl_add_bss_if_needed(ar, bssid, chan, assoc_info,
				     beacon_ie_len) < 0) {
		ath6kl_err("could not add cfg80211 bss entry for "
			   "connect/roamed notification\n");
		return;
	}

	if (vif->sme_state == SME_CONNECTING) {
		/* inform connect result to cfg80211 */
		vif->sme_state = SME_CONNECTED;
		cfg80211_connect_result(ar->net_dev, bssid,
					assoc_req_ie, assoc_req_len,
					assoc_resp_ie, assoc_resp_len,
					WLAN_STATUS_SUCCESS, GFP_KERNEL);
	} else if (vif->sme_state == SME_CONNECTED) {
		/* inform roam event to cfg80211 */
		cfg80211_roamed(ar->net_dev, chan, bssid,
				assoc_req_ie, assoc_req_len,
				assoc_resp_ie, assoc_resp_len, GFP_KERNEL);
	}
}

static int ath6kl_cfg80211_disconnect(struct wiphy *wiphy,
				      struct net_device *dev, u16 reason_code)
{
	struct ath6kl *ar = (struct ath6kl *)ath6kl_priv(dev);
	struct ath6kl_vif *vif = netdev_priv(dev);

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: reason=%u\n", __func__,
		   reason_code);

	if (!ath6kl_cfg80211_ready(ar))
		return -EIO;

	if (test_bit(DESTROY_IN_PROGRESS, &ar->flag)) {
		ath6kl_err("busy, destroy in progress\n");
		return -EBUSY;
	}

	if (down_interruptible(&ar->sem)) {
		ath6kl_err("busy, couldn't get access\n");
		return -ERESTARTSYS;
	}

	vif->reconnect_flag = 0;
	ath6kl_disconnect(ar, vif->fw_vif_idx);
	memset(vif->ssid, 0, sizeof(vif->ssid));
	vif->ssid_len = 0;

	if (!test_bit(SKIP_SCAN, &ar->flag))
		memset(vif->req_bssid, 0, sizeof(vif->req_bssid));

	up(&ar->sem);

	vif->sme_state = SME_DISCONNECTED;

	return 0;
}

void ath6kl_cfg80211_disconnect_event(struct ath6kl *ar, u8 reason,
				      u8 *bssid, u8 assoc_resp_len,
				      u8 *assoc_info, u16 proto_reason)
{
	/* TODO: Findout vif */
	struct ath6kl_vif *vif = ar->vif;

	if (vif->scan_req) {
		cfg80211_scan_done(vif->scan_req, true);
		vif->scan_req = NULL;
	}

	if (vif->nw_type & ADHOC_NETWORK) {
		if (ar->wdev->iftype != NL80211_IFTYPE_ADHOC) {
			ath6kl_dbg(ATH6KL_DBG_WLAN_CFG,
				   "%s: ath6k not in ibss mode\n", __func__);
			return;
		}
		memset(bssid, 0, ETH_ALEN);
		cfg80211_ibss_joined(ar->net_dev, bssid, GFP_KERNEL);
		return;
	}

	if (vif->nw_type & INFRA_NETWORK) {
		if (ar->wdev->iftype != NL80211_IFTYPE_STATION &&
		    ar->wdev->iftype != NL80211_IFTYPE_P2P_CLIENT) {
			ath6kl_dbg(ATH6KL_DBG_WLAN_CFG,
				   "%s: ath6k not in station mode\n", __func__);
			return;
		}
	}

	/*
	 * Send a disconnect command to target when a disconnect event is
	 * received with reason code other than 3 (DISCONNECT_CMD - disconnect
	 * request from host) to make the firmware stop trying to connect even
	 * after giving disconnect event. There will be one more disconnect
	 * event for this disconnect command with reason code DISCONNECT_CMD
	 * which will be notified to cfg80211.
	 */

	if (reason != DISCONNECT_CMD) {
		ath6kl_wmi_disconnect_cmd(ar->wmi, vif->fw_vif_idx);
		return;
	}

	clear_bit(CONNECT_PEND, &vif->flags);

	if (vif->sme_state == SME_CONNECTING) {
		cfg80211_connect_result(ar->net_dev,
				bssid, NULL, 0,
				NULL, 0,
				WLAN_STATUS_UNSPECIFIED_FAILURE,
				GFP_KERNEL);
	} else if (vif->sme_state == SME_CONNECTED) {
		cfg80211_disconnected(ar->net_dev, reason,
				NULL, 0, GFP_KERNEL);
	}

	vif->sme_state = SME_DISCONNECTED;
}

static int ath6kl_cfg80211_scan(struct wiphy *wiphy, struct net_device *ndev,
				struct cfg80211_scan_request *request)
{
	struct ath6kl *ar = (struct ath6kl *)ath6kl_priv(ndev);
	struct ath6kl_vif *vif = netdev_priv(ndev);
	s8 n_channels = 0;
	u16 *channels = NULL;
	int ret = 0;
	u32 force_fg_scan = 0;

	if (!ath6kl_cfg80211_ready(ar))
		return -EIO;

	if (!ar->usr_bss_filter) {
		clear_bit(CLEAR_BSSFILTER_ON_BEACON, &vif->flags);
		ret = ath6kl_wmi_bssfilter_cmd(
			ar->wmi,
			(test_bit(CONNECTED, &vif->flags) ?
			 ALL_BUT_BSS_FILTER : ALL_BSS_FILTER), 0);
		if (ret) {
			ath6kl_err("couldn't set bss filtering\n");
			return ret;
		}
	}

	if (request->n_ssids && request->ssids[0].ssid_len) {
		u8 i;

		if (request->n_ssids > (MAX_PROBED_SSID_INDEX - 1))
			request->n_ssids = MAX_PROBED_SSID_INDEX - 1;

		for (i = 0; i < request->n_ssids; i++)
			ath6kl_wmi_probedssid_cmd(ar->wmi, vif->fw_vif_idx,
						  i + 1, SPECIFIC_SSID_FLAG,
						  request->ssids[i].ssid_len,
						  request->ssids[i].ssid);
	}

	if (request->ie) {
		ret = ath6kl_wmi_set_appie_cmd(ar->wmi, vif->fw_vif_idx,
					       WMI_FRAME_PROBE_REQ,
					       request->ie, request->ie_len);
		if (ret) {
			ath6kl_err("failed to set Probe Request appie for "
				   "scan");
			return ret;
		}
	}

	/*
	 * Scan only the requested channels if the request specifies a set of
	 * channels. If the list is longer than the target supports, do not
	 * configure the list and instead, scan all available channels.
	 */
	if (request->n_channels > 0 &&
	    request->n_channels <= WMI_MAX_CHANNELS) {
		u8 i;

		n_channels = request->n_channels;

		channels = kzalloc(n_channels * sizeof(u16), GFP_KERNEL);
		if (channels == NULL) {
			ath6kl_warn("failed to set scan channels, "
				    "scan all channels");
			n_channels = 0;
		}

		for (i = 0; i < n_channels; i++)
			channels[i] = request->channels[i]->center_freq;
	}

	if (test_bit(CONNECTED, &vif->flags))
		force_fg_scan = 1;

	ret = ath6kl_wmi_startscan_cmd(ar->wmi, vif->fw_vif_idx, WMI_LONG_SCAN,
				       force_fg_scan, false, 0, 0, n_channels,
				       channels);
	if (ret)
		ath6kl_err("wmi_startscan_cmd failed\n");
	else
		vif->scan_req = request;

	kfree(channels);

	return ret;
}

void ath6kl_cfg80211_scan_complete_event(struct ath6kl *ar, int status)
{
	/* TODO: Findout vif */
	struct ath6kl_vif *vif = ar->vif;
	int i;

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: status %d\n", __func__, status);

	if (!vif->scan_req)
		return;

	if ((status == -ECANCELED) || (status == -EBUSY)) {
		cfg80211_scan_done(vif->scan_req, true);
		goto out;
	}

	cfg80211_scan_done(vif->scan_req, false);

	if (vif->scan_req->n_ssids && vif->scan_req->ssids[0].ssid_len) {
		for (i = 0; i < vif->scan_req->n_ssids; i++) {
			ath6kl_wmi_probedssid_cmd(ar->wmi, vif->fw_vif_idx,
						  i + 1, DISABLE_SSID_FLAG,
						  0, NULL);
		}
	}

out:
	vif->scan_req = NULL;
}

static int ath6kl_cfg80211_add_key(struct wiphy *wiphy, struct net_device *ndev,
				   u8 key_index, bool pairwise,
				   const u8 *mac_addr,
				   struct key_params *params)
{
	struct ath6kl *ar = (struct ath6kl *)ath6kl_priv(ndev);
	struct ath6kl_vif *vif = netdev_priv(ndev);
	struct ath6kl_key *key = NULL;
	u8 key_usage;
	u8 key_type;
	int status = 0;

	if (!ath6kl_cfg80211_ready(ar))
		return -EIO;

	if (params->cipher == CCKM_KRK_CIPHER_SUITE) {
		if (params->key_len != WMI_KRK_LEN)
			return -EINVAL;
		return ath6kl_wmi_add_krk_cmd(ar->wmi, params->key);
	}

	if (key_index < WMI_MIN_KEY_INDEX || key_index > WMI_MAX_KEY_INDEX) {
		ath6kl_dbg(ATH6KL_DBG_WLAN_CFG,
			   "%s: key index %d out of bounds\n", __func__,
			   key_index);
		return -ENOENT;
	}

	key = &vif->keys[key_index];
	memset(key, 0, sizeof(struct ath6kl_key));

	if (pairwise)
		key_usage = PAIRWISE_USAGE;
	else
		key_usage = GROUP_USAGE;

	if (params) {
		if (params->key_len > WLAN_MAX_KEY_LEN ||
		    params->seq_len > sizeof(key->seq))
			return -EINVAL;

		key->key_len = params->key_len;
		memcpy(key->key, params->key, key->key_len);
		key->seq_len = params->seq_len;
		memcpy(key->seq, params->seq, key->seq_len);
		key->cipher = params->cipher;
	}

	switch (key->cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
		key_type = WEP_CRYPT;
		break;

	case WLAN_CIPHER_SUITE_TKIP:
		key_type = TKIP_CRYPT;
		break;

	case WLAN_CIPHER_SUITE_CCMP:
		key_type = AES_CRYPT;
		break;

	default:
		return -ENOTSUPP;
	}

	if (((vif->auth_mode == WPA_PSK_AUTH)
	     || (vif->auth_mode == WPA2_PSK_AUTH))
	    && (key_usage & GROUP_USAGE))
		del_timer(&vif->disconnect_timer);

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG,
		   "%s: index %d, key_len %d, key_type 0x%x, key_usage 0x%x, seq_len %d\n",
		   __func__, key_index, key->key_len, key_type,
		   key_usage, key->seq_len);

	vif->def_txkey_index = key_index;

	if (vif->nw_type == AP_NETWORK && !pairwise &&
	    (key_type == TKIP_CRYPT || key_type == AES_CRYPT) && params) {
		ar->ap_mode_bkey.valid = true;
		ar->ap_mode_bkey.key_index = key_index;
		ar->ap_mode_bkey.key_type = key_type;
		ar->ap_mode_bkey.key_len = key->key_len;
		memcpy(ar->ap_mode_bkey.key, key->key, key->key_len);
		if (!test_bit(CONNECTED, &vif->flags)) {
			ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "Delay initial group "
				   "key configuration until AP mode has been "
				   "started\n");
			/*
			 * The key will be set in ath6kl_connect_ap_mode() once
			 * the connected event is received from the target.
			 */
			return 0;
		}
	}

	if (vif->next_mode == AP_NETWORK && key_type == WEP_CRYPT &&
	    !test_bit(CONNECTED, &vif->flags)) {
		/*
		 * Store the key locally so that it can be re-configured after
		 * the AP mode has properly started
		 * (ath6kl_install_statioc_wep_keys).
		 */
		ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "Delay WEP key configuration "
			   "until AP mode has been started\n");
		vif->wep_key_list[key_index].key_len = key->key_len;
		memcpy(vif->wep_key_list[key_index].key, key->key,
		       key->key_len);
		return 0;
	}

	status = ath6kl_wmi_addkey_cmd(ar->wmi, vif->fw_vif_idx,
				       vif->def_txkey_index,
				       key_type, key_usage, key->key_len,
				       key->seq, key->key, KEY_OP_INIT_VAL,
				       (u8 *) mac_addr, SYNC_BOTH_WMIFLAG);

	if (status)
		return -EIO;

	return 0;
}

static int ath6kl_cfg80211_del_key(struct wiphy *wiphy, struct net_device *ndev,
				   u8 key_index, bool pairwise,
				   const u8 *mac_addr)
{
	struct ath6kl *ar = (struct ath6kl *)ath6kl_priv(ndev);
	struct ath6kl_vif *vif = netdev_priv(ndev);

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: index %d\n", __func__, key_index);

	if (!ath6kl_cfg80211_ready(ar))
		return -EIO;

	if (key_index < WMI_MIN_KEY_INDEX || key_index > WMI_MAX_KEY_INDEX) {
		ath6kl_dbg(ATH6KL_DBG_WLAN_CFG,
			   "%s: key index %d out of bounds\n", __func__,
			   key_index);
		return -ENOENT;
	}

	if (!vif->keys[key_index].key_len) {
		ath6kl_dbg(ATH6KL_DBG_WLAN_CFG,
			   "%s: index %d is empty\n", __func__, key_index);
		return 0;
	}

	vif->keys[key_index].key_len = 0;

	return ath6kl_wmi_deletekey_cmd(ar->wmi, vif->fw_vif_idx, key_index);
}

static int ath6kl_cfg80211_get_key(struct wiphy *wiphy, struct net_device *ndev,
				   u8 key_index, bool pairwise,
				   const u8 *mac_addr, void *cookie,
				   void (*callback) (void *cookie,
						     struct key_params *))
{
	struct ath6kl *ar = (struct ath6kl *)ath6kl_priv(ndev);
	struct ath6kl_vif *vif = netdev_priv(ndev);
	struct ath6kl_key *key = NULL;
	struct key_params params;

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: index %d\n", __func__, key_index);

	if (!ath6kl_cfg80211_ready(ar))
		return -EIO;

	if (key_index < WMI_MIN_KEY_INDEX || key_index > WMI_MAX_KEY_INDEX) {
		ath6kl_dbg(ATH6KL_DBG_WLAN_CFG,
			   "%s: key index %d out of bounds\n", __func__,
			   key_index);
		return -ENOENT;
	}

	key = &vif->keys[key_index];
	memset(&params, 0, sizeof(params));
	params.cipher = key->cipher;
	params.key_len = key->key_len;
	params.seq_len = key->seq_len;
	params.seq = key->seq;
	params.key = key->key;

	callback(cookie, &params);

	return key->key_len ? 0 : -ENOENT;
}

static int ath6kl_cfg80211_set_default_key(struct wiphy *wiphy,
					   struct net_device *ndev,
					   u8 key_index, bool unicast,
					   bool multicast)
{
	struct ath6kl *ar = (struct ath6kl *)ath6kl_priv(ndev);
	struct ath6kl_vif *vif = netdev_priv(ndev);
	struct ath6kl_key *key = NULL;
	int status = 0;
	u8 key_usage;
	enum crypto_type key_type = NONE_CRYPT;

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: index %d\n", __func__, key_index);

	if (!ath6kl_cfg80211_ready(ar))
		return -EIO;

	if (key_index < WMI_MIN_KEY_INDEX || key_index > WMI_MAX_KEY_INDEX) {
		ath6kl_dbg(ATH6KL_DBG_WLAN_CFG,
			   "%s: key index %d out of bounds\n",
			   __func__, key_index);
		return -ENOENT;
	}

	if (!vif->keys[key_index].key_len) {
		ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: invalid key index %d\n",
			   __func__, key_index);
		return -EINVAL;
	}

	vif->def_txkey_index = key_index;
	key = &vif->keys[vif->def_txkey_index];
	key_usage = GROUP_USAGE;
	if (vif->prwise_crypto == WEP_CRYPT)
		key_usage |= TX_USAGE;
	if (unicast)
		key_type = vif->prwise_crypto;
	if (multicast)
		key_type = vif->grp_crypto;

	if (vif->next_mode == AP_NETWORK && !test_bit(CONNECTED, &vif->flags))
		return 0; /* Delay until AP mode has been started */

	status = ath6kl_wmi_addkey_cmd(ar->wmi, vif->fw_vif_idx,
				       vif->def_txkey_index,
				       key_type, key_usage,
				       key->key_len, key->seq, key->key,
				       KEY_OP_INIT_VAL, NULL,
				       SYNC_BOTH_WMIFLAG);
	if (status)
		return -EIO;

	return 0;
}

void ath6kl_cfg80211_tkip_micerr_event(struct ath6kl *ar, u8 keyid,
				       bool ismcast)
{
	/* TODO: Findout vif */
	struct ath6kl_vif *vif = ar->vif;

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG,
		   "%s: keyid %d, ismcast %d\n", __func__, keyid, ismcast);

	cfg80211_michael_mic_failure(ar->net_dev, vif->bssid,
				     (ismcast ? NL80211_KEYTYPE_GROUP :
				      NL80211_KEYTYPE_PAIRWISE), keyid, NULL,
				     GFP_KERNEL);
}

static int ath6kl_cfg80211_set_wiphy_params(struct wiphy *wiphy, u32 changed)
{
	struct ath6kl *ar = (struct ath6kl *)wiphy_priv(wiphy);
	int ret;

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: changed 0x%x\n", __func__,
		   changed);

	if (!ath6kl_cfg80211_ready(ar))
		return -EIO;

	if (changed & WIPHY_PARAM_RTS_THRESHOLD) {
		ret = ath6kl_wmi_set_rts_cmd(ar->wmi, wiphy->rts_threshold);
		if (ret != 0) {
			ath6kl_err("ath6kl_wmi_set_rts_cmd failed\n");
			return -EIO;
		}
	}

	return 0;
}

/*
 * The type nl80211_tx_power_setting replaces the following
 * data type from 2.6.36 onwards
*/
static int ath6kl_cfg80211_set_txpower(struct wiphy *wiphy,
				       enum nl80211_tx_power_setting type,
				       int dbm)
{
	struct ath6kl *ar = (struct ath6kl *)wiphy_priv(wiphy);
	u8 ath6kl_dbm;

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: type 0x%x, dbm %d\n", __func__,
		   type, dbm);

	if (!ath6kl_cfg80211_ready(ar))
		return -EIO;

	switch (type) {
	case NL80211_TX_POWER_AUTOMATIC:
		return 0;
	case NL80211_TX_POWER_LIMITED:
		ar->tx_pwr = ath6kl_dbm = dbm;
		break;
	default:
		ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: type 0x%x not supported\n",
			   __func__, type);
		return -EOPNOTSUPP;
	}

	ath6kl_wmi_set_tx_pwr_cmd(ar->wmi, ath6kl_dbm);

	return 0;
}

static int ath6kl_cfg80211_get_txpower(struct wiphy *wiphy, int *dbm)
{
	struct ath6kl *ar = (struct ath6kl *)wiphy_priv(wiphy);
	struct ath6kl_vif *vif = ar->vif;

	if (!ath6kl_cfg80211_ready(ar))
		return -EIO;

	if (test_bit(CONNECTED, &vif->flags)) {
		ar->tx_pwr = 0;

		if (ath6kl_wmi_get_tx_pwr_cmd(ar->wmi) != 0) {
			ath6kl_err("ath6kl_wmi_get_tx_pwr_cmd failed\n");
			return -EIO;
		}

		wait_event_interruptible_timeout(ar->event_wq, ar->tx_pwr != 0,
						 5 * HZ);

		if (signal_pending(current)) {
			ath6kl_err("target did not respond\n");
			return -EINTR;
		}
	}

	*dbm = ar->tx_pwr;
	return 0;
}

static int ath6kl_cfg80211_set_power_mgmt(struct wiphy *wiphy,
					  struct net_device *dev,
					  bool pmgmt, int timeout)
{
	struct ath6kl *ar = ath6kl_priv(dev);
	struct wmi_power_mode_cmd mode;
	struct ath6kl_vif *vif = netdev_priv(dev);

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: pmgmt %d, timeout %d\n",
		   __func__, pmgmt, timeout);

	if (!ath6kl_cfg80211_ready(ar))
		return -EIO;

	if (pmgmt) {
		ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: max perf\n", __func__);
		mode.pwr_mode = REC_POWER;
	} else {
		ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: rec power\n", __func__);
		mode.pwr_mode = MAX_PERF_POWER;
	}

	if (ath6kl_wmi_powermode_cmd(ar->wmi, vif->fw_vif_idx,
	     mode.pwr_mode) != 0) {
		ath6kl_err("wmi_powermode_cmd failed\n");
		return -EIO;
	}

	return 0;
}

static int ath6kl_cfg80211_change_iface(struct wiphy *wiphy,
					struct net_device *ndev,
					enum nl80211_iftype type, u32 *flags,
					struct vif_params *params)
{
	struct ath6kl *ar = ath6kl_priv(ndev);
	struct wireless_dev *wdev = ar->wdev;
	struct ath6kl_vif *vif = netdev_priv(ndev);

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: type %u\n", __func__, type);

	if (!ath6kl_cfg80211_ready(ar))
		return -EIO;

	switch (type) {
	case NL80211_IFTYPE_STATION:
		vif->next_mode = INFRA_NETWORK;
		break;
	case NL80211_IFTYPE_ADHOC:
		vif->next_mode = ADHOC_NETWORK;
		break;
	case NL80211_IFTYPE_AP:
		vif->next_mode = AP_NETWORK;
		break;
	case NL80211_IFTYPE_P2P_CLIENT:
		vif->next_mode = INFRA_NETWORK;
		break;
	case NL80211_IFTYPE_P2P_GO:
		vif->next_mode = AP_NETWORK;
		break;
	default:
		ath6kl_err("invalid interface type %u\n", type);
		return -EOPNOTSUPP;
	}

	wdev->iftype = type;

	return 0;
}

static int ath6kl_cfg80211_join_ibss(struct wiphy *wiphy,
				     struct net_device *dev,
				     struct cfg80211_ibss_params *ibss_param)
{
	struct ath6kl *ar = ath6kl_priv(dev);
	struct ath6kl_vif *vif = netdev_priv(dev);
	int status;

	if (!ath6kl_cfg80211_ready(ar))
		return -EIO;

	vif->ssid_len = ibss_param->ssid_len;
	memcpy(vif->ssid, ibss_param->ssid, vif->ssid_len);

	if (ibss_param->channel)
		vif->ch_hint = ibss_param->channel->center_freq;

	if (ibss_param->channel_fixed) {
		/*
		 * TODO: channel_fixed: The channel should be fixed, do not
		 * search for IBSSs to join on other channels. Target
		 * firmware does not support this feature, needs to be
		 * updated.
		 */
		return -EOPNOTSUPP;
	}

	memset(vif->req_bssid, 0, sizeof(vif->req_bssid));
	if (ibss_param->bssid && !is_broadcast_ether_addr(ibss_param->bssid))
		memcpy(vif->req_bssid, ibss_param->bssid,
		       sizeof(vif->req_bssid));

	ath6kl_set_wpa_version(ar, 0);

	status = ath6kl_set_auth_type(ar, NL80211_AUTHTYPE_OPEN_SYSTEM);
	if (status)
		return status;

	if (ibss_param->privacy) {
		ath6kl_set_cipher(ar, WLAN_CIPHER_SUITE_WEP40, true);
		ath6kl_set_cipher(ar, WLAN_CIPHER_SUITE_WEP40, false);
	} else {
		ath6kl_set_cipher(ar, 0, true);
		ath6kl_set_cipher(ar, 0, false);
	}

	vif->nw_type = vif->next_mode;

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG,
		   "%s: connect called with authmode %d dot11 auth %d"
		   " PW crypto %d PW crypto len %d GRP crypto %d"
		   " GRP crypto len %d channel hint %u\n",
		   __func__,
		   vif->auth_mode, vif->dot11_auth_mode, vif->prwise_crypto,
		   vif->prwise_crypto_len, vif->grp_crypto,
		   vif->grp_crypto_len, vif->ch_hint);

	status = ath6kl_wmi_connect_cmd(ar->wmi, vif->fw_vif_idx, vif->nw_type,
					vif->dot11_auth_mode, vif->auth_mode,
					vif->prwise_crypto,
					vif->prwise_crypto_len,
					vif->grp_crypto, vif->grp_crypto_len,
					vif->ssid_len, vif->ssid,
					vif->req_bssid, vif->ch_hint,
					ar->connect_ctrl_flags);
	set_bit(CONNECT_PEND, &vif->flags);

	return 0;
}

static int ath6kl_cfg80211_leave_ibss(struct wiphy *wiphy,
				      struct net_device *dev)
{
	struct ath6kl *ar = (struct ath6kl *)ath6kl_priv(dev);
	struct ath6kl_vif *vif = netdev_priv(dev);

	if (!ath6kl_cfg80211_ready(ar))
		return -EIO;

	ath6kl_disconnect(ar, vif->fw_vif_idx);
	memset(vif->ssid, 0, sizeof(vif->ssid));
	vif->ssid_len = 0;

	return 0;
}

static const u32 cipher_suites[] = {
	WLAN_CIPHER_SUITE_WEP40,
	WLAN_CIPHER_SUITE_WEP104,
	WLAN_CIPHER_SUITE_TKIP,
	WLAN_CIPHER_SUITE_CCMP,
	CCKM_KRK_CIPHER_SUITE,
};

static bool is_rate_legacy(s32 rate)
{
	static const s32 legacy[] = { 1000, 2000, 5500, 11000,
		6000, 9000, 12000, 18000, 24000,
		36000, 48000, 54000
	};
	u8 i;

	for (i = 0; i < ARRAY_SIZE(legacy); i++)
		if (rate == legacy[i])
			return true;

	return false;
}

static bool is_rate_ht20(s32 rate, u8 *mcs, bool *sgi)
{
	static const s32 ht20[] = { 6500, 13000, 19500, 26000, 39000,
		52000, 58500, 65000, 72200
	};
	u8 i;

	for (i = 0; i < ARRAY_SIZE(ht20); i++) {
		if (rate == ht20[i]) {
			if (i == ARRAY_SIZE(ht20) - 1)
				/* last rate uses sgi */
				*sgi = true;
			else
				*sgi = false;

			*mcs = i;
			return true;
		}
	}
	return false;
}

static bool is_rate_ht40(s32 rate, u8 *mcs, bool *sgi)
{
	static const s32 ht40[] = { 13500, 27000, 40500, 54000,
		81000, 108000, 121500, 135000,
		150000
	};
	u8 i;

	for (i = 0; i < ARRAY_SIZE(ht40); i++) {
		if (rate == ht40[i]) {
			if (i == ARRAY_SIZE(ht40) - 1)
				/* last rate uses sgi */
				*sgi = true;
			else
				*sgi = false;

			*mcs = i;
			return true;
		}
	}

	return false;
}

static int ath6kl_get_station(struct wiphy *wiphy, struct net_device *dev,
			      u8 *mac, struct station_info *sinfo)
{
	struct ath6kl *ar = ath6kl_priv(dev);
	struct ath6kl_vif *vif = netdev_priv(dev);
	long left;
	bool sgi;
	s32 rate;
	int ret;
	u8 mcs;

	if (memcmp(mac, vif->bssid, ETH_ALEN) != 0)
		return -ENOENT;

	if (down_interruptible(&ar->sem))
		return -EBUSY;

	set_bit(STATS_UPDATE_PEND, &vif->flags);

	ret = ath6kl_wmi_get_stats_cmd(ar->wmi, vif->fw_vif_idx);

	if (ret != 0) {
		up(&ar->sem);
		return -EIO;
	}

	left = wait_event_interruptible_timeout(ar->event_wq,
						!test_bit(STATS_UPDATE_PEND,
							  &vif->flags),
						WMI_TIMEOUT);

	up(&ar->sem);

	if (left == 0)
		return -ETIMEDOUT;
	else if (left < 0)
		return left;

	if (vif->target_stats.rx_byte) {
		sinfo->rx_bytes = vif->target_stats.rx_byte;
		sinfo->filled |= STATION_INFO_RX_BYTES;
		sinfo->rx_packets = vif->target_stats.rx_pkt;
		sinfo->filled |= STATION_INFO_RX_PACKETS;
	}

	if (vif->target_stats.tx_byte) {
		sinfo->tx_bytes = vif->target_stats.tx_byte;
		sinfo->filled |= STATION_INFO_TX_BYTES;
		sinfo->tx_packets = vif->target_stats.tx_pkt;
		sinfo->filled |= STATION_INFO_TX_PACKETS;
	}

	sinfo->signal = vif->target_stats.cs_rssi;
	sinfo->filled |= STATION_INFO_SIGNAL;

	rate = vif->target_stats.tx_ucast_rate;

	if (is_rate_legacy(rate)) {
		sinfo->txrate.legacy = rate / 100;
	} else if (is_rate_ht20(rate, &mcs, &sgi)) {
		if (sgi) {
			sinfo->txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
			sinfo->txrate.mcs = mcs - 1;
		} else {
			sinfo->txrate.mcs = mcs;
		}

		sinfo->txrate.flags |= RATE_INFO_FLAGS_MCS;
	} else if (is_rate_ht40(rate, &mcs, &sgi)) {
		if (sgi) {
			sinfo->txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
			sinfo->txrate.mcs = mcs - 1;
		} else {
			sinfo->txrate.mcs = mcs;
		}

		sinfo->txrate.flags |= RATE_INFO_FLAGS_40_MHZ_WIDTH;
		sinfo->txrate.flags |= RATE_INFO_FLAGS_MCS;
	} else {
		ath6kl_dbg(ATH6KL_DBG_WLAN_CFG,
			   "invalid rate from stats: %d\n", rate);
		ath6kl_debug_war(ar, ATH6KL_WAR_INVALID_RATE);
		return 0;
	}

	sinfo->filled |= STATION_INFO_TX_BITRATE;

	if (test_bit(CONNECTED, &vif->flags) &&
	    test_bit(DTIM_PERIOD_AVAIL, &vif->flags) &&
	    vif->nw_type == INFRA_NETWORK) {
		sinfo->filled |= STATION_INFO_BSS_PARAM;
		sinfo->bss_param.flags = 0;
		sinfo->bss_param.dtim_period = vif->assoc_bss_dtim_period;
		sinfo->bss_param.beacon_interval = vif->assoc_bss_beacon_int;
	}

	return 0;
}

static int ath6kl_set_pmksa(struct wiphy *wiphy, struct net_device *netdev,
			    struct cfg80211_pmksa *pmksa)
{
	struct ath6kl *ar = ath6kl_priv(netdev);
	struct ath6kl_vif *vif = netdev_priv(netdev);

	return ath6kl_wmi_setpmkid_cmd(ar->wmi, vif->fw_vif_idx, pmksa->bssid,
				       pmksa->pmkid, true);
}

static int ath6kl_del_pmksa(struct wiphy *wiphy, struct net_device *netdev,
			    struct cfg80211_pmksa *pmksa)
{
	struct ath6kl *ar = ath6kl_priv(netdev);
	struct ath6kl_vif *vif = netdev_priv(netdev);

	return ath6kl_wmi_setpmkid_cmd(ar->wmi, vif->fw_vif_idx, pmksa->bssid,
				       pmksa->pmkid, false);
}

static int ath6kl_flush_pmksa(struct wiphy *wiphy, struct net_device *netdev)
{
	struct ath6kl *ar = ath6kl_priv(netdev);
	struct ath6kl_vif *vif = netdev_priv(netdev);

	if (test_bit(CONNECTED, &vif->flags))
		return ath6kl_wmi_setpmkid_cmd(ar->wmi, vif->fw_vif_idx,
					       vif->bssid, NULL, false);
	return 0;
}

#ifdef CONFIG_PM
static int ar6k_cfg80211_suspend(struct wiphy *wiphy,
				 struct cfg80211_wowlan *wow)
{
	struct ath6kl *ar = wiphy_priv(wiphy);

	return ath6kl_hif_suspend(ar);
}

static int ar6k_cfg80211_resume(struct wiphy *wiphy)
{
	struct ath6kl *ar = wiphy_priv(wiphy);

	return ath6kl_hif_resume(ar);
}
#endif

static int ath6kl_set_channel(struct wiphy *wiphy, struct net_device *dev,
			      struct ieee80211_channel *chan,
			      enum nl80211_channel_type channel_type)
{
	struct ath6kl *ar = ath6kl_priv(dev);
	struct ath6kl_vif *vif = netdev_priv(dev);

	if (!ath6kl_cfg80211_ready(ar))
		return -EIO;

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: center_freq=%u hw_value=%u\n",
		   __func__, chan->center_freq, chan->hw_value);
	vif->next_chan = chan->center_freq;

	return 0;
}

static bool ath6kl_is_p2p_ie(const u8 *pos)
{
	return pos[0] == WLAN_EID_VENDOR_SPECIFIC && pos[1] >= 4 &&
		pos[2] == 0x50 && pos[3] == 0x6f &&
		pos[4] == 0x9a && pos[5] == 0x09;
}

static int ath6kl_set_ap_probe_resp_ies(struct ath6kl_vif *vif,
					const u8 *ies, size_t ies_len)
{
	struct ath6kl *ar = vif->ar;
	const u8 *pos;
	u8 *buf = NULL;
	size_t len = 0;
	int ret;

	/*
	 * Filter out P2P IE(s) since they will be included depending on
	 * the Probe Request frame in ath6kl_send_go_probe_resp().
	 */

	if (ies && ies_len) {
		buf = kmalloc(ies_len, GFP_KERNEL);
		if (buf == NULL)
			return -ENOMEM;
		pos = ies;
		while (pos + 1 < ies + ies_len) {
			if (pos + 2 + pos[1] > ies + ies_len)
				break;
			if (!ath6kl_is_p2p_ie(pos)) {
				memcpy(buf + len, pos, 2 + pos[1]);
				len += 2 + pos[1];
			}
			pos += 2 + pos[1];
		}
	}

	ret = ath6kl_wmi_set_appie_cmd(ar->wmi, vif->fw_vif_idx,
				       WMI_FRAME_PROBE_RESP, buf, len);
	kfree(buf);
	return ret;
}

static int ath6kl_ap_beacon(struct wiphy *wiphy, struct net_device *dev,
			    struct beacon_parameters *info, bool add)
{
	struct ath6kl *ar = ath6kl_priv(dev);
	struct ath6kl_vif *vif = netdev_priv(dev);
	struct ieee80211_mgmt *mgmt;
	u8 *ies;
	int ies_len;
	struct wmi_connect_cmd p;
	int res;
	int i;

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: add=%d\n", __func__, add);

	if (!ath6kl_cfg80211_ready(ar))
		return -EIO;

	if (vif->next_mode != AP_NETWORK)
		return -EOPNOTSUPP;

	if (info->beacon_ies) {
		res = ath6kl_wmi_set_appie_cmd(ar->wmi, vif->fw_vif_idx,
					       WMI_FRAME_BEACON,
					       info->beacon_ies,
					       info->beacon_ies_len);
		if (res)
			return res;
	}
	if (info->proberesp_ies) {
		res = ath6kl_set_ap_probe_resp_ies(vif, info->proberesp_ies,
						   info->proberesp_ies_len);
		if (res)
			return res;
	}
	if (info->assocresp_ies) {
		res = ath6kl_wmi_set_appie_cmd(ar->wmi, vif->fw_vif_idx,
					       WMI_FRAME_ASSOC_RESP,
					       info->assocresp_ies,
					       info->assocresp_ies_len);
		if (res)
			return res;
	}

	if (!add)
		return 0;

	ar->ap_mode_bkey.valid = false;

	/* TODO:
	 * info->interval
	 * info->dtim_period
	 */

	if (info->head == NULL)
		return -EINVAL;
	mgmt = (struct ieee80211_mgmt *) info->head;
	ies = mgmt->u.beacon.variable;
	if (ies > info->head + info->head_len)
		return -EINVAL;
	ies_len = info->head + info->head_len - ies;

	if (info->ssid == NULL)
		return -EINVAL;
	memcpy(vif->ssid, info->ssid, info->ssid_len);
	vif->ssid_len = info->ssid_len;
	if (info->hidden_ssid != NL80211_HIDDEN_SSID_NOT_IN_USE)
		return -EOPNOTSUPP; /* TODO */

	vif->dot11_auth_mode = OPEN_AUTH;

	memset(&p, 0, sizeof(p));

	for (i = 0; i < info->crypto.n_akm_suites; i++) {
		switch (info->crypto.akm_suites[i]) {
		case WLAN_AKM_SUITE_8021X:
			if (info->crypto.wpa_versions & NL80211_WPA_VERSION_1)
				p.auth_mode |= WPA_AUTH;
			if (info->crypto.wpa_versions & NL80211_WPA_VERSION_2)
				p.auth_mode |= WPA2_AUTH;
			break;
		case WLAN_AKM_SUITE_PSK:
			if (info->crypto.wpa_versions & NL80211_WPA_VERSION_1)
				p.auth_mode |= WPA_PSK_AUTH;
			if (info->crypto.wpa_versions & NL80211_WPA_VERSION_2)
				p.auth_mode |= WPA2_PSK_AUTH;
			break;
		}
	}
	if (p.auth_mode == 0)
		p.auth_mode = NONE_AUTH;
	vif->auth_mode = p.auth_mode;

	for (i = 0; i < info->crypto.n_ciphers_pairwise; i++) {
		switch (info->crypto.ciphers_pairwise[i]) {
		case WLAN_CIPHER_SUITE_WEP40:
		case WLAN_CIPHER_SUITE_WEP104:
			p.prwise_crypto_type |= WEP_CRYPT;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			p.prwise_crypto_type |= TKIP_CRYPT;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			p.prwise_crypto_type |= AES_CRYPT;
			break;
		}
	}
	if (p.prwise_crypto_type == 0) {
		p.prwise_crypto_type = NONE_CRYPT;
		ath6kl_set_cipher(ar, 0, true);
	} else if (info->crypto.n_ciphers_pairwise == 1)
		ath6kl_set_cipher(ar, info->crypto.ciphers_pairwise[0], true);

	switch (info->crypto.cipher_group) {
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
		p.grp_crypto_type = WEP_CRYPT;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
		p.grp_crypto_type = TKIP_CRYPT;
		break;
	case WLAN_CIPHER_SUITE_CCMP:
		p.grp_crypto_type = AES_CRYPT;
		break;
	default:
		p.grp_crypto_type = NONE_CRYPT;
		break;
	}
	ath6kl_set_cipher(ar, info->crypto.cipher_group, false);

	p.nw_type = AP_NETWORK;
	vif->nw_type = vif->next_mode;

	p.ssid_len = vif->ssid_len;
	memcpy(p.ssid, vif->ssid, vif->ssid_len);
	p.dot11_auth_mode = vif->dot11_auth_mode;
	p.ch = cpu_to_le16(vif->next_chan);

	res = ath6kl_wmi_ap_profile_commit(ar->wmi, vif->fw_vif_idx, &p);
	if (res < 0)
		return res;

	return 0;
}

static int ath6kl_add_beacon(struct wiphy *wiphy, struct net_device *dev,
			     struct beacon_parameters *info)
{
	return ath6kl_ap_beacon(wiphy, dev, info, true);
}

static int ath6kl_set_beacon(struct wiphy *wiphy, struct net_device *dev,
			     struct beacon_parameters *info)
{
	return ath6kl_ap_beacon(wiphy, dev, info, false);
}

static int ath6kl_del_beacon(struct wiphy *wiphy, struct net_device *dev)
{
	struct ath6kl *ar = ath6kl_priv(dev);
	struct ath6kl_vif *vif = netdev_priv(dev);

	if (vif->nw_type != AP_NETWORK)
		return -EOPNOTSUPP;
	if (!test_bit(CONNECTED, &vif->flags))
		return -ENOTCONN;

	ath6kl_wmi_disconnect_cmd(ar->wmi, vif->fw_vif_idx);
	clear_bit(CONNECTED, &vif->flags);

	return 0;
}

static int ath6kl_change_station(struct wiphy *wiphy, struct net_device *dev,
				 u8 *mac, struct station_parameters *params)
{
	struct ath6kl *ar = ath6kl_priv(dev);
	struct ath6kl_vif *vif = netdev_priv(dev);

	if (vif->nw_type != AP_NETWORK)
		return -EOPNOTSUPP;

	/* Use this only for authorizing/unauthorizing a station */
	if (!(params->sta_flags_mask & BIT(NL80211_STA_FLAG_AUTHORIZED)))
		return -EOPNOTSUPP;

	if (params->sta_flags_set & BIT(NL80211_STA_FLAG_AUTHORIZED))
		return ath6kl_wmi_ap_set_mlme(ar->wmi, vif->fw_vif_idx,
					      WMI_AP_MLME_AUTHORIZE, mac, 0);
	return ath6kl_wmi_ap_set_mlme(ar->wmi, vif->fw_vif_idx,
				      WMI_AP_MLME_UNAUTHORIZE, mac, 0);
}

static int ath6kl_remain_on_channel(struct wiphy *wiphy,
				    struct net_device *dev,
				    struct ieee80211_channel *chan,
				    enum nl80211_channel_type channel_type,
				    unsigned int duration,
				    u64 *cookie)
{
	struct ath6kl *ar = ath6kl_priv(dev);
	struct ath6kl_vif *vif = netdev_priv(dev);

	/* TODO: if already pending or ongoing remain-on-channel,
	 * return -EBUSY */
	*cookie = 1; /* only a single pending request is supported */

	return ath6kl_wmi_remain_on_chnl_cmd(ar->wmi, vif->fw_vif_idx,
					     chan->center_freq, duration);
}

static int ath6kl_cancel_remain_on_channel(struct wiphy *wiphy,
					   struct net_device *dev,
					   u64 cookie)
{
	struct ath6kl *ar = ath6kl_priv(dev);
	struct ath6kl_vif *vif = netdev_priv(dev);

	if (cookie != 1)
		return -ENOENT;

	return ath6kl_wmi_cancel_remain_on_chnl_cmd(ar->wmi, vif->fw_vif_idx);
}

static int ath6kl_send_go_probe_resp(struct ath6kl_vif *vif,
				     const u8 *buf, size_t len,
				     unsigned int freq)
{
	struct ath6kl *ar = vif->ar;
	const u8 *pos;
	u8 *p2p;
	int p2p_len;
	int ret;
	const struct ieee80211_mgmt *mgmt;

	mgmt = (const struct ieee80211_mgmt *) buf;

	/* Include P2P IE(s) from the frame generated in user space. */

	p2p = kmalloc(len, GFP_KERNEL);
	if (p2p == NULL)
		return -ENOMEM;
	p2p_len = 0;

	pos = mgmt->u.probe_resp.variable;
	while (pos + 1 < buf + len) {
		if (pos + 2 + pos[1] > buf + len)
			break;
		if (ath6kl_is_p2p_ie(pos)) {
			memcpy(p2p + p2p_len, pos, 2 + pos[1]);
			p2p_len += 2 + pos[1];
		}
		pos += 2 + pos[1];
	}

	ret = ath6kl_wmi_send_probe_response_cmd(ar->wmi, vif->fw_vif_idx, freq,
						 mgmt->da, p2p, p2p_len);
	kfree(p2p);
	return ret;
}

static int ath6kl_mgmt_tx(struct wiphy *wiphy, struct net_device *dev,
			  struct ieee80211_channel *chan, bool offchan,
			  enum nl80211_channel_type channel_type,
			  bool channel_type_valid, unsigned int wait,
			  const u8 *buf, size_t len, bool no_cck,
			  bool dont_wait_for_ack, u64 *cookie)
{
	struct ath6kl *ar = ath6kl_priv(dev);
	struct ath6kl_vif *vif = netdev_priv(dev);
	u32 id;
	const struct ieee80211_mgmt *mgmt;

	mgmt = (const struct ieee80211_mgmt *) buf;
	if (buf + len >= mgmt->u.probe_resp.variable &&
	    vif->nw_type == AP_NETWORK && test_bit(CONNECTED, &vif->flags) &&
	    ieee80211_is_probe_resp(mgmt->frame_control)) {
		/*
		 * Send Probe Response frame in AP mode using a separate WMI
		 * command to allow the target to fill in the generic IEs.
		 */
		*cookie = 0; /* TX status not supported */
		return ath6kl_send_go_probe_resp(vif, buf, len,
						 chan->center_freq);
	}

	id = vif->send_action_id++;
	if (id == 0) {
		/*
		 * 0 is a reserved value in the WMI command and shall not be
		 * used for the command.
		 */
		id = vif->send_action_id++;
	}

	*cookie = id;
	return ath6kl_wmi_send_action_cmd(ar->wmi, vif->fw_vif_idx, id,
					  chan->center_freq, wait,
					  buf, len);
}

static void ath6kl_mgmt_frame_register(struct wiphy *wiphy,
				       struct net_device *dev,
				       u16 frame_type, bool reg)
{
	struct ath6kl_vif *vif = netdev_priv(dev);

	ath6kl_dbg(ATH6KL_DBG_WLAN_CFG, "%s: frame_type=0x%x reg=%d\n",
		   __func__, frame_type, reg);
	if (frame_type == IEEE80211_STYPE_PROBE_REQ) {
		/*
		 * Note: This notification callback is not allowed to sleep, so
		 * we cannot send WMI_PROBE_REQ_REPORT_CMD here. Instead, we
		 * hardcode target to report Probe Request frames all the time.
		 */
		vif->probe_req_report = reg;
	}
}

static const struct ieee80211_txrx_stypes
ath6kl_mgmt_stypes[NUM_NL80211_IFTYPES] = {
	[NL80211_IFTYPE_STATION] = {
		.tx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		BIT(IEEE80211_STYPE_PROBE_RESP >> 4),
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
	},
	[NL80211_IFTYPE_P2P_CLIENT] = {
		.tx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		BIT(IEEE80211_STYPE_PROBE_RESP >> 4),
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
	},
	[NL80211_IFTYPE_P2P_GO] = {
		.tx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		BIT(IEEE80211_STYPE_PROBE_RESP >> 4),
		.rx = BIT(IEEE80211_STYPE_ACTION >> 4) |
		BIT(IEEE80211_STYPE_PROBE_REQ >> 4)
	},
};

static struct cfg80211_ops ath6kl_cfg80211_ops = {
	.change_virtual_intf = ath6kl_cfg80211_change_iface,
	.scan = ath6kl_cfg80211_scan,
	.connect = ath6kl_cfg80211_connect,
	.disconnect = ath6kl_cfg80211_disconnect,
	.add_key = ath6kl_cfg80211_add_key,
	.get_key = ath6kl_cfg80211_get_key,
	.del_key = ath6kl_cfg80211_del_key,
	.set_default_key = ath6kl_cfg80211_set_default_key,
	.set_wiphy_params = ath6kl_cfg80211_set_wiphy_params,
	.set_tx_power = ath6kl_cfg80211_set_txpower,
	.get_tx_power = ath6kl_cfg80211_get_txpower,
	.set_power_mgmt = ath6kl_cfg80211_set_power_mgmt,
	.join_ibss = ath6kl_cfg80211_join_ibss,
	.leave_ibss = ath6kl_cfg80211_leave_ibss,
	.get_station = ath6kl_get_station,
	.set_pmksa = ath6kl_set_pmksa,
	.del_pmksa = ath6kl_del_pmksa,
	.flush_pmksa = ath6kl_flush_pmksa,
	CFG80211_TESTMODE_CMD(ath6kl_tm_cmd)
#ifdef CONFIG_PM
	.suspend = ar6k_cfg80211_suspend,
	.resume = ar6k_cfg80211_resume,
#endif
	.set_channel = ath6kl_set_channel,
	.add_beacon = ath6kl_add_beacon,
	.set_beacon = ath6kl_set_beacon,
	.del_beacon = ath6kl_del_beacon,
	.change_station = ath6kl_change_station,
	.remain_on_channel = ath6kl_remain_on_channel,
	.cancel_remain_on_channel = ath6kl_cancel_remain_on_channel,
	.mgmt_tx = ath6kl_mgmt_tx,
	.mgmt_frame_register = ath6kl_mgmt_frame_register,
};

struct ath6kl *ath6kl_core_alloc(struct device *dev)
{
	struct ath6kl *ar;
	struct wiphy *wiphy;
	u8 ctr;

	/* create a new wiphy for use with cfg80211 */
	wiphy = wiphy_new(&ath6kl_cfg80211_ops, sizeof(struct ath6kl));

	if (!wiphy) {
		ath6kl_err("couldn't allocate wiphy device\n");
		return NULL;
	}

	ar = wiphy_priv(wiphy);
	ar->p2p = !!ath6kl_p2p;
	ar->wiphy = wiphy;
	ar->dev = dev;

	spin_lock_init(&ar->lock);
	spin_lock_init(&ar->mcastpsq_lock);

	init_waitqueue_head(&ar->event_wq);
	sema_init(&ar->sem, 1);

	INIT_LIST_HEAD(&ar->amsdu_rx_buffer_queue);

	clear_bit(WMI_ENABLED, &ar->flag);
	clear_bit(SKIP_SCAN, &ar->flag);
	clear_bit(DESTROY_IN_PROGRESS, &ar->flag);

	ar->listen_intvl_t = A_DEFAULT_LISTEN_INTERVAL;
	ar->listen_intvl_b = 0;
	ar->tx_pwr = 0;

	ar->intra_bss = 1;
	memset(&ar->sc_params, 0, sizeof(ar->sc_params));
	ar->sc_params.short_scan_ratio = WMI_SHORTSCANRATIO_DEFAULT;
	ar->sc_params.scan_ctrl_flags = DEFAULT_SCAN_CTRL_FLAGS;
	ar->lrssi_roam_threshold = DEF_LRSSI_ROAM_THRESHOLD;

	memset((u8 *)ar->sta_list, 0,
	       AP_MAX_NUM_STA * sizeof(struct ath6kl_sta));

	/* Init the PS queues */
	for (ctr = 0; ctr < AP_MAX_NUM_STA; ctr++) {
		spin_lock_init(&ar->sta_list[ctr].psq_lock);
		skb_queue_head_init(&ar->sta_list[ctr].psq);
	}

	skb_queue_head_init(&ar->mcastpsq);

	memcpy(ar->ap_country_code, DEF_AP_COUNTRY_CODE, 3);

	return ar;
}

int ath6kl_register_ieee80211_hw(struct ath6kl *ar)
{
	struct wiphy *wiphy = ar->wiphy;
	int ret;

	wiphy->mgmt_stypes = ath6kl_mgmt_stypes;

	wiphy->max_remain_on_channel_duration = 5000;

	/* set device pointer for wiphy */
	set_wiphy_dev(wiphy, ar->dev);

	wiphy->interface_modes = BIT(NL80211_IFTYPE_STATION) |
				 BIT(NL80211_IFTYPE_ADHOC) |
				 BIT(NL80211_IFTYPE_AP);
	if (ar->p2p) {
		wiphy->interface_modes |= BIT(NL80211_IFTYPE_P2P_GO) |
					  BIT(NL80211_IFTYPE_P2P_CLIENT);
	}

	/* max num of ssids that can be probed during scanning */
	wiphy->max_scan_ssids = MAX_PROBED_SSID_INDEX;
	wiphy->max_scan_ie_len = 1000; /* FIX: what is correct limit? */
	wiphy->bands[IEEE80211_BAND_2GHZ] = &ath6kl_band_2ghz;
	wiphy->bands[IEEE80211_BAND_5GHZ] = &ath6kl_band_5ghz;
	wiphy->signal_type = CFG80211_SIGNAL_TYPE_MBM;

	wiphy->cipher_suites = cipher_suites;
	wiphy->n_cipher_suites = ARRAY_SIZE(cipher_suites);

	ret = wiphy_register(wiphy);
	if (ret < 0) {
		ath6kl_err("couldn't register wiphy device\n");
		return ret;
	}

	return 0;
}

static int ath6kl_init_if_data(struct ath6kl_vif *vif)
{
	vif->aggr_cntxt = aggr_init(vif->ndev);
	if (!vif->aggr_cntxt) {
		ath6kl_err("failed to initialize aggr\n");
		return -ENOMEM;
	}

	setup_timer(&vif->disconnect_timer, disconnect_timer_handler,
		    (unsigned long) vif->ndev);
	set_bit(WMM_ENABLED, &vif->flags);

	return 0;
}

void ath6kl_deinit_if_data(struct ath6kl_vif *vif)
{
	aggr_module_destroy(vif->aggr_cntxt);

	vif->aggr_cntxt = NULL;

	if (test_bit(NETDEV_REGISTERED, &vif->flags)) {
		unregister_netdev(vif->ndev);
		clear_bit(NETDEV_REGISTERED, &vif->flags);
	}

	free_netdev(vif->ndev);
}

struct net_device *ath6kl_interface_add(struct ath6kl *ar, char *name,
					enum nl80211_iftype type, u8 fw_vif_idx)
{
	struct net_device *ndev;
	struct ath6kl_vif *vif;

	ndev = alloc_netdev(sizeof(*vif), "wlan%d", ether_setup);
	if (!ndev)
		return NULL;

	vif = netdev_priv(ndev);
	ndev->ieee80211_ptr = &vif->wdev;
	vif->wdev.wiphy = ar->wiphy;
	vif->ar = ar;
	ar->vif = vif;
	vif->ndev = ndev;
	SET_NETDEV_DEV(ndev, wiphy_dev(vif->wdev.wiphy));
	vif->wdev.netdev = ndev;
	vif->wdev.iftype = type;
	vif->fw_vif_idx = fw_vif_idx;
	ar->wdev = &vif->wdev;
	ar->net_dev = ndev;

	init_netdev(ndev);

	ath6kl_init_control_info(ar);

	/* TODO: Pass interface specific pointer instead of ar */
	if (ath6kl_init_if_data(vif))
		goto err;

	if (register_netdev(ndev))
		goto err;

	vif->sme_state = SME_DISCONNECTED;
	set_bit(WLAN_ENABLED, &vif->flags);
	ar->wlan_pwr_state = WLAN_POWER_STATE_ON;
	set_bit(NETDEV_REGISTERED, &vif->flags);

	return ndev;

err:
	ath6kl_deinit_if_data(vif);

	return NULL;
}

void ath6kl_deinit_ieee80211_hw(struct ath6kl *ar)
{
	/* TODO: Findout vif */
	struct ath6kl_vif *vif = ar->vif;

	if (vif->scan_req) {
		cfg80211_scan_done(vif->scan_req, true);
		vif->scan_req = NULL;
	}

	wiphy_unregister(ar->wiphy);
	wiphy_free(ar->wiphy);
}
