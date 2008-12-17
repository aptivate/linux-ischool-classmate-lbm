/*
 * Copyright 2002-2005, Instant802 Networks, Inc.
 * Copyright 2005, Devicescape Software, Inc.
 * Copyright 2006-2007	Jiri Benc <jbenc@suse.cz>
 * Copyright 2007-2008	Johannes Berg <johannes@sipsolutions.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef IEEE80211_I_H
#define IEEE80211_I_H

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/if_ether.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/workqueue.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/etherdevice.h>
#include <net/cfg80211.h>
#include <net/wireless.h>
#include <net/iw_handler.h>
#include <net/mac80211.h>
#include "key.h"
#include "sta_info.h"

struct ieee80211_local;

/* Maximum number of broadcast/multicast frames to buffer when some of the
 * associated stations are using power saving. */
#define AP_MAX_BC_BUFFER 128

/* Maximum number of frames buffered to all STAs, including multicast frames.
 * Note: increasing this limit increases the potential memory requirement. Each
 * frame can be up to about 2 kB long. */
#define TOTAL_MAX_TX_BUFFER 512

/* Required encryption head and tailroom */
#define IEEE80211_ENCRYPT_HEADROOM 8
#define IEEE80211_ENCRYPT_TAILROOM 12

/* IEEE 802.11 (Ch. 9.5 Defragmentation) requires support for concurrent
 * reception of at least three fragmented frames. This limit can be increased
 * by changing this define, at the cost of slower frame reassembly and
 * increased memory use (about 2 kB of RAM per entry). */
#define IEEE80211_FRAGMENT_MAX 4

/*
 * Time after which we ignore scan results and no longer report/use
 * them in any way.
 */
#define IEEE80211_SCAN_RESULT_EXPIRE (10 * HZ)

struct ieee80211_fragment_entry {
	unsigned long first_frag_time;
	unsigned int seq;
	unsigned int rx_queue;
	unsigned int last_frag;
	unsigned int extra_len;
	struct sk_buff_head skb_list;
	int ccmp; /* Whether fragments were encrypted with CCMP */
	u8 last_pn[6]; /* PN of the last fragment if CCMP was used */
};


struct ieee80211_bss {
	struct list_head list;
	struct ieee80211_bss *hnext;
	size_t ssid_len;

	atomic_t users;

	u8 bssid[ETH_ALEN];
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 dtim_period;
	u16 capability; /* host byte order */
	enum ieee80211_band band;
	int freq;
	int signal, noise, qual;
	u8 *ies; /* all information elements from the last Beacon or Probe
		  * Response frames; note Beacon frame is not allowed to
		  * override values from Probe Response */
	size_t ies_len;
	bool wmm_used;
#ifdef CONFIG_MAC80211_MESH
	u8 *mesh_id;
	size_t mesh_id_len;
	u8 *mesh_cfg;
#endif
#define IEEE80211_MAX_SUPP_RATES 32
	u8 supp_rates[IEEE80211_MAX_SUPP_RATES];
	size_t supp_rates_len;
	u64 timestamp;
	int beacon_int;

	unsigned long last_probe_resp;
	unsigned long last_update;

	/* during assocation, we save an ERP value from a probe response so
	 * that we can feed ERP info to the driver when handling the
	 * association completes. these fields probably won't be up-to-date
	 * otherwise, you probably don't want to use them. */
	int has_erp_value;
	u8 erp_value;
};

static inline u8 *bss_mesh_cfg(struct ieee80211_bss *bss)
{
#ifdef CONFIG_MAC80211_MESH
	return bss->mesh_cfg;
#endif
	return NULL;
}

static inline u8 *bss_mesh_id(struct ieee80211_bss *bss)
{
#ifdef CONFIG_MAC80211_MESH
	return bss->mesh_id;
#endif
	return NULL;
}

static inline u8 bss_mesh_id_len(struct ieee80211_bss *bss)
{
#ifdef CONFIG_MAC80211_MESH
	return bss->mesh_id_len;
#endif
	return 0;
}


typedef unsigned __bitwise__ ieee80211_tx_result;
#define TX_CONTINUE	((__force ieee80211_tx_result) 0u)
#define TX_DROP		((__force ieee80211_tx_result) 1u)
#define TX_QUEUED	((__force ieee80211_tx_result) 2u)

#define IEEE80211_TX_FRAGMENTED		BIT(0)
#define IEEE80211_TX_UNICAST		BIT(1)
#define IEEE80211_TX_PS_BUFFERED	BIT(2)

struct ieee80211_tx_data {
	struct sk_buff *skb;
	struct net_device *dev;
	struct ieee80211_local *local;
	struct ieee80211_sub_if_data *sdata;
	struct sta_info *sta;
	struct ieee80211_key *key;

