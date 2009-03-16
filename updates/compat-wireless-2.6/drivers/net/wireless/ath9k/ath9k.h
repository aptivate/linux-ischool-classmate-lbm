/*
 * Copyright (c) 2008 Atheros Communications Inc.
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

#ifndef ATH9K_H
#define ATH9K_H

#include <linux/etherdevice.h>
#include <linux/device.h>
#include <net/mac80211.h>
#include <linux/leds.h>
#include <linux/rfkill.h>

#include "hw.h"
#include "rc.h"
#include "debug.h"

struct ath_node;

/* Macro to expand scalars to 64-bit objects */

#define	ito64(x) (sizeof(x) == 8) ?			\
	(((unsigned long long int)(x)) & (0xff)) :	\
	(sizeof(x) == 16) ?				\
	(((unsigned long long int)(x)) & 0xffff) :	\
	((sizeof(x) == 32) ?				\
	 (((unsigned long long int)(x)) & 0xffffffff) : \
	 (unsigned long long int)(x))

/* increment with wrap-around */
#define INCR(_l, _sz)   do {			\
		(_l)++;				\
		(_l) &= ((_sz) - 1);		\
	} while (0)

/* decrement with wrap-around */
#define DECR(_l,  _sz)  do {			\
		(_l)--;				\
		(_l) &= ((_sz) - 1);		\
	} while (0)

#define A_MAX(a, b) ((a) > (b) ? (a) : (b))

#define ASSERT(exp) do {			\
		if (unlikely(!(exp))) {		\
			BUG();			\
		}				\
	} while (0)

#define TSF_TO_TU(_h,_l) \
	((((u32)(_h)) << 22) | (((u32)(_l)) >> 10))

#define	ATH_TXQ_SETUP(sc, i)        ((sc)->tx.txqsetup & (1<<i))

static const u8 ath_bcast_mac[ETH_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

struct ath_config {
	u32 ath_aggr_prot;
	u16 txpowlimit;
	u8 cabqReadytime;
	u8 swBeaconProcess;
};

/*************************/
/* Descriptor Management */
/*************************/

#define ATH_TXBUF_RESET(_bf) do {				\
		(_bf)->bf_status = 0;				\
		(_bf)->bf_lastbf = NULL;			\
		(_bf)->bf_next = NULL;				\
		memset(&((_bf)->bf_state), 0,			\
		       sizeof(struct ath_buf_state));		\
	} while (0)

/**
 * enum buffer_type - Buffer type flags
 *
 * @BUF_HT: Send this buffer using HT capabilities
 * @BUF_AMPDU: This buffer is an ampdu, as part of an aggregate (during TX)
 * @BUF_AGGR: Indicates whether the buffer can be aggregated
 *	(used in aggregation scheduling)
 * @BUF_RETRY: Indicates whether the buffer is retried
 * @BUF_XRETRY: To denote excessive retries of the buffer
 */
enum buffer_type {
	BUF_HT			= BIT(1),
	BUF_AMPDU		= BIT(2),
	BUF_AGGR		= BIT(3),
	BUF_RETRY		= BIT(4),
	BUF_XRETRY		= BIT(5),
};

struct ath_buf_state {
	int bfs_nframes;
	u16 bfs_al;
	u16 bfs_frmlen;
	int bfs_seqno;
	int bfs_tidno;
	int bfs_retries;
	u32 bf_type;
	u32 bfs_keyix;
	enum ath9k_key_type bfs_keytype;
};

#define bf_nframes      	bf_state.bfs_nframes
#define bf_al           	bf_state.bfs_al
#define bf_frmlen       	bf_state.bfs_frmlen
#define bf_retries      	bf_state.bfs_retries
#define bf_seqno        	bf_state.bfs_seqno
#define bf_tidno        	bf_state.bfs_tidno
#define bf_keyix                bf_state.bfs_keyix
#define bf_keytype      	bf_state.bfs_keytype
#define bf_isht(bf)		(bf->bf_state.bf_type & BUF_HT)
#define bf_isampdu(bf)		(bf->bf_state.bf_type & BUF_AMPDU)
#define bf_isaggr(bf)		(bf->bf_state.bf_type & BUF_AGGR)
#define bf_isretried(bf)	(bf->bf_state.bf_type & BUF_RETRY)
#define bf_isxretried(bf)	(bf->bf_state.bf_type & BUF_XRETRY)

