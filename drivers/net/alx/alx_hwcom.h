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

#ifndef _LX_HWCOMMON_H_
#define _LX_HWCOMMON_H_

#define PHY_TYPE_ASIC   0
#define PHY_TYPE_FPGA   1
#define PHY_TYPE_F1 2

#define MAC_TYPE_ASIC   0
#define MAC_TYPE_FPGA   1


#include "alx_sw.h"


#define BIT_31      (1UL << 31)
#define BIT_30      (1L << 30)
#define BIT_29      (1L << 29)
#define BIT_28      (1L << 28)
#define BIT_27      (1L << 27)
#define BIT_26      (1L << 26)
#define BIT_25      (1L << 25)
#define BIT_24      (1L << 24)
#define BIT_23      (1L << 23)
#define BIT_22      (1L << 22)
#define BIT_21      (1L << 21)
#define BIT_20      (1L << 20)
#define BIT_19      (1L << 19)
#define BIT_18      (1L << 18)
#define BIT_17      (1L << 17)
#define BIT_16      (1L << 16)
#define BIT_15      (1L << 15)
#define BIT_14      (1L << 14)
#define BIT_13      (1L << 13)
#define BIT_12      (1L << 12)
#define BIT_11      (1L << 11)
#define BIT_10      (1L << 10)
#define BIT_9       (1L << 9)
#define BIT_8       (1L << 8)
#define BIT_7       (1L << 7)
#define BIT_6       (1L << 6)
#define BIT_5       (1L << 5)
#define BIT_4       (1L << 4)
#define BIT_3       (1L << 3)
#define BIT_2       (1L << 2)
#define BIT_1       (1L << 1)
#define BIT_0       1L

#define BIT_15S     (1U << 15)
#define BIT_14S     (1 << 14)
#define BIT_13S     (1 << 13)
#define BIT_12S     (1 << 12)
#define BIT_11S     (1 << 11)
#define BIT_10S     (1 << 10)
#define BIT_9S      (1 << 9)
#define BIT_8S      (1 << 8)
#define BIT_7S      (1 << 7)
#define BIT_6S      (1 << 6)
#define BIT_5S      (1 << 5)
#define BIT_4S      (1 << 4)
#define BIT_3S      (1 << 3)
#define BIT_2S      (1 << 2)
#define BIT_1S      (1 << 1)
#define BIT_0S      1

#define SHIFT31(x)  ((x) << 31)
#define SHIFT30(x)  ((x) << 30)
#define SHIFT29(x)  ((x) << 29)
#define SHIFT28(x)  ((x) << 28)
#define SHIFT27(x)  ((x) << 27)
#define SHIFT26(x)  ((x) << 26)
#define SHIFT25(x)  ((x) << 25)
#define SHIFT24(x)  ((x) << 24)
#define SHIFT23(x)  ((x) << 23)
#define SHIFT22(x)  ((x) << 22)
#define SHIFT21(x)  ((x) << 21)
#define SHIFT20(x)  ((x) << 20)
#define SHIFT19(x)  ((x) << 19)
#define SHIFT18(x)  ((x) << 18)
#define SHIFT17(x)  ((x) << 17)
#define SHIFT16(x)  ((x) << 16)
#define SHIFT15(x)  ((x) << 15)
#define SHIFT14(x)  ((x) << 14)
#define SHIFT13(x)  ((x) << 13)
#define SHIFT12(x)  ((x) << 12)
#define SHIFT11(x)  ((x) << 11)
#define SHIFT10(x)  ((x) << 10)
#define SHIFT9(x)   ((x) << 9)
#define SHIFT8(x)   ((x) << 8)
#define SHIFT7(x)   ((x) << 7)
#define SHIFT6(x)   ((x) << 6)
#define SHIFT5(x)   ((x) << 5)
#define SHIFT4(x)   ((x) << 4)
#define SHIFT3(x)   ((x) << 3)
#define SHIFT2(x)   ((x) << 2)
#define SHIFT1(x)   ((x) << 1)
#define SHIFT0(x)   ((x) << 0)

#define ALL_32_BITS 0xffffffffUL