	struct ieee80211_channel *channel;

	/* Extra fragments (in addition to the first fragment
	 * in skb) */
	struct sk_buff **extra_frag;
	int num_extra_frag;

	u16 ethertype;
	unsigned int flags;
};


typedef unsigned __bitwise__ ieee80211_rx_result;
#define RX_CONTINUE		((__force ieee80211_rx_result) 0u)
#define RX_DROP_UNUSABLE	((__force ieee80211_rx_result) 1u)
#define RX_DROP_MONITOR		((__force ieee80211_rx_result) 2u)
#define RX_QUEUED		((__force ieee80211_rx_result) 3u)

#define IEEE80211_RX_IN_SCAN		BIT(0)
/* frame is destined to interface currently processed (incl. multicast frames) */
#define IEEE80211_RX_RA_MATCH		BIT(1)
#define IEEE80211_RX_AMSDU		BIT(2)
#define IEEE80211_RX_CMNTR_REPORTED	BIT(3)
#define IEEE80211_RX_FRAGMENTED		BIT(4)

struct ieee80211_rx_data {
	struct sk_buff *skb;
	struct net_device *dev;
	struct ieee80211_local *local;
	struct ieee80211_sub_if_data *sdata;
	struct sta_info *sta;
	struct ieee80211_key *key;
	struct ieee80211_rx_status *status;
	struct ieee80211_rate *rate;

	unsigned int flags;
	int sent_ps_buffered;
	int queue;
	u32 tkip_iv32;
	u16 tkip_iv16;
};

struct ieee80211_tx_stored_packet {
	struct sk_buff *skb;
	struct sk_buff **extra_frag;
	int num_extra_frag;
};

struct beacon_data {
	u8 *head, *tail;
	int head_len, tail_len;
	int dtim_period;
};

struct ieee80211_if_ap {
	struct beacon_data *beacon;

	struct list_head vlans;

	/* yes, this looks ugly, but guarantees that we can later use
	 * bitmap_empty :)
	 * NB: don't touch this bitmap, use sta_info_{set,clear}_tim_bit */
	u8 tim[sizeof(unsigned long) * BITS_TO_LONGS(IEEE80211_MAX_AID + 1)];
	struct sk_buff_head ps_bc_buf;
	atomic_t num_sta_ps; /* number of stations in PS mode */
	int dtim_count;
};

struct ieee80211_if_wds {
	struct sta_info *sta;
	u8 remote_addr[ETH_ALEN];
};

struct ieee80211_if_vlan {
	struct list_head list;
};

struct mesh_stats {
	__u32 fwded_frames;		/* Mesh forwarded frames */
	__u32 dropped_frames_ttl;	/* Not transmitted since mesh_ttl == 0*/
	__u32 dropped_frames_no_route;	/* Not transmitted, no route found */
	atomic_t estab_plinks;
};

#define PREQ_Q_F_START		0x1
#define PREQ_Q_F_REFRESH	0x2
struct mesh_preq_queue {
	struct list_head list;
	u8 dst[ETH_ALEN];
	u8 flags;
};

/* flags used in struct ieee80211_if_sta.flags */
#define IEEE80211_STA_SSID_SET		BIT(0)
#define IEEE80211_STA_BSSID_SET		BIT(1)
#define IEEE80211_STA_PREV_BSSID_SET	BIT(2)
#define IEEE80211_STA_AUTHENTICATED	BIT(3)
#define IEEE80211_STA_ASSOCIATED	BIT(4)
#define IEEE80211_STA_PROBEREQ_POLL	BIT(5)
#define IEEE80211_STA_CREATE_IBSS	BIT(6)
#define IEEE80211_STA_MIXED_CELL	BIT(7)
#define IEEE80211_STA_WMM_ENABLED	BIT(8)
#define IEEE80211_STA_AUTO_SSID_SEL	BIT(10)
#define IEEE80211_STA_AUTO_BSSID_SEL	BIT(11)
#define IEEE80211_STA_AUTO_CHANNEL_SEL	BIT(12)
#define IEEE80211_STA_PRIVACY_INVOKED	BIT(13)
/* flags for MLME request */
#define IEEE80211_STA_REQ_SCAN 0
#define IEEE80211_STA_REQ_DIRECT_PROBE 1
#define IEEE80211_STA_REQ_AUTH 2
#define IEEE80211_STA_REQ_RUN  3

/* STA/IBSS MLME states */
enum ieee80211_sta_mlme_state {
	IEEE80211_STA_MLME_DISABLED,
	IEEE80211_STA_MLME_DIRECT_PROBE,
	IEEE80211_STA_MLME_AUTHENTICATE,
	IEEE80211_STA_MLME_ASSOCIATE,
	IEEE80211_STA_MLME_ASSOCIATED,
	IEEE80211_STA_MLME_IBSS_SEARCH,
	IEEE80211_STA_MLME_IBSS_JOINED,
};

