/*
 * Copyright (c) 2010 - 2011 Atheros Communications Inc.
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

#ifndef _ALX_H_
#define _ALX_H_

#include <linux/types.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/in.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/sctp.h>
#include <linux/pkt_sched.h>
#include <linux/ipv6.h>
#include <linux/slab.h>
#include <net/checksum.h>
#include <net/ip6_checksum.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/mii.h>

#include <linux/bitops.h>
#include <linux/cpumask.h>
#include <linux/aer.h>

#include "alx_sw.h"

/*
 * Definition to enable some features
 */
#undef CONFIG_ALX_MSIX
#undef CONFIG_ALX_MSI
#undef CONFIG_ALX_MTQ
#undef CONFIG_ALX_MRQ
#undef CONFIG_ALX_RSS
/* #define CONFIG_ALX_MSIX */
#define CONFIG_ALX_MSI
#define CONFIG_ALX_MTQ
#define CONFIG_ALX_MRQ
#ifdef CONFIG_ALX_MRQ
#define CONFIG_ALX_RSS
#endif

/*
 * Definition for validate HW
 */
#define ALX_VALID_MTQ 1
#define ALX_VALID_RSS 0
#if ALX_VALID_RSS
#define ALX_DUMP_RSS_DESC 1
#else
#define ALX_DUMP_RSS_DESC 0
#endif

/*
 * Definition for Dumping msg
 */
/* TPD, RRD and RFD Description */
#define ALX_DUMP_RRD_DESC   0
#define ALX_DUMP_RFD_DESC   0
#define ALX_DUMP_TPD_DESC   0

/* Definitions for printing message */
#define ALX_MSG_LV_ERR		1
#define ALX_MSG_LV_EMERG	1
#define ALX_MSG_LV_WARNING	0
#define ALX_MSG_LV_INFO		0
#define ALX_MSG_LV_DEBUG	0
#define ALX_MSG_PFX_NAME	"alx: "

#define ALX_MSG_INIT		BIT_1 /* PCI IF part, And always print */
#define ALX_MSG_PCI		BIT_2 /* PCI IF part, But doesn't */
#define ALX_MSG_IF		BIT_3
#define ALX_MSG_RX		BIT_4
#define ALX_MSG_TX		BIT_5
#define ALX_MSG_INTR		BIT_6
#define ALX_MSG_HW		BIT_7
#define ALX_MSG_WOL		BIT_8
#define ALX_MSG_TIMER		BIT_9
#define ALX_MSG_ETHTOOL		BIT_10
#define ALX_MSG_IOCTL		BIT_11
#define ALX_MSG_PARAM		BIT_12
#define ALX_MSG_FUNC		BIT_13

#define ALX_MSG_GENERAL		(\
		ALX_MSG_INIT)
#define ALX_MSG_ALL		(\
		ALX_MSG_INIT	|\
		ALX_MSG_PCI	|\
		ALX_MSG_IF	|\
		ALX_MSG_RX	|\
		ALX_MSG_TX	|\
		ALX_MSG_INTR	|\
		ALX_MSG_HW	|\
		ALX_MSG_WOL	|\
		ALX_MSG_TIMER	|\
		ALX_MSG_ETHTOOL	|\
		ALX_MSG_IOCTL	|\
		ALX_MSG_FUNC    |\
		ALX_MSG_PARAM)

#define ALX_MSG_DEFAULT		ALX_MSG_GENERAL