struct ath_buf {
	struct list_head list;
	struct ath_buf *bf_lastbf;	/* last buf of this unit (a frame or
					   an aggregate) */
	struct ath_buf *bf_next;	/* next subframe in the aggregate */
	void *bf_mpdu;			/* enclosing frame structure */
	struct ath_desc *bf_desc;	/* virtual addr of desc */
	dma_addr_t bf_daddr;		/* physical addr of desc */
	dma_addr_t bf_buf_addr;		/* physical addr of data buffer */
	u32 bf_status;
	u16 bf_flags;
	struct ath_buf_state bf_state;
	dma_addr_t bf_dmacontext;
};

#define ATH_RXBUF_RESET(_bf)    ((_bf)->bf_status = 0)
#define ATH_BUFSTATUS_STALE     0x00000002

struct ath_descdma {
	const char *dd_name;
	struct ath_desc *dd_desc;
	dma_addr_t dd_desc_paddr;
	u32 dd_desc_len;
	struct ath_buf *dd_bufptr;
	dma_addr_t dd_dmacontext;
};

int ath_descdma_setup(struct ath_softc *sc, struct ath_descdma *dd,
		      struct list_head *head, const char *name,
		      int nbuf, int ndesc);
void ath_descdma_cleanup(struct ath_softc *sc, struct ath_descdma *dd,
			 struct list_head *head);

/***********/
/* RX / TX */
/***********/

#define ATH_MAX_ANTENNA         3
#define ATH_RXBUF               512
#define WME_NUM_TID             16
#define ATH_TXBUF               512
#define ATH_TXMAXTRY            13
#define ATH_11N_TXMAXTRY        10
#define ATH_MGT_TXMAXTRY        4
#define WME_BA_BMP_SIZE         64
#define WME_MAX_BA              WME_BA_BMP_SIZE
#define ATH_TID_MAX_BUFS        (2 * WME_MAX_BA)

#define TID_TO_WME_AC(_tid)				\
	((((_tid) == 0) || ((_tid) == 3)) ? WME_AC_BE :	\
	 (((_tid) == 1) || ((_tid) == 2)) ? WME_AC_BK :	\
	 (((_tid) == 4) || ((_tid) == 5)) ? WME_AC_VI :	\
	 WME_AC_VO)

#define WME_AC_BE   0
#define WME_AC_BK   1
#define WME_AC_VI   2
#define WME_AC_VO   3
#define WME_NUM_AC  4

#define ADDBA_EXCHANGE_ATTEMPTS    10
#define ATH_AGGR_DELIM_SZ          4
#define ATH_AGGR_MINPLEN           256 /* in bytes, minimum packet length */
/* number of delimiters for encryption padding */
#define ATH_AGGR_ENCRYPTDELIM      10
/* minimum h/w qdepth to be sustained to maximize aggregation */
#define ATH_AGGR_MIN_QDEPTH        2
#define ATH_AMPDU_SUBFRAME_DEFAULT 32
#define ATH_AMPDU_LIMIT_MAX        (64 * 1024 - 1)
#define ATH_AMPDU_LIMIT_DEFAULT    ATH_AMPDU_LIMIT_MAX

#define IEEE80211_SEQ_SEQ_SHIFT    4
#define IEEE80211_SEQ_MAX          4096
#define IEEE80211_MIN_AMPDU_BUF    0x8
#define IEEE80211_HTCAP_MAXRXAMPDU_FACTOR 13
#define IEEE80211_WEP_IVLEN        3
#define IEEE80211_WEP_KIDLEN       1
#define IEEE80211_WEP_CRCLEN       4
#define IEEE80211_MAX_MPDU_LEN     (3840 + FCS_LEN +		\
				    (IEEE80211_WEP_IVLEN +	\
				     IEEE80211_WEP_KIDLEN +	\
				     IEEE80211_WEP_CRCLEN))

