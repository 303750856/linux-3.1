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

#include "alx.h"
#include "alx_hwcom.h"

#define DRV_VERSION "1.0.0.0"
#define DRV_NAPI "-NAPI"
#define DRV_VERSION_FULL DRV_VERSION DRV_NAPI

char alx_drv_name[] = "alx";
const char alx_drv_description[] =
	"Atheros(R) AR8131/AR8151/AR8152/AR8161 PCI-E Ethernet Network Driver";
const char alx_drv_version[] = DRV_VERSION_FULL;

static const char alx_copyright[] =
	"Copyright (c) 2007 - 2011 Atheros Corporation";


/* alx_pci_tbl - PCI Device ID Table
 *
 * Wildcard entries (PCI_ANY_ID) should come last
 * Last entry must be all 0s
 *
 * { Vendor ID, Device ID, SubVendor ID, SubDevice ID,
 *   Class, Class Mask, private data (not used) }
 */
#define ALX_ETHER_DEVICE(device_id) {\
	PCI_DEVICE(ALX_VENDOR_ID, device_id)}
DEFINE_PCI_DEVICE_TABLE(alx_pci_tbl) = {
	ALX_ETHER_DEVICE(ALX_DEV_ID_AR8131),
	ALX_ETHER_DEVICE(ALX_DEV_ID_AR8132),
	ALX_ETHER_DEVICE(ALX_DEV_ID_AR8151_V1),
	ALX_ETHER_DEVICE(ALX_DEV_ID_AR8151_V2),
	ALX_ETHER_DEVICE(ALX_DEV_ID_AR8152_V1),
	ALX_ETHER_DEVICE(ALX_DEV_ID_AR8152_V2),
	ALX_ETHER_DEVICE(ALX_DEV_ID_AR8161),
	{0,}
};
MODULE_DEVICE_TABLE(pci, alx_pci_tbl);

MODULE_AUTHOR("Atheros Corporation, <cloud.ren@atheros.com>, "
		"<xiong.huang@atheros.com>");
MODULE_DESCRIPTION("Atheros Gigabit Ethernet Driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VERSION_FULL);


/*
 *  alx_validate_mac_addr - Validate MAC address
 */
static int alx_validate_mac_addr(u8 *mac_addr)
{
	int retval = 0;

	if (mac_addr[0] & 0x01) {
		printk(KERN_DEBUG "MAC address is multicast\n");
		retval = ALX_ERR_MAC_ADDR;
	} else if (mac_addr[0] == 0xff && mac_addr[1] == 0xff) {
		printk(KERN_DEBUG "MAC address is broadcast\n");
		retval = ALX_ERR_MAC_ADDR;
	} else if (mac_addr[0] == 0 && mac_addr[1] == 0 &&
		   mac_addr[2] == 0 && mac_addr[3] == 0 &&
		   mac_addr[4] == 0 && mac_addr[5] == 0) {
		printk(KERN_DEBUG "MAC address is all zeros\n");
		retval = ALX_ERR_MAC_ADDR;
	}
	return retval;
}


/*
 *  alx_set_mac_type - Sets MAC type
 */
static int alx_set_mac_type(struct alx_adapter *adpt)
{
	struct alx_hw *hw = &adpt->hw;
	int retval = 0;

	if (hw->pci_venid == ALX_VENDOR_ID) {
		switch (hw->pci_devid) {
		case ALX_DEV_ID_AR8131:
			hw->mac_type = alx_mac_l1c;
			break;
		case ALX_DEV_ID_AR8132:
			hw->mac_type = alx_mac_l2c;
			break;
		case ALX_DEV_ID_AR8151_V1:
			hw->mac_type = alx_mac_l1d_v1;
			break;
		case ALX_DEV_ID_AR8151_V2:
			/* just use l1d configure */
			hw->mac_type = alx_mac_l1d_v2;
			break;
		case ALX_DEV_ID_AR8152_V1:
			hw->mac_type = alx_mac_l2cb_v1;
			break;
		case ALX_DEV_ID_AR8152_V2:
			if (hw->pci_revid == ALX_REV_ID_AR8152_V2_0)
				hw->mac_type = alx_mac_l2cb_v20;
			else
				hw->mac_type = alx_mac_l2cb_v21;
			break;
		case ALX_DEV_ID_AR8161:
			hw->mac_type = alx_mac_l1f;
			break;
		case ALX_DEV_ID_AR8162:
			hw->mac_type = alx_mac_l2f;
			break;
		default:
			retval = ALX_ERR_NOT_SUPPORTED;
			break;
		}
	} else {
		retval = ALX_ERR_NOT_SUPPORTED;
	}

	DRV_PRINT(HW, INFO, "found mac: %d, returns: %d\n",
		  hw->mac_type, retval);
	return retval;
}

/*
 *  alx_init_hw_callbacks
 */
static int alx_init_hw_callbacks(struct alx_adapter *adpt)
{
	struct alx_hw *hw = &adpt->hw;
	int retval = 0;

	alx_set_mac_type(adpt);


	switch (hw->mac_type) {
	case alx_mac_l1f:
	case alx_mac_l2f:
		retval = alf_init_hw_callbacks(hw);
		break;
	case alx_mac_l1c:
	case alx_mac_l2c:
	case alx_mac_l2cb_v1:
	case alx_mac_l2cb_v20:
	case alx_mac_l2cb_v21:
	case alx_mac_l1d_v1:
	case alx_mac_l1d_v2:
		retval = alc_init_hw_callbacks(hw);
		break;
	default:
		retval = ALX_ERR_NOT_SUPPORTED;
		break;
	}

	return retval;
}


void alx_reinit_locked(struct alx_adapter *adpt)
{

	WARN_ON(in_interrupt());

	/* put off any impending NetWatchDogTimeout ???? TODO */
	adpt->netdev->trans_start = jiffies;

	while (test_and_set_bit(__ALX_RESETTING, &adpt->alx_state))
		msleep(20);

	alx_stop_internal(adpt, ALX_OPEN_CTRL_MAC_EN);

	alx_open_internal(adpt, ALX_OPEN_CTRL_MAC_EN);

	clear_bit(__ALX_RESETTING, &adpt->alx_state);
}


static void alx_task_schedule(struct alx_adapter *adpt)
{
	if (!test_bit(__ALX_DOWN, &adpt->alx_state) &&
	    !test_and_set_bit(__ALX_SERVICE_SCHED, &adpt->alx_state))
		schedule_work(&adpt->alx_task);
}

static void alx_check_lsc(struct alx_adapter *adpt)
{
	SET_ADPT_FLAG(1, LSC_REQUESTED);
	adpt->link_jiffies = jiffies + ALX_TRY_LINK_TIMEOUT;

	if (!test_bit(__ALX_DOWN, &adpt->alx_state))
		alx_task_schedule(adpt);
}


/*
 * alx_tx_timeout - Respond to a Tx Hang
 * @netdev: network interface device structure
 */
static void alx_tx_timeout(struct net_device *netdev)
{
	struct alx_adapter *adpt = netdev_priv(netdev);

	DRV_PRINT(FUNC, DEBUG, "ENTER\n");

	/* Do the reset outside of interrupt context */
	if (!test_bit(__ALX_DOWN, &adpt->alx_state)) {
		SET_ADPT_FLAG(1, RESET_REQUESTED);
		alx_task_schedule(adpt);
	}
}

/*
 * alx_set_multicase_list - Multicast and Promiscuous mode set
 * @netdev: network interface device structure
 */
static void alx_set_multicase_list(struct net_device *netdev)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	struct alx_hw *hw = &adpt->hw;
	struct netdev_hw_addr *mc_ptr;

	DRV_PRINT(FUNC, DEBUG, "ENTER\n");

	/* Check for Promiscuous and All Multicast modes */
	if (netdev->flags & IFF_PROMISC) {
		SET_HW_FLAG(PROMISC_EN);
	} else if (netdev->flags & IFF_ALLMULTI) {
		SET_HW_FLAG(MULTIALL_EN);
		CLI_HW_FLAG(PROMISC_EN);
	} else {
		CLI_HW_FLAG(MULTIALL_EN);
		CLI_HW_FLAG(PROMISC_EN);
	}
	hw->cbs.config_mac_ctrl(hw);

	/* clear the old settings from the multicast hash table */
	hw->cbs.clear_mc_addr(hw);

	/* comoute mc addresses' hash value ,and put it into hash table */
	netdev_for_each_mc_addr(mc_ptr, netdev)
		hw->cbs.set_mc_addr(hw, mc_ptr->addr);
}

/*
 * alx_set_mac - Change the Ethernet Address of the NIC
 */
static int alx_set_mac_addr(struct net_device *netdev, void *data)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	struct alx_hw *hw = &adpt->hw;
	struct sockaddr *addr = data;

	DRV_PRINT(FUNC, DEBUG, "ENTER\n");

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	if (netif_running(netdev))
		return -EBUSY;

	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
	memcpy(hw->mac_addr, addr->sa_data, netdev->addr_len);

	if (hw->cbs.set_mac_addr)
		hw->cbs.set_mac_addr(hw, hw->mac_addr);
	return 0;
}


/*
 * Read / Write Ptr Initialize:
 */
static void alx_init_ring_ptrs(struct alx_adapter *adpt)
{
	int i, j;

	for (i = 0; i < adpt->num_txques; i++) {
		struct alx_tx_queue *txque = adpt->tx_queue[i];
		struct alx_buffer *tpbuf = txque->tpq.tpbuff;
		txque->tpq.produce_idx = 0;
		txque->tpq.consume_idx = 0;
		for (j = 0; j < txque->tpq.count; j++)
			tpbuf[j].dma = 0;
	}



	for (i = 0; i < adpt->num_hw_rxques; i++) {
		struct alx_rx_queue *rxque = adpt->rx_queue[i];
		struct alx_buffer *rfbuf = rxque->rfq.rfbuff;
		rxque->rrq.produce_idx = 0;
		rxque->rrq.consume_idx = 0;
		rxque->rfq.produce_idx = 0;
		rxque->rfq.consume_idx = 0;
		for (j = 0; j < rxque->rfq.count; j++)
			rfbuf[j].dma = 0;
	}

	if (CHK_ADPT_FLAG(0, SRSS_EN))
		goto srrs_enable;

	return;

srrs_enable:
	for (i = 0; i < adpt->num_sw_rxques; i++) {
		struct alx_rx_queue *rxque = adpt->rx_queue[i];
		rxque->swq.produce_idx = 0;
		rxque->swq.consume_idx = 0;
	}
	return;
}


static void alx_config_rss(struct alx_adapter *adpt)
{
	static const u8 key[40] = {
		0xE2, 0x91, 0xD7, 0x3D, 0x18, 0x05, 0xEC, 0x6C,
		0x2A, 0x94, 0xB3, 0x0D, 0xA5, 0x4F, 0x2B, 0xEC,
		0xEA, 0x49, 0xAF, 0x7C, 0xE2, 0x14, 0xAD, 0x3D,
		0xB8, 0x55, 0xAA, 0xBE, 0x6A, 0x3E, 0x67, 0xEA,
		0x14, 0x36, 0x4D, 0x17, 0x3B, 0xED, 0x20, 0x0D};

	struct alx_hw *hw = &adpt->hw;
	u32 reta = 0;
	int i, j;

	/* initialize rss hash type and idt table size */
	hw->rss_hstype = ALX_RSS_HSTYP_ALL_EN;
	hw->rss_idt_size = 0x100;

	/* Fill out redirection table */
	memcpy(hw->rss_key, key, sizeof(hw->rss_key));

	/* Fill out redirection table */
	memset(hw->rss_idt, 0x0, sizeof(hw->rss_idt));
	for (i = 0, j = 0; i < 256; i++, j++) {
		if (j == adpt->max_rxques)
			j = 0;
		reta |= (j << ((i & 7) * 4));
		if ((i & 7) == 7) {
			hw->rss_idt[i>>3] = reta;
			reta = 0;
		}
	}

	if (hw->cbs.config_rss)
		hw->cbs.config_rss(hw, CHK_ADPT_FLAG(0, SRSS_EN));
}


/* alx_receive_skb */
static void alx_receive_skb(struct alx_msix_param *msix,
			    struct sk_buff *skb,
			    u32 vlan_tag, bool vlan_flag)
{
	struct alx_adapter *adpt = msix->adpt;

	if (adpt->vlgrp && vlan_flag) {
		u16 vlan;
		u16 vlan_tag = (u16)vlan_tag;
		AT_TAG_TO_VLAN(vlan_tag, vlan);
	}
	netif_receive_skb(skb);
}

static bool alx_get_rrdesc(struct alx_rx_queue *rxque,
			    struct alx_rrdesc *srrd)
{
	struct alx_rrdesc *hrrd =
			ALX_RRD(rxque, rxque->rrq.consume_idx);

	srrd->rr_dw0 = le32_to_cpu(hrrd->rr_dw0);
	srrd->rr_dw1 = le32_to_cpu(hrrd->rr_dw1);
	srrd->rr_dw2 = le32_to_cpu(hrrd->rr_dw2);
	srrd->rr_dw3 = le32_to_cpu(hrrd->rr_dw3);


	if (!srrd->rr_gnr.update)
		return false;

#if ALX_DUMP_RRD_DESC
	printk(KERN_INFO "RRD [hw]: %08x:%08x:%08x:%08x\n",
			 hrrd->rr_dw0, hrrd->rr_dw1,
			 hrrd->rr_dw2, hrrd->rr_dw3);
	printk(KERN_INFO "RRD [sw]: %08x:%08x:%08x:%08x\n",
			 srrd->rr_dw0, srrd->rr_dw1,
			 srrd->rr_dw2, srrd->rr_dw3);
#endif
	if (likely(srrd->rr_gnr.nor != 1)) {
		/* TODO support mul rfd*/
		printk(KERN_EMERG "Multi rfd not support yet!\n");
	}

	srrd->rr_gnr.update = 0;
	hrrd->rr_dw3 = cpu_to_le32(srrd->rr_dw3);
	if (++rxque->rrq.consume_idx == rxque->rrq.count)
		rxque->rrq.consume_idx = 0;

	return true;
}

static bool alx_set_rfdesc(struct alx_rx_queue *rxque,
			   struct alx_rfdesc *srfd)
{
	struct alx_rfdesc *hrfd =
			ALX_RFD(rxque, rxque->rfq.produce_idx);

	hrfd->rf_qw0 = cpu_to_le64(srfd->rf_qw0);

	if (++rxque->rfq.produce_idx == rxque->rfq.count)
		rxque->rfq.produce_idx = 0;

#if ALX_DUMP_RFD_DESC
	printk(KERN_INFO "RFD [hw]: %08x:%08x\n",
			 hrfd->rf_dw0, hrfd->rf_dw1);
	printk(KERN_INFO "RFD [sw]: %08x:%08x\n",
			 srfd->rf_dw0, srfd->rf_dw1);
#endif
	return true;
}


static bool alx_set_tpdesc(struct alx_tx_queue *txque,
			   struct alx_tpdesc *stpd)
{
	struct alx_tpdesc *htpd;

	txque->tpq.last_produce_idx = txque->tpq.produce_idx;
	htpd = ALX_TPD(txque, txque->tpq.produce_idx);

	if (++txque->tpq.produce_idx == txque->tpq.count)
		txque->tpq.produce_idx = 0;

	htpd->tp_dw0 = cpu_to_le32(stpd->tp_dw0);
	htpd->tp_dw1 = cpu_to_le32(stpd->tp_dw1);
	htpd->tp_qw1 = cpu_to_le64(stpd->tp_qw1);

#if ALX_DUMP_TPD_DESC
	printk(KERN_INFO "TPD [hw]: %08x:%08x:%08x:%08x\n",
			 htpd->tp_dw0, htpd->tp_dw1,
			 htpd->tp_dw2, htpd->tp_dw3);
	printk(KERN_INFO "TPD [sw]: %08x:%08x:%08x:%08x\n",
			 stpd->tp_dw0, stpd->tp_dw1,
			 stpd->tp_dw2, stpd->tp_dw3);
#endif
	return true;
}

static void alx_set_tpdesc_lastfrag(struct alx_tx_queue *txque)
{
	struct alx_tpdesc *htpd;
#define ALX_TPD_LAST_FLAGMENT  0x80000000
	htpd = ALX_TPD(txque, txque->tpq.last_produce_idx);
	htpd->tp_dw1 |= cpu_to_le32(ALX_TPD_LAST_FLAGMENT);
}


