/* 
 *  Acer WMI Laptop Extras
 *
 *  Copyright (C) 2007-2009	Carlos Corbacho <carlos@strangeworlds.co.uk>
 *
 *  Based on acer_acpi:
 *    Copyright (C) 2005-2007	E.M. Smith
 *    Copyright (C) 2007-2008	Carlos Corbacho <cathectic@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/dmi.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include <linux/i8042.h>
#include <linux/rfkill.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>

#include <acpi/acpi_drivers.h>
#define ACER_WMI_DEBUG 0
/********tomsun 2010-02-06*********/
#ifndef NETLINK_ACER
	#define NETLINK_ACER 20
#endif
#include  <net/sock.h>
#include  <linux/netlink.h>
#include  <linux/proc_fs.h>
#include  <linux/list.h>
#include  <linux/skbuff.h>
#include  <linux/moduleparam.h>
#include  <linux/connector.h>
#include  <linux/mutex.h>
#include  <linux/spinlock.h>
#include  <linux/kobject.h>
#include  <linux/sysfs.h>

/***************end****************/


MODULE_AUTHOR("Carlos Corbacho");
MODULE_DESCRIPTION("Acer Laptop WMI Extras Driver");
MODULE_LICENSE("GPL");

#define ACER_LOGPREFIX "acer-wmi: "
#define ACER_ERR KERN_ERR ACER_LOGPREFIX
#define ACER_NOTICE KERN_NOTICE ACER_LOGPREFIX
#define ACER_INFO KERN_INFO ACER_LOGPREFIX

/*
 * The following defines quirks to get some specific functions to work
 * which are known to not be supported over ACPI-WMI (such as the mail LED
 * on WMID based Acer's)
 */
/******tomsun 2010-02-06*********/
#define MAX_PAYLOAD 1024
static struct sock *acer_sock_user;
//static DEFINE_SPINLOCK (acer_lock);
/***************end*************/
struct acer_quirks {
	const char *vendor;
	const char *model;
	u16 quirks;
};

/*
 * Magic Number
 * Meaning is unknown - this number is required for writing to ACPI for AMW0
 * (it's also used in acerhk when directly accessing the BIOS)
 */
#define ACER_AMW0_WRITE	0x9610

/*
 * Bit masks for the AMW0 interface
 */
#define ACER_AMW0_WIRELESS_MASK  0x35
#define ACER_AMW0_BLUETOOTH_MASK 0x34
#define ACER_AMW0_MAILLED_MASK   0x31

/*
 * Method IDs for WMID interface
 */
#define ACER_WMID_GET_WIRELESS_METHODID		1
#define ACER_WMID_GET_BLUETOOTH_METHODID	2
#define ACER_WMID_GET_BRIGHTNESS_METHODID	3
#define ACER_WMID_SET_WIRELESS_METHODID		4
#define ACER_WMID_SET_BLUETOOTH_METHODID	5
#define ACER_WMID_SET_BRIGHTNESS_METHODID	6
#define ACER_WMID_GET_THREEG_METHODID		10
#define ACER_WMID_SET_THREEG_METHODID		11

/*
 * Method IDS for WMID_GUID4 function provider
 * @leon added
 */
#define ACER_WMID_HOTKEY_SET_FUNCTION_METHODID	1
#define ACER_WMID_HOTKEY_GET_FUNCTION_METHODID	2

/*
 * Error Code
 * @leon added
 */
#define ACER_ERROR_CODE_NO_ERROR		0x00
#define ACER_ERROR_CODE_FUNCTION_NOT_SUPPORT	0xE1
#define ACER_ERROR_CODE_UNDEFINED_DEVICE	0xE2
#define ACER_ERROR_CODE_EC_NO_RESPONSE		0xE3
#define ACER_ERROR_CODE_INVALID_PARAMETER	0xE4

/*
 * EC Return Value
 * @leon added
 */
#define ACER_EC_RETURN_VALUE_NO_ERROR		0x00


/*
 * Acer ACPI method GUIDs
 */
#define AMW0_GUID1		"67C3371D-95A3-4C37-BB61-DD47B491DAAB"
#define AMW0_GUID2		"431F16ED-0C2B-444C-B267-27DEB140CF9C"

// GUID 6AF4F258-B401-42fd-BE91-3D4AC2D7C0D3
// ACPI Method for Device Control
// 
// 0x58,0xF2,0xF4,0x6A,0x01,0xB4,0xfd,0x42,0xBE,0x91,0x3D,0x4A,0xC2,0xD7,0xC0,0xD3
// 66, 65					// Object ID BA
// 1,						// Instance count
// 0x02						// Flags (Data)
#define WMID_GUID1		"6AF4F258-B401-42fd-BE91-3D4AC2D7C0D3"	

// GUID 95764E09-FB56-4e83-B31A-37761F60994A
// ACPI Data Block for EC Device
//
// 0x09,0x4E,0x76,0x96,0x56,0xFB,0x83,0x4e,0xB3,0x1A,0x37,0x76,0x1F,0x60,0x99,0x4A
// 65, 65					// Object ID AA
// 1						// Instance count
// 0x01						// Flags (Data)
#define WMID_GUID2		"95764E09-FB56-4e83-B31A-37761F60994A"

// GUID 676AA15E-6A47-4D9F-A2CC-1E6D18D14026
// Event - Hotkey
//
// 0x5E,0xA1,0x6A,0x67,0x47,0x6A,0x9F,0x4D,0xA2,0xCC,0x1E,0x6D,0x18,0xD1,0x40,0x26
// 66, 67					// Object ID BC
// 0						// Instance count
// 0x08						// Flags (Event)
#define WMID_GUID3		"676AA15E-6A47-4D9F-A2CC-1E6D18D14026"

// GUID 61EF69EA-865C-4BC3-A502-A0DEBA0CB531
// Method - Control Device Map
//
// 0xEA,0x69,0xEF,0x61,0x5C,0x86,0xC3,0x4B,0xA5,0x02,0xA0,0xDE,0xBA,0x0C,0xB5,0x31 
// 65,65					// Object ID AA
// 1						// Instance count
// 0x02						// Flags (Data)
#define WMID_GUID4		"61EF69EA-865C-4BC3-A502-A0DEBA0CB531"	

MODULE_ALIAS("wmi:67C3371D-95A3-4C37-BB61-DD47B491DAAB");
MODULE_ALIAS("wmi:6AF4F258-B401-42fd-BE91-3D4AC2D7C0D3");

// leon added for WMID_GUID2,WMID_GUID3,WMID_GUID4 --
//MODULE_ALIAS("wmi:95764E09-FB56-4e83-B31A-37761F60994A");
//MODULE_ALIAS("wmi:676AA15E-6A47-4D9F-A2CC-1E6D18D14026");
//MODULE_ALIAS("wmi:61EF69EA-865C-4BC3-A502-A0DEBA0CB531");
// --

/* Temporary workaround until the WMI sysfs interface goes in */
// leon marked for following 2.6.33-rc4 "acer-wmi.c"
MODULE_ALIAS("dmi:*:*Acer*:*:");

/*
 * Interface capability flags
 */
#define ACER_CAP_MAILLED		(1<<0)
#define ACER_CAP_WIRELESS		(1<<1)
#define ACER_CAP_BLUETOOTH		(1<<2)
#define ACER_CAP_BRIGHTNESS		(1<<3)
#define ACER_CAP_THREEG			(1<<4)
#define ACER_CAP_ANY			(0xFFFFFFFF)

/*
 * Interface type flags
 */
enum interface_flags {
	ACER_AMW0,
	ACER_AMW0_V2,
	ACER_WMID,
};

#define ACER_DEFAULT_WIRELESS  0
#define ACER_DEFAULT_BLUETOOTH 0
#define ACER_DEFAULT_MAILLED   0
#define ACER_DEFAULT_THREEG    0

static int max_brightness = 0xF;

static int mailled = -1;
static int brightness = -1;
static int threeg = -1;
static int force_series;