/* return whether a bit at index _n in bitmap _bm is set
 * _sz is the size of the bitmap  */
#define ATH_BA_ISSET(_bm, _n)  (((_n) < (WME_BA_BMP_SIZE)) &&		\
				((_bm)[(_n) >> 5] & (1 << ((_n) & 31))))

/* return block-ack bitmap index given sequence and starting sequence */
#define ATH_BA_INDEX(_st, _seq) (((_seq) - (_st)) & (IEEE80211_SEQ_MAX - 1))

/* returns delimiter padding required given the packet length */
#define ATH_AGGR_GET_NDELIM(_len)					\
	(((((_len) + ATH_AGGR_DELIM_SZ) < ATH_AGGR_MINPLEN) ?           \
	  (ATH_AGGR_MINPLEN - (_len) - ATH_AGGR_DELIM_SZ) : 0) >> 2)

#define BAW_WITHIN(_start, _bawsz, _seqno) \
	((((_seqno) - (_start)) & 4095) < (_bawsz))

#define ATH_DS_BA_SEQ(_ds)         ((_ds)->ds_us.tx.ts_seqnum)
#define ATH_DS_BA_BITMAP(_ds)      (&(_ds)->ds_us.tx.ba_low)
#define ATH_DS_TX_BA(_ds)          ((_ds)->ds_us.tx.ts_flags & ATH9K_TX_BA)
#define ATH_AN_2_TID(_an, _tidno)  (&(_an)->tid[(_tidno)])

enum ATH_AGGR_STATUS {
	ATH_AGGR_DONE,
	ATH_AGGR_BAW_CLOSED,
	ATH_AGGR_LIMITED,
};

struct ath_txq {
	u32 axq_qnum;
	u32 *axq_link;
	struct list_head axq_q;
	spinlock_t axq_lock;
	u32 axq_depth;
	u8 axq_aggr_depth;
	u32 axq_totalqueued;
	bool stopped;
	struct ath_buf *axq_linkbuf;

	/* first desc of the last descriptor that contains CTS */
	struct ath_desc *axq_lastdsWithCTS;

	/* final desc of the gating desc that determines whether
	   lastdsWithCTS has been DMA'ed or not */
	struct ath_desc *axq_gatingds;

	struct list_head axq_acq;
};

#define AGGR_CLEANUP         BIT(1)
#define AGGR_ADDBA_COMPLETE  BIT(2)
#define AGGR_ADDBA_PROGRESS  BIT(3)

struct ath_atx_tid {
	struct list_head list;
	struct list_head buf_q;
	struct ath_node *an;
	struct ath_atx_ac *ac;
	struct ath_buf *tx_buf[ATH_TID_MAX_BUFS];
	u16 seq_start;
	u16 seq_next;
	u16 baw_size;
	int tidno;
	int baw_head;	/* first un-acked tx buffer */
	int baw_tail;	/* next unused tx buffer slot */
	int sched;
	int paused;
	u8 state;
	int addba_exchangeattempts;
};

struct ath_atx_ac {
	int sched;
	int qnum;
	struct list_head list;
	struct list_head tid_q;
};

struct ath_tx_control {
	struct ath_txq *txq;
	int if_id;
	enum ath9k_internal_frame_type frame_type;
};

struct ath_xmit_status {
	int retries;
	int flags;
#define ATH_TX_ERROR        0x01
#define ATH_TX_XRETRY       0x02
#define ATH_TX_BAR          0x04
};

/* All RSSI values are noise floor adjusted */
struct ath_tx_stat {
	int rssi;
	int rssictl[ATH_MAX_ANTENNA];
	int rssiextn[ATH_MAX_ANTENNA];
	int rateieee;
	int rateKbps;
	int ratecode;
	int flags;
	u32 airtime;	/* time on air per final tx rate */
};

struct aggr_rifs_param {
	int param_max_frames;
	int param_max_len;
	int param_rl;
	int param_al;
	struct ath_rc_series *param_rcs;
};