static int alx_refresh_rx_buffer(struct alx_rx_queue *rxque)
{
	struct alx_adapter *adpt = netdev_priv(rxque->netdev);
	struct alx_hw *hw = &adpt->hw;
	struct alx_buffer *curr_rxbuf;
	struct alx_buffer *next_rxbuf;
	struct alx_rfdesc srfd;
	struct sk_buff *skb;
	void *skb_data = NULL;
	u16 count = 0;
	u16 next_produce_idx;

	next_produce_idx = rxque->rfq.produce_idx;
	if (++next_produce_idx == rxque->rfq.count)
		next_produce_idx = 0;
	curr_rxbuf = GET_RF_BUFFER(rxque, rxque->rfq.produce_idx);
	next_rxbuf = GET_RF_BUFFER(rxque, next_produce_idx);

	/* this always has a blank rx_buffer*/
	while (next_rxbuf->dma == 0) {
		skb = dev_alloc_skb(adpt->rxbuf_size);
		if (unlikely(!skb)) {
			DRV_PRINT(RX, INFO, "alloc rx buffer failed\n");
			break;
		}

		/*
		 * Make buffer alignment 2 beyond a 16 byte boundary
		 * this will result in a 16 byte aligned IP header after
		 * the 14 byte MAC header is removed
		 */
		skb_data = skb->data;
		/*skb_reserve(skb, NET_IP_ALIGN);*/
		curr_rxbuf->skb = skb;
		curr_rxbuf->length = adpt->rxbuf_size;
		curr_rxbuf->dma = dma_map_single(rxque->dev,
						 skb_data,
						 curr_rxbuf->length,
						 DMA_FROM_DEVICE);
		srfd.rf_gnr.addr = curr_rxbuf->dma;
		alx_set_rfdesc(rxque, &srfd);

		next_produce_idx = rxque->rfq.produce_idx;
		if (++next_produce_idx == rxque->rfq.count)
			next_produce_idx = 0;
		curr_rxbuf = GET_RF_BUFFER(rxque, rxque->rfq.produce_idx);
		next_rxbuf = GET_RF_BUFFER(rxque, next_produce_idx);
		count++;
	}

	if (count) {
		wmb();
		MEM_W16(hw, rxque->produce_reg, rxque->rfq.produce_idx);
		DRV_PRINT(RX, INFO, "RX[%d]: prod_reg[0x%x] = 0x%x, "
			  "rfq.produce_idx = 0x%x\n",
			  rxque->que_idx, rxque->produce_reg,
			  rxque->rfq.produce_idx, rxque->rfq.produce_idx);
	}
	return count;
}


static void alx_clean_rfdesc(struct alx_rx_queue *rxque,
		struct alx_rrdesc *srrd)
{
	struct alx_buffer *rfbuf = rxque->rfq.rfbuff;
	u32 consume_idx = srrd->rr_gnr.si;
	u32 i;

	for (i = 0; i < srrd->rr_gnr.nor; i++) {
		rfbuf[consume_idx].skb = NULL;
		if (++consume_idx == rxque->rfq.count)
			consume_idx = 0;
	}
	rxque->rfq.consume_idx = consume_idx;

	return;
}


static bool alx_dispatch_rx_irq(struct alx_msix_param *msix,
				struct alx_rx_queue *rxque)
{
	struct alx_adapter *adpt = msix->adpt;
	struct pci_dev *pdev = adpt->pdev;
	struct net_device *netdev  = adpt->netdev;

	struct alx_rrdesc srrd;
	struct alx_buffer *rfbuf;
	struct sk_buff *skb;
	struct alx_rx_queue *swque;
	struct alx_sw_buffer *curr_swbuf;
	struct alx_sw_buffer *next_swbuf;

	u16 next_produce_idx;
	u16 count = 0;

	while (1) {
		if (!alx_get_rrdesc(rxque, &srrd))
			break;

		if (srrd.rr_gnr.res || srrd.rr_gnr.lene) {
			alx_clean_rfdesc(rxque, &srrd);
			DRV_PRINT(RX, WARNING, "wrong packet!"
				  "rrd->word3 is 0x%08x\n", srrd.rr_dw3);
			continue;
		}

		/* Good Receive */
		if (likely(srrd.rr_gnr.nor == 1)) {
			rfbuf = GET_RF_BUFFER(rxque, srrd.rr_gnr.si);
			pci_unmap_single(pdev, rfbuf->dma,
					 rfbuf->length, DMA_FROM_DEVICE);
			rfbuf->dma = 0;
			skb = rfbuf->skb;
			DRV_PRINT(RX, INFO, "skb addr = %p, rxbuf_len = %x\n",
				  skb->data, rfbuf->length);
		} else {
			/* TODO */
			DRV_PRINT(RX, EMERG, "Multil rfd not support yet!\n");
			break;
		}
		alx_clean_rfdesc(rxque, &srrd);

		skb_put(skb, srrd.rr_gnr.pkt_len - ETH_FCS_LEN);
		skb->protocol = eth_type_trans(skb, netdev);
		skb->dev = netdev;
		skb->ip_summed = CHECKSUM_NONE;

		/* start to dispatch */
		swque = adpt->rx_queue[srrd.rr_gnr.rss_cpu];
		next_produce_idx = swque->swq.produce_idx;
		if (++next_produce_idx == swque->swq.count)
			next_produce_idx = 0;

		curr_swbuf = GET_SW_BUFFER(swque, swque->swq.produce_idx);
		next_swbuf = GET_SW_BUFFER(swque, next_produce_idx);

		/*
		 * if full, will discard the packet,
		 * and at lease has a blank sw_buffer.
		 */
		if (!next_swbuf->skb) {
			curr_swbuf->skb = skb;
			curr_swbuf->vlan_tag = srrd.rr_gnr.vlan_tag;
			curr_swbuf->vlan_flag = srrd.rr_gnr.vlan_flag;
			if (++swque->swq.produce_idx == swque->swq.count)
				swque->swq.produce_idx = 0;
		}

		count++;
		if (count == 32)
			break;
	}
	if (count)
		alx_refresh_rx_buffer(rxque);
	return true;
}



static bool alx_handle_srx_irq(struct alx_msix_param *msix,
			       struct alx_rx_queue *rxque,
			       int *num_pkts, int max_pkts)
{
	struct alx_adapter *adpt = msix->adpt;
	struct net_device *netdev = adpt->netdev;
	struct alx_sw_buffer *swbuf;
	bool retval = true;

	while (rxque->swq.consume_idx != rxque->swq.produce_idx) {
		swbuf = GET_SW_BUFFER(rxque, rxque->swq.consume_idx);

		alx_receive_skb(msix, swbuf->skb, swbuf->vlan_tag,
				(bool)swbuf->vlan_flag);
		swbuf->skb = NULL;
		netdev->last_rx = jiffies;

		if (++rxque->swq.consume_idx == rxque->swq.count)
			rxque->swq.consume_idx = 0;

		(*num_pkts)++;
		if (*num_pkts >= max_pkts) {
			retval = false;
			break;
		}
	}
	return retval;
}

static bool alx_handle_rx_irq(struct alx_msix_param *msix,
			      struct alx_rx_queue *rxque,
			      int *num_pkts, int max_pkts)
{
	struct alx_adapter *adpt = msix->adpt;
	struct pci_dev *pdev = adpt->pdev;
	struct net_device *netdev  = adpt->netdev;

	struct alx_rrdesc srrd;
	struct alx_buffer *rfbuf;
	struct sk_buff *skb;

	u16 count = 0;

	while (1) {
		if (!alx_get_rrdesc(rxque, &srrd))
			break;

		if (srrd.rr_gnr.res || srrd.rr_gnr.lene) {
			alx_clean_rfdesc(rxque, &srrd);
			DRV_PRINT(RX, WARNING, "wrong packet!"
				  "rrd->word3 is 0x%08x\n", srrd.rr_dw3);
			continue;
		}

		/* TODO: Good Receive */
		if (likely(srrd.rr_gnr.nor == 1)) {
			rfbuf = GET_RF_BUFFER(rxque, srrd.rr_gnr.si);
			pci_unmap_single(pdev, rfbuf->dma, rfbuf->length,
					 DMA_FROM_DEVICE);
			rfbuf->dma = 0;
			skb = rfbuf->skb;
		} else {
			/* TODO */
			DRV_PRINT(RX, EMERG, "Multil rfd not support yet!\n");
			break;
		}
		alx_clean_rfdesc(rxque, &srrd);

		skb_put(skb, srrd.rr_gnr.pkt_len - ETH_FCS_LEN);
		skb->protocol = eth_type_trans(skb, netdev);
		skb->dev = netdev;
		skb_checksum_none_assert(skb);

		alx_receive_skb(msix, skb, srrd.rr_gnr.vlan_tag,
				srrd.rr_gnr.vlan_flag);

		netdev->last_rx = jiffies;

		count++;

		(*num_pkts)++;
		if (*num_pkts >= max_pkts)
			break;
	}
	if (count)
		alx_refresh_rx_buffer(rxque);

	return true;
}


static bool alx_handle_tx_irq(struct alx_msix_param *msix,
			      struct alx_tx_queue *txque)
{
	struct alx_adapter *adpt = msix->adpt;
	struct alx_hw *hw = &adpt->hw;
	struct alx_buffer *tpbuf;
	u16 consume_data;

	MEM_R16(hw, txque->consume_reg, &consume_data);
	DRV_PRINT(TX, INFO, "TX[%d]: consume_reg[0x%x] = 0x%x, "
		  "tpq.consume_idx = 0x%x.\n",
		  txque->que_idx, txque->consume_reg, consume_data,
		  txque->tpq.consume_idx);


	while (txque->tpq.consume_idx != consume_data) {
		tpbuf = GET_TP_BUFFER(txque, txque->tpq.consume_idx);
		if (tpbuf->dma) {
			pci_unmap_page(adpt->pdev, tpbuf->dma, tpbuf->length,
				       DMA_TO_DEVICE);
			tpbuf->dma = 0;
		}

		if (tpbuf->skb) {
			dev_kfree_skb_irq(tpbuf->skb);
			tpbuf->skb = NULL;
		}

		if (++txque->tpq.consume_idx == txque->tpq.count)
			txque->tpq.consume_idx = 0;
	}

	if (netif_queue_stopped(adpt->netdev) &&
		netif_carrier_ok(adpt->netdev)) {
		netif_wake_queue(adpt->netdev);
	}
	return true;
}

static irqreturn_t alx_msix_timer(int irq, void *data)
{
	struct alx_msix_param *msix = data;
	struct alx_adapter *adpt = msix->adpt;
	struct alx_hw *hw = &adpt->hw;
	u32 isr;

	DRV_PRINT(FUNC, DEBUG, "ENTER\n");

	hw->cbs.disable_msix_intr(hw, msix->vec_idx);

	MEM_R32(hw, ALX_ISR, &isr);
	isr = isr & (ALX_ISR_TIMER | ALX_ISR_MANU);


	if (isr == 0) {
		hw->cbs.enable_msix_intr(hw, msix->vec_idx);
		return IRQ_NONE;
	}

	/* Ack ISR */
	MEM_W32(hw, ALX_ISR, isr);

	if (isr & ALX_ISR_MANU) {
		adpt->net_stats.tx_carrier_errors++;
		alx_check_lsc(adpt);
	}

	hw->cbs.enable_msix_intr(hw, msix->vec_idx);

	return IRQ_HANDLED;
}


static irqreturn_t alx_msix_alert(int irq, void *data)
{
	struct alx_msix_param *msix = data;
	struct alx_adapter *adpt = msix->adpt;
	struct alx_hw *hw = &adpt->hw;
	u32 isr;

	DRV_PRINT(FUNC, DEBUG, "ENTER\n");

	hw->cbs.disable_msix_intr(hw, msix->vec_idx);

	MEM_R32(hw, ALX_ISR, &isr);
	isr = isr & ALX_ISR_ALERT_MASK;

	if (isr == 0) {
		hw->cbs.enable_msix_intr(hw, msix->vec_idx);
		return IRQ_NONE;
	}
	MEM_W32(hw, ALX_ISR, isr);

	hw->cbs.enable_msix_intr(hw, msix->vec_idx);

	return IRQ_HANDLED;
}

static irqreturn_t alx_msix_smb(int irq, void *data)
{
	struct alx_msix_param *msix = data;
	struct alx_adapter *adpt = msix->adpt;
	struct alx_hw *hw = &adpt->hw;

	DRV_PRINT(FUNC, DEBUG, "ENTER\n");

	hw->cbs.disable_msix_intr(hw, msix->vec_idx);

	hw->cbs.enable_msix_intr(hw, msix->vec_idx);

	return IRQ_HANDLED;
}

static irqreturn_t alx_msix_phy(int irq, void *data)
{
	struct alx_msix_param *msix = data;
	struct alx_adapter *adpt = msix->adpt;
	struct alx_hw *hw = &adpt->hw;

	DRV_PRINT(FUNC, DEBUG, "ENTER\n");

	hw->cbs.disable_msix_intr(hw, msix->vec_idx);

	if (hw->cbs.ack_phy_intr)
		hw->cbs.ack_phy_intr(hw);

	adpt->net_stats.tx_carrier_errors++;
	alx_check_lsc(adpt);

	hw->cbs.enable_msix_intr(hw, msix->vec_idx);

	return IRQ_HANDLED;
}

/*
 * alx_msix_rtx
 */
static irqreturn_t alx_msix_rtx(int irq, void *data)
{
	struct alx_msix_param *msix = data;
	struct alx_adapter  *adpt = msix->adpt;
	struct alx_hw *hw = &adpt->hw;

	DRV_PRINT(INTR, INFO, "msix vec_idx = %d.\n", msix->vec_idx);

	hw->cbs.disable_msix_intr(hw, msix->vec_idx);
	if (!msix->rx_count && !msix->tx_count) {
		hw->cbs.enable_msix_intr(hw, msix->vec_idx);
		return IRQ_HANDLED;
	}

	napi_schedule(&msix->napi);
	return IRQ_HANDLED;
}

/*
 * alx_napi_msix_rtx
 */
static int alx_napi_msix_rtx(struct napi_struct *napi, int max_pkts)
{
	struct alx_msix_param *msix =
			       container_of(napi, struct alx_msix_param, napi);
	struct alx_adapter *adpt = msix->adpt;
	struct alx_hw *hw = &adpt->hw;
	struct alx_rx_queue *rxque;
	struct alx_rx_queue *swque;
	struct alx_tx_queue *txque;
	unsigned long flags = 0;
	bool complete = true;
	int num_pkts = 0;
	int rque_idx, tque_idx;
	int i, j;

	DRV_PRINT(INTR, INFO, "NAPI: enter alx_napi_msix_rtx.\n");

	/* RX */
	for (i = 0; i < msix->rx_count; i++) {
		rque_idx = msix->rx_map[i];
		num_pkts = 0;
		if (CHK_ADPT_FLAG(0, SRSS_EN)) {
			if (!spin_trylock_irqsave(&adpt->rx_lock, flags))
				goto clean_sw_irq;

			for (j = 0; j < adpt->num_hw_rxques; j++)
				alx_dispatch_rx_irq(msix, adpt->rx_queue[j]);

			spin_unlock_irqrestore(&adpt->rx_lock, flags);
clean_sw_irq:
			swque = adpt->rx_queue[rque_idx];
			complete &= alx_handle_srx_irq(msix, swque, &num_pkts,
						       max_pkts);

		} else {
			rxque = adpt->rx_queue[rque_idx];
			complete &= alx_handle_rx_irq(msix, rxque, &num_pkts,
						      max_pkts);
		}
	}


	/* Handle TX */
	for (i = 0; i < msix->tx_count; i++) {
		tque_idx = msix->tx_map[i];
		txque = adpt->tx_queue[tque_idx];
		complete &= alx_handle_tx_irq(msix, txque);
	}

	if (!complete) {
		DRV_PRINT(INTR, INFO, "Some packets in the queue "
				"are not handled!");
		num_pkts = max_pkts;
	}

	DRV_PRINT(INTR, INFO, "num_pkts = %d, max_pkts = %d.\n",
			num_pkts, max_pkts);
	/* If all work done, exit the polling mode */
	if (num_pkts < max_pkts) {
		napi_complete(napi);
		if (!test_bit(__ALX_DOWN, &adpt->alx_state))
			hw->cbs.enable_msix_intr(hw, msix->vec_idx);
	}

	return num_pkts;
}



/*
 * alx_napi_legacy_rtx - NAPI Rx polling callback
 * @adpt: board private structure
 */