module_param(mailled, int, 0444);
module_param(brightness, int, 0444);
module_param(threeg, int, 0444);
module_param(force_series, int, 0444);
MODULE_PARM_DESC(mailled, "Set initial state of Mail LED");
MODULE_PARM_DESC(brightness, "Set initial LCD backlight brightness");
MODULE_PARM_DESC(threeg, "Set initial state of 3G hardware");
MODULE_PARM_DESC(force_series, "Force a different laptop series");


/*added by tomsun 2010-02-05*/
static char *wmi_default="wmi_default=no";
module_param(wmi_default, charp, 0444);
MODULE_PARM_DESC(wmi_default, "wmi_default=yes or wmi_default=no");
struct acer_function_map
{
	u8 button_number;
	u8 key_type;
	u16 function_map;
};

struct acer_hotkey_function_data
{
	struct dmi_header header;
	u16 support_1;
	u16 support_2;
	u16 support_3;
	u16 support_4;
	u16 support_5;
	struct acer_function_map hotkey_function[];
};

static u16  commu_status = 0;
static u16  commu_devices = 0;
static u16 device_enable = 1;

struct acer_function_map *hotkey_button;
static int hotkey_num;
static  void parse_acer_table (const struct dmi_header *dm);
static void find_hotkeys (const struct dmi_header *dm, void *dummy);
static u16  wmi_get_hotkey_map(u8 keynumber);
static void acer_wmi_get_device_status (u8 hotkey_number, u16 function_bit);
static u8  commu_keynumber;
static u8  wmi_get_commu_keynumber(void);
/***********end**************/


struct acer_data {
	int mailled;
	int threeg;
	int brightness;
};

struct acer_debug {
	struct dentry *root;
	struct dentry *devices;
	u32 wmid_devices;
};

static struct rfkill *wireless_rfkill;
static struct rfkill *bluetooth_rfkill;

/* Each low-level interface must define at least some of the following */
struct wmi_interface {
	/* The WMI device type */
	u32 type;

	/* The capabilities this interface provides */
	u32 capability;

	/* Private data for the current interface */
	struct acer_data data;

	/* debugfs entries associated with this interface */
	struct acer_debug debug;
};

/* The static interface pointer, points to the currently detected interface */
static struct wmi_interface *interface;

/*
 * Embedded Controller quirks
 * Some laptops require us to directly access the EC to either enable or query
 * features that are not available through WMI.
 */

struct quirk_entry {
	u8 wireless;
	u8 mailled;
	s8 brightness;
	u8 bluetooth;
};

static struct quirk_entry *quirks;

/*
 * leon added for Generate Hotkey Event of WMID_GUID3
 */
struct acer_wmi_hotkey_generate_hotkey_event_return_value {
	u8 function_number;	// function number (0x1): Hotkey Event
	u8 key_number;		// refer to button number of SMBIOS definition
	u16 device_status;	// refer to function bitmap of SMBIOS definition
	u8 reserved_1;
	u8 reserved_2;
	u8 reserved_3;
	u8 reserved_4;
};

struct acer_wmi_hotkey_hotkey_break_event_return_value {
	u8 function_number;	// function number (0x2): Hotkey Break Event
	u8 key_number;		// refer to button number of SMBIOS definition
	u8 reserved_1;
	u8 reserved_2;
	u8 reserved_3;
	u8 reserved_4;
	u8 reserved_5;
	u8 reserved_6;
};

/*
 * leon added for SetFunction of WMID_GUID4
 */
struct acer_wmi_hotkey_set_function1_input_arg2 {
	u8 function_number;
	u16 communication_status;
	u16 other_status;
	u8 launch_manager_status;
	u8 reserved_1;
	u8 reserved_2;
};

struct acer_wmi_hotkey_set_function1_return_value {
	u8 error_code;
	u8 ec_return;
	u8 reserved_1;
	u8 reserved_2;
};

struct acer_wmi_hotkey_set_function2_input_arg2 {
	u8 function_number;
	u8 hotkey_number;
	u16 next_device;		// refer to control device map fot bit definition
	u8 reserved_1;
	u8 reserved_2;
	u8 reserved_3;
};

struct acer_wmi_hotkey_set_function2_return_value {
	u8 error_code;
	u8 ec_return;
	u8 reserved_1;
	u8 reserved_2;
};

/*
 * leon added for GetFunction of WMID_GUID4
 */
struct acer_wmi_hotkey_get_function1_input_arg2 {
	u8 function_number;
	u8 hotkey_number;
	u16 function_bit;
};

struct acer_wmi_hotkey_get_function1_return_value {
	u8 error_code;
	u8 ec_return;
	u16 device_status;
	u8 reserved_1;
	u8 reserved_2;
	u8 reserved_3;
	u8 reserved_4;
};
/*********2011-03-23*******/
static acpi_status
acer_wmi_hotkey_set_function2(u32 method_id,
                              struct acer_wmi_hotkey_set_function2_input_arg2 in,
                              struct acer_wmi_hotkey_set_function2_return_value *out);

static ssize_t devices_read(struct kobject *kobj, struct kobj_attribute *attr,
                                char* buf)
{
        return sprintf(buf, "0x%04x\n", commu_devices);
}

static struct kobj_attribute devices_attribute =
        __ATTR(devices, 0444, devices_read, NULL);


static size_t status_read(struct kobject *kobj, struct kobj_attribute *attr,
			 	char *buf)
{
	return sprintf(buf, "0x%04x\n", commu_status);
}

static struct kobj_attribute status_attribute =
	__ATTR(status, 0444, status_read, NULL);

static size_t enable_read(struct kobject* kobj, struct kobj_attribute *attr, 
				char* buf)
{
	return sprintf(buf, "0x%04x\n", device_enable);
}

static size_t enable_write(struct kobject* kobj, struct kobj_attribute *attr,
				const char* buf, size_t count)
{
	sscanf(buf, "%x\n", &device_enable);
	struct acer_wmi_hotkey_set_function2_return_value result;
	struct acer_wmi_hotkey_set_function2_input_arg2 arg2 = {
		.function_number = 0x2,
		.hotkey_number = commu_keynumber,
		.next_device = device_enable,
	};
	acer_wmi_hotkey_set_function2(ACER_WMID_HOTKEY_SET_FUNCTION_METHODID, arg2, &result);
	acer_wmi_get_device_status (commu_keynumber, device_enable);		
	return count;
}

static struct kobj_attribute enable_attribute =
	__ATTR(enable, 0666, enable_read, enable_write);


static struct attribute *attrs[] = {
        &devices_attribute.attr,
	&status_attribute.attr,
	&enable_attribute.attr,
        NULL,
};

static struct attribute_group commu_attr_group =
{
        .attrs = attrs,
};

static struct kobject *comm_kobj;
/***********end************/

/********added by tomsun 2010-02-05********/
static u16  wmi_get_hotkey_map(u8 key)
{
	int i;
	for (i = 0; i < hotkey_num; i++)
	{
		if (hotkey_button[i].button_number == key)
		{
			return hotkey_button[i].function_map;
		}
	}
	return 0;
}

static u8 wmi_get_commu_keynumber(void)
{
	int i;
	for (i = 0; i < hotkey_num; i++)
	{
		if (hotkey_button[i].button_number >= 0x01 && hotkey_button[i].button_number <= 0x1f){	
			if (hotkey_button[i].function_map & 0x0841)
				return hotkey_button[i].button_number;
		}
	}
	return  0;
}

/*******************end*****************/

static void set_quirks(void)
{
	if (!interface)
		return;

	if (quirks->mailled)
		interface->capability |= ACER_CAP_MAILLED;

	if (quirks->brightness)
		interface->capability |= ACER_CAP_BRIGHTNESS;
}

static int dmi_matched(const struct dmi_system_id *dmi)
{
	quirks = dmi->driver_data;
	return 0;
}

static struct quirk_entry quirk_unknown = {
};

static struct quirk_entry quirk_acer_aspire_1520 = {
	.brightness = -1,
};

static struct quirk_entry quirk_acer_travelmate_2490 = {
	.mailled = 1,
};

/* This AMW0 laptop has no bluetooth */
static struct quirk_entry quirk_medion_md_98300 = {
	.wireless = 1,
};