#define FIELD_GETX(_x, _name)   (((_x) & (_name##_MASK)) >> (_name##_SHIFT))
#define FIELD_SETS(_x, _name, _v)   (\
(_x) =                               \
((_x) & ~(_name##_MASK))            |\
(((u16)(_v) << (_name##_SHIFT)) & (_name##_MASK)))
#define FIELD_SETL(_x, _name, _v)   (\
(_x) =                               \
((_x) & ~(_name##_MASK))            |\
(((u32)(_v) << (_name##_SHIFT)) & (_name##_MASK)))
#define FIELDL(_name, _v) (((u32)(_v) << (_name##_SHIFT)) & (_name##_MASK))
#define FIELDS(_name, _v) (((u16)(_v) << (_name##_SHIFT)) & (_name##_MASK))



#define LX_SWAP_DW(_x) (\
	(((_x) << 24) & 0xFF000000UL) |\
	(((_x) <<  8) & 0x00FF0000UL) |\
	(((_x) >>  8) & 0x0000FF00UL) |\
	(((_x) >> 24) & 0x000000FFUL))

#define LX_SWAP_W(_x) (\
	(((_x) >> 8) & 0x00FFU) |\
	(((_x) << 8) & 0xFF00U))


#define LX_ERR_SUCCESS          0x0000
#define LX_ERR_ALOAD            0x0001
#define LX_ERR_RSTMAC           0x0002
#define LX_ERR_PARM             0x0003
#define LX_ERR_MIIBUSY          0x0004

/* link capability */
#define LX_LC_10H               0x01
#define LX_LC_10F               0x02
#define LX_LC_100H              0x04
#define LX_LC_100F              0x08
#define LX_LC_1000F             0x10
#define LX_LC_ALL               \
	(LX_LC_10H|LX_LC_10F|LX_LC_100H|LX_LC_100F|LX_LC_1000F)

/* options for MAC contrl */
#define LX_MACSPEED_1000        BIT_0S  /* 1:1000M, 0:10/100M */
#define LX_MACDUPLEX_FULL       BIT_1S  /* 1:full, 0:half */
#define LX_FLT_BROADCAST        BIT_2S  /* 1:enable rx-broadcast */
#define LX_FLT_MULTI_ALL        BIT_3S
#define LX_FLT_DIRECT           BIT_4S
#define LX_FLT_PROMISC          BIT_5S
#define LX_FC_TXEN              BIT_6S
#define LX_FC_RXEN              BIT_7S
#define LX_VLAN_STRIP           BIT_8S
#define LX_LOOPBACK             BIT_9S
#define LX_ADD_FCS              BIT_10S
#define LX_SINGLE_PAUSE         BIT_11S


/* interop between drivers */
#define LX_DRV_TYPE_MASK                SHIFT27(0x1FUL)
#define LX_DRV_TYPE_SHIFT               27
#define LX_DRV_TYPE_UNKNOWN             0
#define LX_DRV_TYPE_BIOS                1
#define LX_DRV_TYPE_BTROM               2
#define LX_DRV_TYPE_PKT                 3
#define LX_DRV_TYPE_NDS2                4
#define LX_DRV_TYPE_UEFI                5
#define LX_DRV_TYPE_NDS5                6
#define LX_DRV_TYPE_NDS62               7
#define LX_DRV_TYPE_NDS63               8
#define LX_DRV_TYPE_LNX                 9
#define LX_DRV_TYPE_ODI16               10
#define LX_DRV_TYPE_ODI32               11
#define LX_DRV_TYPE_FRBSD               12
#define LX_DRV_TYPE_NTBSD               13
#define LX_DRV_TYPE_WCE                 14
#define LX_DRV_PHY_AUTO                 BIT_26  /* 1:auto, 0:force */
#define LX_DRV_PHY_1000                 BIT_25
#define LX_DRV_PHY_100                  BIT_24
#define LX_DRV_PHY_10                   BIT_23
#define LX_DRV_PHY_DUPLEX               BIT_22  /* 1:full, 0:half */
#define LX_DRV_PHY_FC                   BIT_21  /* 1:en flow control */
#define LX_DRV_PHY_MASK                 SHIFT21(0x1FUL)
#define LX_DRV_PHY_SHIFT                21
#define LX_DRV_PHY_UNKNOWN              0
#define LX_DRV_DISABLE                  BIT_18
#define LX_DRV_WOLS5_EN                 BIT_17
#define LX_DRV_WOLS5_BIOS_EN            BIT_16
#define LX_DRV_AZ_EN                    BIT_12
#define LX_DRV_WOLPATTERN_EN            BIT_11
#define LX_DRV_WOLLINKUP_EN             BIT_10
#define LX_DRV_WOLMAGIC_EN              BIT_9
#define LX_DRV_WOLCAP_BIOS_EN           BIT_8
#define LX_DRV_ASPM_SPD1000LMT_MASK     SHIFT4(3UL)
#define LX_DRV_ASPM_SPD1000LMT_SHIFT    4
#define LX_DRV_ASPM_SPD1000LMT_100M     0
#define LX_DRV_ASPM_SPD1000LMT_NO       1
#define LX_DRV_ASPM_SPD1000LMT_1M       2
#define LX_DRV_ASPM_SPD1000LMT_10M      3
#define LX_DRV_ASPM_SPD100LMT_MASK      SHIFT2(3UL)
#define LX_DRV_ASPM_SPD100LMT_SHIFT     2
#define LX_DRV_ASPM_SPD100LMT_1M        0
#define LX_DRV_ASPM_SPD100LMT_10M       1
#define LX_DRV_ASPM_SPD100LMT_100M      2
#define LX_DRV_ASPM_SPD100LMT_NO        3
#define LX_DRV_ASPM_SPD10LMT_MASK       SHIFT0(3UL)
#define LX_DRV_ASPM_SPD10LMT_SHIFT      0
#define LX_DRV_ASPM_SPD10LMT_1M         0
#define LX_DRV_ASPM_SPD10LMT_10M        1
#define LX_DRV_ASPM_SPD10LMT_100M       2
#define LX_DRV_ASPM_SPD10LMT_NO         3

/* flag of phy inited */
#define LX_PHY_INITED           0x003F

/* check if the mac address is valid */
#define macaddr_valid(_addr) (\
	0 == ((*(u8 *)(_addr))&1) && \
	!(0 == *(u32 *)(_addr) && 0 == *((u16 *)(_addr)+2)))

#endif/*_LX_HWCOMMON_H_*/