struct ath_node {
	struct ath_softc *an_sc;
	struct ath_atx_tid tid[WME_NUM_TID];
	struct ath_atx_ac ac[WME_NUM_AC];
	u16 maxampdu;
	u8 mpdudensity;
};

struct ath_tx {
	u16 seq_no;
	u32 txqsetup;
	int hwq_map[ATH9K_WME_AC_VO+1];
	spinlock_t txbuflock;
	struct list_head txbuf;
	struct ath_txq txq[ATH9K_NUM_TX_QUEUES];
	struct ath_descdma txdma;
};

struct ath_rx {
	u8 defant;
	u8 rxotherant;
	u32 *rxlink;
	int bufsize;
	unsigned int rxfilter;
	spinlock_t rxflushlock;
	spinlock_t rxbuflock;
	struct list_head rxbuf;
	struct ath_descdma rxdma;
};

int ath_startrecv(struct ath_softc *sc);
bool ath_stoprecv(struct ath_softc *sc);
void ath_flushrecv(struct ath_softc *sc);
u32 ath_calcrxfilter(struct ath_softc *sc);
int ath_rx_init(struct ath_softc *sc, int nbufs);
void ath_rx_cleanup(struct ath_softc *sc);
int ath_rx_tasklet(struct ath_softc *sc, int flush);
struct ath_txq *ath_txq_setup(struct ath_softc *sc, int qtype, int subtype);
void ath_tx_cleanupq(struct ath_softc *sc, struct ath_txq *txq);
int ath_tx_setup(struct ath_softc *sc, int haltype);
void ath_drain_all_txq(struct ath_softc *sc, bool retry_tx);
void ath_draintxq(struct ath_softc *sc,
		     struct ath_txq *txq, bool retry_tx);
void ath_tx_node_init(struct ath_softc *sc, struct ath_node *an);
void ath_tx_node_cleanup(struct ath_softc *sc, struct ath_node *an);
void ath_txq_schedule(struct ath_softc *sc, struct ath_txq *txq);
int ath_tx_init(struct ath_softc *sc, int nbufs);
int ath_tx_cleanup(struct ath_softc *sc);
struct ath_txq *ath_test_get_txq(struct ath_softc *sc, struct sk_buff *skb);
int ath_txq_update(struct ath_softc *sc, int qnum,
		   struct ath9k_tx_queue_info *q);
int ath_tx_start(struct ieee80211_hw *hw, struct sk_buff *skb,
		 struct ath_tx_control *txctl);
void ath_tx_tasklet(struct ath_softc *sc);
void ath_tx_cabq(struct ieee80211_hw *hw, struct sk_buff *skb);
bool ath_tx_aggr_check(struct ath_softc *sc, struct ath_node *an, u8 tidno);
int ath_tx_aggr_start(struct ath_softc *sc, struct ieee80211_sta *sta,
		      u16 tid, u16 *ssn);
int ath_tx_aggr_stop(struct ath_softc *sc, struct ieee80211_sta *sta, u16 tid);
void ath_tx_aggr_resume(struct ath_softc *sc, struct ieee80211_sta *sta, u16 tid);

/********/
/* VIFs */
/********/

struct ath_vif {
	int av_bslot;
	enum nl80211_iftype av_opmode;
	struct ath_buf *av_bcbuf;
	struct ath_tx_control av_btxctl;
	u8 bssid[ETH_ALEN]; /* current BSSID from config_interface */
};

/*******************/
/* Beacon Handling */
/*******************/

/*
 * Regardless of the number of beacons we stagger, (i.e. regardless of the
 * number of BSSIDs) if a given beacon does not go out even after waiting this
 * number of beacon intervals, the game's up.
 */
#define BSTUCK_THRESH           	(9 * ATH_BCBUF)
#define	ATH_BCBUF               	1
#define ATH_DEFAULT_BINTVAL     	100 /* TU */
#define ATH_DEFAULT_BMISS_LIMIT 	10
#define IEEE80211_MS_TO_TU(x)           (((x) * 1000) / 1024)

struct ath_beacon_config {
	u16 beacon_interval;
	u16 listen_interval;
	u16 dtim_period;
	u16 bmiss_timeout;
	u8 dtim_count;
};

