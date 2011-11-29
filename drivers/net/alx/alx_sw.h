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

#ifndef _ALX_SW_H_
#define _ALX_SW_H_

#include <linux/netdevice.h>
#include <linux/crc32.h>

/* Vendor ID */
#define ALX_VENDOR_ID   0x1969

/* Device IDs */
#define ALX_DEV_ID_AR8131	0x1063	/* l1c */
#define ALX_DEV_ID_AR8132	0x1062	/* l2c */
#define ALX_DEV_ID_AR8151_V1	0x1073	/* l1d_v1 */
#define ALX_DEV_ID_AR8151_V2	0x1083	/* l1d_v2 */
#define ALX_DEV_ID_AR8152_V1	0x2060	/* l2cb_v1 */
#define ALX_DEV_ID_AR8152_V2	0x2062	/* l2cb_v2 */
#define ALX_DEV_ID_AR8161	0x1091	/* l1f */
#define ALX_DEV_ID_AR8162	0x1090	/* l2f */

#define ALX_REV_ID_AR8152_V1_0	0xc0
#define ALX_REV_ID_AR8152_V1_1	0xc1
#define ALX_REV_ID_AR8152_V2_0	0xc0
#define ALX_REV_ID_AR8152_V2_1	0xc1

#define ALX_REV_ID_AR8161_V2_0  0x10  /* B0 */

/* Generic Registers */
#define ALX_ISR			0x1600
#define ALX_IMR			0x1604

#define ALX_ISR_DIS		BIT_31
#define ALX_ISR_RX_Q7		BIT_30
#define ALX_ISR_RX_Q6		BIT_29
#define ALX_ISR_RX_Q5		BIT_28
#define ALX_ISR_RX_Q4		BIT_27
#define ALX_ISR_PCIE_LNKDOWN	BIT_26
#define ALX_ISR_PCIE_CERR	BIT_25
#define ALX_ISR_PCIE_NFERR	BIT_24
#define ALX_ISR_PCIE_FERR	BIT_23
#define ALX_ISR_PCIE_UR		BIT_22
#define ALX_ISR_MAC_TX		BIT_21
#define ALX_ISR_MAC_RX		BIT_20
#define ALX_ISR_RX_Q3		BIT_19
#define ALX_ISR_RX_Q2		BIT_18
#define ALX_ISR_RX_Q1		BIT_17
#define ALX_ISR_RX_Q0		BIT_16
#define ALX_ISR_TX_Q0		BIT_15
#define ALX_ISR_TXQ_TO		BIT_14
#define ALX_ISR_PHY_LPW		BIT_13
#define ALX_ISR_PHY		BIT_12
#define ALX_ISR_TX_CREDIT	BIT_11
#define ALX_ISR_DMAW		BIT_10
#define ALX_ISR_DMAR		BIT_9
#define ALX_ISR_TXF_UR		BIT_8
#define ALX_ISR_TX_Q3		BIT_7
#define ALX_ISR_TX_Q2		BIT_6
#define ALX_ISR_TX_Q1		BIT_5
#define ALX_ISR_RFD_UR		BIT_4
#define ALX_ISR_RXF_OV		BIT_3
#define ALX_ISR_MANU		BIT_2
#define ALX_ISR_TIMER		BIT_1
#define ALX_ISR_SMB		BIT_0

#define ALX_IMR_NORMAL_MASK  (\
		ALX_ISR_MANU    |\
		ALX_ISR_RXF_OV  |\
		ALX_ISR_RFD_UR  |\
		ALX_ISR_TXF_UR  |\
		ALX_ISR_DMAR    |\
		ALX_ISR_TXQ_TO  |\
		ALX_ISR_DMAW    |\
		ALX_ISR_TXQ     |\
		ALX_ISR_RXQ     |\
		ALX_ISR_PHY_LPW |\
		ALX_ISR_PHY     |\
		ALX_ISR_PCIE_LNKDOWN)

#define ALX_ISR_ALERT_MASK  (\
		ALX_ISR_DMAR		|\
		ALX_ISR_DMAW		|\
		ALX_ISR_TXQ_TO		|\
		ALX_ISR_PCIE_FERR	|\
		ALX_ISR_PCIE_LNKDOWN	|\
		ALX_ISR_RFD_UR		|\
		ALX_ISR_RXF_OV)