/* bitfield of allowed auth algs */
#define IEEE80211_AUTH_ALG_OPEN BIT(0)
#define IEEE80211_AUTH_ALG_SHARED_KEY BIT(1)
#define IEEE80211_AUTH_ALG_LEAP BIT(2)

struct ieee80211_if_sta {
	struct timer_list timer;
	struct work_struct work;
	u8 bssid[ETH_ALEN], prev_bssid[ETH_ALEN];
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	enum ieee80211_sta_mlme_state state;
	size_t ssid_len;
	u8 scan_ssid[IEEE80211_MAX_SSID_LEN];
	size_t scan_ssid_len;
	u16 aid;
	u16 ap_capab, capab;
	u8 *extra_ie; /* to be added to the end of AssocReq */
	size_t extra_ie_len;

	/* The last AssocReq/Resp IEs */
	u8 *assocreq_ies, *assocresp_ies;
	size_t assocreq_ies_len, assocresp_ies_len;

	struct sk_buff_head skb_queue;

	int assoc_scan_tries; /* number of scans done pre-association */
	int direct_probe_tries; /* retries for direct probes */
	int auth_tries; /* retries for auth req */
	int assoc_tries; /* retries for assoc req */

	unsigned long request;

	unsigned long last_probe;

	unsigned int flags;

	unsigned int auth_algs; /* bitfield of allowed auth algs */
	int auth_alg; /* currently used IEEE 802.11 authentication algorithm */
	int auth_transaction;

	unsigned long ibss_join_req;
	struct sk_buff *probe_resp; /* ProbeResp template for IBSS */
	u32 supp_rates_bits[IEEE80211_NUM_BANDS];

	int wmm_last_param_set;
};

struct ieee80211_if_mesh {
	struct work_struct work;
	struct timer_list housekeeping_timer;
	struct timer_list mesh_path_timer;
	struct sk_buff_head skb_queue;

	bool housekeeping;

	u8 mesh_id[IEEE80211_MAX_MESH_ID_LEN];
	size_t mesh_id_len;
	/* Active Path Selection Protocol Identifier */
	u8 mesh_pp_id[4];
	/* Active Path Selection Metric Identifier */
	u8 mesh_pm_id[4];
	/* Congestion Control Mode Identifier */
	u8 mesh_cc_id[4];
	/* Local mesh Destination Sequence Number */
	u32 dsn;
	/* Last used PREQ ID */
	u32 preq_id;
	atomic_t mpaths;
	/* Timestamp of last DSN update */
	unsigned long last_dsn_update;
	/* Timestamp of last DSN sent */
	unsigned long last_preq;
	struct mesh_rmc *rmc;
	spinlock_t mesh_preq_queue_lock;
	struct mesh_preq_queue preq_queue;
	int preq_queue_len;
	struct mesh_stats mshstats;
	struct mesh_config mshcfg;
	u32 mesh_seqnum;
	bool accepting_plinks;
};

#ifdef CONFIG_MAC80211_MESH
#define IEEE80211_IFSTA_MESH_CTR_INC(msh, name)	\
	do { (msh)->mshstats.name++; } while (0)
#else
#define IEEE80211_IFSTA_MESH_CTR_INC(msh, name) \
	do { } while (0)
#endif

/**
 * enum ieee80211_sub_if_data_flags - virtual interface flags
 *
 * @IEEE80211_SDATA_ALLMULTI: interface wants all multicast packets
 * @IEEE80211_SDATA_PROMISC: interface is promisc
 * @IEEE80211_SDATA_USERSPACE_MLME: userspace MLME is active
 * @IEEE80211_SDATA_OPERATING_GMODE: operating in G-only mode
 * @IEEE80211_SDATA_DONT_BRIDGE_PACKETS: bridge packets between
 *	associated stations and deliver multicast frames both
 *	back to wireless media and to the local net stack.
 */
enum ieee80211_sub_if_data_flags {
	IEEE80211_SDATA_ALLMULTI		= BIT(0),
	IEEE80211_SDATA_PROMISC			= BIT(1),
	IEEE80211_SDATA_USERSPACE_MLME		= BIT(2),
	IEEE80211_SDATA_OPERATING_GMODE		= BIT(3),
	IEEE80211_SDATA_DONT_BRIDGE_PACKETS	= BIT(4),
};

struct ieee80211_sub_if_data {
	struct list_head list;

	struct wireless_dev wdev;

	/* keys */
	struct list_head key_list;

	struct net_device *dev;
	struct ieee80211_local *local;

	unsigned int flags;

	int drop_unencrypted;