struct ath_beacon {
	enum {
		OK,		/* no change needed */
		UPDATE,		/* update pending */
		COMMIT		/* beacon sent, commit change */
	} updateslot;		/* slot time update fsm */

	u32 beaconq;
	u32 bmisscnt;
	u32 ast_be_xmit;
	u64 bc_tstamp;
	struct ieee80211_vif *bslot[ATH_BCBUF];
	struct ath_wiphy *bslot_aphy[ATH_BCBUF];
	int slottime;
	int slotupdate;
	struct ath9k_tx_queue_info beacon_qi;
	struct ath_descdma bdma;
	struct ath_txq *cabq;
	struct list_head bbuf;
};

void ath_beacon_tasklet(unsigned long data);
void ath_beacon_config(struct ath_softc *sc, struct ieee80211_vif *vif);
int ath_beaconq_setup(struct ath_hw *ah);
int ath_beacon_alloc(struct ath_wiphy *aphy, struct ieee80211_vif *vif);
void ath_beacon_return(struct ath_softc *sc, struct ath_vif *avp);

/*******/
/* ANI */
/*******/

#define ATH_STA_SHORT_CALINTERVAL 1000    /* 1 second */
#define ATH_AP_SHORT_CALINTERVAL  100     /* 100 ms */
#define ATH_ANI_POLLINTERVAL      100     /* 100 ms */
#define ATH_LONG_CALINTERVAL      30000   /* 30 seconds */
#define ATH_RESTART_CALINTERVAL   1200000 /* 20 minutes */

struct ath_ani {
	bool caldone;
	int16_t noise_floor;
	unsigned int longcal_timer;
	unsigned int shortcal_timer;
	unsigned int resetcal_timer;
	unsigned int checkani_timer;
	struct timer_list timer;
};

/********************/
/*   LED Control    */
/********************/

#define ATH_LED_PIN	1
#define ATH_LED_ON_DURATION_IDLE	350	/* in msecs */
#define ATH_LED_OFF_DURATION_IDLE	250	/* in msecs */

enum ath_led_type {
	ATH_LED_RADIO,
	ATH_LED_ASSOC,
	ATH_LED_TX,
	ATH_LED_RX
};

struct ath_led {
	struct ath_softc *sc;
	struct led_classdev led_cdev;
	enum ath_led_type led_type;
	char name[32];
	bool registered;
};

/* Rfkill */
#define ATH_RFKILL_POLL_INTERVAL	2000 /* msecs */

struct ath_rfkill {
	struct rfkill *rfkill;
	struct delayed_work rfkill_poll;
	char rfkill_name[32];
};

/********************/
/* Main driver core */
/********************/

/*
 * Default cache line size, in bytes.
 * Used when PCI device not fully initialized by bootrom/BIOS
*/
#define DEFAULT_CACHELINE       32
#define	ATH_DEFAULT_NOISE_FLOOR -95
#define ATH_REGCLASSIDS_MAX     10
#define ATH_CABQ_READY_TIME     80      /* % of beacon interval */
#define ATH_MAX_SW_RETRIES      10
#define ATH_CHAN_MAX            255
#define IEEE80211_WEP_NKID      4       /* number of key ids */

/*
 * The key cache is used for h/w cipher state and also for
 * tracking station state such as the current tx antenna.
 * We also setup a mapping table between key cache slot indices
 * and station state to short-circuit node lookups on rx.
 * Different parts have different size key caches.  We handle
 * up to ATH_KEYMAX entries (could dynamically allocate state).
 */
#define	ATH_KEYMAX	        128     /* max key cache size we handle */

#define ATH_TXPOWER_MAX         100     /* .5 dBm units */
#define ATH_RSSI_DUMMY_MARKER   0x127
#define ATH_RATE_DUMMY_MARKER   0