static struct quirk_entry quirk_fujitsu_amilo_li_1718 = {
	.wireless = 2,
};

/* The Aspire One has a dummy ACPI-WMI interface - disable it */
static struct dmi_system_id __devinitdata acer_blacklist[] = {
	{
		.ident = "Acer Aspire One (SSD)",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "AOA110"),
		},
	},
	{
		.ident = "Acer Aspire One (HDD)",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "AOA150"),
		},
	},
	{}
};

static struct dmi_system_id acer_quirks[] = {
	{
		.callback = dmi_matched,
		.ident = "Acer Aspire 1360",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 1360"),
		},
		.driver_data = &quirk_acer_aspire_1520,
	},
	{
		.callback = dmi_matched,
		.ident = "Acer Aspire 1520",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 1520"),
		},
		.driver_data = &quirk_acer_aspire_1520,
	},
	{
		.callback = dmi_matched,
		.ident = "Acer Aspire 3100",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 3100"),
		},
		.driver_data = &quirk_acer_travelmate_2490,
	},
	{
		.callback = dmi_matched,
		.ident = "Acer Aspire 3610",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 3610"),
		},
		.driver_data = &quirk_acer_travelmate_2490,
	},
	{
		.callback = dmi_matched,
		.ident = "Acer Aspire 5100",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5100"),
		},
		.driver_data = &quirk_acer_travelmate_2490,
	},
	{
		.callback = dmi_matched,
		.ident = "Acer Aspire 5610",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5610"),
		},
		.driver_data = &quirk_acer_travelmate_2490,
	},
	{
		.callback = dmi_matched,
		.ident = "Acer Aspire 5630",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5630"),
		},
		.driver_data = &quirk_acer_travelmate_2490,
	},
	{
		.callback = dmi_matched,
		.ident = "Acer Aspire 5650",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5650"),
		},
		.driver_data = &quirk_acer_travelmate_2490,
	},
	{
		.callback = dmi_matched,
		.ident = "Acer Aspire 5680",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 5680"),
		},
		.driver_data = &quirk_acer_travelmate_2490,
	},
	{
		.callback = dmi_matched,
		.ident = "Acer Aspire 9110",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "Aspire 9110"),
		},
		.driver_data = &quirk_acer_travelmate_2490,
	},
	{
		.callback = dmi_matched,
		.ident = "Acer TravelMate 2490",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "TravelMate 2490"),
		},
		.driver_data = &quirk_acer_travelmate_2490,
	},
	{
		.callback = dmi_matched,
		.ident = "Acer TravelMate 4200",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Acer"),
			DMI_MATCH(DMI_PRODUCT_NAME, "TravelMate 4200"),
		},
		.driver_data = &quirk_acer_travelmate_2490,
	},
	{
		.callback = dmi_matched,
		.ident = "Fujitsu Siemens Amilo Li 1718",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "FUJITSU SIEMENS"),
			DMI_MATCH(DMI_PRODUCT_NAME, "AMILO Li 1718"),
		},
		.driver_data = &quirk_fujitsu_amilo_li_1718,
	},
	{
		.callback = dmi_matched,
		.ident = "Medion MD 98300",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "MEDION"),
			DMI_MATCH(DMI_PRODUCT_NAME, "WAM2030"),
		},
		.driver_data = &quirk_medion_md_98300,
	},
	{}
};

/* Find which quirks are needed for a particular vendor/ model pair */
static void find_quirks(void)
{
	if (!force_series) {
		// leon added --
		//printk(ACER_INFO "leon-debug: [find_quirks]dmi_check_system(acer_quirks)\n");
		// --
		dmi_check_system(acer_quirks);
	} else if (force_series == 2490) {
		// leon added --
		//printk(ACER_INFO "leon-debug: [find_quirks]quirks = &quirk_acerr_travelmate_2490\n");
		// --
		quirks = &quirk_acer_travelmate_2490;
	}

	if (quirks == NULL) {
		// leon added --
		//printk(ACER_INFO "leon-debug: [find_quirks]quirks = &quirk_unknown\n");
		// --
		quirks = &quirk_unknown;
	}
	// leon added --
	//printk(ACER_INFO "leon-debug: [find_quirks]set_quirks()\n");
	// --
	set_quirks();
}

/*
 * General interface convenience methods
 */

static bool has_cap(u32 cap)
{
	if ((interface->capability & cap) != 0)
		return 1;

	return 0;
}

/*
 * AMW0 (V1) interface
 */
struct wmab_args {
	u32 eax;
	u32 ebx;
	u32 ecx;
	u32 edx;
};

struct wmab_ret {
	u32 eax;
	u32 ebx;
	u32 ecx;
	u32 edx;
	u32 eex;
};

static acpi_status wmab_execute(struct wmab_args *regbuf,
struct acpi_buffer *result)
{
	struct acpi_buffer input;
	acpi_status status;
	input.length = sizeof(struct wmab_args);
	input.pointer = (u8 *)regbuf;

	status = wmi_evaluate_method(AMW0_GUID1, 1, 1, &input, result);

	return status;
}

static acpi_status AMW0_get_u32(u32 *value, u32 cap,
struct wmi_interface *iface)
{
	int err;
	u8 result;

	switch (cap) {
	case ACER_CAP_MAILLED:
		switch (quirks->mailled) {
		default:
			err = ec_read(0xA, &result);
			if (err)
				return AE_ERROR;
			*value = (result >> 7) & 0x1;
			return AE_OK;
		}
		break;
	case ACER_CAP_WIRELESS:
		switch (quirks->wireless) {
		case 1:
			err = ec_read(0x7B, &result);
			if (err)
				return AE_ERROR;
			*value = result & 0x1;
			return AE_OK;
		case 2:
			err = ec_read(0x71, &result);
			if (err)
				return AE_ERROR;
			*value = result & 0x1;
			return AE_OK;
		default:
			err = ec_read(0xA, &result);
			if (err)
				return AE_ERROR;
			*value = (result >> 2) & 0x1;
			return AE_OK;
		}
		break;
	case ACER_CAP_BLUETOOTH:
		switch (quirks->bluetooth) {
		default:
			err = ec_read(0xA, &result);
			if (err)
				return AE_ERROR;
			*value = (result >> 4) & 0x1;
			return AE_OK;
		}
		break;
	case ACER_CAP_BRIGHTNESS:
		switch (quirks->brightness) {
		default:
			err = ec_read(0x83, &result);
			if (err)
				return AE_ERROR;
			*value = result;
			return AE_OK;
		}
		break;
	default:
		return AE_ERROR;
	}
	return AE_OK;
}

static acpi_status AMW0_set_u32(u32 value, u32 cap, struct wmi_interface *iface)
{
	struct wmab_args args;

	args.eax = ACER_AMW0_WRITE;
	args.ebx = value ? (1<<8) : 0;
	args.ecx = args.edx = 0;

	switch (cap) {
	case ACER_CAP_MAILLED:
		if (value > 1)
			return AE_BAD_PARAMETER;
		args.ebx |= ACER_AMW0_MAILLED_MASK;
		break;
	case ACER_CAP_WIRELESS:
		if (value > 1)
			return AE_BAD_PARAMETER;
		args.ebx |= ACER_AMW0_WIRELESS_MASK;
		break;
	case ACER_CAP_BLUETOOTH:
		if (value > 1)
			return AE_BAD_PARAMETER;
		args.ebx |= ACER_AMW0_BLUETOOTH_MASK;
		break;
	case ACER_CAP_BRIGHTNESS:
		if (value > max_brightness)
			return AE_BAD_PARAMETER;
		switch (quirks->brightness) {
		default:
			return ec_write(0x83, value);
			break;
		}
	default:
		return AE_ERROR;
	}

	/* Actually do the set */
	return wmab_execute(&args, NULL);
}