	/* Fragment table for host-based reassembly */
	struct ieee80211_fragment_entry	fragments[IEEE80211_FRAGMENT_MAX];
	unsigned int fragment_next;

#define NUM_DEFAULT_KEYS 4
	struct ieee80211_key *keys[NUM_DEFAULT_KEYS];
	struct ieee80211_key *default_key;

	u16 sequence_number;

	/*
	 * AP this belongs to: self in AP mode and
	 * corresponding AP in VLAN mode, NULL for
	 * all others (might be needed later in IBSS)
	 */
	struct ieee80211_if_ap *bss;

	int force_unicast_rateidx; /* forced TX rateidx for unicast frames */
	int max_ratectrl_rateidx; /* max TX rateidx for rate control */

	union {
		struct ieee80211_if_ap ap;
		struct ieee80211_if_wds wds;
		struct ieee80211_if_vlan vlan;
		struct ieee80211_if_sta sta;
#ifdef CONFIG_MAC80211_MESH
		struct ieee80211_if_mesh mesh;
#endif
		u32 mntr_flags;
	} u;

#ifdef CONFIG_MAC80211_DEBUGFS
	struct dentry *debugfsdir;
	union {
		struct {
			struct dentry *drop_unencrypted;
			struct dentry *state;
			struct dentry *bssid;
			struct dentry *prev_bssid;
			struct dentry *ssid_len;
			struct dentry *aid;
			struct dentry *ap_capab;
			struct dentry *capab;
			struct dentry *extra_ie_len;
			struct dentry *auth_tries;
			struct dentry *assoc_tries;
			struct dentry *auth_algs;
			struct dentry *auth_alg;
			struct dentry *auth_transaction;
			struct dentry *flags;
			struct dentry *force_unicast_rateidx;
			struct dentry *max_ratectrl_rateidx;
		} sta;
		struct {
			struct dentry *drop_unencrypted;
			struct dentry *num_sta_ps;
			struct dentry *dtim_count;
			struct dentry *force_unicast_rateidx;
			struct dentry *max_ratectrl_rateidx;
			struct dentry *num_buffered_multicast;
		} ap;
		struct {
			struct dentry *drop_unencrypted;
			struct dentry *peer;
			struct dentry *force_unicast_rateidx;
			struct dentry *max_ratectrl_rateidx;
		} wds;
		struct {
			struct dentry *drop_unencrypted;
			struct dentry *force_unicast_rateidx;
			struct dentry *max_ratectrl_rateidx;
		} vlan;
		struct {
			struct dentry *mode;
		} monitor;
	} debugfs;
	struct {
		struct dentry *default_key;
	} common_debugfs;

#ifdef CONFIG_MAC80211_MESH
	struct dentry *mesh_stats_dir;
	struct {
		struct dentry *fwded_frames;
		struct dentry *dropped_frames_ttl;
		struct dentry *dropped_frames_no_route;
		struct dentry *estab_plinks;
		struct timer_list mesh_path_timer;
	} mesh_stats;

	struct dentry *mesh_config_dir;
	struct {
		struct dentry *dot11MeshRetryTimeout;
		struct dentry *dot11MeshConfirmTimeout;
		struct dentry *dot11MeshHoldingTimeout;
		struct dentry *dot11MeshMaxRetries;
		struct dentry *dot11MeshTTL;
		struct dentry *auto_open_plinks;
		struct dentry *dot11MeshMaxPeerLinks;
		struct dentry *dot11MeshHWMPactivePathTimeout;
		struct dentry *dot11MeshHWMPpreqMinInterval;
		struct dentry *dot11MeshHWMPnetDiameterTraversalTime;
		struct dentry *dot11MeshHWMPmaxPREQretries;
		struct dentry *path_refresh_time;
		struct dentry *min_discovery_timeout;
	} mesh_config;
#endif

#endif
	/* must be last, dynamically sized area in this! */
	struct ieee80211_vif vif;
};

static inline
struct ieee80211_sub_if_data *vif_to_sdata(struct ieee80211_vif *p)
{
	return container_of(p, struct ieee80211_sub_if_data, vif);
}

static inline void
ieee80211_sdata_set_mesh_id(struct ieee80211_sub_if_data *sdata,
			    u8 mesh_id_len, u8 *mesh_id)
{
#ifdef CONFIG_MAC80211_MESH
	struct ieee80211_if_mesh *ifmsh = &sdata->u.mesh;
	ifmsh->mesh_id_len = mesh_id_len;
	memcpy(ifmsh->mesh_id, mesh_id, mesh_id_len);
#else
	WARN_ON(1);
#endif
}

enum {
	IEEE80211_RX_MSG	= 1,
	IEEE80211_TX_STATUS_MSG	= 2,
	IEEE80211_DELBA_MSG	= 3,
	IEEE80211_ADDBA_MSG	= 4,
};