#define ALX_ISR_TXQ  (\
		ALX_ISR_TX_Q0	|\
		ALX_ISR_TX_Q1	|\
		ALX_ISR_TX_Q2	|\
		ALX_ISR_TX_Q3)

#define ALX_ISR_RXQ  (\
		ALX_ISR_RX_Q0	|\
		ALX_ISR_RX_Q1	|\
		ALX_ISR_RX_Q2	|\
		ALX_ISR_RX_Q3	|\
		ALX_ISR_RX_Q4	|\
		ALX_ISR_RX_Q5	|\
		ALX_ISR_RX_Q6	|\
		ALX_ISR_RX_Q7)

#define ALX_ISR_OVER  (\
		ALX_ISR_RFD_UR	|\
		ALX_ISR_RXF_OV	|\
		ALX_ISR_TXF_UR)

#define ALX_ISR_ERROR  (\
		ALX_ISR_DMAR	|\
		ALX_ISR_TXQ_TO	|\
		ALX_ISR_DMAW	|\
		ALX_ISR_PCIE_LNKDOWN)

/* MISC Register */
#define ALX_MISC                        0x19C0
#define ALX_MISC_INTNLOSC_OPEN          BIT_3

/* delay function */
#define US_DELAY(_hw, _n)	__US_DELAY(_n)
#define MS_DELAY(_hw, _n)	__MS_DELAY(_n)
#define __US_DELAY(_n)		udelay(_n)
#define __MS_DELAY(_n)		mdelay(_n)

/* DMA address */
#define DMA_ADDR_HI_MASK     0xffffffff00000000ULL
#define DMA_ADDR_LO_MASK     0x00000000ffffffffULL

#define ALX_DMA_ADDR_HI(_addr)	 \
		((u32)(((u64)(_addr) & DMA_ADDR_HI_MASK) >> 32))
#define ALX_DMA_ADDR_LO(_addr)	 \
		((u32)((u64)(_addr) & DMA_ADDR_LO_MASK))

/* mac address length */
#define ALX_ETH_LENGTH_OF_ADDRESS	6
#define ALX_ETH_LENGTH_OF_HEADER	ETH_HLEN

#define ALX_ETH_CRC(_addr, _len)	ether_crc((_len), (_addr));

/* Autonegotiation advertised speeds */
/* Link speed */
#define ALX_LINK_SPEED_UNKNOWN		0x0
#define ALX_LINK_SPEED_10_HALF		0x0001
#define ALX_LINK_SPEED_10_FULL		0x0002
#define ALX_LINK_SPEED_100_HALF		0x0004
#define ALX_LINK_SPEED_100_FULL		0x0008
#define ALX_LINK_SPEED_1GB_FULL		0x0020

#define ALX_LINK_SPEED_DEFAULT        (\
		ALX_LINK_SPEED_10_HALF  |\
		ALX_LINK_SPEED_10_FULL  |\
		ALX_LINK_SPEED_100_HALF |\
		ALX_LINK_SPEED_100_FULL |\
		ALX_LINK_SPEED_1GB_FULL)

#define ALX_MAX_SETUP_LNK_CYCLE		100

/* Device Type definitions for new protocol MDIO commands */
#define ALX_MDIO_PMA_PMD_DEV		0x1
#define ALX_MDIO_PCS_DEV		0x3
#define ALX_MDIO_AUTO_NEG_DEV		0x7
#define ALX_MDIO_NORM_DEV		0x0

#define ALX_MDIO_DEV_TYPE_MASK		0xFF00
#define ALX_MDIO_DEV_TYPE_SHIFT		0x8
#define ALX_MDIO_REG_ADDR_MASK		0xff
#define ALX_MDIO_REG_ADDR_SHIFT		0x0

/* MDIO Lock */
#define ALX_MDIO_LOCK_INIT(_lock)	\
		spin_lock_init((_lock))
#define ALX_MDIO_LOCK(_lock)		\
		spin_lock_irqsave((_lock), hw->mdio_flags)
#define ALX_MDIO_UNLOCK(_lock)		\
		spin_unlock_irqrestore((_lock), hw->mdio_flags)

/* Wake On Lan */
#define ALX_WOL_PHY	BIT_0 /* PHY Status Change */
#define ALX_WOL_MAGIC	BIT_1 /* Magic Packet */

#define ALX_MAX_EEPROM_LEN	0x200
#define ALX_MAX_HWREG_LEN	0x200