static acpi_status AMW0_find_mailled(void)
{
	struct wmab_args args;
	struct wmab_ret ret;
	acpi_status status = AE_OK;
	struct acpi_buffer out = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;

	args.eax = 0x86;
	args.ebx = args.ecx = args.edx = 0;

	status = wmab_execute(&args, &out);
	if (ACPI_FAILURE(status))
		return status;

	obj = (union acpi_object *) out.pointer;
	if (obj && obj->type == ACPI_TYPE_BUFFER &&
	obj->buffer.length == sizeof(struct wmab_ret)) {
		ret = *((struct wmab_ret *) obj->buffer.pointer);
	} else {
		return AE_ERROR;
	}

	if (ret.eex & 0x1)
		interface->capability |= ACER_CAP_MAILLED;

	kfree(out.pointer);

	return AE_OK;
}

static acpi_status AMW0_set_capabilities(void)
{
	struct wmab_args args;
	struct wmab_ret ret;
	acpi_status status = AE_OK;
	struct acpi_buffer out = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;

	/*
	 * On laptops with this strange GUID (non Acer), normal probing doesn't
	 * work.
	 */
	if (wmi_has_guid(AMW0_GUID2)) {
		interface->capability |= ACER_CAP_WIRELESS;
		return AE_OK;
	}

	args.eax = ACER_AMW0_WRITE;
	args.ecx = args.edx = 0;

	args.ebx = 0xa2 << 8;
	args.ebx |= ACER_AMW0_WIRELESS_MASK;

	status = wmab_execute(&args, &out);
	if (ACPI_FAILURE(status))
		return status;

	obj = (union acpi_object *) out.pointer;
	if (obj && obj->type == ACPI_TYPE_BUFFER &&
	obj->buffer.length == sizeof(struct wmab_ret)) {
		ret = *((struct wmab_ret *) obj->buffer.pointer);
	} else {
		return AE_ERROR;
	}

	if (ret.eax & 0x1)
		interface->capability |= ACER_CAP_WIRELESS;

	args.ebx = 2 << 8;
	args.ebx |= ACER_AMW0_BLUETOOTH_MASK;

	status = wmab_execute(&args, &out);
	if (ACPI_FAILURE(status))
		return status;

	obj = (union acpi_object *) out.pointer;
	if (obj && obj->type == ACPI_TYPE_BUFFER
	&& obj->buffer.length == sizeof(struct wmab_ret)) {
		ret = *((struct wmab_ret *) obj->buffer.pointer);
	} else {
		return AE_ERROR;
	}

	if (ret.eax & 0x1)
		interface->capability |= ACER_CAP_BLUETOOTH;

	kfree(out.pointer);

	/*
	 * This appears to be safe to enable, since all Wistron based laptops
	 * appear to use the same EC register for brightness, even if they
	 * differ for wireless, etc
	 */
	if (quirks->brightness >= 0)
		interface->capability |= ACER_CAP_BRIGHTNESS;

	return AE_OK;
}

static struct wmi_interface AMW0_interface = {
	.type = ACER_AMW0,
};

static struct wmi_interface AMW0_V2_interface = {
	.type = ACER_AMW0_V2,
};

/*
 * New interface (The WMID interface)
 */
static acpi_status
WMI_execute_u32(u32 method_id, u32 in, u32 *out)
{
	struct acpi_buffer input = { (acpi_size) sizeof(u32), (void *)(&in) };
	struct acpi_buffer result = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	u32 tmp;
	acpi_status status;

	status = wmi_evaluate_method(WMID_GUID1, 1, method_id, &input, &result);

	if (ACPI_FAILURE(status))
		return status;

	obj = (union acpi_object *) result.pointer;
	if (obj && obj->type == ACPI_TYPE_BUFFER &&
		obj->buffer.length == sizeof(u32)) {
		tmp = *((u32 *) obj->buffer.pointer);
	} else {
		tmp = 0;
	}

	if (out)
		*out = tmp;

	kfree(result.pointer);

	return status;
}

static acpi_status WMID_get_u32(u32 *value, u32 cap,
struct wmi_interface *iface)
{
	acpi_status status;
	u8 tmp;
	u32 result, method_id = 0;

	switch (cap) {
	case ACER_CAP_WIRELESS:
		method_id = ACER_WMID_GET_WIRELESS_METHODID;
		break;
	case ACER_CAP_BLUETOOTH:
		method_id = ACER_WMID_GET_BLUETOOTH_METHODID;
		break;
	case ACER_CAP_BRIGHTNESS:
		method_id = ACER_WMID_GET_BRIGHTNESS_METHODID;
		break;
	case ACER_CAP_THREEG:
		method_id = ACER_WMID_GET_THREEG_METHODID;
		break;
	case ACER_CAP_MAILLED:
		if (quirks->mailled == 1) {
			ec_read(0x9f, &tmp);
			*value = tmp & 0x1;
			return 0;
		}
	default:
		return AE_ERROR;
	}
	status = WMI_execute_u32(method_id, 0, &result);

	if (ACPI_SUCCESS(status))
		*value = (u8)result;

	return status;
}

static acpi_status WMID_set_u32(u32 value, u32 cap, struct wmi_interface *iface)
{
	u32 method_id = 0;
	char param;

	switch (cap) {
	case ACER_CAP_BRIGHTNESS:
		if (value > max_brightness)
			return AE_BAD_PARAMETER;
		method_id = ACER_WMID_SET_BRIGHTNESS_METHODID;
		break;
	case ACER_CAP_WIRELESS:
		if (value > 1)
			return AE_BAD_PARAMETER;
		method_id = ACER_WMID_SET_WIRELESS_METHODID;
		break;
	case ACER_CAP_BLUETOOTH:
		if (value > 1)
			return AE_BAD_PARAMETER;
		method_id = ACER_WMID_SET_BLUETOOTH_METHODID;
		break;
	case ACER_CAP_THREEG:
		if (value > 1)
			return AE_BAD_PARAMETER;
		method_id = ACER_WMID_SET_THREEG_METHODID;
		break;
	case ACER_CAP_MAILLED:
		if (value > 1)
			return AE_BAD_PARAMETER;
		if (quirks->mailled == 1) {
			param = value ? 0x92 : 0x93;
			i8042_lock_chip();
			i8042_command(&param, 0x1059);
			i8042_unlock_chip();
			return 0;
		}
		break;
	default:
		return AE_ERROR;
	}
	return WMI_execute_u32(method_id, (u32)value, NULL);
}

static acpi_status WMID_set_capabilities(void)
{
	struct acpi_buffer out = {ACPI_ALLOCATE_BUFFER, NULL};
	union acpi_object *obj;
	acpi_status status;
	u32 devices;

	// leon added --
	//printk(ACER_INFO "leon-debug: [WMID_set_capabilities]wmi_query_block(WMID_GUID2(%s), 1, &out)\n", WMID_GUID2);
	// --
	status = wmi_query_block(WMID_GUID2, 1, &out);
	if (ACPI_FAILURE(status))
	{
		// leon added --
		//printk(ACER_INFO "leon-debug: [WMID_set_capabilities]{ACPI_FAILURE(%d)} return status(%d)\n", status, status);
		// --
		return status;
	}

	obj = (union acpi_object *) out.pointer;
	if (obj && obj->type == ACPI_TYPE_BUFFER &&
		obj->buffer.length == sizeof(u32)) {
		// leon added --
		//printk(ACER_INFO "leon-debug: [WMID_set_capabilities]devices = *((u32 *) obj->buffer.pointer)\n");
		// --
		devices = *((u32 *) obj->buffer.pointer);
	} else {
		// leon added --
		//printk(ACER_INFO "leon-debug: [WMID_set_capabilities]!{obj && obj->type == ACPI_TYPE_BUFFER}\n"
		//	"&& {obj->buffer.length == sizeof(u32)} return AE_ERROR\n");
		// --
		return AE_ERROR;
	}

	/* Not sure on the meaning of the relevant bits yet to detect these */

	// leon added --
	//printk(ACER_INFO "leon-debug: [WMID_set_capabilities]interface->capability(old) = %d\n", interface->capability);
	// --

	interface->capability |= ACER_CAP_WIRELESS;
	interface->capability |= ACER_CAP_THREEG;

	/* WMID always provides brightness methods */
	interface->capability |= ACER_CAP_BRIGHTNESS;

	if (devices & 0x10)
		interface->capability |= ACER_CAP_BLUETOOTH;

	// leon added --
	//printk(ACER_INFO "leon-debug: [WMID_set_capabilities]interface->capability(new) = %d\n", interface->capability);
	// --

	if (!(devices & 0x20))
		max_brightness = 0x9;

	// leon added --
	//printk(ACER_INFO "leon-debug: [WMID_set_capabilities]return status(%d)\n", status);
	// --
	return status;
}