#define SC_OP_INVALID           BIT(0)
#define SC_OP_BEACONS           BIT(1)
#define SC_OP_RXAGGR            BIT(2)
#define SC_OP_TXAGGR            BIT(3)
#define SC_OP_CHAINMASK_UPDATE  BIT(4)
#define SC_OP_FULL_RESET        BIT(5)
#define SC_OP_PREAMBLE_SHORT    BIT(6)
#define SC_OP_PROTECT_ENABLE    BIT(7)
#define SC_OP_RXFLUSH           BIT(8)
#define SC_OP_LED_ASSOCIATED    BIT(9)
#define SC_OP_RFKILL_REGISTERED BIT(10)
#define SC_OP_RFKILL_SW_BLOCKED BIT(11)
#define SC_OP_RFKILL_HW_BLOCKED BIT(12)
#define SC_OP_WAIT_FOR_BEACON   BIT(13)
#define SC_OP_LED_ON            BIT(14)
#define SC_OP_SCANNING          BIT(15)
#define SC_OP_TSF_RESET         BIT(16)

struct ath_bus_ops {
	void		(*read_cachesize)(struct ath_softc *sc, int *csz);
	void		(*cleanup)(struct ath_softc *sc);
	bool		(*eeprom_read)(struct ath_hw *ah, u32 off, u16 *data);
};

struct ath_wiphy;

struct ath_softc {
	struct ieee80211_hw *hw;
	struct device *dev;

	spinlock_t wiphy_lock; /* spinlock to protect ath_wiphy data */
	struct ath_wiphy *pri_wiphy;
	struct ath_wiphy **sec_wiphy; /* secondary wiphys (virtual radios); may
				       * have NULL entries */
	int num_sec_wiphy; /* number of sec_wiphy pointers in the array */
	int chan_idx;
	int chan_is_ht;
	struct ath_wiphy *next_wiphy;
	struct work_struct chan_work;
	int wiphy_select_failures;
	unsigned long wiphy_select_first_fail;
	struct delayed_work wiphy_work;
	unsigned long wiphy_scheduler_int;
	int wiphy_scheduler_index;

	struct tasklet_struct intr_tq;
	struct tasklet_struct bcon_tasklet;
	struct ath_hw *sc_ah;
	void __iomem *mem;
	int irq;
	spinlock_t sc_resetlock;
	struct mutex mutex;

	u8 curbssid[ETH_ALEN];
	u8 bssidmask[ETH_ALEN];
	u32 intrstatus;
	u32 sc_flags; /* SC_OP_* */
	u16 curtxpow;
	u16 curaid;
	u16 cachelsz;
	u8 nbcnvifs;
	u16 nvifs;
	u8 tx_chainmask;
	u8 rx_chainmask;
	u32 keymax;
	DECLARE_BITMAP(keymap, ATH_KEYMAX);
	u8 splitmic;
	atomic_t ps_usecount;
	enum ath9k_int imask;
	enum ath9k_ht_extprotspacing ht_extprotspacing;
	enum ath9k_ht_macmode tx_chan_width;

	struct ath_config config;
	struct ath_rx rx;
	struct ath_tx tx;
	struct ath_beacon beacon;
	struct ieee80211_rate rates[IEEE80211_NUM_BANDS][ATH_RATE_MAX];
	struct ath_rate_table *hw_rate_table[ATH9K_MODE_MAX];
	struct ath_rate_table *cur_rate_table;
	struct ieee80211_supported_band sbands[IEEE80211_NUM_BANDS];

	struct ath_led radio_led;
	struct ath_led assoc_led;
	struct ath_led tx_led;
	struct ath_led rx_led;
	struct delayed_work ath_led_blink_work;
	int led_on_duration;
	int led_off_duration;
	int led_on_cnt;
	int led_off_cnt;

	struct ath_rfkill rf_kill;
	struct ath_ani ani;
	struct ath9k_node_stats nodestats;
#ifdef CONFIG_ATH9K_DEBUG
	struct ath9k_debug debug;
#endif
	struct ath_bus_ops *bus_ops;
};