/* RSS Settings */
enum alx_rss_mode {
	alx_rss_mode_disable = 0,
	alx_rss_sig_que = 1,
	alx_rss_mul_que_sig_int = 2,
	alx_rss_mul_que_mul_int = 4,
};

/* Flow Control Settings */
enum alx_fc_mode {
	alx_fc_none = 0,
	alx_fc_rx_pause,
	alx_fc_tx_pause,
	alx_fc_full,
	alx_fc_default
};

/* WRR Restrict Settings */
enum alx_wrr_mode {
	alx_wrr_mode_none = 0,
	alx_wrr_mode_high,
	alx_wrr_mode_high2,
	alx_wrr_mode_all
};

enum alx_mac_type {
	alx_mac_unknown = 0,
	alx_mac_l1c,
	alx_mac_l2c,
	alx_mac_l1d_v1,
	alx_mac_l1d_v2,
	alx_mac_l2cb_v1,
	alx_mac_l2cb_v20,
	alx_mac_l2cb_v21,
	alx_mac_l1f,
	alx_mac_l2f,
};


/* Statistics counters collected by the MAC */
struct alx_hw_stats {
	/* rx */
	unsigned long rx_ok;
	unsigned long rx_bcast;
	unsigned long rx_mcast;
	unsigned long rx_pause;
	unsigned long rx_ctrl;
	unsigned long rx_fcs_err;
	unsigned long rx_len_err;
	unsigned long rx_byte_cnt;
	unsigned long rx_runt;
	unsigned long rx_frag;
	unsigned long rx_sz_64B;
	unsigned long rx_sz_127B;
	unsigned long rx_sz_255B;
	unsigned long rx_sz_511B;
	unsigned long rx_sz_1023B;
	unsigned long rx_sz_1518B;
	unsigned long rx_sz_max;
	unsigned long rx_ov_sz;
	unsigned long rx_ov_rxf;
	unsigned long rx_ov_rrd;
	unsigned long rx_align_err;
	unsigned long rx_bc_byte_cnt;
	unsigned long rx_mc_byte_cnt;
	unsigned long rx_err_addr;

	/* tx */
	unsigned long tx_ok;
	unsigned long tx_bcast;
	unsigned long tx_mcast;
	unsigned long tx_pause;
	unsigned long tx_exc_defer;
	unsigned long tx_ctrl;
	unsigned long tx_defer;
	unsigned long tx_byte_cnt;
	unsigned long tx_sz_64B;
	unsigned long tx_sz_127B;
	unsigned long tx_sz_255B;
	unsigned long tx_sz_511B;
	unsigned long tx_sz_1023B;
	unsigned long tx_sz_1518B;
	unsigned long tx_sz_max;
	unsigned long tx_single_col;
	unsigned long tx_multi_col;
	unsigned long tx_late_col;
	unsigned long tx_abort_col;
	unsigned long tx_underrun;
	unsigned long tx_trd_eop;
	unsigned long tx_len_err;
	unsigned long tx_trunc;
	unsigned long tx_bc_byte_cnt;
	unsigned long tx_mc_byte_cnt;
	unsigned long update;
};

/* HW callback function pointer table */
struct alx_hw;
struct alx_hw_callbacks {
	/* NIC */
	int (*identify_nic)(struct alx_hw *);
	/* PHY */
	int (*init_phy)(struct alx_hw *);
	int (*reset_phy)(struct alx_hw *);
	int (*read_phy_reg)(struct alx_hw *, u32, u16, u16 *);
	int (*write_phy_reg)(struct alx_hw *, u32, u16, u16);
	/* Link */
	int (*setup_phy_link)(struct alx_hw *, u32, bool, bool);
	int (*setup_phy_link_speed)(struct alx_hw *, u32, bool, bool);
	int (*check_phy_link)(struct alx_hw *, u32 *, bool *);

	/* MAC */
	int (*reset_mac)(struct alx_hw *);
	int (*start_mac)(struct alx_hw *);
	int (*stop_mac)(struct alx_hw *);
	int (*config_mac)(struct alx_hw *, u16, u16, u16, u16, u16);
	int (*get_mac_addr)(struct alx_hw *, u8 *);
	int (*set_mac_addr)(struct alx_hw *, u8 *);
	int (*clear_mac_addr)(struct alx_hw *);
	int (*set_mc_addr)(struct alx_hw *, u8 *);
	int (*clear_mc_addr)(struct alx_hw *);