static struct wmi_interface wmid_interface = {
	.type = ACER_WMID,
};

/*
 * Generic Device (interface-independent)
 */

static acpi_status get_u32(u32 *value, u32 cap)
{
	acpi_status status = AE_ERROR;

	switch (interface->type) {
	case ACER_AMW0:
		status = AMW0_get_u32(value, cap, interface);
		break;
	case ACER_AMW0_V2:
		if (cap == ACER_CAP_MAILLED) {
			status = AMW0_get_u32(value, cap, interface);
			break;
		}
	case ACER_WMID:
		status = WMID_get_u32(value, cap, interface);
		break;
	}

	return status;
}

static acpi_status set_u32(u32 value, u32 cap)
{
	acpi_status status;

	if (interface->capability & cap) {
		switch (interface->type) {
		case ACER_AMW0:
			return AMW0_set_u32(value, cap, interface);
		case ACER_AMW0_V2:
			if (cap == ACER_CAP_MAILLED)
				return AMW0_set_u32(value, cap, interface);

			/*
			 * On some models, some WMID methods don't toggle
			 * properly. For those cases, we want to run the AMW0
			 * method afterwards to be certain we've really toggled
			 * the device state.
			 */
			if (cap == ACER_CAP_WIRELESS ||
				cap == ACER_CAP_BLUETOOTH) {
				status = WMID_set_u32(value, cap, interface);
				if (ACPI_FAILURE(status))
					return status;

				return AMW0_set_u32(value, cap, interface);
			}
		case ACER_WMID:
			return WMID_set_u32(value, cap, interface);
		default:
			return AE_BAD_PARAMETER;
		}
	}
	return AE_BAD_PARAMETER;
}


/*
 * LED device (Mail LED only, no other LEDs known yet)
 */
static void mail_led_set(struct led_classdev *led_cdev,
enum led_brightness value)
{
	set_u32(value, ACER_CAP_MAILLED);
}

static struct led_classdev mail_led = {
	.name = "acer-wmi::mail",
	.brightness_set = mail_led_set,
};

static int __devinit acer_led_init(struct device *dev)
{
	return led_classdev_register(dev, &mail_led);
}

static void acer_led_exit(void)
{
	led_classdev_unregister(&mail_led);
}

/*
 * Backlight device
 */
static struct backlight_device *acer_backlight_device;

static int read_brightness(struct backlight_device *bd)
{
	u32 value;
	get_u32(&value, ACER_CAP_BRIGHTNESS);
	return value;
}

static int update_bl_status(struct backlight_device *bd)
{
	int intensity = bd->props.brightness;

	if (bd->props.power != FB_BLANK_UNBLANK)
		intensity = 0;
	if (bd->props.fb_blank != FB_BLANK_UNBLANK)
		intensity = 0;

	set_u32(intensity, ACER_CAP_BRIGHTNESS);

	return 0;
}

static struct backlight_ops acer_bl_ops = {
	.get_brightness = read_brightness,
	.update_status = update_bl_status,
};

static int __devinit acer_backlight_init(struct device *dev)
{
	struct backlight_device *bd;

	/* added by nancy 2010-11-23 */
	struct backlight_properties props;
	memset(&props, 0, sizeof(struct backlight_properties));
	bd = backlight_device_register("acer-wmi", dev, NULL, &acer_bl_ops, &props);
	/*---------end----------*/

	//bd = backlight_device_register("acer-wmi", dev, NULL, &acer_bl_ops);
	if (IS_ERR(bd)) {
		printk(ACER_ERR "Could not register Acer backlight device\n");
		acer_backlight_device = NULL;
		return PTR_ERR(bd);
	}

	acer_backlight_device = bd;

	bd->props.power = FB_BLANK_UNBLANK;
	bd->props.brightness = max_brightness;
	bd->props.max_brightness = max_brightness;
	backlight_update_status(bd);
	return 0;
}

static void acer_backlight_exit(void)
{
	backlight_device_unregister(acer_backlight_device);
}

/*
 * Rfkill devices
 */
static void acer_rfkill_update(struct work_struct *ignored);
static DECLARE_DELAYED_WORK(acer_rfkill_work, acer_rfkill_update);
static void acer_rfkill_update(struct work_struct *ignored)
{
	u32 state;
	acpi_status status;

	status = get_u32(&state, ACER_CAP_WIRELESS);
	if (ACPI_SUCCESS(status))
		rfkill_set_sw_state(wireless_rfkill, !state);

	if (has_cap(ACER_CAP_BLUETOOTH)) {
		status = get_u32(&state, ACER_CAP_BLUETOOTH);
		if (ACPI_SUCCESS(status))
			rfkill_set_sw_state(bluetooth_rfkill, !state);
	}

	schedule_delayed_work(&acer_rfkill_work, round_jiffies_relative(HZ));
}

static int acer_rfkill_set(void *data, bool blocked)
{
	acpi_status status;
	u32 cap = (unsigned long)data;
	status = set_u32(!blocked, cap);
	if (ACPI_FAILURE(status))
		return -ENODEV;
	return 0;
}

static const struct rfkill_ops acer_rfkill_ops = {
	.set_block = acer_rfkill_set,
};

static struct rfkill *acer_rfkill_register(struct device *dev,
					   enum rfkill_type type,
					   char *name, u32 cap)
{
	int err;
	struct rfkill *rfkill_dev;

	rfkill_dev = rfkill_alloc(name, dev, type,
				  &acer_rfkill_ops,
				  (void *)(unsigned long)cap);
	if (!rfkill_dev)
		return ERR_PTR(-ENOMEM);

	err = rfkill_register(rfkill_dev);
	if (err) {
		rfkill_destroy(rfkill_dev);
		return ERR_PTR(err);
	}
	return rfkill_dev;
}

static int acer_rfkill_init(struct device *dev)
{
	wireless_rfkill = acer_rfkill_register(dev, RFKILL_TYPE_WLAN,
		"acer-wireless", ACER_CAP_WIRELESS);
	if (IS_ERR(wireless_rfkill))
		return PTR_ERR(wireless_rfkill);

	if (has_cap(ACER_CAP_BLUETOOTH)) {
		bluetooth_rfkill = acer_rfkill_register(dev,
			RFKILL_TYPE_BLUETOOTH, "acer-bluetooth",
			ACER_CAP_BLUETOOTH);
		if (IS_ERR(bluetooth_rfkill)) {
			rfkill_unregister(wireless_rfkill);
			rfkill_destroy(wireless_rfkill);
			return PTR_ERR(bluetooth_rfkill);
		}
	}

	schedule_delayed_work(&acer_rfkill_work, round_jiffies_relative(HZ));

	return 0;
}

static void acer_rfkill_exit(void)
{
	cancel_delayed_work_sync(&acer_rfkill_work);

	rfkill_unregister(wireless_rfkill);
	rfkill_destroy(wireless_rfkill);

	if (has_cap(ACER_CAP_BLUETOOTH)) {
		rfkill_unregister(bluetooth_rfkill);
		rfkill_destroy(bluetooth_rfkill);
	}
	return;
}

/*
 * sysfs interface
 */
static ssize_t show_bool_threeg(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u32 result; \
	acpi_status status = get_u32(&result, ACER_CAP_THREEG);
	if (ACPI_SUCCESS(status))
		return sprintf(buf, "%u\n", result);
	return sprintf(buf, "Read error\n");
}