#define DRV_PRINT(_mlv, _klv, _fmt, _args...) \
	if (ALX_MSG_LV_##_klv || (ALX_MSG_##_mlv & adpt->msg_flags)) {\
		printk(KERN_##_klv ALX_MSG_PFX_NAME "%s: %s: " _fmt, \
			adpt->netdev->name, __func__ , ## _args); \
	}

/*
 * Definitions for ioctl
 *
 * redefine them as alx own ioctl vector
 */
#define SIOCDEVGMACREG	0x89F0	/* Read MAC Register */
#define SIOCDEVSMACREG	0x89F1	/* Write MAC Register */
/* This structure is used in all SIOCxMIIxxx ioctl calls */
struct mac_ioctl_data {
	__u32	reg_num;
	__u32	reg_val;
};

#if ALX_VALID_RSS
#define SIOCDEVVALIDRSS 0x89FA
struct valid_rss_ioctl_data {
	__u16	cmd_id;
	__u16	tbl_idx;
	__u16	tbl_val;
}
#endif

/* TODO: refine */
#define AT_VLAN_TO_TAG(_vlan, _tag)      \
	_tag =  (((_vlan >> 8) & 0xFF) | \
		 ((_vlan & 0xFF) << 8))

#define AT_TAG_TO_VLAN(_tag, _vlan)       \
	_vlan = ((((_tag) >> 8) & 0xFF) | \
		(((_tag) & 0xFF) << 8))

/* Coalescing Message Block */
struct coals_msg_block {
	int test;
};


#define BAR_0   0

#define ALX_DEF_RX_BUF_SIZE	1536
#define ALX_MAX_JUMBO_PKT_SIZE	(9*1024)
#define ALX_MAX_TSO_PKT_SIZE	(7*1024)

#define ALX_MAX_ETH_FRAME_SIZE	ALX_MAX_JUMBO_PKT_SIZE
#define ALX_MIN_ETH_FRAME_SIZE	68


#define ALX_MAX_RX_QUEUES	8
#define ALX_MAX_TX_QUEUES	4
#define ALX_MAX_HANDLED_INTRS	5

#define ALX_WATCHDOG_TIME   (5 * HZ)

struct alx_cmb {
	char name[IFNAMSIZ + 9];
	void *cmb;
	dma_addr_t dma;
};
struct alx_smb {
	char name[IFNAMSIZ + 9];
	void *smb;
	dma_addr_t dma;
};


/*
 * RRD : definition
 */
struct alx_rrdes_general {
	u32 xsum:16;
	u32 nor:4;  /* number of RFD */
	u32 si:12;  /* start index of rfd-ring */

	u32 hash;

	u32 vlan_tag:16; /* vlan-tag */
	u32 pid:8;       /* Header Length of Header-Data Split. WORD unit */
	u32 reserve0:1;
	u32 rss_cpu:3;   /* CPU number used by RSS */
#if 0
	u32 rss_t6:1;	/* TCP(IPv6) flag for RSS hash algrithm */
	u32 rss_i6:1;	/* IPv6 flag for RSS hash algrithm */
	u32 rss_t4:1;	/* TCP(IPv4)  flag for RSS hash algrithm */
	u32 rss_i4:1;	/* IPv4 flag for RSS hash algrithm */
#endif
	u32 rss_flag:4;

	u32 pkt_len:14; /* length of the packet */
	u32 l4f:1;      /* L4(TCP/UDP) checksum failed */
	u32 ipf:1;      /* IP checksum failed */
	u32 vlan_flag:1;/* vlan tag */
	u32 reserve1:3;
	u32 res:1;      /* received error summary */
	u32 crc:1;      /* crc error */
	u32 fae:1;      /* frame alignment error */
	u32 trunc:1;    /* truncated packet, larger than MTU */
	u32 runt:1;     /* runt packet */
	u32 icmp:1;     /* incomplete packet,
			 * due to insufficient rx-descriptor
			 */
	u32 bar:1;      /* broadcast address received */
	u32 mar:1;      /* multicast address received */
	u32 type:1;     /* ethernet type */
	u32 fov:1;      /* fifo overflow*/
	u32 lene:1;     /* length error */
	u32 update:1;   /* update*/
};

struct alx_rrdesc {
	union {
		struct alx_rrdes_general gnr;
		/* flat format */
		union {
			struct {
				u32 dw0;
				u32 dw1;
				u32 dw2;
				u32 dw3;
			} d;
			struct {
				u64 qw0;
				u64 qw1;
			} q;
		} fmt;
	} rr_desc;
};
#define rr_dw0	rr_desc.fmt.d.dw0
#define rr_dw1	rr_desc.fmt.d.dw1
#define rr_dw2	rr_desc.fmt.d.dw2
#define rr_dw3	rr_desc.fmt.d.dw3
#define rr_gnr	rr_desc.gnr

/*
 * TPD : definition
 */

/* RFD desciptor */
struct alx_rfdes_general {
	u64   addr;
};

struct alx_rfdesc {
	union {
		struct alx_rfdes_general gnr;
		/* flat format */
		union {
			struct {
				u32 dw0;
				u32 dw1;
			} d;
			struct {
				u64 qw0;
			} q;
		} fmt;
	} rf_desc;
};
#define rf_dw0	rf_desc.fmt.d.dw0
#define rf_dw1	rf_desc.fmt.d.dw1
#define rf_qw0	rf_desc.fmt.q.qw0
#define rf_gnr	rf_desc.gnr

/*
 * TPD : definition
 */

/* tpd - general parameter format */
struct alx_tpdes_general {
	u32  buffer_len:16; /* include 4-byte CRC */
	u32  vlan_tag:16;

	u32  l4hdr_offset:8; /* tcp/udp header offset to the 1st byte of
			      * the packet
			      */
	u32  c_csum:1;   /* must be 0 in this format */
	u32  ip_csum:1;  /* do ip(v4) header checksum offload */
	u32  tcp_csum:1; /* do tcp checksum offload, both ipv4 and ipv6 */
	u32  udp_csum:1; /* do udp checksum offlaod, both ipv4 and ipv6 */
	u32  lso:1;
	u32  lso_v2:1;  /* must be 0 in this format */
	u32  vtagged:1; /* vlan-id tagged already */
	u32  instag:1;  /* insert vlan tag */

	u32  ipv4:1;    /* ipv4 packet */
	u32  type:1;    /* type of packet (ethernet_ii(1) or snap(0)) */
	u32  reserve:12; /* reserved, must be 0 */
	u32  epad:1;     /* even byte padding when this packet */
	u32  last_frag:1; /* last fragment(buffer) of the packet */

	u64 addr;
};

/* tpd - custom checksum parameter format */
struct alx_tpdes_checksum {
	u32 buffer_len:16; /* include 4-byte CRC */
	u32 vlan_tag:16;

	u32 payld_offset:8; /* payload offset to the 1st byte of
			     *  the packet
			     */
	u32 c_sum:1;  /* do custom chekcusm offload,
		       * must be 1 in this format
		       */
	u32 ip_sum:1;   /* must be 0 in thhis format */
	u32 tcp_sum:1;  /* must be 0 in this format */
	u32 udp_sum:1;  /* must be 0 in this format */
	u32 lso:1;      /* must be 0 in this format */
	u32 lso_v2:1;   /* must be 0 in this format */
	u32 vtagged:1;  /* vlan-id tagged already */
	u32 instag:1;   /* insert vlan tag */

	u32 ipv4:1;     /* ipv4 packet */
	u32 type:1;     /* type of packet (ethernet_ii(1) or snap(0)) */
	u32 cxsum_offset:8;  /* checksum offset to the 1st byte of
			      * the packet
			      */
	u32 reserve:4;  /* reserved, must be 0 */
	u32 epad:1;     /* even byte padding when this packet */
	u32 last_frag:1; /* last fragment(buffer) of the packet */

	u64 addr;
};


/* tpd - tcp large send format (v1/v2) */
struct alx_tpdes_tso {
	u32 buffer_len:16; /* include 4-byte CRC */
	u32 vlan_tag:16;

	u32 tcphdr_offset:8; /* tcp hdr offset to the 1st byte of packet */
	u32 c_sum:1;   /* must be 0 in this format */
	u32 ip_sum:1;  /* must be 0 in thhis format */
	u32 tcp_sum:1; /* must be 0 in this format */
	u32 udp_sum:1; /* must be 0 in this format */
	u32 lso:1;     /* do tcp large send (ipv4 only) */
	u32 lso_v2:1;  /* must be 0 in this format */
	u32 vtagged:1; /* vlan-id tagged already */
	u32 instag:1;  /* insert vlan tag */

	u32 ipv4:1;    /* ipv4 packet */
	u32 type:1;    /* type of packet (ethernet_ii(1) or snap(0)) */
	u32 mss:13;    /* MSS if do tcp large send */
	u32 last_frag:1; /* last fragment(buffer) of the packet */

	u32 addr_lo;
	u32 addr_hi;
};

struct alx_tpdesc {
	union {
		struct alx_tpdes_general   gnr;
		struct alx_tpdes_checksum  sum;
		struct alx_tpdes_tso       tso;
		/* flat format */
		union {
			struct {
				u32 dw0;
				u32 dw1;
				u32 dw2;
				u32 dw3;
			} d;
			struct {
				u64 qw0;
				u64 qw1;
			} q;
		} fmt;
	} tp_desc;
};
#define tp_dw0	tp_desc.fmt.d.dw0
#define tp_dw1	tp_desc.fmt.d.dw1
#define tp_dw2	tp_desc.fmt.d.dw2
#define tp_dw3	tp_desc.fmt.d.dw3
#define tp_qw0	tp_desc.fmt.q.qw0
#define tp_qw1	tp_desc.fmt.q.qw1
#define tp_gnr	tp_desc.gnr
#define tp_sum	tp_desc.sum
#define tp_tso	tp_desc.tso


#define ALX_RRD(_que, _i)	\
		(&(((struct alx_rrdesc *)(_que)->rrq.rrdesc)[(_i)]))
#define ALX_RFD(_que, _i)	\
		(&(((struct alx_rfdesc *)(_que)->rfq.rfdesc)[(_i)]))
#define ALX_TPD(_que, _i)	\
		(&(((struct alx_tpdesc *)(_que)->tpq.tpdesc)[(_i)]))


/*
 * alx_ring_header represents a single, contiguous block of DMA space
 * mapped for the three descriptor rings (tpd, rfd, rrd) and the two
 * message blocks (cmb, smb) described below
 */
struct alx_ring_header {
	void *desc;             /* virtual address */
	dma_addr_t dma;         /* physical address*/
	unsigned int size;      /* length in bytes */
	unsigned int used;
};


/*
 * alx_buffer is wrapper around a pointer to a socket buffer
 * so a DMA handle can be stored along with the skb
 */
struct alx_buffer {
	struct sk_buff *skb;    /* socket buffer */
	u16 length;             /* rx buffer length */
	dma_addr_t dma;
};

struct alx_sw_buffer {
	struct sk_buff *skb;    /* socket buffer */

	u32 vlan_tag:16;
	u32 vlan_flag:1;
	u32 reserved:15;
};

/* receive free descriptor (rfd) queue */
struct alx_rfd_queue {
	struct alx_buffer *rfbuff;
	struct alx_rfdesc *rfdesc;	/* virtual address */
	dma_addr_t rfdma;	/* physical address */
	u16 size;	/* length in bytes */
	u16 count;	/* number of descriptors in the ring */

	u16 produce_idx; /* it's written to rxque->produce_reg */
	u16 consume_idx; /* unused*/
};

/* receive return desciptor (rrd) queue */
struct alx_rrd_queue {
	struct alx_rrdesc *rrdesc;	/* virtual address */
	dma_addr_t rrdma;	/* physical address */
	u16 size;	/* length in bytes */
	u16 count;	/* number of descriptors in the ring */

	u16 produce_idx; /* unused */
	u16 consume_idx; /* rxque->consume_reg */
};

/* software desciptor (swd) queue */
struct alx_swd_queue {
	struct alx_sw_buffer *swbuff;
	u16 count;	/* number of descriptors in the ring */
	u16 produce_idx;
	u16 consume_idx;
};

/* rx queue */
struct alx_rx_queue {
	struct device *dev;	/* device for dma mapping */
	struct net_device *netdev;	/* netdev ring belongs to */
	struct alx_msix_param *msix;

	struct alx_rrd_queue rrq;
	struct alx_rfd_queue rfq;
	struct alx_swd_queue swq;


	u16 que_idx;		/* index in multi rx queues*/
	u16 max_packets;		/* max work per interrupt */

	u16 produce_reg;
	u16 consume_reg;
	u32 flags;
};
#define ALX_RX_FLAG_SW_QUE    BIT_0
#define ALX_RX_FLAG_HW_QUE    BIT_1
#define CHK_RX_FLAG(_flag)    CHK_FLAG(rxque, RX, _flag)
#define SET_RX_FLAG(_flag)    SET_FLAG(rxque, RX, _flag)
#define CLI_RX_FLAG(_flag)    CLI_FLAG(rxque, RX, _flag)

#define GET_RF_BUFFER(_rque, _i)    (&((_rque)->rfq.rfbuff[(_i)]))
#define GET_SW_BUFFER(_rque, _i)    (&((_rque)->swq.swbuff[(_i)]))


/* transimit packet descriptor (tpd) ring */
struct alx_tpd_queue {
	struct alx_buffer *tpbuff;
	struct alx_tpdesc *tpdesc;	/* virtual address */
	dma_addr_t tpdma;	/* physical address */
	u16 size;	/* length in bytes */
	u16 count;	/* number of descriptors in the ring */

	u16 produce_idx;
	u16 consume_idx;
	u16 last_produce_idx;
};

/* tx queue */
struct alx_tx_queue {
	struct device *dev;	/* device for dma mapping */
	struct net_device *netdev;	/* netdev ring belongs to */

	struct alx_tpd_queue tpq;
	struct alx_msix_param *msix;

	u16 que_idx;     /* needed for multiqueue queue management */
	u16 max_packets; /* max packets per interrupt */

	u16 produce_reg;
	u16 consume_reg;
};
#define GET_TP_BUFFER(_tque, _i)    (&((_tque)->tpq.tpbuff[(_i)]))


/*
 * definition for array allocations.
 */
#define ALX_MAX_MSIX_INTRS	16
#define ALX_MAX_RX_QUEUES	8
#define ALX_MAX_TX_QUEUES	4

enum alx_msix_type {
	alx_msix_type_rx,
	alx_msix_type_tx,
	alx_msix_type_other,
};
#define ALX_MSIX_TYPE_OTH_TIMER		0
#define ALX_MSIX_TYPE_OTH_ALERT		1
#define ALX_MSIX_TYPE_OTH_SMB		2
#define ALX_MSIX_TYPE_OTH_PHY		3

/* ALX_MAX_MSIX_INTRS of these are allocated,
 * but we only use one per queue-specific vector.
 */
struct alx_msix_param {
	struct alx_adapter *adpt;
	unsigned int vec_idx; /* index in HW interrupt vector */
	char name[IFNAMSIZ + 9];

	/* msix interrupts for queue */
	u8 rx_map[ALX_MAX_RX_QUEUES];
	u8 tx_map[ALX_MAX_TX_QUEUES];
	u8 rx_count;     /* Rx ring count assigned to this vector */
	u8 tx_count;     /* Tx ring count assigned to this vector */

	struct napi_struct napi;
	cpumask_var_t affinity_mask;
	u32 flags;
};

#define ALX_MSIX_FLAG_RX0	BIT_0
#define ALX_MSIX_FLAG_RX1	BIT_1
#define ALX_MSIX_FLAG_RX2	BIT_2
#define ALX_MSIX_FLAG_RX3	BIT_3
#define ALX_MSIX_FLAG_RX4	BIT_4
#define ALX_MSIX_FLAG_RX5	BIT_5
#define ALX_MSIX_FLAG_RX6	BIT_6
#define ALX_MSIX_FLAG_RX7	BIT_7
#define ALX_MSIX_FLAG_TX0	BIT_8
#define ALX_MSIX_FLAG_TX1	BIT_9
#define ALX_MSIX_FLAG_TX2	BIT_10
#define ALX_MSIX_FLAG_TX3	BIT_11
#define ALX_MSIX_FLAG_TIMER	BIT_12
#define ALX_MSIX_FLAG_ALERT	BIT_13
#define ALX_MSIX_FLAG_SMB	BIT_14
#define ALX_MSIX_FLAG_PHY	BIT_15

#define ALX_MSIX_FLAG_RXS		(\
		ALX_MSIX_FLAG_RX0	|\
		ALX_MSIX_FLAG_RX1	|\
		ALX_MSIX_FLAG_RX2	|\
		ALX_MSIX_FLAG_RX3	|\
		ALX_MSIX_FLAG_RX4	|\
		ALX_MSIX_FLAG_RX5	|\
		ALX_MSIX_FLAG_RX6	|\
		ALX_MSIX_FLAG_RX7)
