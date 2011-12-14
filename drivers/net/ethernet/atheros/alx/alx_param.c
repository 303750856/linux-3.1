/*
 * Copyright(c) 1999 - 2010 Intel Corporation.
 * Copyright(c) 2007 - 2011 Atheros Corporation. All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.*
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * The full GNU General Public License is included in this distribution in
 * the file called "COPYING".
 */

#include "alx.h"
#include "alx_hwcom.h"

/* This is the only thing that needs to be changed to adjust the
 * maximum number of ports that the driver can manage.
 */
#define ALX_MAX_NIC	 32

#define OPTION_UNSET	-1
#define OPTION_DIS	 0
#define OPTION_EN	 1

/* All parameters are treated the same, as an integer array of values.
 * This macro just reduces the need to repeat the same declaration code
 * over and over (plus this helps to avoid typo bugs).
 */

#define ALX_PARAM_INIT { [0 ... ALX_MAX_NIC] = OPTION_UNSET }
#ifndef module_param_array
/* Module Parameters are always initialized to -1, so that the driver
 * can tell the difference between no user specified value or the
 * user asking for the default value.
 * The true default values are loaded in when alx_check_options is called.
 *
 * This is a GCC extension to ANSI C.
 * See the item "Labeled Elements in Initializers" in the section
 * "Extensions to the C Language Family" of the GCC documentation.
 */

#define ALX_PARAM(X, desc) \
	static const int __devinitdata X[ALX_MAX_NIC+1] = ALX_PARAM_INIT; \
	MODULE_PARM(X, "1-" __MODULE_STRING(ALX_MAX_NIC) "i"); \
	MODULE_PARM_DESC(X, desc);