static ssize_t set_bool_threeg(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	u32 tmp = simple_strtoul(buf, NULL, 10);
	acpi_status status = set_u32(tmp, ACER_CAP_THREEG);
		if (ACPI_FAILURE(status))
			return -EINVAL;
	return count;
}
static DEVICE_ATTR(threeg, S_IWUGO | S_IRUGO | S_IWUSR, show_bool_threeg,
	set_bool_threeg);

static ssize_t show_interface(struct device *dev, struct device_attribute *attr,
	char *buf)
{
	switch (interface->type) {
	case ACER_AMW0:
		return sprintf(buf, "AMW0\n");
	case ACER_AMW0_V2:
		return sprintf(buf, "AMW0 v2\n");
	case ACER_WMID:
		return sprintf(buf, "WMID\n");
	default:
		return sprintf(buf, "Error!\n");
	}
}

static DEVICE_ATTR(interface, S_IWUGO | S_IRUGO | S_IWUSR,
	show_interface, NULL);

/*
 * debugfs functions
 */
static u32 get_wmid_devices(void)
{
	struct acpi_buffer out = {ACPI_ALLOCATE_BUFFER, NULL};
	union acpi_object *obj;
	acpi_status status;


	status = wmi_query_block(WMID_GUID2, 1, &out);
	if (ACPI_FAILURE(status))
		return 0;

	obj = (union acpi_object *) out.pointer;
	if (obj && obj->type == ACPI_TYPE_BUFFER &&
		obj->buffer.length == sizeof(u32)) {
		return *((u32 *) obj->buffer.pointer);
	} else {
		return 0;
	}
}

/*
 * acer_wmi_hotkey_set_function1 (Set Device Default Value and Report LM status)
 * @leon added for WMID_GUID4
 * @method_id:0x1, Byte[0]:0x1, Set Device Default Value and Report LM status
 * @method_id:0x1, Byte[1]:0x2, Set Next Device
 */
static acpi_status 
acer_wmi_hotkey_set_function1(u32 method_id, 
			      struct acer_wmi_hotkey_set_function1_input_arg2 in, 
			      struct acer_wmi_hotkey_set_function1_return_value *out)
{
	struct acpi_buffer input = { (acpi_size) sizeof(struct acer_wmi_hotkey_set_function1_input_arg2),
					(void *)(&in) };
	struct acpi_buffer result = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;

	struct acer_wmi_hotkey_set_function1_return_value tmp;
	acpi_status status;

	
	status = wmi_evaluate_method(WMID_GUID4, 1, method_id, &input, &result);

	if (ACPI_FAILURE(status)) {
		return status;
		return -EINVAL;
	}

	obj = (union acpi_object *) result.pointer;
	if (obj && obj->type == ACPI_TYPE_BUFFER 
		&& obj->buffer.length == sizeof(struct acer_wmi_hotkey_set_function1_return_value)) {
		tmp = *((struct acer_wmi_hotkey_set_function1_return_value *)obj->buffer.pointer);
	} else {
		return -EINVAL;
	}

	if (out)
		*out = tmp;

	kfree(result.pointer);

	return status;	
}

/*
 * acer_wmi_hotkey_set_function2 (Set Next Device)
 * @leon added for WMID_GUID4
 * @method_id:0x1, Byte[1]:0x2, Set Next Device
 */
static acpi_status 
acer_wmi_hotkey_set_function2(u32 method_id, 
			      struct acer_wmi_hotkey_set_function2_input_arg2 in, 
			      struct acer_wmi_hotkey_set_function2_return_value *out)
{
	struct acpi_buffer input = { (acpi_size) sizeof(struct acer_wmi_hotkey_set_function2_input_arg2),
					(void *)(&in) };
	struct acpi_buffer result = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;

	struct acer_wmi_hotkey_set_function2_return_value tmp;
	acpi_status status;

	
	status = wmi_evaluate_method(WMID_GUID4, 1, method_id, &input, &result);

	if (ACPI_FAILURE(status)) {
		return status;
		return -EINVAL;
	}

	obj = (union acpi_object *) result.pointer;
	if (obj && obj->type == ACPI_TYPE_BUFFER 
		&& obj->buffer.length == sizeof(struct acer_wmi_hotkey_set_function2_return_value)) {
		tmp = *((struct acer_wmi_hotkey_set_function2_return_value *)obj->buffer.pointer);
	} else {
		return -EINVAL;
	}

	if (out)
		*out = tmp;

	kfree(result.pointer);

	return status;	
}

/*
 * acer_wmi_hotkey_get_function1 
 * @method_id:0x2, Byte[0]:0x1, Get Device status
 */
static acpi_status acer_wmi_hotkey_get_function1(u32 method_id, 
						 struct acer_wmi_hotkey_get_function1_input_arg2 in, 
						 struct acer_wmi_hotkey_get_function1_return_value *out)
{
	struct acpi_buffer input = { (acpi_size) sizeof(struct acer_wmi_hotkey_get_function1_input_arg2),
					(void *)(&in) };
	struct acpi_buffer result = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;

	struct acer_wmi_hotkey_get_function1_return_value tmp;
	acpi_status status;

	status = wmi_evaluate_method(WMID_GUID4, 1, method_id, &input, &result);
	if (ACPI_FAILURE(status)) {
		return -EINVAL;
	}
	obj = (union acpi_object *) result.pointer;
	if (obj && obj->type == ACPI_TYPE_BUFFER 
		&& obj->buffer.length == sizeof(struct acer_wmi_hotkey_get_function1_return_value)) {
		tmp = *((struct acer_wmi_hotkey_get_function1_return_value *)obj->buffer.pointer);
	} else {
		return -EINVAL;
	}

	if (out)
		*out = tmp;
	kfree(result.pointer);
	return status;	
}
/*
 * to get device status
 */
static void acer_wmi_get_device_status (u8 hotkey_number, u16 function_bit)
{
	struct acer_wmi_hotkey_get_function1_input_arg2 arg2 = {
		.function_number = 0x1,
		.hotkey_number = hotkey_number,
		.function_bit = function_bit,
	};

	struct acer_wmi_hotkey_get_function1_return_value result;
	
	acer_wmi_hotkey_get_function1(ACER_WMID_HOTKEY_GET_FUNCTION_METHODID, arg2, &result);
	commu_status = result.device_status;
}

/*
 * acer_wmi_notify
 * @leon added
 */