/* maximum number of hardware queues we support. */
#define QD_MAX_QUEUES (IEEE80211_MAX_AMPDU_QUEUES + IEEE80211_MAX_QUEUES)

struct ieee80211_master_priv {
	struct ieee80211_local *local;
};

struct ieee80211_local {
	/* embed the driver visible part.
	 * don't cast (use the static inlines below), but we keep
	 * it first anyway so they become a no-op */
	struct ieee80211_hw hw;

	const struct ieee80211_ops *ops;

	unsigned long queue_pool[BITS_TO_LONGS(QD_MAX_QUEUES)];

	struct net_device *mdev; /* wmaster# - "master" 802.11 device */
	int open_count;
	int monitors, cooked_mntrs;
	/* number of interfaces with corresponding FIF_ flags */
	int fif_fcsfail, fif_plcpfail, fif_control, fif_other_bss;
	unsigned int filter_flags; /* FIF_* */
	struct iw_statistics wstats;
	u8 wstats_flags;
	bool tim_in_locked_section; /* see ieee80211_beacon_get() */
	int tx_headroom; /* required headroom for hardware/radiotap */

	/* Tasklet and skb queue to process calls from IRQ mode. All frames
	 * added to skb_queue will be processed, but frames in
	 * skb_queue_unreliable may be dropped if the total length of these
	 * queues increases over the limit. */
#define IEEE80211_IRQSAFE_QUEUE_LIMIT 128
	struct tasklet_struct tasklet;
	struct sk_buff_head skb_queue;
	struct sk_buff_head skb_queue_unreliable;

	/* Station data */
	/*
	 * The lock only protects the list, hash, timer and counter
	 * against manipulation, reads are done in RCU. Additionally,
	 * the lock protects each BSS's TIM bitmap.
	 */
	spinlock_t sta_lock;
	unsigned long num_sta;
	struct list_head sta_list;
	struct list_head sta_flush_list;
	struct work_struct sta_flush_work;
	struct sta_info *sta_hash[STA_HASH_SIZE];
	struct timer_list sta_cleanup;

	unsigned long queues_pending[BITS_TO_LONGS(IEEE80211_MAX_QUEUES)];
	unsigned long queues_pending_run[BITS_TO_LONGS(IEEE80211_MAX_QUEUES)];
	struct ieee80211_tx_stored_packet pending_packet[IEEE80211_MAX_QUEUES];
	struct tasklet_struct tx_pending_tasklet;

	/* number of interfaces with corresponding IFF_ flags */
	atomic_t iff_allmultis, iff_promiscs;

	struct rate_control_ref *rate_ctrl;

	int rts_threshold;
	int fragmentation_threshold;

	struct crypto_blkcipher *wep_tx_tfm;
	struct crypto_blkcipher *wep_rx_tfm;
	u32 wep_iv;

	struct list_head interfaces;

	/*
	 * Key lock, protects sdata's key_list and sta_info's
	 * key pointers (write access, they're RCU.)
	 */
	spinlock_t key_lock;


	/* Scanning and BSS list */
	bool sw_scanning, hw_scanning;
	int scan_channel_idx;
	enum ieee80211_band scan_band;

	enum { SCAN_SET_CHANNEL, SCAN_SEND_PROBE } scan_state;
	unsigned long last_scan_completed;
	struct delayed_work scan_work;
	struct ieee80211_sub_if_data *scan_sdata;
	struct ieee80211_channel *oper_channel, *scan_channel;
	enum nl80211_channel_type oper_channel_type;
	u8 scan_ssid[IEEE80211_MAX_SSID_LEN];
	size_t scan_ssid_len;
	struct list_head bss_list;
	struct ieee80211_bss *bss_hash[STA_HASH_SIZE];
	spinlock_t bss_lock;