#define ALX_MSIX_FLAG_TXS		(\
		ALX_MSIX_FLAG_TX0	|\
		ALX_MSIX_FLAG_TX1	|\
		ALX_MSIX_FLAG_TX2	|\
		ALX_MSIX_FLAG_TX3)
#define ALX_MSIX_FLAG_ALL		(\
		ALX_MSIX_FLAG_RXS	|\
		ALX_MSIX_FLAG_TXS	|\
		ALX_MSIX_FLAG_TIMER	|\
		ALX_MSIX_FLAG_ALERT	|\
		ALX_MSIX_FLAG_SMB	|\
		ALX_MSIX_FLAG_PHY)

#define CHK_MSIX_FLAG(_flag)	CHK_FLAG(msix, MSIX, _flag)
#define SET_MSIX_FLAG(_flag)	SET_FLAG(msix, MSIX, _flag)
#define CLI_MSIX_FLAG(_flag)	CLI_FLAG(msix, MSIX, _flag)

/*
 *board specific private data structure
 */
struct alx_adapter {
	struct net_device *netdev;
	struct pci_dev *pdev;
	struct net_device_stats net_stats;
	bool netdev_registered;

	struct vlan_group   *vlgrp;
	u16 bd_number;    /* board number;*/

	struct alx_msix_param *msix[ALX_MAX_MSIX_INTRS];
	struct msix_entry *msix_entries;
	int num_msix_rxques;
	int num_msix_txques;
	int num_msix_noques;    /* true count of msix_noques for device */
	int num_msix_intrs;