static int alx_napi_legacy_rtx(struct napi_struct *napi, int max_pkts)
{
	struct alx_msix_param *msix =
				container_of(napi, struct alx_msix_param, napi);
	struct alx_adapter *adpt = msix->adpt;
	struct alx_hw *hw = &adpt->hw;
	int complete = true;
	int num_pkts = 0;
	int que_idx;

	DRV_PRINT(INTR, INFO, "NAPI: enter alx_napi_legacy_rtx.\n");

	/* Keep link state information with original netdev */
	if (!netif_carrier_ok(adpt->netdev))
		goto enable_rtx_irq;

	for (que_idx = 0; que_idx < adpt->num_txques; que_idx++)
		complete &= alx_handle_tx_irq(msix, adpt->tx_queue[que_idx]);

	for (que_idx = 0; que_idx < adpt->num_hw_rxques; que_idx++) {
		num_pkts = 0;
		complete &= alx_handle_rx_irq(msix, adpt->rx_queue[que_idx],
					      &num_pkts, max_pkts);
	}

	if (!complete)
		num_pkts = max_pkts;

	if (num_pkts < max_pkts) {
enable_rtx_irq:
		napi_complete(napi);
		hw->intr_mask |= (ALX_ISR_RXQ | ALX_ISR_TXQ);
		MEM_W32(hw, ALX_IMR, hw->intr_mask);
	}
	return num_pkts;
}


static void alx_set_msix_flags(struct alx_msix_param *msix,
		enum alx_msix_type type, int index)
{
	if (type == alx_msix_type_rx) {
		switch (index) {
		case 0:
			SET_MSIX_FLAG(RX0);
			break;
		case 1:
			SET_MSIX_FLAG(RX1);
			break;
		case 2:
			SET_MSIX_FLAG(RX2);
			break;
		case 3:
			SET_MSIX_FLAG(RX3);
			break;
		case 4:
			SET_MSIX_FLAG(RX4);
			break;
		case 5:
			SET_MSIX_FLAG(RX5);
			break;
		case 6:
			SET_MSIX_FLAG(RX6);
			break;
		case 7:
			SET_MSIX_FLAG(RX7);
			break;
		default:
			printk(KERN_ERR "alx_set_msix_flags: rx error.");
			break;
		}
	} else if (type == alx_msix_type_tx) {
		switch (index) {
		case 0:
			SET_MSIX_FLAG(TX0);
			break;
		case 1:
			SET_MSIX_FLAG(TX1);
			break;
		case 2:
			SET_MSIX_FLAG(TX2);
			break;
		case 3:
			SET_MSIX_FLAG(TX3);
			break;
		default:
			printk(KERN_ERR "alx_set_msix_flags: tx error.");
			break;
		}
	} else if (type == alx_msix_type_other) {
		switch (index) {
		case ALX_MSIX_TYPE_OTH_TIMER:
			SET_MSIX_FLAG(TIMER);
			break;
		case ALX_MSIX_TYPE_OTH_ALERT:
			SET_MSIX_FLAG(ALERT);
			break;
		case ALX_MSIX_TYPE_OTH_SMB:
			SET_MSIX_FLAG(SMB);
			break;
		case ALX_MSIX_TYPE_OTH_PHY:
			SET_MSIX_FLAG(PHY);
			break;
		default:
			printk(KERN_ERR "alx_set_msix_flags: other error.");
			break;
		}
	}
}

/* alx_setup_msix_maps */
static int alx_setup_msix_maps(struct alx_adapter *adpt)
{
	int msix_idx = 0;
	int que_idx = 0;
	int num_rxques = adpt->num_rxques;
	int num_txques = adpt->num_txques;
	int num_msix_rxques = adpt->num_msix_rxques;
	int num_msix_txques = adpt->num_msix_txques;
	int num_msix_noques = adpt->num_msix_noques;
	int retval = 0;

	if (!CHK_ADPT_FLAG(0, MSIX_EN))
		goto out;

	if (CHK_ADPT_FLAG(0, FIXED_MSIX))
		goto fixed_msix_map;

	DRV_PRINT(IF, ERR, "don't support non-fixed msix map\n");
	return -1;

fixed_msix_map:
	/*
	 * For RX queue msix map
	 */
	msix_idx = 0;
	for (que_idx = 0; que_idx < num_msix_rxques; que_idx++, msix_idx++) {
		struct alx_msix_param *msix = adpt->msix[msix_idx];
		if (que_idx < num_rxques) {
			adpt->rx_queue[que_idx]->msix = msix;
			msix->rx_map[msix->rx_count] = que_idx;
			msix->rx_count++;
			alx_set_msix_flags(msix, alx_msix_type_rx, que_idx);
		}
	}
	if (msix_idx != num_msix_rxques)
		DRV_PRINT(IF, ERR, "msix_idx is wrong.\n");

	/*
	 * For TX queue msix map
	 */
	for (que_idx = 0; que_idx < num_msix_txques; que_idx++, msix_idx++) {
		struct alx_msix_param *msix = adpt->msix[msix_idx];
		if (que_idx < num_txques) {
			adpt->tx_queue[que_idx]->msix = msix;
			msix->tx_map[msix->tx_count] = que_idx;
			msix->tx_count++;
			alx_set_msix_flags(msix, alx_msix_type_tx, que_idx);
		}
	}
	if (msix_idx != (num_msix_rxques + num_msix_txques))
		DRV_PRINT(IF, ERR, "msix_idx is wrong.\n");


	/*
	 * For NON queue msix map
	 */
	for (que_idx = 0; que_idx < num_msix_noques; que_idx++, msix_idx++) {
		struct alx_msix_param *msix = adpt->msix[msix_idx];
		alx_set_msix_flags(msix, alx_msix_type_other, que_idx);
	}
out:
	return retval;
}

static inline void alx_reset_msix_maps(struct alx_adapter *adpt)
{
	int que_idx, msix_idx;

	for (que_idx = 0; que_idx < adpt->num_rxques; que_idx++)
		adpt->rx_queue[que_idx]->msix = NULL;
	for (que_idx = 0; que_idx < adpt->num_txques; que_idx++)
		adpt->tx_queue[que_idx]->msix = NULL;

	for (msix_idx = 0; msix_idx < adpt->num_msix_intrs; msix_idx++) {
		struct alx_msix_param *msix = adpt->msix[msix_idx];
		memset(msix->rx_map, 0, sizeof(msix->rx_map));
		memset(msix->tx_map, 0, sizeof(msix->tx_map));
		msix->rx_count = 0;
		msix->tx_count = 0;
		CLI_MSIX_FLAG(ALL);
	}
}


/*
 * alx_enable_intr - Enable default interrupt generation settings
 */
static inline void alx_enable_intr(struct alx_adapter *adpt)
{
	struct alx_hw *hw = &adpt->hw;
	int i;

	if (!atomic_dec_and_test(&adpt->irq_sem))
		return;

	if (hw->cbs.enable_legacy_intr)
		hw->cbs.enable_legacy_intr(hw);

	/* enable all MSIX IRQs */
	for (i = 0; i < adpt->num_msix_intrs; i++) {
		if (hw->cbs.disable_msix_intr)
			hw->cbs.disable_msix_intr(hw, i);
		if (hw->cbs.enable_msix_intr)
			hw->cbs.enable_msix_intr(hw, i);
	}
}

/* alx_disable_intr - Mask off interrupt generation on the NIC */
static inline void alx_disable_intr(struct alx_adapter *adpt)
{
	struct alx_hw *hw = &adpt->hw;
	atomic_inc(&adpt->irq_sem);

	if (hw->cbs.disable_legacy_intr)
		hw->cbs.disable_legacy_intr(hw);

	if (CHK_ADPT_FLAG(0, MSIX_EN)) {
		int i;
		for (i = 0; i < adpt->num_msix_intrs; i++) {
			synchronize_irq(adpt->msix_entries[i].vector);
			hw->cbs.disable_msix_intr(hw, i);
		}
	} else {
		synchronize_irq(adpt->pdev->irq);
	}


}

/*
 * alx_interrupt - Interrupt Handler
 * @irq: interrupt number
 * @data: pointer to a network interface device structure
 */
static irqreturn_t alx_interrupt(int irq, void *data)
{
	struct net_device *netdev  = data;
	struct alx_adapter *adpt = netdev_priv(netdev);
	struct pci_dev *pdev = adpt->pdev;
	struct alx_hw *hw = &adpt->hw;
	struct alx_msix_param *msix = adpt->msix[0];
	int max_intrs = ALX_MAX_HANDLED_INTRS;
	u32 isr, status;

	do {
		MEM_R32(hw, ALX_ISR, &isr);
		status = isr & hw->intr_mask;

		if (status == 0) {
			MEM_W32(hw, ALX_ISR, 0);
			if (max_intrs != ALX_MAX_HANDLED_INTRS)
				return IRQ_HANDLED;
			return IRQ_NONE;
		}

		/* ack ISR to PHY register */
		if (status & ALX_ISR_PHY)
			hw->cbs.ack_phy_intr(hw);
		/* ack ISR to MAC register */
		MEM_W32(hw, ALX_ISR, status | ALX_ISR_DIS);

		/* check if PCIE PHY Link down */
		if (status & ALX_ISR_ERROR) {
			DRV_PRINT(INTR, ERR, "ISR error (status = 0x%lx).\n",
					     status & ALX_ISR_ERROR);
			/* reset MAC */
			SET_ADPT_FLAG(1, RESET_REQUESTED);
			alx_task_schedule(adpt);
			return IRQ_HANDLED;
		}

		if (status & (ALX_ISR_RXQ | ALX_ISR_TXQ)) {
			if (napi_schedule_prep(&(msix->napi))) {
				hw->intr_mask &= ~(ALX_ISR_RXQ | ALX_ISR_TXQ);
				MEM_W32(hw, ALX_IMR, hw->intr_mask);
				__napi_schedule(&(msix->napi));
			}
		}

		if (status & ALX_ISR_OVER) {
			dev_err(&pdev->dev,
					"TX/RX over flow (status = 0x%lx).\n",
					status & ALX_ISR_OVER);
		}

		/* link event */
		if (status & (ALX_ISR_PHY | ALX_ISR_MANU)) {
			adpt->net_stats.tx_carrier_errors++;
			alx_check_lsc(adpt);
			break;
		}

	} while (--max_intrs > 0);
	/* re-enable Interrupt*/
	MEM_W32(hw, ALX_ISR, 0);
	return IRQ_HANDLED;
}


/*
 * alx_request_msix_irqs - Initialize MSI-X interrupts
 */
static int alx_request_msix_irq(struct alx_adapter *adpt)
{
	struct net_device *netdev = adpt->netdev;
	irqreturn_t (*handler)(int, void *);
	int msix_idx;
	int num_msix_intrs = adpt->num_msix_intrs;
	int rx_idx = 0, tx_idx = 0;
	int i;
	int retval;

	retval = alx_setup_msix_maps(adpt);
	if (retval)
		return retval;

	for (msix_idx = 0; msix_idx < num_msix_intrs; msix_idx++) {
		struct alx_msix_param *msix = adpt->msix[msix_idx];

		if (CHK_MSIX_FLAG(RXS) && CHK_MSIX_FLAG(TXS)) {
			handler = &alx_msix_rtx;
			sprintf(msix->name, "%s:%s%d",
					    netdev->name, "rtx", rx_idx);
			rx_idx++;
			tx_idx++;
		} else if (CHK_MSIX_FLAG(RXS)) {
			handler = &alx_msix_rtx;
			sprintf(msix->name, "%s:%s%d",
					    netdev->name, "rx", rx_idx);
			rx_idx++;
		} else if (CHK_MSIX_FLAG(TXS)) {
			handler = &alx_msix_rtx;
			sprintf(msix->name, "%s:%s%d",
					    netdev->name, "tx", tx_idx);
			tx_idx++;
		} else if (CHK_MSIX_FLAG(TIMER)) {
			handler = &alx_msix_timer;
			sprintf(msix->name, "%s:%s", netdev->name, "timer");
		} else if (CHK_MSIX_FLAG(ALERT)) {
			handler = &alx_msix_alert;
			sprintf(msix->name, "%s:%s", netdev->name, "alert");
		} else if (CHK_MSIX_FLAG(SMB)) {
			handler = &alx_msix_smb;
			sprintf(msix->name, "%s:%s", netdev->name, "smb");
		} else if (CHK_MSIX_FLAG(PHY)) {
			handler = &alx_msix_phy;
			sprintf(msix->name, "%s:%s", netdev->name, "phy");
		} else {
			DRV_PRINT(IF, INFO, "The MSIX Entry [%d] is blank.\n",
					    msix->vec_idx);
			continue;
		}
		DRV_PRINT(IF, INFO, "the MSIX entry [%d] is %s.\n",
				    msix->vec_idx, msix->name);
		retval = request_irq(adpt->msix_entries[msix_idx].vector,
				     handler, 0, msix->name, msix);
		if (retval) {
			DRV_PRINT(IF, ERR, "request_irq failed for MSIX "
					   "Error: %d\n", retval);
			goto free_msix_irq;
		}
		/* assign the mask for this irq */
		irq_set_affinity_hint(adpt->msix_entries[msix_idx].vector,
				      msix->affinity_mask);
	}
	return retval;


free_msix_irq:
	for (i = 0; i < msix_idx; i++) {
		irq_set_affinity_hint(adpt->msix_entries[i].vector, NULL);
		free_irq(adpt->msix_entries[i].vector, adpt->msix[i]);
	}
	CLI_ADPT_FLAG(0, MSIX_EN);
	pci_disable_msix(adpt->pdev);
	kfree(adpt->msix_entries);
	adpt->msix_entries = NULL;
	return retval;
}

/*
 * alx_request_irq - initialize interrupts
 */
static int alx_request_irq(struct alx_adapter *adpt)
{
	struct net_device *netdev = adpt->netdev;
	int retval;

	/* request MSIX irq */
	if (CHK_ADPT_FLAG(0, MSIX_EN)) {
		retval = alx_request_msix_irq(adpt);
		if (retval)
			DRV_PRINT(IF, ERR, "request msix irq failed, "
					"error = %d.\n", retval);
		goto out;
	}

	/* request MSI irq */
	if (CHK_ADPT_FLAG(0, MSI_EN)) {
		retval = request_irq(adpt->pdev->irq, &alx_interrupt, 0,
			netdev->name, netdev);
		if (retval)
			DRV_PRINT(IF, ERR, "request msix irq failed, "
					"error = %d.\n", retval);
		goto out;
	}

	/* request shared irq */
	retval = request_irq(adpt->pdev->irq, &alx_interrupt, IRQF_SHARED,
			netdev->name, netdev);
	if (retval)
		DRV_PRINT(IF, ERR, "request shared irq failed, "
				"error = %d\n", retval);
out:
	return retval;
}


static void alx_free_irq(struct alx_adapter *adpt)
{
	struct net_device *netdev = adpt->netdev;
	int i;

	if (CHK_ADPT_FLAG(0, MSIX_EN)) {
		for (i = 0; i < adpt->num_msix_intrs; i++) {
			struct alx_msix_param *msix = adpt->msix[i];
			DRV_PRINT(IF, INFO, "msix entry = %d\n", i);
			if (!CHK_MSIX_FLAG(ALL))
				continue;
			if (CHK_MSIX_FLAG(RXS) || CHK_MSIX_FLAG(TXS)) {
				irq_set_affinity_hint(
					adpt->msix_entries[i].vector, NULL);
			}
			free_irq(adpt->msix_entries[i].vector, msix);
		}
		alx_reset_msix_maps(adpt);
	} else {
		free_irq(adpt->pdev->irq, netdev);
	}
}


static void alx_vlan_rx_register(struct net_device *netdev,
				 struct vlan_group *grp)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	struct alx_hw *hw = &adpt->hw;

	if (!test_bit(__ALX_DOWN, &adpt->alx_state))
		alx_disable_intr(adpt);

	adpt->vlgrp = grp;
	if (adpt->vlgrp) {
		/* enable VLAN tag insert/strip */
		SET_HW_FLAG(VLANSTRIP_EN);
	} else {
		/* disable VLAN tag insert/strip */
		CLI_HW_FLAG(VLANSTRIP_EN);
	}
	hw->cbs.config_mac_ctrl(hw);

	if (!test_bit(__ALX_DOWN, &adpt->alx_state))
		alx_enable_intr(adpt);
}

static void alx_restore_vlan(struct alx_adapter *adpt)
{
	alx_vlan_rx_register(adpt->netdev, adpt->vlgrp);
}


static void alx_napi_enable_all(struct alx_adapter *adpt)
{
	struct alx_msix_param *msix;
	int num_msix_intrs = adpt->num_msix_intrs;
	int msix_idx;

	if (!CHK_ADPT_FLAG(0, MSIX_EN))
		num_msix_intrs = 1;

	for (msix_idx = 0; msix_idx < num_msix_intrs; msix_idx++) {
		struct napi_struct *napi;
		msix = adpt->msix[msix_idx];
		napi = &msix->napi;
		napi_enable(napi);
	}
}

static void alx_napi_disable_all(struct alx_adapter *adpt)
{
	struct alx_msix_param *msix;
	int num_msix_intrs = adpt->num_msix_intrs;
	int msix_idx;

	if (!CHK_ADPT_FLAG(0, MSIX_EN))
		num_msix_intrs = 1;

	for (msix_idx = 0; msix_idx < num_msix_intrs; msix_idx++) {
		msix = adpt->msix[msix_idx];
		napi_disable(&msix->napi);
	}
}