	/* intr */
	int (*ack_phy_intr)(struct alx_hw *);
	int (*enable_legacy_intr)(struct alx_hw *);
	int (*disable_legacy_intr)(struct alx_hw *);
	int (*enable_msix_intr)(struct alx_hw *, u8);
	int (*disable_msix_intr)(struct alx_hw *, u8);

	/* Configure */
	int (*config_rx)(struct alx_hw *);
	int (*config_tx)(struct alx_hw *);
	int (*config_fc)(struct alx_hw *);
	int (*config_rss)(struct alx_hw *, bool);
	int (*config_msix)(struct alx_hw *, u16, bool, bool);
	int (*config_wol)(struct alx_hw *, u32);
	int (*config_aspm)(struct alx_hw *, bool, bool);
	int (*config_mac_ctrl)(struct alx_hw *);
	int (*config_pow_save)(struct alx_hw *, u32,
				bool, bool, bool, bool);
	int (*reset_pcie)(struct alx_hw *, bool, bool);

	/* NVRam function */
	int (*check_nvram)(struct alx_hw *, bool *);
	int (*read_nvram)(struct alx_hw *, u16, u32 *);
	int (*write_nvram)(struct alx_hw *, u16, u32);

	/* Others */
	int (*get_ethtool_regs)(struct alx_hw *, void *);
};

struct alx_hw {
	struct alx_adapter	*adpt;
	struct alx_hw_callbacks	 cbs;
	u8 __iomem	*hw_addr; /* inner register address */
	u16		pci_venid;
	u16		pci_devid;
	u16		pci_sub_devid;
	u16		pci_sub_venid;
	u8		pci_revid;

	bool		long_cable;
	bool		aps_en;
	bool		hi_txperf;
	bool		msi_lnkpatch;
	u32		hwreg_sz;
	u32		eeprom_sz;

	/* PHY parameter */
	u32		phy_id;
	u32		autoneg_advertised;
	u32		link_speed;
	bool		link_up;
	spinlock_t	mdio_lock;
	unsigned long	mdio_flags;

	/* MAC parameter */
	enum alx_mac_type	mac_type;
	u8	mac_addr[ALX_ETH_LENGTH_OF_ADDRESS];
	u8	mac_perm_addr[ALX_ETH_LENGTH_OF_ADDRESS];
	u32	mtu;
	u16	rxstat_reg;
	u16	rxstat_sz;
	u16	txstat_reg;
	u16	txstat_sz;

	u16	tx_prod_reg[4];
	u16	tx_cons_reg[4];
	u16	rx_prod_reg[2];
	u16	rx_cons_reg[2];
	u64	tpdma[4];
	u64	rfdma[2];
	u64	rrdma[2];

	/* WRR parameter */
	enum alx_wrr_mode	wrr_mode;
	u32	wrr_prio0;
	u32	wrr_prio1;
	u32	wrr_prio2;
	u32	wrr_prio3;

	/* RSS parameter */
	enum alx_rss_mode	rss_mode;
	u8   rss_hstype;
	u8   rss_base_cpu;
	u16  rss_idt_size;
	u32  rss_idt[32];
	u8   rss_key[40];

	/* flow control parameter */
	enum alx_fc_mode cur_fc_mode; /* FC mode in effect */
	enum alx_fc_mode req_fc_mode; /* FC mode requested by caller */
	bool disable_fc_autoneg; /* Do not autonegotiate FC */
	bool fc_was_autonegged; /* Is current_mode the result of autonegging? */
	bool fc_single_pause;

	/* Others */
	u32	preamble;
	u32	intr_mask;
	u16	smb_timer;
	u16	imt;    /* Interrupt Moderator timer ( 2us resolution) */
	u32	flags;
};
#define ALX_HW_FLAG_L0S_CAP		BIT_0
#define ALX_HW_FLAG_L0S_EN		BIT_1
#define ALX_HW_FLAG_L1_CAP		BIT_2
#define ALX_HW_FLAG_L1_EN		BIT_3
#define ALX_HW_FLAG_PWSAVE_CAP		BIT_4
#define ALX_HW_FLAG_PWSAVE_EN		BIT_5
#define ALX_HW_FLAG_AZ_CAP		BIT_6
#define ALX_HW_FLAG_AZ_EN		BIT_7
#define ALX_HW_FLAG_PTP_CAP		BIT_8
#define ALX_HW_FLAG_PTP_EN		BIT_9
#define ALX_HW_FLAG_GIGA_CAP		BIT_10