	int min_msix_intrs;
	int max_msix_intrs;

	/* All Descriptor memory */
	struct alx_ring_header ring_header;

	/* TX */
	struct alx_tx_queue *tx_queue[ALX_MAX_TX_QUEUES];
	/* RX */
	struct alx_rx_queue *rx_queue[ALX_MAX_RX_QUEUES];

	u16  num_txques;
	u16  num_rxques; /* equal max(num_hw_rxques, num_sw_rxques) */
	u16  num_hw_rxques;
	u16  num_sw_rxques;

	u16  max_rxques;
	u16  max_txques;

	u16  num_txdescs;
	u16  num_rxdescs;

	u32  rxbuf_size;

	struct alx_cmb cmb;
	struct alx_smb smb;

	/* structs defined in alx_hw.h */
	struct alx_hw       hw;
	struct alx_hw_stats hw_stats;

	u32 *config_space;

	struct work_struct alx_task;
	struct timer_list  alx_timer;
	unsigned long      alx_state;

	unsigned long link_jiffies;

	u32 wol;
	spinlock_t tx_lock;
	spinlock_t rx_lock;
	atomic_t irq_sem;

	u16 msg_flags;
	u32 flags[2];
};

#define ALX_ADPT_FLAG_0_MSI_CAP		BIT_0
#define ALX_ADPT_FLAG_0_MSI_EN		BIT_1
#define ALX_ADPT_FLAG_0_MSIX_CAP	BIT_2
#define ALX_ADPT_FLAG_0_MSIX_EN		BIT_3
#define ALX_ADPT_FLAG_0_MRQ_CAP		BIT_4
#define ALX_ADPT_FLAG_0_MRQ_EN		BIT_5
#define ALX_ADPT_FLAG_0_MTQ_CAP		BIT_6
#define ALX_ADPT_FLAG_0_MTQ_EN		BIT_7
#define ALX_ADPT_FLAG_0_SRSS_CAP	BIT_8
#define ALX_ADPT_FLAG_0_SRSS_EN		BIT_9
#define ALX_ADPT_FLAG_0_FIXED_MSIX	BIT_28