static void alx_clean_tx_queue(struct alx_tx_queue *txque)
{
	struct device *dev = txque->dev;
	unsigned long size;
	u16 i;

	/* ring already cleared, nothing to do */
	if (!txque->tpq.tpbuff)
		return;

	for (i = 0; i < txque->tpq.count; i++) {
		struct alx_buffer *tpbuf;
		tpbuf = GET_TP_BUFFER(txque, i);
		if (tpbuf->dma) {
			pci_unmap_single(to_pci_dev(dev),
					tpbuf->dma,
					tpbuf->length,
					DMA_TO_DEVICE);
			tpbuf->dma = 0;
		}
		if (tpbuf->skb) {
			dev_kfree_skb_any(tpbuf->skb);
			tpbuf->skb = NULL;
		}
	}

	size = sizeof(struct alx_buffer) * txque->tpq.count;
	memset(txque->tpq.tpbuff, 0, size);

	/* Zero out Tx-buffers */
	memset(txque->tpq.tpdesc, 0, txque->tpq.size);

	txque->tpq.consume_idx = 0;
	txque->tpq.produce_idx = 0;
}


/*
 * alx_clean_all_tx_queues
 */
static void alx_clean_all_tx_queues(struct alx_adapter *adpt)
{
	int i;

	for (i = 0; i < adpt->num_txques; i++)
		alx_clean_tx_queue(adpt->tx_queue[i]);
}

static void alx_clean_rx_queue(struct alx_rx_queue *rxque)
{
	struct device *dev = rxque->dev;
	unsigned long size;
	int i;

	if (CHK_RX_FLAG(HW_QUE)) {
		/* ring already cleared, nothing to do */
		if (!rxque->rfq.rfbuff)
			goto clean_sw_queue;

		for (i = 0; i < rxque->rfq.count; i++) {
			struct alx_buffer *rfbuf;
			rfbuf = GET_RF_BUFFER(rxque, i);

			if (rfbuf->dma) {
				pci_unmap_single(to_pci_dev(dev),
						rfbuf->dma,
						rfbuf->length,
						DMA_FROM_DEVICE);
				rfbuf->dma = 0;
			}
			if (rfbuf->skb) {
				dev_kfree_skb(rfbuf->skb);
				rfbuf->skb = NULL;
			}
		}
		size =  sizeof(struct alx_buffer) * rxque->rfq.count;
		memset(rxque->rfq.rfbuff, 0, size);

		/* zero out the descriptor ring */
		memset(rxque->rrq.rrdesc, 0, rxque->rrq.size);
		rxque->rrq.produce_idx = 0;
		rxque->rrq.consume_idx = 0;

		memset(rxque->rfq.rfdesc, 0, rxque->rfq.size);
		rxque->rfq.produce_idx = 0;
		rxque->rfq.consume_idx = 0;
	}
clean_sw_queue:
	if (CHK_RX_FLAG(SW_QUE)) {
		/* ring already cleared, nothing to do */
		if (!rxque->swq.swbuff)
			return;

		for (i = 0; i < rxque->swq.count; i++) {
			struct alx_sw_buffer *swbuf;
			swbuf = GET_SW_BUFFER(rxque, i);

			/* swq doesn't map DMA*/

			if (swbuf->skb) {
				dev_kfree_skb(swbuf->skb);
				swbuf->skb = NULL;
			}
		}
		size =  sizeof(struct alx_buffer) * rxque->swq.count;
		memset(rxque->swq.swbuff, 0, size);

		/* swq doesn't have any descripter rings */
		rxque->swq.produce_idx = 0;
		rxque->swq.consume_idx = 0;
	}
}


/*
 * alx_clean_all_rx_queues
 */
static void alx_clean_all_rx_queues(struct alx_adapter *adpt)
{
	int i;
	for (i = 0; i < adpt->num_rxques; i++)
		alx_clean_rx_queue(adpt->rx_queue[i]);
}


/**
 * alx_set_rss_queues: Allocate queues for RSS
 * @adpt: board private structure to initialize
 **/
static inline void alx_set_num_txques(struct alx_adapter *adpt)
{
	struct alx_hw *hw = &adpt->hw;

	if (hw->mac_type == alx_mac_l1f || hw->mac_type == alx_mac_l2f)
		adpt->num_txques = 4;
	else
		adpt->num_txques = 2;

	return;
}

/*
 * alx_set_rss_queues: Allocate queues for RSS
 * @adpt: board private structure to initialize
 */
static inline void alx_set_num_rxques(struct alx_adapter *adpt)
{
	if (CHK_ADPT_FLAG(0, SRSS_CAP)) {
		adpt->num_hw_rxques = 1;
		adpt->num_sw_rxques = adpt->max_rxques;
		adpt->num_rxques =
			max(adpt->num_hw_rxques, adpt->num_sw_rxques);
	}

	return;
}

/*
 * alx_set_num_queues: Allocate queues for device, feature dependant
 * @adpt: board private structure to initialize
 **/
static void alx_set_num_queues(struct alx_adapter *adpt)
{
	/* Start with default case */
	adpt->num_txques = 1;
	adpt->num_rxques = 1;
	adpt->num_hw_rxques = 1;
	adpt->num_sw_rxques = 0;

	alx_set_num_rxques(adpt);
	alx_set_num_txques(adpt);

	return;
}

/* alx_alloc_all_rtx_queue - allocate all queues */
static int alx_alloc_all_rtx_queue(struct alx_adapter *adpt)
{
	int que_idx;

	for (que_idx = 0; que_idx < adpt->num_txques; que_idx++) {
		struct alx_tx_queue *txque = adpt->tx_queue[que_idx];

		txque = kzalloc(sizeof(struct alx_tx_queue), GFP_KERNEL);
		if (!txque)
			goto err_alloc_tx_queue;
		txque->tpq.count = adpt->num_txdescs;
		txque->que_idx = que_idx;
		txque->dev = &adpt->pdev->dev;
		txque->netdev = adpt->netdev;

		adpt->tx_queue[que_idx] = txque;
	}

	for (que_idx = 0; que_idx < adpt->num_rxques; que_idx++) {
		struct alx_rx_queue *rxque = adpt->rx_queue[que_idx];

		rxque = kzalloc(sizeof(struct alx_rx_queue), GFP_KERNEL);
		if (!rxque)
			goto err_alloc_rx_queue;
		rxque->rrq.count = adpt->num_rxdescs;
		rxque->rfq.count = adpt->num_rxdescs;
		rxque->swq.count = adpt->num_rxdescs;
		rxque->que_idx = que_idx;
		rxque->dev = &adpt->pdev->dev;
		rxque->netdev = adpt->netdev;

		if (CHK_ADPT_FLAG(0, SRSS_EN)) {
			if (que_idx < adpt->num_hw_rxques)
				SET_RX_FLAG(HW_QUE);
			if (que_idx < adpt->num_sw_rxques)
				SET_RX_FLAG(SW_QUE);
		} else {
			SET_RX_FLAG(HW_QUE);
		}
		adpt->rx_queue[que_idx] = rxque;
	}
	DRV_PRINT(INIT, DEBUG, "num_tx_descs = %d, num_rx_descs = %d\n",
			adpt->num_txdescs, adpt->num_rxdescs);
	return 0;

err_alloc_rx_queue:
	DRV_PRINT(INIT, ERR, "goto err_alloc_rx_queue");
	for (que_idx = 0; que_idx < adpt->num_rxques; que_idx++)
		kfree(adpt->rx_queue[que_idx]);
err_alloc_tx_queue:
	DRV_PRINT(INIT, ERR, "goto err_alloc_tx_queue");
	for (que_idx = 0; que_idx < adpt->num_txques; que_idx++)
		kfree(adpt->tx_queue[que_idx]);
	return -ENOMEM;
}


/* alx_free_all_rtx_queue */
static void alx_free_all_rtx_queue(struct alx_adapter *adpt)
{
	int que_idx;

	for (que_idx = 0; que_idx < adpt->num_txques; que_idx++) {
		kfree(adpt->tx_queue[que_idx]);
		adpt->tx_queue[que_idx] = NULL;
	}
	for (que_idx = 0; que_idx < adpt->num_rxques; que_idx++) {
		kfree(adpt->rx_queue[que_idx]);
		adpt->rx_queue[que_idx] = NULL;
	}
}

/* alx_set_interrupt_param - set interrupt parameter */
static int alx_set_interrupt_param(struct alx_adapter *adpt)
{
	struct alx_msix_param *msix;
	int (*poll)(struct napi_struct *, int);
	int msix_idx;

	if (CHK_ADPT_FLAG(0, MSIX_EN)) {
		poll = &alx_napi_msix_rtx;
	} else {
		adpt->num_msix_intrs = 1;
		poll = &alx_napi_legacy_rtx;
	}

	for (msix_idx = 0; msix_idx < adpt->num_msix_intrs; msix_idx++) {
		msix = kzalloc(sizeof(struct alx_msix_param),
					   GFP_KERNEL);
		if (!msix)
			goto err_alloc_msix;
		msix->adpt = adpt;
		msix->vec_idx = msix_idx;
		/* Allocate the affinity_hint cpumask, configure the mask */
		if (!alloc_cpumask_var(&msix->affinity_mask, GFP_KERNEL))
			goto err_alloc_cpumask;

		cpumask_set_cpu((msix_idx % num_online_cpus()),
				msix->affinity_mask);

		netif_napi_add(adpt->netdev, &msix->napi, (*poll), 64);
		adpt->msix[msix_idx] = msix;
	}
	return 0;

err_alloc_cpumask:
	kfree(msix);
	adpt->msix[msix_idx] = NULL;
err_alloc_msix:
	for (msix_idx--; msix_idx >= 0; msix_idx--) {
		msix = adpt->msix[msix_idx];
		netif_napi_del(&msix->napi);
		free_cpumask_var(msix->affinity_mask);
		kfree(msix);
		adpt->msix[msix_idx] = NULL;
	}
	DRV_PRINT(INTR, ERR, "can't allocate memory.\n");
	return -ENOMEM;
}

/**
 * alx_reset_interrupt_param - Free memory allocated for interrupt vectors
 * @adpt: board private structure to initialize
 **/
static void alx_reset_interrupt_param(struct alx_adapter *adpt)
{
	int msix_idx;

	for (msix_idx = 0; msix_idx < adpt->num_msix_intrs; msix_idx++) {
		struct alx_msix_param *msix = adpt->msix[msix_idx];
		netif_napi_del(&msix->napi);
		free_cpumask_var(msix->affinity_mask);
		kfree(msix);
		adpt->msix[msix_idx] = NULL;
	}
}

/* set msix interrupt mode */
static int alx_set_msix_interrupt_mode(struct alx_adapter *adpt)
{
	int msix_intrs, msix_idx;
	int retval = 0;

	adpt->msix_entries = kcalloc(adpt->max_msix_intrs,
				sizeof(struct msix_entry), GFP_KERNEL);
	if (!adpt->msix_entries) {
		DRV_PRINT(INTR, ERR, "can't allocate msix entry.\n");
		retval = -1;
		goto try_msi_mode;
	}

	for (msix_idx = 0; msix_idx < adpt->max_msix_intrs; msix_idx++)
		adpt->msix_entries[msix_idx].entry = msix_idx;


	msix_intrs = adpt->max_msix_intrs;
	while (msix_intrs >= adpt->min_msix_intrs) {
		retval = pci_enable_msix(adpt->pdev, adpt->msix_entries,
				      msix_intrs);
		if (!retval) /* Success in acquiring all requested vectors. */
			break;
		else if (retval < 0)
			msix_intrs = 0; /* Nasty failure, quit now */
		else /* error == number of vectors we should try again with */
			msix_intrs = retval;
	}
	if (msix_intrs < adpt->min_msix_intrs) {
		DRV_PRINT(INTR, INFO, "can't enable MSI-X interrupts\n");
		CLI_ADPT_FLAG(0, MSIX_EN);
		kfree(adpt->msix_entries);
		adpt->msix_entries = NULL;
		goto try_msi_mode;
	}

	DRV_PRINT(INTR, INFO, "enable MSI-X interrupts, num_msix_intrs = %d\n",
			msix_intrs);
	SET_ADPT_FLAG(0, MSIX_EN);
	if (CHK_ADPT_FLAG(0, SRSS_CAP))
		SET_ADPT_FLAG(0, SRSS_EN);

	adpt->num_msix_intrs = min(msix_intrs, adpt->max_msix_intrs);
	retval = 0;
	return retval;

try_msi_mode:
	CLI_ADPT_FLAG(0, SRSS_CAP);
	CLI_ADPT_FLAG(0, SRSS_EN);
	alx_set_num_queues(adpt);
	retval = -1;
	return retval;
}

/* set msi interrupt mode */
static int alx_set_msi_interrupt_mode(struct alx_adapter *adpt)
{
	int retval;

	retval = pci_enable_msi(adpt->pdev);
	if (retval) {
		DRV_PRINT(INTR, INFO, "can't enable MSI interrupt. "
				"retval: %d\n", retval);
		return retval;
	}
	SET_ADPT_FLAG(0, MSI_EN);
	return retval;
}

/* set interrupt mode */
static int alx_set_interrupt_mode(struct alx_adapter *adpt)
{
	int retval = 0;

	if (CHK_ADPT_FLAG(0, MSIX_CAP)) {
		DRV_PRINT(INTR, INFO, "Try to set MSIX interrupt.\n");
		retval = alx_set_msix_interrupt_mode(adpt);
		if (!retval)
			return retval;
	}

	if (CHK_ADPT_FLAG(0, MSI_CAP)) {
		DRV_PRINT(INTR, INFO, "Try to set MSI interrupt.\n");
		retval = alx_set_msi_interrupt_mode(adpt);
		if (!retval)
			return retval;
	}

	DRV_PRINT(INTR, INFO, "can't enable MSIX and MSI interrupt. "
			"And enable Legacy interrupt.\n");
	retval = 0;
	return retval;
}


static void alx_reset_interrupt_mode(struct alx_adapter *adpt)
{
	if (CHK_ADPT_FLAG(0, MSIX_EN)) {
		CLI_ADPT_FLAG(0, MSIX_EN);
		pci_disable_msix(adpt->pdev);
		kfree(adpt->msix_entries);
		adpt->msix_entries = NULL;
	} else if (CHK_ADPT_FLAG(0, MSI_EN)) {
		CLI_ADPT_FLAG(0, MSI_EN);
		pci_disable_msi(adpt->pdev);
	}
}


static int __devinit alx_init_adapter_special(struct alx_adapter *adpt)
{
	switch (adpt->hw.mac_type) {
	case alx_mac_l1f:
		goto init_alf_adapter;
		break;
	case alx_mac_l1c:
	case alx_mac_l1d_v1:
	case alx_mac_l1d_v2:
	case alx_mac_l2c:
	case alx_mac_l2cb_v1:
	case alx_mac_l2cb_v20:
	case alx_mac_l2cb_v21:
		goto init_alc_adapter;
		break;
	default:
		break;
	}
	return -1;

init_alc_adapter:
	if (CHK_ADPT_FLAG(0, MSIX_CAP))
		DRV_PRINT(INIT, ERR, "ALC doesn't support MSIX.\n");

	/* msi for tx, rx and none queues */
	adpt->num_msix_txques = 0;
	adpt->num_msix_rxques = 0;
	adpt->num_msix_noques = 0;
	return 0;

init_alf_adapter:
	if (CHK_ADPT_FLAG(0, MSIX_CAP)) {
		/* msix for tx, rx and none queues */
		adpt->num_msix_txques = 4;
		adpt->num_msix_rxques = 8;
		adpt->num_msix_noques = ALF_MAX_MSIX_NOQUE_INTRS;

		/* msix vector range */
		adpt->max_msix_intrs = ALF_MAX_MSIX_INTRS;
		adpt->min_msix_intrs = ALF_MIN_MSIX_INTRS;
	} else {
		/* msi for tx, rx and none queues */
		adpt->num_msix_txques = 0;
		adpt->num_msix_rxques = 0;
		adpt->num_msix_noques = 0;
	}
	return 0;

}
/*
 * alx_init_adapter
 */