#define ALX_HW_FLAG_PROMISC_EN		BIT_16   /* for mac ctrl reg */
#define ALX_HW_FLAG_VLANSTRIP_EN	BIT_17   /* for mac ctrl reg */
#define ALX_HW_FLAG_MULTIALL_EN		BIT_18   /* for mac ctrl reg */
#define CHK_HW_FLAG(_flag)	CHK_FLAG(hw, HW, _flag)
#define SET_HW_FLAG(_flag)	SET_FLAG(hw, HW, _flag)
#define CLI_HW_FLAG(_flag)	CLI_FLAG(hw, HW, _flag)


/* RSS hstype Definitions */
#define ALX_RSS_HSTYP_IPV4_EN	BIT_0
#define ALX_RSS_HSTYP_TCP4_EN	BIT_1
#define ALX_RSS_HSTYP_IPV6_EN	BIT_2
#define ALX_RSS_HSTYP_TCP6_EN	BIT_3
#define ALX_RSS_HSTYP_ALL_EN         (\
		ALX_RSS_HSTYP_IPV4_EN | \
		ALX_RSS_HSTYP_TCP4_EN | \
		ALX_RSS_HSTYP_IPV6_EN | \
		ALX_RSS_HSTYP_TCP6_EN)


/* definitions for flags */
#define CHK_FLAG_ARRAY(_st, _idx, _type, _flag)	\
		((_st)->flags[_idx] & (ALX_##_type##_FLAG_##_idx##_##_flag))
#define CHK_FLAG(_st, _type, _flag)	\
		((_st)->flags & (ALX_##_type##_FLAG_##_flag))

#define SET_FLAG_ARRAY(_st, _idx, _type, _flag) \
		((_st)->flags[_idx] |= (ALX_##_type##_FLAG_##_idx##_##_flag))
#define SET_FLAG(_st, _type, _flag) \
		((_st)->flags |= (ALX_##_type##_FLAG_##_flag))

#define CLI_FLAG_ARRAY(_st, _idx, _type, _flag) \
		((_st)->flags[_idx] &= ~(ALX_##_type##_FLAG_##_idx##_##_flag))
#define CLI_FLAG(_st, _type, _flag) \
		((_st)->flags &= ~(ALX_##_type##_FLAG_##_flag))


/* definitions for compatible with *_hw.c and *_hw.h */
#define __far
#define DEBUG_INFO(_a, _b)
#define DEBUG_INFOS(_a, _b)

typedef struct alx_hw ETHCONTEXT;
typedef ETHCONTEXT * PETHCONTEXT;


#define MAC_TYPE	MAC_TYPE_ASIC   /* (1:FPGA, 0:ASIC) */
#define PHY_TYPE	PHY_TYPE_ASIC   /* (2:F1, 1:FPGA, 0:ASIC) */

#define MAC_TYPE_ASIC	0
#define MAC_TYPE_FPGA	1
#define PHY_TYPE_ASIC	0
#define PHY_TYPE_FPGA	1
#define PHY_TYPE_F1	2   /* (2:F1, 1:FPGA, 0:ASIC) */


#define MEM_FLUSH(_hw)						\
	do {							\
		readl((_hw)->hw_addr);				\
	} while (0)

#define MEM_W32(_hw, _reg, _val)				\
	do {							\
		if (((_hw)->mac_type == alx_mac_l2cb_v20) &&	\
		    ((_reg) < 0x1400))				\
			readl((_hw)->hw_addr + (_reg));		\
		writel((_val), ((_hw)->hw_addr + _reg));	\
	} while (0)

#define MEM_R32(_hw, _reg, _pval)				\
	do {							\
		if (unlikely(!(_hw)->link_up))			\
			readl((_hw)->hw_addr + (_reg));		\
		*(u32 *)_pval = readl((_hw)->hw_addr + (_reg));	\
	} while (0)


#define MEM_W16(_hw, _reg, _val)				\
	do {							\
		if (((_hw)->mac_type == alx_mac_l2cb_v20) &&	\
		    ((_reg) < 0x1400))				\
			readl((_hw)->hw_addr + (_reg));		\
		writew((_val), ((_hw)->hw_addr + _reg));	\
	} while (0)


#define MEM_R16(_hw, _reg, _pval)				\
	do {							\
		if (unlikely(!(_hw)->link_up))			\
			readw((_hw)->hw_addr + (_reg));		\
		*(u16 *)_pval = readw((_hw)->hw_addr + (_reg));	\
	} while (0)


#define MEM_W8(_hw, _reg, _val)					\
	do {							\
		if (((_hw)->mac_type == alx_mac_l2cb_v20) &&	\
		    ((_reg) < 0x1400))				\
			readl((_hw)->hw_addr + (_reg));		\
		writeb((_val), ((_hw)->hw_addr + _reg));	\
	} while (0)


#define MEM_R8(_hw, _reg, _pval)				\
	do {							\
		if (unlikely(!(_hw)->link_up))			\
			readb((_hw)->hw_addr + (_reg));		\
		*(u8 *)_pval = readb((_hw)->hw_addr + (_reg));	\
	} while (0)


int alx_cfg_r32(struct alx_hw *hw, int reg, u32 *pval);
int alx_cfg_w32(struct alx_hw *hw, int reg, u32 val);

#define CFG_W32(_hw, _reg, _val) 	alx_cfg_w32(_hw, _reg, _val)
#define CFG_R32(_hw, _reg, _pval)	alx_cfg_r32(_hw, _reg, _pval)
#define CFG_W16(_hw, _reg, _val)	MEM_W16(_hw, _reg, _val)
#define CFG_R16(_hw, _reg, _pval)	MEM_R16(_hw, _reg, _pval)


/* special definitions for hw */
#define ALF_MAX_MSIX_NOQUE_INTRS	4
#define ALF_MIN_MSIX_NOQUE_INTRS	4
#define ALF_MAX_MSIX_QUEUE_INTRS	12
#define ALF_MIN_MSIX_QUEUE_INTRS	12
#define ALF_MAX_MSIX_INTRS \
		(ALF_MAX_MSIX_QUEUE_INTRS + ALF_MAX_MSIX_NOQUE_INTRS)
#define ALF_MIN_MSIX_INTRS \
		(ALF_MIN_MSIX_NOQUE_INTRS + ALF_MIN_MSIX_QUEUE_INTRS)


/* function */
extern int alc_init_hw_callbacks(struct alx_hw *hw);
extern int alf_init_hw_callbacks(struct alx_hw *hw);

/* Error Codes */
#define ALX_ERR_MAC_INIT		-1
#define ALX_ERR_MAC_RESET		-2
#define ALX_ERR_MAC_START		-3
#define ALX_ERR_MAC_STOP		-4
#define ALX_ERR_MAC_CONFIGURE		-5
#define ALX_ERR_MAC_ADDR		-6

#define ALX_ERR_PHY_RESET		-22
#define ALX_ERR_PHY_SETUP_LNK		-23
#define ALX_ERR_PHY_CHECK_LNK		-24
#define ALX_ERR_PHY_READ_REG		-25
#define ALX_ERR_PHY_WRITE_REG		-26
#define ALX_ERR_PHY_RESOLVED		-27

#define ALX_ERR_PCIE_RESET		-40
#define ALX_ERR_PWR_SAVING		-41
#define ALX_ERR_ASPM			-42
#define ALX_ERR_DISABLE_DRV		-43
#define ALX_ERR_FLOW_CONTROL		-44

#define ALX_ERR_NOT_SUPPORTED		0x7FFFFFFE

/* Logging message functions */
#define alx_hw_err(_hw, _format, ...)				\
	alx_hw_printk(ERR, _format, ##__VA_ARGS__)
#ifdef DBG	
#define alx_hw_warn(_hw, _format, ...)				\
	alx_hw_printk(WARNING, _format, ##__VA_ARGS__)
#define alx_hw_info(_hw, _format, ...)				\
	alx_hw_printk(INFO, _format, ##__VA_ARGS__)
#else
#define alx_hw_warn(hw, fmt, ...)
#define alx_hw_info(hw, fmt, ...)
#endif

#define alx_hw_printk(_klevel, _format, ...)		\
	do {							\
		printk(KERN_##_klevel "alx_hw: %s: " _format,	\
			__func__ , ##__VA_ARGS__);		\
	} while (0)

#endif /* _ALX_SW_H_ */