	/* SNMP counters */
	/* dot11CountersTable */
	u32 dot11TransmittedFragmentCount;
	u32 dot11MulticastTransmittedFrameCount;
	u32 dot11FailedCount;
	u32 dot11RetryCount;
	u32 dot11MultipleRetryCount;
	u32 dot11FrameDuplicateCount;
	u32 dot11ReceivedFragmentCount;
	u32 dot11MulticastReceivedFrameCount;
	u32 dot11TransmittedFrameCount;
	u32 dot11WEPUndecryptableCount;

#ifdef CONFIG_MAC80211_LEDS
	int tx_led_counter, rx_led_counter;
	struct led_trigger *tx_led, *rx_led, *assoc_led, *radio_led;
	char tx_led_name[32], rx_led_name[32],
	     assoc_led_name[32], radio_led_name[32];
#endif

#ifdef CONFIG_MAC80211_DEBUGFS
	struct work_struct sta_debugfs_add;
#endif

#ifdef CONFIG_MAC80211_DEBUG_COUNTERS
	/* TX/RX handler statistics */
	unsigned int tx_handlers_drop;
	unsigned int tx_handlers_queued;
	unsigned int tx_handlers_drop_unencrypted;
	unsigned int tx_handlers_drop_fragment;
	unsigned int tx_handlers_drop_wep;
	unsigned int tx_handlers_drop_not_assoc;
	unsigned int tx_handlers_drop_unauth_port;
	unsigned int rx_handlers_drop;
	unsigned int rx_handlers_queued;
	unsigned int rx_handlers_drop_nullfunc;
	unsigned int rx_handlers_drop_defrag;
	unsigned int rx_handlers_drop_short;
	unsigned int rx_handlers_drop_passive_scan;
	unsigned int tx_expand_skb_head;
	unsigned int tx_expand_skb_head_cloned;
	unsigned int rx_expand_skb_head;
	unsigned int rx_expand_skb_head2;
	unsigned int rx_handlers_fragments;
	unsigned int tx_status_drop;
#define I802_DEBUG_INC(c) (c)++
#else /* CONFIG_MAC80211_DEBUG_COUNTERS */
#define I802_DEBUG_INC(c) do { } while (0)
#endif /* CONFIG_MAC80211_DEBUG_COUNTERS */


	int total_ps_buffered; /* total number of all buffered unicast and
				* multicast packets for power saving stations
				*/
	int wifi_wme_noack_test;
	unsigned int wmm_acm; /* bit field of ACM bits (BIT(802.1D tag)) */

#ifdef CONFIG_MAC80211_DEBUGFS
	struct local_debugfsdentries {
		struct dentry *rcdir;
		struct dentry *rcname;
		struct dentry *frequency;
		struct dentry *rts_threshold;
		struct dentry *fragmentation_threshold;
		struct dentry *short_retry_limit;
		struct dentry *long_retry_limit;
		struct dentry *total_ps_buffered;
		struct dentry *wep_iv;
		struct dentry *statistics;
		struct local_debugfsdentries_statsdentries {
			struct dentry *transmitted_fragment_count;
			struct dentry *multicast_transmitted_frame_count;
			struct dentry *failed_count;
			struct dentry *retry_count;
			struct dentry *multiple_retry_count;
			struct dentry *frame_duplicate_count;
			struct dentry *received_fragment_count;
			struct dentry *multicast_received_frame_count;
			struct dentry *transmitted_frame_count;
			struct dentry *wep_undecryptable_count;
			struct dentry *num_scans;
#ifdef CONFIG_MAC80211_DEBUG_COUNTERS
			struct dentry *tx_handlers_drop;
			struct dentry *tx_handlers_queued;
			struct dentry *tx_handlers_drop_unencrypted;
			struct dentry *tx_handlers_drop_fragment;
			struct dentry *tx_handlers_drop_wep;
			struct dentry *tx_handlers_drop_not_assoc;
			struct dentry *tx_handlers_drop_unauth_port;
			struct dentry *rx_handlers_drop;
			struct dentry *rx_handlers_queued;
			struct dentry *rx_handlers_drop_nullfunc;
			struct dentry *rx_handlers_drop_defrag;
			struct dentry *rx_handlers_drop_short;
			struct dentry *rx_handlers_drop_passive_scan;
			struct dentry *tx_expand_skb_head;
			struct dentry *tx_expand_skb_head_cloned;
			struct dentry *rx_expand_skb_head;
			struct dentry *rx_expand_skb_head2;
			struct dentry *rx_handlers_fragments;
			struct dentry *tx_status_drop;
#endif
			struct dentry *dot11ACKFailureCount;
			struct dentry *dot11RTSFailureCount;
			struct dentry *dot11FCSErrorCount;
			struct dentry *dot11RTSSuccessCount;
		} stats;
		struct dentry *stations;
		struct dentry *keys;
	} debugfs;
#endif
};

static inline struct ieee80211_sub_if_data *
IEEE80211_DEV_TO_SUB_IF(struct net_device *dev)
{
	struct ieee80211_local *local = wdev_priv(dev->ieee80211_ptr);

	BUG_ON(!local || local->mdev == dev);

	return netdev_priv(dev);
}

/* this struct represents 802.11n's RA/TID combination */
struct ieee80211_ra_tid {
	u8 ra[ETH_ALEN];
	u16 tid;
};

/* Parsed Information Elements */
struct ieee802_11_elems {
	u8 *ie_start;
	size_t total_len;