static int __devinit alx_init_adapter(struct alx_adapter *adpt)
{
	struct alx_hw *hw   = &adpt->hw;
	struct pci_dev	*pdev = adpt->pdev;
	u32 revision;
	int max_frame;

	/* PCI config space info */

	hw->pci_venid = pdev->vendor;
	hw->pci_devid = pdev->device;
	MEM_R32(hw, PCI_CLASS_REVISION, &revision);
	hw->pci_revid = revision & 0xFF;
	hw->pci_sub_venid = pdev->subsystem_vendor;
	hw->pci_sub_devid = pdev->subsystem_device;


	if (alx_init_hw_callbacks(adpt) != 0) {
		DRV_PRINT(INIT, ERR, "set hw function pointers failed\n");
		return -1;
	}

	if (hw->cbs.identify_nic(hw) != 0) {
		DRV_PRINT(INIT, ERR, "hw is disabled\n");
		return -1;
	}

	/* Set adapter flags */
	switch (hw->mac_type) {
	case alx_mac_l1f:
#ifdef CONFIG_ALX_MSI
		SET_ADPT_FLAG(0, MSI_CAP);
#endif
#ifdef CONFIG_ALX_MSIX
		SET_ADPT_FLAG(0, MSIX_CAP);
#endif
		if (CHK_ADPT_FLAG(0, MSIX_CAP)) {
			SET_ADPT_FLAG(0, FIXED_MSIX);
			SET_ADPT_FLAG(0, MRQ_CAP);
#ifdef CONFIG_ALX_RSS
			SET_ADPT_FLAG(0, SRSS_CAP);
#endif
		}
		pdev->dev_flags |= PCI_DEV_FLAGS_MSI_INTX_DISABLE_BUG;
		break;
	case alx_mac_l1c:
	case alx_mac_l1d_v1:
	case alx_mac_l1d_v2:
	case alx_mac_l2c:
	case alx_mac_l2cb_v1:
	case alx_mac_l2cb_v20:
	case alx_mac_l2cb_v21:
#ifdef CONFIG_ALX_MSI
		SET_ADPT_FLAG(0, MSI_CAP);
#endif
		break;
	default:
		break;
	}

	/* set default for alx_adapter */
	adpt->max_msix_intrs = 1;
	adpt->min_msix_intrs = 1;
	max_frame = adpt->netdev->mtu + ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN;
	adpt->rxbuf_size = adpt->netdev->mtu > ALX_DEF_RX_BUF_SIZE ?
			ALIGN(max_frame, 8) : ALX_DEF_RX_BUF_SIZE;

	/* set default for alx_hw */
	hw->link_up = false;
	hw->link_speed = ALX_LINK_SPEED_UNKNOWN;
	hw->preamble = 7;
	hw->intr_mask = ALX_IMR_NORMAL_MASK;
	hw->smb_timer = 400; /* 400ms */
	hw->mtu = adpt->netdev->mtu;

	/* set default for wrr */
	hw->wrr_prio0 = 4;
	hw->wrr_prio1 = 4;
	hw->wrr_prio2 = 4;
	hw->wrr_prio3 = 4;
	hw->wrr_mode = alx_wrr_mode_none;

	/* set default flow control settings */
	hw->req_fc_mode = alx_fc_full;
	hw->cur_fc_mode = alx_fc_full;	/* init for ethtool output */
	hw->disable_fc_autoneg = false;
	hw->fc_was_autonegged = false;
	hw->fc_single_pause = true;

	/* set default for rss info*/
	hw->rss_hstype = 0;
	hw->rss_mode = alx_rss_mode_disable;
	hw->rss_idt_size = 0;
	hw->rss_base_cpu = 0;
	memset(hw->rss_idt, 0x0, sizeof(hw->rss_idt));
	memset(hw->rss_key, 0x0, sizeof(hw->rss_key));

	atomic_set(&adpt->irq_sem, 1);
	spin_lock_init(&adpt->tx_lock);
	spin_lock_init(&adpt->rx_lock);

	alx_init_adapter_special(adpt);

	if (hw->cbs.init_phy) {
		if (hw->cbs.init_phy(hw))
			return -1;
	}

	set_bit(__ALX_DOWN, &adpt->alx_state);
	return 0;
}


static int  alx_set_register_info_special(struct alx_adapter *adpt)
{
	struct alx_hw *hw = &adpt->hw;
	int num_txques = adpt->num_txques;

	switch (adpt->hw.mac_type) {
	case alx_mac_l1f:
		goto cache_alf_register;
		break;
	case alx_mac_l1c:
	case alx_mac_l1d_v1:
	case alx_mac_l1d_v2:
	case alx_mac_l2c:
	case alx_mac_l2cb_v1:
	case alx_mac_l2cb_v20:
	case alx_mac_l2cb_v21:
		goto cache_alc_register;
		break;
	default:
		break;
	}
	return -1;

cache_alc_register:
	/* setting for Produce Index and Consume Index */
	adpt->rx_queue[0]->produce_reg = hw->rx_prod_reg[0];
	adpt->rx_queue[0]->consume_reg = hw->rx_cons_reg[0];

	switch (num_txques) {
	case 2:
		adpt->tx_queue[1]->produce_reg = hw->tx_prod_reg[1];
		adpt->tx_queue[1]->consume_reg = hw->tx_cons_reg[1];
	case 1:
		adpt->tx_queue[0]->produce_reg = hw->tx_prod_reg[0];
		adpt->tx_queue[0]->consume_reg = hw->tx_cons_reg[0];
		break;
	}
	return 0;

cache_alf_register:
	/* setting for Produce Index and Consume Index */
	adpt->rx_queue[0]->produce_reg = hw->rx_prod_reg[0];
	adpt->rx_queue[0]->consume_reg = hw->rx_cons_reg[0];

	switch (num_txques) {
	case 4:
		adpt->tx_queue[3]->produce_reg = hw->tx_prod_reg[3];
		adpt->tx_queue[3]->consume_reg = hw->tx_cons_reg[3];
	case 3:
		adpt->tx_queue[2]->produce_reg = hw->tx_prod_reg[2];
		adpt->tx_queue[2]->consume_reg = hw->tx_cons_reg[2];
	case 2:
		adpt->tx_queue[1]->produce_reg = hw->tx_prod_reg[1];
		adpt->tx_queue[1]->consume_reg = hw->tx_cons_reg[1];
	case 1:
		adpt->tx_queue[0]->produce_reg = hw->tx_prod_reg[0];
		adpt->tx_queue[0]->consume_reg = hw->tx_cons_reg[0];
	}
	return 0;
}


/* alx_alloc_tx_descriptor - allocate Tx Descriptors */
static int alx_alloc_tx_descriptor(struct alx_adapter *adpt,
				   struct alx_tx_queue *txque)
{
	struct alx_ring_header *ring_header = &adpt->ring_header;
	int size;

	DRV_PRINT(IF, INFO, "tpq.count = %d\n", txque->tpq.count);

	size = sizeof(struct alx_buffer) * txque->tpq.count;
	txque->tpq.tpbuff = kzalloc(size, GFP_KERNEL);
	if (!txque->tpq.tpbuff)
		goto err_alloc_tpq_buffer;
	memset(txque->tpq.tpbuff, 0, size);

	/* round up to nearest 4K */
	txque->tpq.size = txque->tpq.count * sizeof(struct alx_tpdesc);

	txque->tpq.tpdma = ring_header->dma + ring_header->used;
	txque->tpq.tpdesc = ring_header->desc + ring_header->used;
	adpt->hw.tpdma[txque->que_idx] = (u64)txque->tpq.tpdma;
	ring_header->used += ALIGN(txque->tpq.size, 8);

	txque->tpq.produce_idx = 0;
	txque->tpq.consume_idx = 0;
	txque->max_packets = txque->tpq.count;
	return 0;

err_alloc_tpq_buffer:
	kfree(txque->tpq.tpbuff);
	txque->tpq.tpbuff = NULL;
	DRV_PRINT(IF, ERR, "Unable to allocate memory "
		"for the Tx descriptor.\n");
	return -ENOMEM;
}

/* alx_alloc_all_tx_descriptor - allocate all Tx Descriptors */
static int alx_alloc_all_tx_descriptor(struct alx_adapter *adpt)
{
	int i, retval = 0;
	DRV_PRINT(IF, INFO, "num_tques = %d\n", adpt->num_txques);

	for (i = 0; i < adpt->num_txques; i++) {
		retval = alx_alloc_tx_descriptor(adpt, adpt->tx_queue[i]);
		if (!retval)
			continue;

		DRV_PRINT(IF, ERR, "Allocation for Tx Queue %u failed\n", i);
		break;
	}

	return retval;
}

/* alx_alloc_rx_descriptor - allocate Rx Descriptors */
static int alx_alloc_rx_descriptor(struct alx_adapter *adpt,
				   struct alx_rx_queue *rxque)
{
	struct alx_ring_header *ring_header = &adpt->ring_header;
	int size;

	DRV_PRINT(IF, INFO, "RRD.count = %d, RFD.count = %d, "
			"SWD.count = %d.\n",
			rxque->rrq.count,
			rxque->rfq.count,
			rxque->swq.count);

	if (CHK_RX_FLAG(HW_QUE)) {
		/* alloc buffer info */
		size = sizeof(struct alx_buffer) * rxque->rfq.count;
		rxque->rfq.rfbuff = kzalloc(size, GFP_KERNEL);
		if (!rxque->rfq.rfbuff)
			goto err_alloc_rfq_buffer;
		memset(rxque->rfq.rfbuff, 0, size);

		/*
		 * set dma's point of rrq and rfq
		 */

		/* Round up to nearest 4K */
		rxque->rrq.size =
			rxque->rrq.count * sizeof(struct alx_rrdesc);
		rxque->rfq.size =
			rxque->rfq.count * sizeof(struct alx_rfdesc);

		rxque->rrq.rrdma = ring_header->dma + ring_header->used;
		rxque->rrq.rrdesc = ring_header->desc + ring_header->used;
		adpt->hw.rrdma[rxque->que_idx] = (u64)rxque->rrq.rrdma;
		ring_header->used += ALIGN(rxque->rrq.size, 8);

		rxque->rfq.rfdma = ring_header->dma + ring_header->used;
		rxque->rfq.rfdesc = ring_header->desc + ring_header->used;
		adpt->hw.rfdma[rxque->que_idx] = (u64)rxque->rfq.rfdma;
		ring_header->used += ALIGN(rxque->rfq.size, 8);

		/* clean all counts within rxque */
		rxque->rrq.produce_idx = 0;
		rxque->rrq.consume_idx = 0;

		rxque->rfq.produce_idx = 0;
		rxque->rfq.consume_idx = 0;
	}

	if (CHK_RX_FLAG(SW_QUE)) {
		size = sizeof(struct alx_sw_buffer) * rxque->swq.count;
		rxque->swq.swbuff = kzalloc(size, GFP_KERNEL);
		if (!rxque->swq.swbuff)
			goto err_alloc_swq_buffer;
		memset(rxque->swq.swbuff, 0, size);

		rxque->swq.consume_idx = 0;
		rxque->swq.produce_idx = 0;
	}

	rxque->max_packets = rxque->rrq.count / 2;
	return 0;

err_alloc_swq_buffer:
	kfree(rxque->swq.swbuff);
	rxque->swq.swbuff = NULL;
err_alloc_rfq_buffer:
	kfree(rxque->rfq.rfbuff);
	rxque->rfq.rfbuff = NULL;
	DRV_PRINT(IF, ERR, "Unable to allocate memory "
		"for the Rx descriptor\n");
	return -ENOMEM;
}

/* alx_alloc_all_rx_descriptor - allocate all Rx Descriptors */
static int alx_alloc_all_rx_descriptor(struct alx_adapter *adpt)
{
	int i, error = 0;

	for (i = 0; i < adpt->num_rxques; i++) {
		error = alx_alloc_rx_descriptor(adpt, adpt->rx_queue[i]);
		if (!error)
			continue;
		DRV_PRINT(IF, ERR, "Allocation for Rx Queue %u failed\n", i);
		break;
	}

	return error;
}

/* alx_free_tx_descriptor - Free Tx Descriptor */
static void alx_free_tx_descriptor(struct alx_tx_queue *txque)
{
	alx_clean_tx_queue(txque);

	kfree(txque->tpq.tpbuff);
	txque->tpq.tpbuff = NULL;

	/* if not set, then don't free */
	if (!txque->tpq.tpdesc)
		return;
	txque->tpq.tpdesc = NULL;
}

/* alx_free_all_tx_descriptor - Free all Tx Descriptor */
static void alx_free_all_tx_descriptor(struct alx_adapter *adpt)
{
	int i;

	for (i = 0; i < adpt->num_txques; i++)
		alx_free_tx_descriptor(adpt->tx_queue[i]);
}

/* alx_free_all_rx_descriptor - Free all Rx Descriptor */
static void alx_free_rx_descriptor(struct alx_rx_queue *rxque)
{
	alx_clean_rx_queue(rxque);

	if (CHK_RX_FLAG(HW_QUE)) {
		kfree(rxque->rfq.rfbuff);
		rxque->rfq.rfbuff = NULL;

		/* if not set, then don't free */
		if (!rxque->rrq.rrdesc)
			return;
		rxque->rrq.rrdesc = NULL;

		if (!rxque->rfq.rfdesc)
			return;
		rxque->rfq.rfdesc = NULL;
	}

	if (CHK_RX_FLAG(SW_QUE)) {
		kfree(rxque->swq.swbuff);
		rxque->swq.swbuff = NULL;
	}
}

/* alx_free_all_rx_descriptor - Free all Rx Descriptor */
static void alx_free_all_rx_descriptor(struct alx_adapter *adpt)
{
	int i;
	for (i = 0; i < adpt->num_rxques; i++)
		alx_free_rx_descriptor(adpt->rx_queue[i]);
}

/*
 * alx_alloc_all_rtx_descriptor - allocate Tx / RX descriptor queues
 * @adpt: board private structure
 */
static int alx_alloc_all_rtx_descriptor(struct alx_adapter *adpt)
{
	struct pci_dev *pdev = adpt->pdev;
	struct alx_ring_header *ring_header = &adpt->ring_header;
	int num_tques = adpt->num_txques;
	int num_rques = adpt->num_hw_rxques;
	unsigned int num_tx_descs = adpt->num_txdescs;
	unsigned int num_rx_descs = adpt->num_rxdescs;
	int retval;

	/*
	 * real ring DMA buffer
	 * each ring/block may need up to 8 bytes for alignment, hence the
	 * additional bytes tacked onto the end.
	 */
	ring_header->size =
		num_tques * num_tx_descs * sizeof(struct alx_tpdesc) +
		num_rques * num_rx_descs * sizeof(struct alx_rfdesc) +
		num_rques * num_rx_descs * sizeof(struct alx_rrdesc) +
		sizeof(struct coals_msg_block) +
		sizeof(struct alx_hw_stats) +
		num_tques * 8 + num_rques * 2 * 8 + 8 * 2;
	DRV_PRINT(IF, INFO, "num_tques = %d, num_tx_descs = %d.\n",
			num_tques, num_tx_descs);
	DRV_PRINT(IF, INFO, "num_rques = %d, num_rx_descs = %d.\n",
			num_rques, num_rx_descs);

	ring_header->used = 0;
	ring_header->desc = pci_alloc_consistent(pdev, ring_header->size,
				&ring_header->dma);

	if (!ring_header->desc) {
		DRV_PRINT(IF, ERR, "pci_alloc_consistend failed\n");
		retval = -ENOMEM;
		goto err_alloc_dma;
	}
	memset(ring_header->desc, 0, ring_header->size);
	ring_header->used = ALIGN(ring_header->dma, 8) - ring_header->dma;

	DRV_PRINT(IF, INFO, "Ring Header: size = %d, used= %d.\n",
		ring_header->size, ring_header->used);

	/* allocate receive descriptors */
	retval = alx_alloc_all_tx_descriptor(adpt);
	if (retval)
		goto err_alloc_tx;

	/* allocate receive descriptors */
	retval = alx_alloc_all_rx_descriptor(adpt);
	if (retval)
		goto err_alloc_rx;

	/* Init CMB dma address */
	adpt->cmb.dma = ring_header->dma + ring_header->used;
	adpt->cmb.cmb = (u8 *) ring_header->desc + ring_header->used;
	ring_header->used += ALIGN(sizeof(struct coals_msg_block), 8);

	adpt->smb.dma = ring_header->dma + ring_header->used;
	adpt->smb.smb = (u8 *)ring_header->desc + ring_header->used;
	ring_header->used += ALIGN(sizeof(struct alx_hw_stats), 8);

	return 0;

err_alloc_rx:
	alx_free_all_rx_descriptor(adpt);
err_alloc_tx:
	alx_free_all_tx_descriptor(adpt);
err_alloc_dma:
	return retval;
}


/*
 * alx_alloc_all_rtx_descriptor - allocate Tx / RX descriptor queues
 * @adpt: board private structure
 */