#define ALX_ADPT_FLAG_1_RESET_REQUESTED		BIT_0
#define ALX_ADPT_FLAG_1_LSC_REQUESTED		BIT_1
#define ALX_ADPT_FLAG_1_DBG_REQUESTED		BIT_2

#define CHK_ADPT_FLAG(_idx, _flag)	\
		CHK_FLAG_ARRAY(adpt, _idx, ADPT, _flag)
#define SET_ADPT_FLAG(_idx, _flag)	\
		SET_FLAG_ARRAY(adpt, _idx, ADPT, _flag)
#define CLI_ADPT_FLAG(_idx, _flag)	\
		CLI_FLAG_ARRAY(adpt, _idx, ADPT, _flag)

#ifdef HAVE_NETDEV_STATS_IN_NETDEV
#define GET_NETDEV_STATS(_adpt)		&((_adpt)->netdev->stats);
#else
#define GET_NETDEV_STATS(_adpt)		&((_adpt)->net_stats);
#endif /* HAVE_NETDEV_STATS_IN_NETDEV */


/* default to trying for four seconds */
#define ALX_TRY_LINK_TIMEOUT (4 * HZ)

enum alx_state_t {
	__ALX_TESTING,
	__ALX_RESETTING,
	__ALX_DOWN,
	__ALX_SERVICE_SCHED,
	__ALX_IN_SFP_INIT,
};


#define ALX_OPEN_CTRL_IRQ_EN	BIT_0
#define ALX_OPEN_CTRL_MAC_EN	BIT_1

/* needed by alx_ethtool.c */
extern char alx_drv_name[];
extern const char alx_drv_version[];
extern int alx_open_internal(struct alx_adapter *adpt, u32 ctrl);
extern void alx_stop_internal(struct alx_adapter *adpt, u32 ctrl);
extern void alx_reinit_locked(struct alx_adapter *adpt);
extern void alx_set_ethtool_ops(struct net_device *netdev);
#ifdef ETHTOOL_OPS_COMPAT
extern int ethtool_ioctl(struct ifreq *ifr);
#endif



#endif /* _ALX_H_ */