static void acer_wmi_notify(u32 value, void *context)
{
	struct acpi_buffer response = { ACPI_ALLOCATE_BUFFER, NULL};
	union acpi_object *obj;
	struct acer_wmi_hotkey_generate_hotkey_event_return_value result;
	u16 acer_hotkey;

	struct sk_buff *skb = NULL;
	struct nlmsghdr *nlh;
	int len;
	static int wimax = 0;

	wmi_get_event_data(value, &response);

	obj = (union acpi_object *)response.pointer;
	printk (ACER_ERR "@@@@ LINPUS %s\n", __func__);
	if (obj && obj->type == ACPI_TYPE_BUFFER 
		&& obj->buffer.length == sizeof(struct acer_wmi_hotkey_generate_hotkey_event_return_value)) {
		result = *((struct acer_wmi_hotkey_generate_hotkey_event_return_value *)obj->buffer.pointer);
		
		if (result.function_number == 0x1) {
			acer_hotkey = wmi_get_hotkey_map (result.key_number);
			printk (ACER_ERR "you have press  result.key_number: 0x%02x  fn+hokey 0x%04x\n",result.key_number, acer_hotkey);
			if (result.key_number >= 0x01 && result.key_number <= 0x1F) {
				struct acer_wmi_hotkey_set_function2_input_arg2 arg2 = {
					.function_number = 0x2,
					.hotkey_number = result.key_number,
				};
				acer_hotkey = wmi_get_hotkey_map (result.key_number);
				printk (ACER_ERR "you have press the wifi fn+hokey 0x%04x\n", acer_hotkey);
				if ( acer_hotkey & 0x0001)
				{
					commu_status = result.device_status;
#if  0
					if (result.device_status & 0x0001)   // if WIFI on
					{
						if (result.device_status == 0x0081)
							wimax = 1;
						arg2.next_device = 0x0000;
						skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
						nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
						len = strncpy (NLMSG_DATA(nlh), "wifi off", sizeof("wifi off"));
						NETLINK_CB (skb).dst_group = 1;
						netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);
					}
					else
					{
						skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
						nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
						len = strncpy (NLMSG_DATA(nlh), "wifi on", sizeof("wifi on"));
						NETLINK_CB (skb).dst_group = 1;
						netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);
						if (wimax){
							arg2.next_device = 0x0081;
							wimax = 0;
						}else{
							arg2.next_device = 0x0001;   //if Wifi off
						}
					}
					struct acer_wmi_hotkey_set_function2_return_value result2;
					acer_wmi_hotkey_set_function2(ACER_WMID_HOTKEY_SET_FUNCTION_METHODID, arg2, &result2);
#endif 
					skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
					nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
					len = strncpy (NLMSG_DATA(nlh), "wifi event", sizeof("wifi event"));
					NETLINK_CB (skb).dst_group = 1;
					netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);
				}

				if (acer_hotkey & 0x4000)
				{
					printk ("RF status:0x%04x\n", result.device_status);
					if (result.device_status & 0x4000)
					{
						skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
                                                nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
                                                len = strncpy (NLMSG_DATA(nlh), "RF on", sizeof("RF on"));
                                                NETLINK_CB (skb).dst_group = 1;
                                                netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);
					}else{
						skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
                                                nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
                                                len = strncpy (NLMSG_DATA(nlh), "RF off", sizeof("RF off"));
                                                NETLINK_CB (skb).dst_group = 1;
                                                netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);
					}
				}
			}
			else if (result.key_number >= 0x21 && result.key_number <= 0x3F) {
				acer_hotkey = wmi_get_hotkey_map (result.key_number);
				if (acer_hotkey & 0x0008){
						skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
						nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
						len = strncpy (NLMSG_DATA(nlh), "call sn", sizeof("call sn"));
						NETLINK_CB (skb).dst_group = 1;
						netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);

				}else if (acer_hotkey & 0x0004){
						skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
						nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
						len = strncpy (NLMSG_DATA(nlh), "call p_key", sizeof("call p_key"));
						NETLINK_CB (skb).dst_group = 1;
						netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);

				}else if (acer_hotkey & 0x0001){
						skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
						nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
						len = strncpy (NLMSG_DATA(nlh), "call backup", sizeof("call backup"));
						NETLINK_CB (skb).dst_group = 1;
						netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);
				}else if (acer_hotkey & 0x0010){
						skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
						nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
						len = strncpy (NLMSG_DATA(nlh), "call app1", sizeof("call app1"));
						NETLINK_CB (skb).dst_group = 1;
						netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);
				}else if (acer_hotkey & 0x0020){
						skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
						nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
						len = strncpy (NLMSG_DATA(nlh), "call app2", sizeof("call app2"));
						NETLINK_CB (skb).dst_group = 1;
						netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);
				}
			}
#if  0 /*20110302*/
			else if (result.key_number >= 0x41 && result.key_number <= 0x5F) {
				acer_hotkey = wmi_get_hotkey_map (result.key_number);
				printk (ACER_ERR "@@@@ LINPUS MEDIA HOTKEY 0X%04x\n", acer_hotkey);
				if ( acer_hotkey & 0x0001)
				{
						skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
						nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
						len = strncpy (NLMSG_DATA(nlh), "volume up", sizeof("volume up"));
						NETLINK_CB (skb).dst_group = 1;
						netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);
				}else if (acer_hotkey & 0x0002){
						skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
						nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
						len = strncpy (NLMSG_DATA(nlh), "volume down", sizeof("volume down"));
						NETLINK_CB (skb).dst_group = 1;
						netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);
				}else if (acer_hotkey & 0x0004){
						skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
						nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
						len = strncpy (NLMSG_DATA(nlh), "mute", sizeof("mute"));
						NETLINK_CB (skb).dst_group = 1;
						netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);
				}
			}
#endif 
			else if (result.key_number >= 0x61 && result.key_number <= 0x7F) {
				acer_hotkey = wmi_get_hotkey_map (result.key_number);
				if ( acer_hotkey & 0x0001){
					skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
					nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
					len = strncpy (NLMSG_DATA(nlh), "brightness up", sizeof("brightness up"));
					NETLINK_CB (skb).dst_group = 1;
					netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);
				}else if (acer_hotkey & 0x0002){
					skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
					nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
					len = strncpy (NLMSG_DATA(nlh), "brightness down", sizeof("brightness down"));
					NETLINK_CB (skb).dst_group = 1;
					netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);
				}else if (acer_hotkey & 0x0004){
				}else if (acer_hotkey & 0x0008){
					skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
					nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
					len = strncpy (NLMSG_DATA(nlh), "display switch", sizeof("display switch"));
					NETLINK_CB (skb).dst_group = 1;
					netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);
				}
			}
			else if (result.key_number >= 0x81 && result.key_number <= 0x9F) {
				acer_hotkey = wmi_get_hotkey_map (result.key_number);
				if (acer_hotkey & 0x0002)
				{
					printk (ACER_ERR "@@@@  LINPUS TOUCHPAD STATUS 0X%04x\n", result.device_status);
					if (result.device_status & 0x0002)
					{
						skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
						nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
						len = strncpy (NLMSG_DATA(nlh), "touchpad on", sizeof("touchpad on"));
						NETLINK_CB (skb).dst_group = 1;
						netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);
					}else{
						skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
						nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
						len = strncpy (NLMSG_DATA(nlh), "touchpad off", sizeof("touchpad off"));
						NETLINK_CB (skb).dst_group = 1;
						netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);
					}
				}else if (acer_hotkey & 0x0001){
						skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
						nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
						len = strncpy (NLMSG_DATA(nlh), "odd eject", sizeof("odd eject"));
						NETLINK_CB (skb).dst_group = 1;
						netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);	
				} 
				
			}
			else if (result.key_number >= 0xA0 && result.key_number <= 0xFE)
			{
			}
			else if (result.key_number == 0x00 || result.key_number == 0xFF)
			{
			}
			// notify user space application via acpi events

		} else if (result.function_number == 0x2) {
			//printk(ACER_INFO "leon-debug: [acer_wmi_notify]{ACPI _WED Return}Hotkey Break Event\n");

			if (result.key_number >= 0x01 && result.key_number <= 0x1F)
			{
			}
			else if (result.key_number >= 0x21 && result.key_number <= 0x3F)
			{
			}
			else if (result.key_number >= 0x41 && result.key_number <= 0x5F)
			{	
			}
			else if (result.key_number >= 0x61 && result.key_number <= 0x7F)
			{
				acer_hotkey = wmi_get_hotkey_map (result.key_number);
				skb = alloc_skb (NLMSG_SPACE(MAX_PAYLOAD), GFP_KERNEL);
                                nlh = NLMSG_PUT (skb, 0, 0, NLMSG_DONE, NLMSG_SPACE(MAX_PAYLOAD));
                                len = strncpy (NLMSG_DATA(nlh), "display exit", sizeof("display exit"));
                                NETLINK_CB (skb).dst_group = 1;
                                netlink_broadcast (acer_sock_user, skb, 0, 1, GFP_KERNEL);	
			}
			else if (result.key_number >= 0x81 && result.key_number <= 0x9F)
			{	
			}
			else if (result.key_number >= 0xA0 && result.key_number <= 0xFE)
			{
			}
			else if (result.key_number == 0x00 || result.key_number == 0xFF)
			{
			}
		} else
			printk(ACER_INFO " -debug: [acer_wmi_notify]{ACPI _WED Return}Unknown Event\n");

	} else {
		printk(ACER_INFO " -debug: [acer_wmi_notify]Unknown response received\n");
	}
nlmsg_failure:
		//printk(ACER_INFO "tomsun -debug : acer nlmsg failure\n");
		printk(ACER_INFO " -debug : acer nlmsg passed\n");
}

/*
 * Platform device
 */
static int __devinit acer_platform_probe(struct platform_device *device)
{
	int err;

	return err;

}