static void alx_free_all_rtx_descriptor(struct alx_adapter *adpt)
{
	struct pci_dev *pdev = adpt->pdev;
	struct alx_ring_header *ring_header = &adpt->ring_header;

	alx_free_all_tx_descriptor(adpt);
	alx_free_all_rx_descriptor(adpt);

	adpt->cmb.dma = 0;
	adpt->cmb.cmb = NULL;
	adpt->smb.dma = 0;
	adpt->smb.smb = NULL;

	pci_free_consistent(pdev, ring_header->size, ring_header->desc,
					ring_header->dma);
	ring_header->desc = NULL;
	ring_header->size = ring_header->used = 0;
}


/*
 * alx_change_mtu - Change the Maximum Transfer Unit
 * @netdev: network interface device structure
 * @new_mtu: new value for maximum frame size
 */
static int alx_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	int old_mtu   = netdev->mtu;
	int max_frame = new_mtu + ETH_HLEN + ETH_FCS_LEN + VLAN_HLEN;
	struct pci_dev *pdev = adpt->pdev;

	DRV_PRINT(FUNC, DEBUG, "ENTER\n");

	if ((max_frame < ALX_MIN_ETH_FRAME_SIZE) ||
	    (max_frame > ALX_MAX_ETH_FRAME_SIZE)) {
		dev_warn(&pdev->dev, "invalid MTU setting\n");
		return -EINVAL;
	}
	/* set MTU */
	if (old_mtu != new_mtu && netif_running(netdev)) {

		DRV_PRINT(IF, INFO, "changing MTU from %d to %d\n",
				netdev->mtu, new_mtu);
		netdev->mtu = new_mtu;
		adpt->hw.mtu = new_mtu;
		adpt->rxbuf_size = new_mtu > ALX_DEF_RX_BUF_SIZE ?
			ALIGN(max_frame, 8) : ALX_DEF_RX_BUF_SIZE;
		if (new_mtu > ALX_MAX_TSO_PKT_SIZE) {
			adpt->netdev->features &= ~NETIF_F_TSO;
			adpt->netdev->features &= ~NETIF_F_TSO6;
		} else {
			adpt->netdev->features |= NETIF_F_TSO;
			adpt->netdev->features |= NETIF_F_TSO6;
		}

		alx_reinit_locked(adpt);
	}

	return 0;
}


int alx_open_internal(struct alx_adapter *adpt, u32 ctrl)
{
	struct alx_hw *hw = &adpt->hw;
	int retval = 0;
	int i;

	alx_init_ring_ptrs(adpt);

	alx_set_multicase_list(adpt->netdev);
	alx_restore_vlan(adpt);

	if (hw->cbs.start_mac)
		retval = hw->cbs.start_mac(hw);

	if (hw->cbs.config_mac)
		retval = hw->cbs.config_mac(hw, adpt->rxbuf_size,
				adpt->num_hw_rxques, adpt->num_rxdescs,
				adpt->num_txques, adpt->num_txdescs);

	if (hw->cbs.config_tx)
		retval = hw->cbs.config_tx(hw);

	if (hw->cbs.config_rx)
		retval = hw->cbs.config_rx(hw);

	alx_config_rss(adpt);

	for (i = 0; i < adpt->num_hw_rxques; i++)
		alx_refresh_rx_buffer(adpt->rx_queue[i]);

	/* configure HW regsiters of MSIX */
	if (hw->cbs.config_msix)
		retval = hw->cbs.config_msix(hw, adpt->num_msix_intrs,
					CHK_ADPT_FLAG(0, MSIX_EN),
					CHK_ADPT_FLAG(0, MSI_EN));

	if (ctrl & ALX_OPEN_CTRL_IRQ_EN) {
		retval = alx_request_irq(adpt);
		if (retval)
			goto err_request_irq;
	}

	/* enable NAPI, INTR and TX */
	alx_napi_enable_all(adpt);

	alx_enable_intr(adpt);

	netif_tx_start_all_queues(adpt->netdev);

	clear_bit(__ALX_DOWN, &adpt->alx_state);

	/* check link status */
	SET_ADPT_FLAG(1, LSC_REQUESTED);
	adpt->link_jiffies = jiffies + ALX_TRY_LINK_TIMEOUT;
	mod_timer(&adpt->alx_timer, jiffies);

	return retval;

err_request_irq:
	alx_clean_all_rx_queues(adpt);
	return retval;
}


void alx_stop_internal(struct alx_adapter *adpt, u32 ctrl)
{
	struct net_device *netdev = adpt->netdev;
	struct alx_hw *hw = &adpt->hw;

	set_bit(__ALX_DOWN, &adpt->alx_state);

	netif_tx_stop_all_queues(netdev);
	/* call carrier off first to avoid false dev_watchdog timeouts */
	netif_carrier_off(netdev);
	netif_tx_disable(netdev);

	alx_disable_intr(adpt);

	alx_napi_disable_all(adpt);

	if (ctrl & ALX_OPEN_CTRL_IRQ_EN)
		alx_free_irq(adpt);

	CLI_ADPT_FLAG(1, LSC_REQUESTED);
	CLI_ADPT_FLAG(1, RESET_REQUESTED);
	CLI_ADPT_FLAG(1, DBG_REQUESTED);
	del_timer_sync(&adpt->alx_timer);

	/* reset MAC to disable all RX/TX */
	if (ctrl & ALX_OPEN_CTRL_MAC_EN) {
		if (hw->cbs.reset_mac)
			hw->cbs.reset_mac(hw);
	}
	adpt->hw.link_speed = ALX_LINK_SPEED_UNKNOWN;

	alx_clean_all_tx_queues(adpt);
	alx_clean_all_rx_queues(adpt);
}


/*
 * alx_open - Called when a network interface is made active
 * @netdev: network interface device structure
 */
static int alx_open(struct net_device *netdev)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	struct alx_hw *hw = &adpt->hw;
	int retval;

	DRV_PRINT(FUNC, DEBUG, "ENTER\n");

	/* disallow open during test */
	if (test_bit(__ALX_TESTING, &adpt->alx_state))
		return -EBUSY;

	netif_carrier_off(netdev);

	/* allocate rx/tx dma buffer & descriptors */
	retval = alx_alloc_all_rtx_descriptor(adpt);
	if (retval) {
		DRV_PRINT(IF, ERR, "error in alx_alloc_all_rtx_descriptor.\n");
		goto err_alloc_rtx;
	}

	retval = alx_open_internal(adpt, ALX_OPEN_CTRL_IRQ_EN);
	if (retval)
		goto err_open_internal;

	return retval;

err_open_internal:
	alx_stop_internal(adpt, ALX_OPEN_CTRL_IRQ_EN);
err_alloc_rtx:
	alx_free_all_rtx_descriptor(adpt);
	hw->cbs.reset_mac(hw);
	return retval;
}

/*
 * alx_stop - Disables a network interface
 * @netdev: network interface device structure
 */
static int alx_stop(struct net_device *netdev)
{
	struct alx_adapter *adpt = netdev_priv(netdev);

	DRV_PRINT(FUNC, DEBUG, "ENTER\n");

	WARN_ON(test_bit(__ALX_RESETTING, &adpt->alx_state));
	alx_stop_internal(adpt, (ALX_OPEN_CTRL_IRQ_EN |
			ALX_OPEN_CTRL_MAC_EN));
	alx_free_all_rtx_descriptor(adpt);

	return 0;
}

#ifdef CONFIG_PM
int alx_resume(struct pci_dev *pdev)
{
	struct alx_adapter *adpt = pci_get_drvdata(pdev);
	struct net_device *netdev = adpt->netdev;
	struct alx_hw *hw = &adpt->hw;
	u32 retval;

	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	/*
	 * pci_restore_state clears dev->state_saved so call
	 * pci_save_state to restore it.
	 */
	pci_save_state(pdev);

	pci_enable_wake(pdev, PCI_D3hot, 0);
	pci_enable_wake(pdev, PCI_D3cold, 0);

	retval = hw->cbs.reset_pcie(hw, true, true);
	retval = hw->cbs.reset_phy(hw);
	retval = hw->cbs.reset_mac(hw);
	retval = hw->cbs.setup_phy_link(hw, hw->autoneg_advertised, true,
			!hw->disable_fc_autoneg);

	retval = hw->cbs.config_wol(hw, 0);

	if (netif_running(netdev)) {
		retval = alx_open_internal(adpt, 0);
		if (retval)
			return retval;
	}

	netif_device_attach(netdev);
	return 0;
}
#endif

/*
 * alx_shutdown_internal is not used when power management
 * is disabled on older kernels (<2.6.12). causes a compile
 * warning/error, because it is defined and not used.
 */
int alx_shutdown_internal(struct pci_dev *pdev, pm_message_t state)
{
	struct alx_adapter *adpt = pci_get_drvdata(pdev);
	struct net_device *netdev = adpt->netdev;
	struct alx_hw *hw = &adpt->hw;
	u32 wufc = adpt->wol;
	u16 lpa;
	u32 speed, adv_speed, misc;
	bool link_up;
	int i;
#ifdef CONFIG_PM
	int retval = 0;
#endif

	hw->cbs.config_aspm(hw, false, false);

	netif_device_detach(netdev);
	if (netif_running(netdev)) {
		alx_stop_internal(adpt, 0);
		alx_free_irq(adpt);
		/* alx_free_all_rtx_descriptor(adpt); */
	}
	/* alx_clear_intr_scheme(adpt); */
#ifdef CONFIG_PM
	retval = pci_save_state(pdev);
	if (retval)
		return retval;
#endif
	hw->cbs.check_phy_link(hw, &speed, &link_up);

	if (link_up) {
		if (hw->mac_type == alx_mac_l1f) {
			MEM_R32(hw, ALX_MISC, &misc);
			misc |= ALX_MISC_INTNLOSC_OPEN;
			MEM_W32(hw, ALX_MISC, misc);
		}

		retval = hw->cbs.read_phy_reg(hw, ALX_MDIO_NORM_DEV,
				MII_LPA, &lpa);
		if (retval)
			return retval;

		adv_speed = ALX_LINK_SPEED_10_HALF;
		if (lpa & LPA_10FULL)
			adv_speed = ALX_LINK_SPEED_10_FULL;
		else if (lpa & LPA_10HALF)
			adv_speed = ALX_LINK_SPEED_10_HALF;
		else if (lpa & LPA_100FULL)
			adv_speed = ALX_LINK_SPEED_100_FULL;
		else if (lpa & LPA_100HALF)
			adv_speed = ALX_LINK_SPEED_100_HALF;

		retval = hw->cbs.setup_phy_link(hw, adv_speed, true,
				!hw->disable_fc_autoneg);
		if (retval)
			return retval;

		for (i = 0; i < ALX_MAX_SETUP_LNK_CYCLE; i++) {
			__MS_DELAY(100);
			retval = hw->cbs.check_phy_link(hw, &speed, &link_up);
			if (retval)
				continue;
			if (link_up)
				break;
		}
	} else {
		speed = ALX_LINK_SPEED_10_HALF;
		link_up = false;
	}
	hw->link_speed = speed;
	hw->link_up = link_up;

	retval = hw->cbs.config_wol(hw, wufc);
	if (retval)
		return retval;

	/* clear phy interrupt */
	retval = hw->cbs.ack_phy_intr(hw);
	if (retval)
		return retval;

	if (wufc) {
		/* pcie patch */
		device_set_wakeup_enable(&pdev->dev, 1);
	}

	retval = hw->cbs.config_pow_save(hw, adpt->hw.link_speed,
			(wufc ? true : false), false,
			(wufc & ALX_WOL_MAGIC ? true : false), true);
	if (retval)
		return retval;
	pci_enable_wake(pdev, pci_choose_state(pdev, state), (wufc ? 1 : 0));
	pci_disable_device(pdev);
	pci_set_power_state(pdev, pci_choose_state(pdev, state));

	return 0;
}

#ifdef CONFIG_PM
int alx_suspend(struct pci_dev *pdev, pm_message_t state)
{
	int retval;

	retval = alx_shutdown_internal(pdev, state);
	if (retval)
		return retval;

	return 0;
}
#endif

void alx_shutdown(struct pci_dev *pdev)
{
	alx_shutdown_internal(pdev, PMSG_SUSPEND);
}


/**
 * alx_update_hw_stats - Update the board statistics counters.
 * @adpt: board private structure
 **/
static void alx_update_hw_stats(struct alx_adapter *adpt)
{
	struct net_device_stats *net_stats;
	struct alx_hw *hw = &adpt->hw;
	struct alx_hw_stats *hwstats = &adpt->hw_stats;
	unsigned long *hwstat_item = NULL;
	u32 hwstat_reg;
	u32 hwstat_data;

	if (test_bit(__ALX_DOWN, &adpt->alx_state) ||
	    test_bit(__ALX_RESETTING, &adpt->alx_state))
		return;

	/* update RX status */
	hwstat_reg  = hw->rxstat_reg;
	hwstat_item = &hwstats->rx_ok;
	while (hwstat_reg < hw->rxstat_reg + hw->rxstat_sz) {
		MEM_R32(hw, hwstat_reg, &hwstat_data);
		*hwstat_item += hwstat_data;
		hwstat_reg += 4;
		hwstat_item++;
	}

	/* update TX status */
	hwstat_reg  = hw->txstat_reg;
	hwstat_item = &hwstats->tx_ok;
	while (hwstat_reg < hw->txstat_reg + hw->txstat_sz) {
		MEM_R32(hw, hwstat_reg, &hwstat_data);
		*hwstat_item += hwstat_data;
		hwstat_reg += 4;
		hwstat_item++;
	}

	net_stats = GET_NETDEV_STATS(adpt);
	net_stats->rx_packets = hwstats->rx_ok;
	net_stats->tx_packets = hwstats->tx_ok;
	net_stats->rx_bytes   = hwstats->rx_byte_cnt;
	net_stats->tx_bytes   = hwstats->tx_byte_cnt;
	net_stats->multicast  = hwstats->rx_mcast;
	net_stats->collisions = hwstats->tx_single_col +
		hwstats->tx_multi_col * 2 +
		hwstats->tx_late_col + hwstats->tx_abort_col;

	net_stats->rx_errors  = hwstats->rx_frag + hwstats->rx_fcs_err +
		hwstats->rx_len_err + hwstats->rx_ov_sz +
		hwstats->rx_ov_rrd + hwstats->rx_align_err;

	net_stats->rx_fifo_errors   = hwstats->rx_ov_rxf;
	net_stats->rx_length_errors = hwstats->rx_len_err;
	net_stats->rx_crc_errors    = hwstats->rx_fcs_err;
	net_stats->rx_frame_errors  = hwstats->rx_align_err;
	net_stats->rx_over_errors   = hwstats->rx_ov_rrd + hwstats->rx_ov_rxf;

	net_stats->rx_missed_errors = hwstats->rx_ov_rrd + hwstats->rx_ov_rxf;

	net_stats->tx_errors = hwstats->tx_late_col + hwstats->tx_abort_col +
		hwstats->tx_underrun + hwstats->tx_trunc;
	net_stats->tx_fifo_errors    = hwstats->tx_underrun;
	net_stats->tx_aborted_errors = hwstats->tx_abort_col;
	net_stats->tx_window_errors  = hwstats->tx_late_col;
}


/**
 * alx_get_hw_stats - Get System Network Statistics
 * @netdev: network interface device structure
 *
 * Returns the address of the device statistics structure.
 * The statistics are actually updated from the timer callback.
 **/
static struct net_device_stats *alx_get_hw_stats(struct net_device *netdev)
{
	struct alx_adapter *adpt = netdev_priv(netdev);

	DRV_PRINT(FUNC, DEBUG, "ENTER\n");

	alx_update_hw_stats(adpt);
	return GET_NETDEV_STATS(adpt);
}


static void alx_debug_task_routine(struct alx_adapter *adpt)
{
	if (!CHK_ADPT_FLAG(1, DBG_REQUESTED))
		return;
	CLI_ADPT_FLAG(1, DBG_REQUESTED);

	/*debug code put here */
}