	/* pointers to IEs */
	u8 *ssid;
	u8 *supp_rates;
	u8 *fh_params;
	u8 *ds_params;
	u8 *cf_params;
	u8 *tim;
	u8 *ibss_params;
	u8 *challenge;
	u8 *wpa;
	u8 *rsn;
	u8 *erp_info;
	u8 *ext_supp_rates;
	u8 *wmm_info;
	u8 *wmm_param;
	struct ieee80211_ht_cap *ht_cap_elem;
	struct ieee80211_ht_info *ht_info_elem;
	u8 *mesh_config;
	u8 *mesh_id;
	u8 *peer_link;
	u8 *preq;
	u8 *prep;
	u8 *perr;
	u8 *ch_switch_elem;
	u8 *country_elem;
	u8 *pwr_constr_elem;
	u8 *quiet_elem; 	/* first quite element */

	/* length of them, respectively */
	u8 ssid_len;
	u8 supp_rates_len;
	u8 fh_params_len;
	u8 ds_params_len;
	u8 cf_params_len;
	u8 tim_len;
	u8 ibss_params_len;
	u8 challenge_len;
	u8 wpa_len;
	u8 rsn_len;
	u8 erp_info_len;
	u8 ext_supp_rates_len;
	u8 wmm_info_len;
	u8 wmm_param_len;
	u8 mesh_config_len;
	u8 mesh_id_len;
	u8 peer_link_len;
	u8 preq_len;
	u8 prep_len;
	u8 perr_len;
	u8 ch_switch_elem_len;
	u8 country_elem_len;
	u8 pwr_constr_elem_len;
	u8 quiet_elem_len;
	u8 num_of_quiet_elem;	/* can be more the one */
};

static inline struct ieee80211_local *hw_to_local(
	struct ieee80211_hw *hw)
{
	return container_of(hw, struct ieee80211_local, hw);
}

static inline struct ieee80211_hw *local_to_hw(
	struct ieee80211_local *local)
{
	return &local->hw;
}


static inline int ieee80211_bssid_match(const u8 *raddr, const u8 *addr)
{
	return compare_ether_addr(raddr, addr) == 0 ||
	       is_broadcast_ether_addr(raddr);
}


int ieee80211_hw_config(struct ieee80211_local *local, u32 changed);
int ieee80211_if_config(struct ieee80211_sub_if_data *sdata, u32 changed);
void ieee80211_tx_set_protected(struct ieee80211_tx_data *tx);
void ieee80211_bss_info_change_notify(struct ieee80211_sub_if_data *sdata,
				      u32 changed);
void ieee80211_configure_filter(struct ieee80211_local *local);

/* wireless extensions */
extern const struct iw_handler_def ieee80211_iw_handler_def;

/* STA/IBSS code */
void ieee80211_sta_setup_sdata(struct ieee80211_sub_if_data *sdata);
void ieee80211_scan_work(struct work_struct *work);
void ieee80211_sta_rx_mgmt(struct ieee80211_sub_if_data *sdata, struct sk_buff *skb,
			   struct ieee80211_rx_status *rx_status);
int ieee80211_sta_set_ssid(struct ieee80211_sub_if_data *sdata, char *ssid, size_t len);
int ieee80211_sta_get_ssid(struct ieee80211_sub_if_data *sdata, char *ssid, size_t *len);
int ieee80211_sta_set_bssid(struct ieee80211_sub_if_data *sdata, u8 *bssid);
void ieee80211_sta_req_auth(struct ieee80211_sub_if_data *sdata,
			    struct ieee80211_if_sta *ifsta);
struct sta_info *ieee80211_ibss_add_sta(struct ieee80211_sub_if_data *sdata,
					u8 *bssid, u8 *addr, u64 supp_rates);
int ieee80211_sta_deauthenticate(struct ieee80211_sub_if_data *sdata, u16 reason);
int ieee80211_sta_disassociate(struct ieee80211_sub_if_data *sdata, u16 reason);
u32 ieee80211_reset_erp_info(struct ieee80211_sub_if_data *sdata);
u64 ieee80211_sta_get_rates(struct ieee80211_local *local,
			    struct ieee802_11_elems *elems,
			    enum ieee80211_band band);
void ieee80211_send_probe_req(struct ieee80211_sub_if_data *sdata, u8 *dst,
			      u8 *ssid, size_t ssid_len);

/* scan/BSS handling */
int ieee80211_request_scan(struct ieee80211_sub_if_data *sdata,
			   u8 *ssid, size_t ssid_len);
int ieee80211_scan_results(struct ieee80211_local *local,
			   struct iw_request_info *info,
			   char *buf, size_t len);
ieee80211_rx_result
ieee80211_scan_rx(struct ieee80211_sub_if_data *sdata,
		  struct sk_buff *skb,
		  struct ieee80211_rx_status *rx_status);
void ieee80211_rx_bss_list_init(struct ieee80211_local *local);
void ieee80211_rx_bss_list_deinit(struct ieee80211_local *local);
int ieee80211_sta_set_extra_ie(struct ieee80211_sub_if_data *sdata,
			       char *ie, size_t len);