static int acer_platform_remove(struct platform_device *device)
{
	return 0;
}

static int acer_platform_suspend(struct platform_device *dev,
pm_message_t state)
{
	struct acer_data *data = &interface->data;

	if (!data)
		return -ENOMEM;
	return 0;
}

static int acer_platform_resume(struct platform_device *device)
{
	struct acer_data *data = &interface->data;

	if (!data)
		return -ENOMEM;
/*
	if (has_cap(ACER_CAP_MAILLED))
		set_u32(data->mailled, ACER_CAP_MAILLED);

	if (has_cap(ACER_CAP_BRIGHTNESS))
		set_u32(data->brightness, ACER_CAP_BRIGHTNESS);
*/
	return 0;
}

static struct platform_driver acer_platform_driver = {
	.driver = {
		.name = "acer-wmi",
		.owner = THIS_MODULE,
	},
	.probe = acer_platform_probe,
	.remove = acer_platform_remove,
	.suspend = acer_platform_suspend,
	.resume = acer_platform_resume,
};

static struct platform_device *acer_platform_device;

static int remove_sysfs(struct platform_device *device)
{
	if (has_cap(ACER_CAP_THREEG))
		device_remove_file(&device->dev, &dev_attr_threeg);

	device_remove_file(&device->dev, &dev_attr_interface);

	return 0;
}

static int create_sysfs(void)
{
	int retval = -ENOMEM;

	if (has_cap(ACER_CAP_THREEG)) {
		retval = device_create_file(&acer_platform_device->dev,
			&dev_attr_threeg);
		if (retval)
			goto error_sysfs;
	}

	retval = device_create_file(&acer_platform_device->dev,
		&dev_attr_interface);
	if (retval)
		goto error_sysfs;

	return 0;

error_sysfs:
		remove_sysfs(acer_platform_device);
	return retval;
}

static void remove_debugfs(void)
{
	debugfs_remove(interface->debug.devices);
	debugfs_remove(interface->debug.root);
}

static int create_debugfs(void)
{
	interface->debug.root = debugfs_create_dir("acer-wmi", NULL);
	if (!interface->debug.root) {
		printk(ACER_ERR "Failed to create debugfs directory");
		return -ENOMEM;
	}

	interface->debug.devices = debugfs_create_u32("devices", S_IRUGO,
					interface->debug.root,
					&interface->debug.wmid_devices);
	if (!interface->debug.devices)
		goto error_debugfs;

	return 0;

error_debugfs:
	remove_debugfs();
	return -ENOMEM;
}
/*added by tomsun*/
static  void parse_acer_table (const struct dmi_header *dm)
{
	hotkey_num = (dm->length - 14)/sizeof (struct acer_function_map);
	
	struct acer_hotkey_function_data *hotkey_table = container_of (dm, struct acer_hotkey_function_data, header);
	
	if (dm->length < 16)
		return;
	commu_devices = hotkey_table->support_1;
	
	hotkey_button = krealloc (hotkey_button, hotkey_num*sizeof (struct acer_function_map), GFP_KERNEL);

	memcpy (hotkey_button, hotkey_table->hotkey_function, sizeof (struct acer_function_map)*hotkey_num);
}

static void find_hotkeys (const struct dmi_header *dm, void *dummy)
{
	if (dm->type == 0xaa)
		parse_acer_table (dm);
}
/*******end*******/


static int __init acer_wmi_init(void)
{
	int err_event;

	printk(ACER_INFO "Acer Laptop ACPI-WMI Extras\n");

	if (dmi_check_system(acer_blacklist)) {
		printk(ACER_INFO "Blacklisted hardware detected - "
				"not loading\n");
		return -ENODEV;
	}

	find_quirks();

	/*
	 * Detect which ACPI-WMI interface we're using.
	 */
	if (wmi_has_guid(AMW0_GUID1) && wmi_has_guid(WMID_GUID1))
		interface = &AMW0_V2_interface;

	if (!wmi_has_guid(AMW0_GUID1) && wmi_has_guid(WMID_GUID1))
	{
		interface = &wmid_interface;
	}
	
	
	if (wmi_has_guid(AMW0_GUID1) && !wmi_has_guid(WMID_GUID1)) {
		interface = &AMW0_interface;

		if (ACPI_FAILURE(AMW0_set_capabilities())) {
			printk(ACER_ERR "Unable to detect available AMW0 "
					"devices\n");
			return -ENODEV;
		}
	}
	
	
	/*added by tomsun*/
	dmi_walk (find_hotkeys, NULL);
	commu_keynumber = wmi_get_commu_keynumber();
	acer_wmi_get_device_status(commu_keynumber, commu_devices); 
	

	/*end*/

	if (wmi_has_guid(WMID_GUID4)) {
		struct acer_wmi_hotkey_set_function1_input_arg2 arg2 = {
			.function_number = 0x1,
			.communication_status = 0xFFFF,
			.other_status = 0xFFFF,
			/*.launch_manager_status = 0x41,*/
		};
		if (!strcmp (wmi_default, "wmi_default=no")){
			arg2.launch_manager_status=0x1;
		}else if (!strcmp (wmi_default, "wmi_default=on")){
			arg2.communication_status=0x0001;
			arg2.launch_manager_status=0x41;
		}else if (!strcmp(wmi_default, "wmi_default=off")){
			arg2.communication_status=0x0000;
			arg2.launch_manager_status=0x41;
		}
		struct acer_wmi_hotkey_set_function1_return_value result;
		acer_sock_user = netlink_kernel_create (&init_net, NETLINK_LITE_HOTKEY, 0, NULL, NULL, THIS_MODULE);
		if (acer_sock_user == NULL)
		{
			printk (ACER_ERR "####  create netlink sock failer\n");
		}
		acer_wmi_hotkey_set_function1(ACER_WMID_HOTKEY_SET_FUNCTION_METHODID, arg2, &result);
	}


	if (wmi_has_guid(WMID_GUID3)) {
                err_event = wmi_install_notify_handler(WMID_GUID3, acer_wmi_notify, NULL);
        }
	if (platform_driver_register(&acer_platform_driver)) {
		printk(ACER_ERR "Unable to register platform driver.\n");
		goto error_platform_register;
	}
	
#if ACER_WMI_DEBUG
	if (wmi_has_guid(AMW0_GUID1))
		AMW0_find_mailled();

	if (!interface) {
		printk(ACER_ERR "No or unsupported WMI interface, unable to "
				"load\n");
		return -ENODEV;
	}
	
	
	set_quirks();
	if (acpi_video_backlight_support() && has_cap(ACER_CAP_BRIGHTNESS)) {
		interface->capability &= ~ACER_CAP_BRIGHTNESS;
		printk(ACER_INFO "Brightness must be controlled by "
		       "generic video driver\n");
	}

	if (platform_driver_register(&acer_platform_driver)) {
		printk(ACER_ERR "Unable to register platform driver.\n");
		goto error_platform_register;
	}
#endif
	acer_platform_device = platform_device_alloc("acer-wmi", -1);
	platform_device_add(acer_platform_device);

	/*tomsun 2011-03-24*/
	comm_kobj = kobject_create_and_add ("acer-wmi", kernel_kobj);
        if (!comm_kobj)
                return -ENOMEM;
        err_event = sysfs_create_group(comm_kobj, &commu_attr_group);
        if (err_event)
                kobject_put (comm_kobj);
	/*end*/

	return 0;

error_platform_register:
	return -ENODEV;
}

static void __exit acer_wmi_exit(void)
{
	kfree (hotkey_button);
	remove_sysfs(acer_platform_device);
	remove_debugfs();
	platform_device_del(acer_platform_device);
	platform_driver_unregister(&acer_platform_driver);
	
	if (wmi_has_guid(WMID_GUID3)) {

                wmi_remove_notify_handler(WMID_GUID3);
        }
	if (!acer_sock_user){
		netlink_kernel_release(acer_sock_user);	
	}
	printk(ACER_INFO "Acer Laptop WMI Extras unloaded\n");
	return;
}

module_init(acer_wmi_init);
module_exit(acer_wmi_exit);