static void alx_link_task_routine(struct alx_adapter *adpt)
{
	struct net_device *netdev = adpt->netdev;
	struct alx_hw *hw = &adpt->hw;
	char *link_desc;

	if (!CHK_ADPT_FLAG(1, LSC_REQUESTED))
		return;
	CLI_ADPT_FLAG(1, LSC_REQUESTED);

	if (test_bit(__ALX_DOWN, &adpt->alx_state))
		return;

	if (hw->cbs.check_phy_link) {
		hw->cbs.check_phy_link(hw,
			&hw->link_speed, &hw->link_up);
	} else {
		/* always assume link is up, if no check link function */
		hw->link_speed = ALX_LINK_SPEED_1GB_FULL;
		hw->link_up = true;
	}
	DRV_PRINT(TIMER, INFO, "link_speed = %d, link_up = %d\n",
		  hw->link_speed, hw->link_up);

	if (!hw->link_up && time_after(adpt->link_jiffies, jiffies))
		SET_ADPT_FLAG(1, LSC_REQUESTED);

	if (hw->link_up) {
		if (netif_carrier_ok(netdev))
			return;

		link_desc = (hw->link_speed == ALX_LINK_SPEED_1GB_FULL) ?
			"1 Gbps Duplex Full" :
			(hw->link_speed == ALX_LINK_SPEED_100_FULL ?
			 "100 Mbps Duplex Full" :
			 (hw->link_speed == ALX_LINK_SPEED_100_HALF ?
			  "100 Mbps Duplex Half" :
			  (hw->link_speed == ALX_LINK_SPEED_10_FULL ?
			   "10 Mbps Duplex Full" :
			   (hw->link_speed == ALX_LINK_SPEED_10_HALF ?
			    "10 Mbps Duplex HALF" :
			    "unknown speed"))));
		DRV_PRINT(TIMER, INFO, "NIC Link is Up %s\n", link_desc);

		hw->cbs.config_aspm(hw, true, true);
		hw->cbs.start_mac(hw);
		netif_carrier_on(netdev);
		netif_tx_wake_all_queues(netdev);
	} else {
		/* only continue if link was up previously */
		if (!netif_carrier_ok(netdev))
			return;

		hw->link_speed = 0;
		DRV_PRINT(TIMER, INFO, "NIC Link is Down\n");
		netif_carrier_off(netdev);
		netif_tx_stop_all_queues(netdev);

		hw->cbs.config_aspm(hw, false, true);
		hw->cbs.stop_mac(hw);
		hw->cbs.setup_phy_link(hw, hw->autoneg_advertised, true,
				!hw->disable_fc_autoneg);
	}
}


static void alx_reset_task_routine(struct alx_adapter *adpt)
{
	if (!CHK_ADPT_FLAG(1, RESET_REQUESTED))
		return;
	CLI_ADPT_FLAG(1, RESET_REQUESTED);

	if (test_bit(__ALX_DOWN, &adpt->alx_state) ||
	    test_bit(__ALX_RESETTING, &adpt->alx_state))
		return;

	alx_reinit_locked(adpt);
}


/**
 * alx_timer_routine - Timer Call-back
 * @data: pointer to adapter cast into an unsigned long
 **/
static void alx_timer_routine(unsigned long data)
{
	struct alx_adapter *adpt = (struct alx_adapter *)data;
	unsigned long delay;

	/* poll faster when waiting for link */
	if (CHK_ADPT_FLAG(1, LSC_REQUESTED))
		delay = HZ / 10;
	else
		delay = HZ * 2;

	/* Reset the timer */
	mod_timer(&adpt->alx_timer, delay + jiffies);

	alx_task_schedule(adpt);
}
/**
 * alx_task_routine - manages and runs subtasks
 * @work: pointer to work_struct containing our data
 **/
static void alx_task_routine(struct work_struct *work)
{
	struct alx_adapter *adpt = container_of(work,
				struct alx_adapter, alx_task);
	/* test state of adapter */
	BUG_ON(!test_bit(__ALX_SERVICE_SCHED, &adpt->alx_state));

	/* reset task */
	alx_reset_task_routine(adpt);

	/* link task */
	alx_link_task_routine(adpt);

	/* debug task */
	alx_debug_task_routine(adpt);

	/* flush memory to make sure state is correct before next watchog */
	smp_mb__before_clear_bit();

	clear_bit(__ALX_SERVICE_SCHED, &adpt->alx_state);
}


/* Calculate the transmit packet descript needed*/
static bool alx_check_num_tpdescs(struct alx_tx_queue *txque,
				  const struct sk_buff *skb)
{
	u16 num_required = 1;
	u16 num_available = 0;
	u16 produce_idx = txque->tpq.produce_idx;
	u16 consume_idx = txque->tpq.consume_idx;
	int i = 0;

	u16 proto_hdr_len = 0;
	if (skb_is_gso(skb)) {
		proto_hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);
		if (proto_hdr_len < skb_headlen(skb))
			num_required++;
		if (skb_shinfo(skb)->gso_type & SKB_GSO_TCPV6)
			num_required++;
	}
	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++)
		num_required++;
	num_available = (u16)(consume_idx > produce_idx) ?
		(consume_idx - produce_idx - 1) :
		(txque->tpq.count + consume_idx - produce_idx - 1);

	return num_required < num_available;
}


static int alx_tso_csum(struct alx_adapter *adpt, struct alx_tx_queue *txque,
			struct sk_buff *skb, struct alx_tpdesc *stpd)
{
	struct pci_dev *pdev = adpt->pdev;
	u8 hdr_len;
	u32 real_len;
	int error;

	if (skb_is_gso(skb)) {
		if (skb_header_cloned(skb)) {
			error = pskb_expand_head(skb, 0, 0, GFP_ATOMIC);
			if (unlikely(error))
				return -1;
		}

		if (skb->protocol == htons(ETH_P_IP)) {
			real_len = (((unsigned char *)ip_hdr(skb) - skb->data)
					+ ntohs(ip_hdr(skb)->tot_len));

			if (real_len < skb->len)
				pskb_trim(skb, real_len);

			hdr_len = (skb_transport_offset(skb) + tcp_hdrlen(skb));
			if (unlikely(skb->len == hdr_len)) {
				/* only xsum need */
				dev_warn(&pdev->dev,
				      "IPV4 tso with zero data??\n");
				goto check_sum;
			} else {
				ip_hdr(skb)->check = 0;
				tcp_hdr(skb)->check = ~csum_tcpudp_magic(
							ip_hdr(skb)->saddr,
							ip_hdr(skb)->daddr,
							0, IPPROTO_TCP, 0);
				stpd->tp_gnr.ipv4 = 1;
			}
		}

		if (skb_shinfo(skb)->gso_type & SKB_GSO_TCPV6) {
			struct alx_tpdesc etpd;
			memset(&etpd, 0, sizeof(struct alx_tpdesc));
			memset(stpd, 0, sizeof(struct alx_tpdesc));

			ipv6_hdr(skb)->payload_len = 0;
			/* check payload == 0 byte ? */
			hdr_len = (skb_transport_offset(skb) + tcp_hdrlen(skb));
			if (unlikely(skb->len == hdr_len)) {
				/* only xsum need */
				dev_warn(&pdev->dev,
					"IPV6 tso with zero data??\n");
				goto check_sum;
			} else
				tcp_hdr(skb)->check = ~csum_ipv6_magic(
						&ipv6_hdr(skb)->saddr,
						&ipv6_hdr(skb)->daddr,
						0, IPPROTO_TCP, 0);
			etpd.tp_tso.addr_lo = skb->len;
			etpd.tp_tso.lso = 0x1;
			etpd.tp_tso.lso_v2 = 0x1;
			stpd->tp_tso.lso_v2 = 0x1;
			alx_set_tpdesc(txque, &etpd);
		}

		stpd->tp_tso.lso = 0x1;
		stpd->tp_tso.tcphdr_offset = skb_transport_offset(skb);
		stpd->tp_tso.mss = skb_shinfo(skb)->gso_size;
		return 0;
	}

check_sum:
	if (likely(skb->ip_summed == CHECKSUM_PARTIAL)) {
		u8 css, cso;
		cso = skb_transport_offset(skb);

		if (unlikely(cso & 0x1)) {
			dev_err(&pdev->dev,
			   "pay load offset should not ant event number\n");
			return -1;
		} else {
			css = cso + skb->csum_offset;

			stpd->tp_sum.payld_offset = cso >> 1;
			stpd->tp_sum.cxsum_offset = css >> 1;
			stpd->tp_sum.c_sum = 0x1;
		}
	}
	return 0;
}

static void alx_tx_map(struct alx_adapter *adpt, struct sk_buff *skb,
		       struct alx_tpdesc *stpd, struct alx_tx_queue *txque)
{
	struct alx_buffer *tpbuf = NULL;

	unsigned int nr_frags = skb_shinfo(skb)->nr_frags;

	unsigned int len = skb_headlen(skb);

	u16 map_len = 0;
	u16 mapped_len = 0;
	u16 hdr_len = 0;
	u16 f;
	u32 tso = stpd->tp_tso.lso;

	if (tso) {
		/* TSO */
		map_len = hdr_len = skb_transport_offset(skb) + tcp_hdrlen(skb);

		tpbuf = GET_TP_BUFFER(txque, txque->tpq.produce_idx);
		tpbuf->length = map_len;
		tpbuf->dma = dma_map_single(txque->dev,
					skb->data, hdr_len, DMA_TO_DEVICE);
		mapped_len += map_len;
		stpd->tp_gnr.addr = tpbuf->dma;
		stpd->tp_gnr.buffer_len = tpbuf->length;

		alx_set_tpdesc(txque, stpd);
	}

	if (mapped_len < len) {
		tpbuf = GET_TP_BUFFER(txque, txque->tpq.produce_idx);
		tpbuf->length = len - mapped_len;
		tpbuf->dma =
			dma_map_single(txque->dev, skb->data + mapped_len,
					tpbuf->length, DMA_TO_DEVICE);
		stpd->tp_gnr.addr = tpbuf->dma;
		stpd->tp_gnr.buffer_len  = tpbuf->length;
		alx_set_tpdesc(txque, stpd);
	}

	for (f = 0; f < nr_frags; f++) {
		struct skb_frag_struct *frag;

		frag = &skb_shinfo(skb)->frags[f];

		tpbuf = GET_TP_BUFFER(txque, txque->tpq.produce_idx);
		tpbuf->length = frag->size;
		tpbuf->dma =
			dma_map_page(txque->dev, frag->page,
					frag->page_offset,
					tpbuf->length,
					DMA_TO_DEVICE);

		stpd->tp_gnr.addr = tpbuf->dma;
		stpd->tp_gnr.buffer_len  = tpbuf->length;

		alx_set_tpdesc(txque, stpd);
	}


	/* The last tpd */
	alx_set_tpdesc_lastfrag(txque);
	/* The last buffer info contain the skb address,
	   so it will be free after unmap */
	tpbuf->skb = skb;
}


netdev_tx_t alx_start_xmit_frames(struct sk_buff *skb,
				  struct alx_adapter *adpt,
				  struct alx_tx_queue *txque)
{
	struct alx_hw *hw = &adpt->hw;
	unsigned long flags = 0;
	struct alx_tpdesc stpd; /* normal*/

	if (test_bit(__ALX_DOWN, &adpt->alx_state)) {
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if (!spin_trylock_irqsave(&adpt->tx_lock, flags)) {
		DRV_PRINT(TX, EMERG, "tx locked!\n");
		return NETDEV_TX_LOCKED;
	}

	if (!alx_check_num_tpdescs(txque, skb)) {
		/* no enough descriptor, just stop queue */
		netif_stop_queue(adpt->netdev);
		spin_unlock_irqrestore(&adpt->tx_lock, flags);
		return NETDEV_TX_BUSY;
	}

	DRV_PRINT(TX, INFO, "Before XMIT: TX[%d]: tpq.consume_idx = 0x%x, "
		  "tpq.produce_idx = 0x%x\n",
		  txque->que_idx, txque->tpq.consume_idx,
		  txque->tpq.produce_idx);
	memset(&stpd, 0, sizeof(struct alx_tpdesc));
	/* do TSO and check sum */
	if (alx_tso_csum(adpt, txque, skb, &stpd) != 0) {
		spin_unlock_irqrestore(&adpt->tx_lock, flags);
		dev_kfree_skb_any(skb);
		return NETDEV_TX_OK;
	}

	if (unlikely(adpt->vlgrp && vlan_tx_tag_present(skb))) {
		u32 vlan = vlan_tx_tag_get(skb);
		stpd.tp_gnr.instag = 0x1;
		stpd.tp_gnr.vlan_tag = vlan;
	}

	if (skb_network_offset(skb) != ETH_HLEN)
		stpd.tp_gnr.type = 0x1; /* Ethernet frame */

	alx_tx_map(adpt, skb, &stpd, txque);


	/* update produce idx */
	wmb();
	MEM_W16(hw, txque->produce_reg, txque->tpq.produce_idx);
	DRV_PRINT(TX, INFO, "TX[%d]: Produce Reg[0x%x] = 0x%x.\n",
		  txque->que_idx, txque->produce_reg,
		  txque->tpq.produce_idx);

	spin_unlock_irqrestore(&adpt->tx_lock, flags);
	return NETDEV_TX_OK;
}

static netdev_tx_t alx_start_xmit(struct sk_buff *skb,
				  struct net_device *netdev)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	struct alx_tx_queue *txque;

	txque = adpt->tx_queue[0];
	return alx_start_xmit_frames(skb, adpt, txque);
}



/*
 * alx_mii_ioctl -
 */
static int alx_mii_ioctl(struct net_device *netdev,
			 struct ifreq *ifr, int cmd)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	struct alx_hw *hw = &adpt->hw;
	struct mii_ioctl_data *data = if_mii(ifr);
	u32 device_type;
	u16 reg_addr;
	int retval = 0;

	DRV_PRINT(FUNC, DEBUG, "ENTER\n");

	if (!netif_running(netdev))
		return -EINVAL;

	switch (cmd) {
	case SIOCGMIIPHY:
		data->phy_id = 0;
		break;

	case SIOCGMIIREG:
		if (!capable(CAP_NET_ADMIN)) {
			retval = -EPERM;
			goto out;
		}

		if (data->reg_num & ~(0x1F)) {
			retval = -EFAULT;
			goto out;
		}
		device_type = ALX_MDIO_NORM_DEV;
		reg_addr = data->reg_num;

		retval = hw->cbs.read_phy_reg(hw, device_type, reg_addr,
					      &data->val_out);
		if (retval) {
			retval = -EIO;
			goto out;
		}
		break;

	case SIOCSMIIREG:
		if (!capable(CAP_NET_ADMIN)) {
			retval = -EPERM;
			goto out;
		}

		if (data->reg_num & ~(0x1F)) {
			retval = -EFAULT;
			goto out;
		}
		device_type = ALX_MDIO_NORM_DEV;
		reg_addr = data->reg_num;

		DRV_PRINT(IOCTL, DEBUG, "write %x %x",
			  data->reg_num, data->val_in);
		retval = hw->cbs.write_phy_reg(hw, device_type, reg_addr,
					       data->val_in);
		if (retval) {
			retval = -EIO;
			goto out;
		}
		break;
	default:
		retval = -EOPNOTSUPP;
		break;
	}
out:
	return retval;

}


/*
 * alx_mac_ioctl -
 */
static int alx_mac_ioctl(struct net_device *netdev,
			 struct ifreq *ifr, int cmd)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	struct alx_hw *hw = &adpt->hw;
	struct mac_ioctl_data *data = (struct mac_ioctl_data *)&ifr->ifr_ifru;
	int retval = 0;

	DRV_PRINT(FUNC, DEBUG, "ENTER\n");

	if (!netif_running(netdev))
		return -EINVAL;

	switch (cmd) {
	case SIOCDEVGMACREG:
		DRV_PRINT(IOCTL, DEBUG, "read mac %x %x",
			  data->reg_num, data->reg_val);
		MEM_R32(hw, data->reg_num, &data->reg_val);
		break;

	case SIOCDEVSMACREG:
		DRV_PRINT(IOCTL, DEBUG, "write mac %x %x",
			  data->reg_num, data->reg_val);
		MEM_W32(hw, data->reg_num, data->reg_val);
		break;
	default:
		retval = -EOPNOTSUPP;
		break;
	}

	return retval;
}

static int alx_ioctl(struct net_device *netdev, struct ifreq *ifr, int cmd)
{
	switch (cmd) {
	case SIOCGMIIPHY:
	case SIOCGMIIREG:
	case SIOCSMIIREG:
		return alx_mii_ioctl(netdev, ifr, cmd);

	case SIOCDEVGMACREG: /* Read MAC Register */
	case SIOCDEVSMACREG: /* Write MAC Register */
		return alx_mac_ioctl(netdev, ifr, cmd);
	default:
		return -EOPNOTSUPP;
	}
}


#ifdef CONFIG_NET_POLL_CONTROLLER
static void alx_poll_controller(struct net_device *netdev)
{
	struct alx_adapter *adpt = netdev_priv(netdev);
	int num_msix_intrs = adpt->num_msix_intrs;
	int msix_idx;

	DRV_PRINT(FUNC, DEBUG, "ENTER\n");

	/* if interface is down do nothing */
	if (test_bit(__ALX_DOWN, &adpt->alx_state))
		return;

	if (CHK_ADPT_FLAG(0, MSIX_EN)) {
		for (msix_idx = 0; msix_idx < num_msix_intrs; msix_idx++) {
			struct alx_msix_param *msix = adpt->msix[msix_idx];
			if (CHK_MSIX_FLAG(RXS) || CHK_MSIX_FLAG(TXS))
				alx_msix_rtx(0, msix);
			else if (CHK_MSIX_FLAG(TIMER))
				alx_msix_timer(0, msix);
			else if (CHK_MSIX_FLAG(ALERT))
				alx_msix_alert(0, msix);
			else if (CHK_MSIX_FLAG(SMB))
				alx_msix_smb(0, msix);
			else if (CHK_MSIX_FLAG(PHY))
				alx_msix_phy(0, msix);
		}
	} else {
		alx_interrupt(adpt->pdev->irq, netdev);
	}
}
#endif