struct ath_wiphy {
	struct ath_softc *sc; /* shared for all virtual wiphys */
	struct ieee80211_hw *hw;
	enum ath_wiphy_state {
		ATH_WIPHY_INACTIVE,
		ATH_WIPHY_ACTIVE,
		ATH_WIPHY_PAUSING,
		ATH_WIPHY_PAUSED,
		ATH_WIPHY_SCAN,
	} state;
	int chan_idx;
	int chan_is_ht;
};

int ath_reset(struct ath_softc *sc, bool retry_tx);
int ath_get_hal_qnum(u16 queue, struct ath_softc *sc);
int ath_get_mac80211_qnum(u32 queue, struct ath_softc *sc);
int ath_cabq_update(struct ath_softc *);

static inline void ath_read_cachesize(struct ath_softc *sc, int *csz)
{
	sc->bus_ops->read_cachesize(sc, csz);
}

static inline void ath_bus_cleanup(struct ath_softc *sc)
{
	sc->bus_ops->cleanup(sc);
}

extern struct ieee80211_ops ath9k_ops;

irqreturn_t ath_isr(int irq, void *dev);
void ath_cleanup(struct ath_softc *sc);
int ath_attach(u16 devid, struct ath_softc *sc);
void ath_detach(struct ath_softc *sc);
const char *ath_mac_bb_name(u32 mac_bb_version);
const char *ath_rf_name(u16 rf_version);
void ath_set_hw_capab(struct ath_softc *sc, struct ieee80211_hw *hw);
void ath9k_update_ichannel(struct ath_softc *sc, struct ieee80211_hw *hw,
			   struct ath9k_channel *ichan);
void ath_update_chainmask(struct ath_softc *sc, int is_ht);
int ath_set_channel(struct ath_softc *sc, struct ieee80211_hw *hw,
		    struct ath9k_channel *hchan);
void ath_radio_enable(struct ath_softc *sc);
void ath_radio_disable(struct ath_softc *sc);

#ifdef CONFIG_PCI
int ath_pci_init(void);
void ath_pci_exit(void);
#else
static inline int ath_pci_init(void) { return 0; };
static inline void ath_pci_exit(void) {};
#endif

#ifdef CONFIG_ATHEROS_AR71XX
int ath_ahb_init(void);
void ath_ahb_exit(void);
#else
static inline int ath_ahb_init(void) { return 0; };
static inline void ath_ahb_exit(void) {};
#endif

static inline void ath9k_ps_wakeup(struct ath_softc *sc)
{
	if (atomic_inc_return(&sc->ps_usecount) == 1)
		if (sc->sc_ah->power_mode !=  ATH9K_PM_AWAKE) {
			sc->sc_ah->restore_mode = sc->sc_ah->power_mode;
			ath9k_hw_setpower(sc->sc_ah, ATH9K_PM_AWAKE);
		}
}

static inline void ath9k_ps_restore(struct ath_softc *sc)
{
	if (atomic_dec_and_test(&sc->ps_usecount))
		if ((sc->hw->conf.flags & IEEE80211_CONF_PS) &&
		    !(sc->sc_flags & SC_OP_WAIT_FOR_BEACON))
			ath9k_hw_setpower(sc->sc_ah,
					  sc->sc_ah->restore_mode);
}


void ath9k_set_bssid_mask(struct ieee80211_hw *hw);
int ath9k_wiphy_add(struct ath_softc *sc);
int ath9k_wiphy_del(struct ath_wiphy *aphy);
void ath9k_tx_status(struct ieee80211_hw *hw, struct sk_buff *skb);
int ath9k_wiphy_pause(struct ath_wiphy *aphy);
int ath9k_wiphy_unpause(struct ath_wiphy *aphy);
int ath9k_wiphy_select(struct ath_wiphy *aphy);
void ath9k_wiphy_set_scheduler(struct ath_softc *sc, unsigned int msec_int);
void ath9k_wiphy_chan_work(struct work_struct *work);
bool ath9k_wiphy_started(struct ath_softc *sc);
void ath9k_wiphy_pause_all_forced(struct ath_softc *sc,
				  struct ath_wiphy *selected);
bool ath9k_wiphy_scanning(struct ath_softc *sc);
void ath9k_wiphy_work(struct work_struct *work);

#endif /* ATH9K_H */