void ieee80211_mlme_notify_scan_completed(struct ieee80211_local *local);
int ieee80211_start_scan(struct ieee80211_sub_if_data *scan_sdata,
			 u8 *ssid, size_t ssid_len);
struct ieee80211_bss *
ieee80211_bss_info_update(struct ieee80211_local *local,
			  struct ieee80211_rx_status *rx_status,
			  struct ieee80211_mgmt *mgmt,
			  size_t len,
			  struct ieee802_11_elems *elems,
			  int freq, bool beacon);
struct ieee80211_bss *
ieee80211_rx_bss_add(struct ieee80211_local *local, u8 *bssid, int freq,
		     u8 *ssid, u8 ssid_len);
struct ieee80211_bss *
ieee80211_rx_bss_get(struct ieee80211_local *local, u8 *bssid, int freq,
		     u8 *ssid, u8 ssid_len);
void ieee80211_rx_bss_put(struct ieee80211_local *local,
			  struct ieee80211_bss *bss);

/* interface handling */
int ieee80211_if_add(struct ieee80211_local *local, const char *name,
		     struct net_device **new_dev, enum nl80211_iftype type,
		     struct vif_params *params);
int ieee80211_if_change_type(struct ieee80211_sub_if_data *sdata,
			     enum nl80211_iftype type);
void ieee80211_if_remove(struct ieee80211_sub_if_data *sdata);
void ieee80211_remove_interfaces(struct ieee80211_local *local);

/* tx handling */
void ieee80211_clear_tx_pending(struct ieee80211_local *local);
void ieee80211_tx_pending(unsigned long data);
int ieee80211_master_start_xmit(struct sk_buff *skb, struct net_device *dev);
int ieee80211_monitor_start_xmit(struct sk_buff *skb, struct net_device *dev);
int ieee80211_subif_start_xmit(struct sk_buff *skb, struct net_device *dev);

/* HT */
void ieee80211_ht_cap_ie_to_sta_ht_cap(struct ieee80211_supported_band *sband,
				       struct ieee80211_ht_cap *ht_cap_ie,
				       struct ieee80211_sta_ht_cap *ht_cap);
u32 ieee80211_enable_ht(struct ieee80211_sub_if_data *sdata,
			struct ieee80211_ht_info *hti,
			u16 ap_ht_cap_flags);
void ieee80211_send_bar(struct ieee80211_sub_if_data *sdata, u8 *ra, u16 tid, u16 ssn);

void ieee80211_sta_stop_rx_ba_session(struct ieee80211_sub_if_data *sdata, u8 *da,
				u16 tid, u16 initiator, u16 reason);
void ieee80211_sta_tear_down_BA_sessions(struct ieee80211_sub_if_data *sdata, u8 *addr);
void ieee80211_process_delba(struct ieee80211_sub_if_data *sdata,
			     struct sta_info *sta,
			     struct ieee80211_mgmt *mgmt, size_t len);
void ieee80211_process_addba_resp(struct ieee80211_local *local,
				  struct sta_info *sta,
				  struct ieee80211_mgmt *mgmt,
				  size_t len);
void ieee80211_process_addba_request(struct ieee80211_local *local,
				     struct sta_info *sta,
				     struct ieee80211_mgmt *mgmt,
				     size_t len);

/* Spectrum management */
void ieee80211_process_measurement_req(struct ieee80211_sub_if_data *sdata,
				       struct ieee80211_mgmt *mgmt,
				       size_t len);

/* utility functions/constants */
extern void *mac80211_wiphy_privid; /* for wiphy privid */
extern const unsigned char rfc1042_header[6];
extern const unsigned char bridge_tunnel_header[6];
u8 *ieee80211_get_bssid(struct ieee80211_hdr *hdr, size_t len,
			enum nl80211_iftype type);
int ieee80211_frame_duration(struct ieee80211_local *local, size_t len,
			     int rate, int erp, int short_preamble);
void mac80211_ev_michael_mic_failure(struct ieee80211_sub_if_data *sdata, int keyidx,
				     struct ieee80211_hdr *hdr);
void ieee80211_set_wmm_default(struct ieee80211_sub_if_data *sdata);
void ieee80211_tx_skb(struct ieee80211_sub_if_data *sdata, struct sk_buff *skb,
		      int encrypt);
void ieee802_11_parse_elems(u8 *start, size_t len,
			    struct ieee802_11_elems *elems);
int ieee80211_set_freq(struct ieee80211_sub_if_data *sdata, int freq);
u64 ieee80211_mandatory_rates(struct ieee80211_local *local,
			      enum ieee80211_band band);

#ifdef CONFIG_MAC80211_NOINLINE
#define debug_noinline noinline
#else
#define debug_noinline
#endif

#endif /* IEEE80211_I_H */