static const struct net_device_ops alx_netdev_ops = {
	.ndo_open               = alx_open,
	.ndo_stop               = alx_stop,
	.ndo_start_xmit         = alx_start_xmit,
	.ndo_get_stats          = alx_get_hw_stats,
	.ndo_set_rx_mode        = alx_set_multicase_list,
	.ndo_validate_addr      = eth_validate_addr,
	.ndo_set_mac_address    = alx_set_mac_addr,
	.ndo_change_mtu         = alx_change_mtu,
	.ndo_do_ioctl           = alx_ioctl,
	.ndo_tx_timeout         = alx_tx_timeout,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller    = alx_poll_controller,
#endif
};


/*
 * alx_init - Device Initialization Routine
 */
int __devinit alx_init(struct pci_dev *pdev,
		       const struct pci_device_id *ent)
{
	struct net_device *netdev;
	struct alx_adapter *adpt = NULL;
	struct alx_hw *hw = NULL;
	static int cards_found;
	int retval;

	/* enable device (incl. PCI PM wakeup and hotplug setup) */
	retval = pci_enable_device_mem(pdev);
	if (retval) {
		dev_err(&pdev->dev, "cannot enable PCI device\n");
		goto err_alloc_device;
	}

	/*
	 * The alx chip can DMA to 64-bit addresses, but it uses a single
	 * shared register for the high 32 bits, so only a single, aligned,
	 * 4 GB physical address range can be used at a time.
	 */
	if (!dma_set_mask(&pdev->dev, DMA_BIT_MASK(64)) &&
	    !dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(64))) {
		dev_info(&pdev->dev, "DMA to 64-BIT addresses.\n");
	} else {
		retval = dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));
		if (retval) {
			retval = dma_set_coherent_mask(&pdev->dev,
						       DMA_BIT_MASK(32));
			if (retval) {
				dev_err(&pdev->dev, "No usable DMA "
					"configuration, aborting\n");
				goto err_alloc_pci_res;
			}
		}
	}

	retval = pci_request_selected_regions(pdev, pci_select_bars(pdev,
					IORESOURCE_MEM), alx_drv_name);
	if (retval) {
		dev_err(&pdev->dev,
			"pci_request_selected_regions failed 0x%x\n", retval);
		goto err_alloc_pci_res;
	}


	pci_enable_pcie_error_reporting(pdev);
	pci_set_master(pdev);

	netdev = alloc_etherdev(sizeof(struct alx_adapter));
	if (netdev == NULL) {
		dev_err(&pdev->dev, "etherdev alloc failed\n");
		retval = -ENOMEM;
		goto err_alloc_netdev;
	}

	SET_NETDEV_DEV(netdev, &pdev->dev);
	netdev->irq  = pdev->irq;

	adpt = netdev_priv(netdev);
	pci_set_drvdata(pdev, adpt);
	adpt->netdev = netdev;
	adpt->pdev = pdev;
	hw = &adpt->hw;
	hw->adpt = adpt;
	adpt->msg_flags = ALX_MSG_DEFAULT;

	adpt->hw.hw_addr = ioremap(pci_resource_start(pdev, BAR_0),
				   pci_resource_len(pdev, BAR_0));
	if (!adpt->hw.hw_addr) {
		DRV_PRINT(INIT, ERR, "cannot map device registers\n");
		retval = -EIO;
		goto err_iomap;
	}
	netdev->base_addr = (unsigned long)adpt->hw.hw_addr;

	/* set cb member of netdev structure*/
	netdev->netdev_ops = &alx_netdev_ops;
	alx_set_ethtool_ops(netdev);
	netdev->watchdog_timeo = ALX_WATCHDOG_TIME;
	strncpy(netdev->name, pci_name(pdev), sizeof(netdev->name) - 1);

	adpt->bd_number = cards_found;

	/* init alx_adapte structure */
	retval = alx_init_adapter(adpt);
	if (retval) {
		DRV_PRINT(INIT, ERR, "net device private data init failed\n");
		goto err_init_adapter;
	}

	/* 1. reset pcie */
	retval = hw->cbs.reset_pcie(hw, true, true);
	if (retval) {
		DRV_PRINT(INIT, ERR, "PCIE Reset failed (%d).\n", retval);
		retval = -EIO;
		goto err_init_adapter;
	}

	/* 2. Init GPHY as early as possible due to power saving issue  */
	retval = hw->cbs.reset_phy(hw);
	if (retval) {
		DRV_PRINT(INIT, ERR, "PHY Reset failed (%d).\n", retval);
		retval = -EIO;
		goto err_init_adapter;
	}

	/* 3. reset mac */
	retval = hw->cbs.reset_mac(hw);
	if (retval) {
		DRV_PRINT(INIT, ERR, "MAC Reset failed (%d).\n", retval);
		retval = -EIO;
		goto err_init_adapter;
	}

	/* 4. setup link to put it in a known good starting state */
	retval = hw->cbs.setup_phy_link(hw, hw->autoneg_advertised, true,
					!hw->disable_fc_autoneg);

	/* 5. get mac addr and perm mac addr, set to register */
	if (hw->cbs.get_mac_addr) {
		retval = hw->cbs.get_mac_addr(hw, hw->mac_perm_addr);
		retval = hw->cbs.get_mac_addr(hw, hw->mac_addr);
	}
	if (hw->cbs.set_mac_addr)
		retval = hw->cbs.set_mac_addr(hw, hw->mac_addr);

	/* 6. get user settings */
	adpt->num_txdescs = 1024;
	adpt->num_rxdescs = 512;
	adpt->max_rxques = min(ALX_MAX_RX_QUEUES, (int)num_online_cpus());
	adpt->max_txques = min(ALX_MAX_TX_QUEUES, (int)num_online_cpus());


	netdev->features = NETIF_F_SG |
			   NETIF_F_HW_CSUM |
			   NETIF_F_HW_VLAN_TX |
			   NETIF_F_HW_VLAN_RX;
	netdev->features |= NETIF_F_TSO;
	netdev->features |= NETIF_F_TSO6;


	memcpy(netdev->dev_addr, hw->mac_perm_addr, netdev->addr_len);
	memcpy(netdev->perm_addr, hw->mac_perm_addr, netdev->addr_len);
	if (alx_validate_mac_addr(netdev->perm_addr)) {
		DRV_PRINT(INIT, INFO, "invalid MAC address\n");
		retval = -EIO;
		goto err_init_adapter;
	}

	setup_timer(&adpt->alx_timer, &alx_timer_routine,
		    (unsigned long)adpt);
	INIT_WORK(&adpt->alx_task, alx_task_routine);

	/* Number of supported queues */
	alx_set_num_queues(adpt);
	retval = alx_set_interrupt_mode(adpt);
	if (retval) {
		DRV_PRINT(INIT, ERR, "can't set interrupt mode.\n");
		goto err_set_interrupt_mode;
	}

	retval = alx_set_interrupt_param(adpt);
	if (retval) {
		DRV_PRINT(INIT, ERR, "can't set interrupt parameter.\n");
		goto err_set_interrupt_param;
	}

	retval = alx_alloc_all_rtx_queue(adpt);
	if (retval) {
		DRV_PRINT(INIT, ERR, "can't allocate memory for queues\n");
		goto err_alloc_rtx_queue;
	}

	alx_set_register_info_special(adpt);

	DRV_PRINT(PCI, INFO, "num_msix_noque_intrs = %d, "
		  "num_msix_rxque_intrs = %d, "
		  "num_msix_txque_intrs = %d.\n",
		  adpt->num_msix_noques,
		  adpt->num_msix_rxques,
		  adpt->num_msix_txques);
	DRV_PRINT(PCI, INFO, "num_msix_all_intrs = %d.\n",
		  adpt->num_msix_intrs);

	/* WOL not supported for all but the following */
	switch (hw->pci_devid) {
	case ALX_DEV_ID_AR8131:
	case ALX_DEV_ID_AR8132:
	case ALX_DEV_ID_AR8151_V1:
	case ALX_DEV_ID_AR8151_V2:
	case ALX_DEV_ID_AR8152_V1:
	case ALX_DEV_ID_AR8152_V2:
		adpt->wol = (ALX_WOL_MAGIC | ALX_WOL_PHY);
		break;
	case ALX_DEV_ID_AR8161:
	case ALX_DEV_ID_AR8162:
		adpt->wol = (ALX_WOL_MAGIC | ALX_WOL_PHY);
		break;
	default:
		adpt->wol = 0;
		break;
	}
	device_set_wakeup_enable(&adpt->pdev->dev, adpt->wol);

	set_bit(__ALX_DOWN, &adpt->alx_state);
	strcpy(netdev->name, "eth%d");
	retval = register_netdev(netdev);
	if (retval) {
		DRV_PRINT(INIT, ERR, "register netdevice failed\n");
		goto err_register_netdev;
	}
	adpt->netdev_registered = true;

	/* carrier off reporting is important to ethtool even BEFORE open */
	netif_carrier_off(netdev);
	/* keep stopping all the transmit queues for older kernels */
	netif_tx_stop_all_queues(netdev);

	/* print the MAC address */
	DRV_PRINT(INIT, INFO, "%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
		  netdev->dev_addr[0], netdev->dev_addr[1],
		  netdev->dev_addr[2], netdev->dev_addr[3],
		  netdev->dev_addr[4], netdev->dev_addr[5]);

	DRV_PRINT(INIT, INFO, "RX Queue Count = %u, HRX Queue Count = %u, "
		  "SRX Queue Count = %u, TX Queue Count = %u\n",
		  adpt->num_rxques, adpt->num_hw_rxques, adpt->num_sw_rxques,
		  adpt->num_txques);

	/* print the adapter capability */
	if (CHK_ADPT_FLAG(0, MSI_CAP))
		DRV_PRINT(INIT, INFO, "MSI Capable: %s.\n",
			  CHK_ADPT_FLAG(0, MSI_EN) ? "Enable" : "Disable");
	if (CHK_ADPT_FLAG(0, MSIX_CAP))
		DRV_PRINT(INIT, INFO, "MSIX Capable: %s.\n",
			  CHK_ADPT_FLAG(0, MSIX_EN) ? "Enable" : "Disable");
	if (CHK_ADPT_FLAG(0, MRQ_CAP))
		DRV_PRINT(INIT, INFO, "MRQ Capable: %s.\n",
			  CHK_ADPT_FLAG(0, MRQ_EN) ? "Enable" : "Disable");
	if (CHK_ADPT_FLAG(0, MRQ_CAP))
		DRV_PRINT(INIT, INFO, "MTQ Capable: %s.\n",
			  CHK_ADPT_FLAG(0, MTQ_EN) ? "Enable" : "Disable");
	if (CHK_ADPT_FLAG(0, SRSS_CAP))
		DRV_PRINT(INIT, INFO, "RSS(SW) Capable: %s.\n",
			  CHK_ADPT_FLAG(0, SRSS_EN) ? "Enable" : "Disable");

	DRV_PRINT(INIT, INFO, "Atheros Gigabit Network Connection\n");
	cards_found++;
	return 0;

err_register_netdev:
	alx_free_all_rtx_queue(adpt);
err_alloc_rtx_queue:
	alx_reset_interrupt_param(adpt);
err_set_interrupt_param:
	alx_reset_interrupt_mode(adpt);
err_set_interrupt_mode:
err_init_adapter:
	iounmap(adpt->hw.hw_addr);
err_iomap:
	free_netdev(netdev);
err_alloc_netdev:
	pci_release_selected_regions(pdev,
				     pci_select_bars(pdev, IORESOURCE_MEM));
err_alloc_pci_res:
	pci_disable_device(pdev);
err_alloc_device:
	DRV_PRINT(INIT, INFO, "Error when probe device(%d).\n", retval);
	return retval;
}

/*
 * alx_remove - Device Removal Routine
 * @pdev: PCI device information struct
 */
static void __devexit alx_remove(struct pci_dev *pdev)
{
	struct alx_adapter *adpt = pci_get_drvdata(pdev);
	struct alx_hw *hw = &adpt->hw;
	struct net_device *netdev = adpt->netdev;
	int que_idx;

	set_bit(__ALX_DOWN, &adpt->alx_state);
	cancel_work_sync(&adpt->alx_task);

	hw->cbs.config_pow_save(hw, ALX_LINK_SPEED_UNKNOWN,
				false, false, false, false);

	if (adpt->netdev_registered) {
		unregister_netdev(netdev);
		adpt->netdev_registered = false;
	}

	for (que_idx = 0; que_idx < adpt->num_txques; que_idx++) {
		kfree(adpt->tx_queue[que_idx]);
		adpt->tx_queue[que_idx] = NULL;
	}
	for (que_idx = 0; que_idx < adpt->num_rxques; que_idx++) {
		kfree(adpt->rx_queue[que_idx]);
		adpt->rx_queue[que_idx] = NULL;
	}
	alx_reset_interrupt_param(adpt);
	alx_reset_interrupt_mode(adpt);

	iounmap(adpt->hw.hw_addr);
	pci_release_selected_regions(pdev,
				     pci_select_bars(pdev, IORESOURCE_MEM));

	DRV_PRINT(INIT, INFO, "complete\n");
	free_netdev(netdev);

	pci_disable_pcie_error_reporting(pdev);

	pci_disable_device(pdev);
}


/*
 * alx_pci_error_detected - called when PCI error is detected
 */
static pci_ers_result_t alx_pci_error_detected(struct pci_dev *pdev,
					       pci_channel_state_t state)
{
	struct alx_adapter *adpt = pci_get_drvdata(pdev);
	struct net_device *netdev = adpt->netdev;

	netif_device_detach(netdev);

	if (state == pci_channel_io_perm_failure)
		return PCI_ERS_RESULT_DISCONNECT;

	if (netif_running(netdev))
		alx_stop_internal(adpt, ALX_OPEN_CTRL_MAC_EN);
	pci_disable_device(pdev);

	/* Request a slot reset. */
	return PCI_ERS_RESULT_NEED_RESET;
}

/*
 * alx_pci_error_slot_reset - called after the pci bus has been reset.
 */
static pci_ers_result_t alx_pci_error_slot_reset(struct pci_dev *pdev)
{
	struct alx_adapter *adpt = pci_get_drvdata(pdev);
	pci_ers_result_t result;

	if (pci_enable_device_mem(pdev)) {
		DRV_PRINT(INIT, ERR,
			"Cannot re-enable PCI device after reset.\n");
		result =  PCI_ERS_RESULT_DISCONNECT;
	} else {
		pci_set_master(pdev);

		pci_enable_wake(pdev, PCI_D3hot, 0);
		pci_enable_wake(pdev, PCI_D3cold, 0);

		adpt->hw.cbs.reset_mac(&adpt->hw);

		result = PCI_ERS_RESULT_RECOVERED;
	}

	pci_cleanup_aer_uncorrect_error_status(pdev);

	return result;
}

/*
 * alx_pci_error_resume
 */
static void alx_pci_error_resume(struct pci_dev *pdev)
{
	struct alx_adapter *adpt = pci_get_drvdata(pdev);
	struct net_device *netdev = adpt->netdev;

	if (netif_running(netdev)) {
		if (alx_open_internal(adpt, 0))
			return;
	}

	netif_device_attach(netdev);
}

static struct pci_error_handlers alx_err_handler = {
	.error_detected = alx_pci_error_detected,
	.slot_reset     = alx_pci_error_slot_reset,
	.resume         = alx_pci_error_resume,
};


static struct pci_driver alx_driver = {
	.name     = alx_drv_name,
	.id_table = alx_pci_tbl,
	.probe    = alx_init,
	.remove   = __devexit_p(alx_remove),
#ifdef CONFIG_PM
	.suspend  = alx_suspend,
	.resume   = alx_resume,
#endif
	.shutdown = alx_shutdown,
	.err_handler = &alx_err_handler
};


static int __init alx_init_module(void)
{
	int retval;
	printk(KERN_INFO "%s - version %s\n",
			alx_drv_description, alx_drv_version);
	printk(KERN_INFO "%s\n", alx_copyright);
	retval = pci_register_driver(&alx_driver);

	return retval;
}
module_init(alx_init_module);



static void __exit alx_exit_module(void)
{
	pci_unregister_driver(&alx_driver);
}


module_exit(alx_exit_module);