#else
#define ALX_PARAM(X, desc) \
	static int __devinitdata X[ALX_MAX_NIC+1] = ALX_PARAM_INIT; \
	static unsigned int num_##X; \
	module_param_array_named(X, X, int, &num_##X, 0); \
	MODULE_PARM_DESC(X, desc);
#endif


/*
 * Transmit description count
 * Valid Range: 32-1024
 * Default Value: 1024
 */
ALX_PARAM(txd_cnt, "Transmit description count");
#define ALX_MIN_TXD	32
#define ALX_MAX_TXD	1024
#define ALX_DEF_TXD	1024

/*
 * Receive description count
 * Valid Range: 64-1024
 * Default Value: 512
 */
ALX_PARAM(rxd_cnt, "Receive description count");
#define ALX_MIN_RXD	128
#define ALX_MAX_RXD	1024
#define ALX_DEF_RXD	512

/*
 * Interrupt Moderate Timer in units of 2 us
 * Valid Range: 50-65535
 * Default Value: 100(200us)
 */
ALX_PARAM(imt, "Interrupt Moderator Timer ");
#define ALX_MIN_IMT	50
#define ALX_MAX_IMT	65000
#define ALX_DEF_IMT	100


struct alx_setting {
	enum { enable_setting, range_setting, list_setting } type;
	const char *name;
	const char *err;
	int  def;
	union {
		struct { /* range_setting info */
			int min;
			int max;
		} r;
		struct { /* list_setting info */
			int nr;
			struct alx_setting_list {
				int i;
				char *str;
			} *p;
		} l;
	} arg;
};

static int __devinit alx_validate_setting(unsigned int *value,
			struct alx_setting *set)
{
	if (*value == OPTION_UNSET) {
		*value = set->def;
		return 0;
	}

	switch (set->type) {
	case enable_setting:
		switch (*value) {
		case OPTION_EN:
			printk(KERN_INFO "alx: %s Enabled\n", set->name);
			return 0;
		case OPTION_DIS:
			printk(KERN_INFO "alx: %s Disabled\n", set->name);
			return 0;
		}
		break;
	case range_setting:
		if (*value >= set->arg.r.min && *value <= set->arg.r.max) {
			printk(KERN_INFO "alx: %s set to %i\n",
				set->name, *value);
			return 0;
		}
		break;
	case list_setting:{
		int i;
		struct alx_setting_list *ent;

		for (i = 0; i < set->arg.l.nr; i++) {
			ent = &set->arg.l.p[i];
			if (*value == ent->i) {
				if (ent->str[0] != '\0')
					printk(KERN_INFO "alx: %s\n", ent->str);
				return 0;
			}
		}
	}
		break;
	default:
		BUG();
	}

	printk(KERN_INFO "alx: Invalid %s specified (%i) %s\n",
			set->name, *value, set->err);
	*value = set->def;
	return -1;
}

/*
 * alx_get_user_settings - Get user settings for command line parameters
 * @adpt: board private structure
 */
void __devinit alx_get_user_settings(struct alx_adapter *adpt)
{
	int bd = adpt->bd_number;
	if (bd >= ALX_MAX_NIC) {
		printk(KERN_NOTICE
		       "Warning: no configuration for board #%d\n", bd);
		printk(KERN_NOTICE "Using defaults for all values\n");
#ifndef module_param_array
		bd = ALX_MAX_NIC;
#endif
	}

	{ /* Transmit Ring Size */
		struct alx_setting set = {
			.type = range_setting,
			.name = "Transmit Ddescription Count",
			.err  = "using default of "
				__MODULE_STRING(ALX_DEF_TXD),
			.def  = ALX_DEF_TXD,
			.arg  = { .r = { .min = ALX_MIN_TXD,
					.max = ALX_MAX_TXD} }
		};
		int txd_count = set.def;

#ifdef module_param_array
		if (num_txd_cnt > bd) {
#endif
			txd_count = txd_cnt[bd];
			alx_validate_setting(&txd_count, &set);
			adpt->num_txdescs = (u32)txd_count & 0xFFFC;
#ifdef module_param_array
		} else {
			adpt->num_txdescs = set.def;
		}
#endif
	}

	{ /* Receive Ring Size */
		struct alx_setting set = {
			.type = range_setting,
			.name = "Receive Ddescription Count",
			.err  = "using default of "
				__MODULE_STRING(ALX_DEF_RXD),
			.def  = ALX_DEF_RXD,
			.arg  = { .r = { .min = ALX_MIN_RXD,
					.max = ALX_MAX_RXD} }
		};
		int rx_count = set.def;

#ifdef module_param_array
		if (num_rxd_cnt > bd) {
#endif
			rx_count = rxd_cnt[bd];
			alx_validate_setting(&rx_count, &set);
			adpt->num_rxdescs = (u32)rx_count;
#ifdef module_param_array
		} else {
			adpt->num_rxdescs = set.def;
		}
#endif
	}

	{ /* Interrupt Moderate Timer */
		struct alx_setting set = {
			.type = range_setting,
			.name = "Interrupt Moderate Timer",
			.err  = "using default of "
				__MODULE_STRING(ALX_DEF_IMT),
			.def  = ALX_DEF_IMT,
			.arg  = { .r = { .min = ALX_MIN_IMT,
					 .max = ALX_MAX_IMT} }
		} ;
		int intr_mod = set.def;

#ifdef module_param_array
		if (num_imt > bd) {
#endif
			intr_mod = imt[bd];
			alx_validate_setting(&intr_mod, &set);
			adpt->hw.imt = (u16) intr_mod;
#ifdef module_param_array
		} else {
			adpt->hw.imt = set.def;
		}
#endif

	}

	alx_netif_info(adpt, HW, "User setting - num_rxdescs = %d.\n",
		       adpt->num_rxdescs);
	alx_netif_info(adpt, HW, "User setting - num_txdescs = %d.\n",
		       adpt->num_txdescs);
	alx_netif_info(adpt, HW, "User setting - hw_imt = %d.\n",
		       adpt->hw.imt);
}
