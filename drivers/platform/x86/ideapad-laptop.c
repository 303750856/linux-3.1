/*
 *  ideapad-laptop.c - Lenovo IdeaPad ACPI Extras
 *
 *  Copyright © 2010 Intel Corporation
 *  Copyright © 2010 David Woodhouse <dwmw2@infradead.org>
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <acpi/acpi_bus.h>
#include <acpi/acpi_drivers.h>
#include <linux/rfkill.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/backlight.h>
#include <linux/fb.h>

#define IDEAPAD_RFKILL_DEV_NUM	(3)

#define CFG_BT_BIT	(16)
#define CFG_3G_BIT	(17)
#define CFG_WIFI_BIT	(18)
#define CFG_CAMERA_BIT	(19)

/******** mike 20110901 *********/
/************ end ***************/

#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/proc_fs.h>
#include <linux/skbuff.h>
#include <linux/connector.h>


#define EVENT_CAMARA		0
#define EVENT_TOUCHPAD		1
#define EVENT_WIRELESS		2
#define EVENT_3G		3
#define EVENT_BLUETOOTH		4
#define EVENT_RESOL_CHANGE	5
#define EVENT_BRIGHTNESS	6

static struct {
	char *on_cmd;
	char *off_cmd;
	int on_cmd_len;
	int off_cmd_len;
	int r_cmd;
	int cfgbit;

} socket_cmd [] = {
	{ "camara on"	,	"camara off"	,	9	,	10,	0x1D	,	19 },
	{ "touchpad on"	,	"touchpad off"	,	11	,	12,	0x1B	,	0 },
	{ "wireless on"	,	"wireless off"	,	11	,	12,	0x14	,	18 }, 
	{ "3G on"	,	"3G off"	,	5	,	6,	0x1F	,	17 },
	{ "Bluetooth on",	"Bluetooth off",	12	,	13,	0x16	,	16 },	
};

static unsigned int device_exist = 0;
static unsigned int device_enable = 1;

static struct sock *lenovo_sock;

/************ end ***************/

struct ideapad_private {
	struct rfkill *rfk[IDEAPAD_RFKILL_DEV_NUM];
	struct platform_device *platform_device;
	struct input_dev *inputdev;
	struct backlight_device *blightdev;
	unsigned long cfg;
};

static struct acpi_device *acpi_device_global = NULL;
static acpi_handle ideapad_handle;
static bool no_bt_rfkill;

module_param(no_bt_rfkill, bool, 0664);
MODULE_PARM_DESC(no_bt_rfkill, "No rfkill for bluetooth.");

/*
 * ACPI Helpers
 */
#define IDEAPAD_EC_TIMEOUT (100) /* in ms */

static int read_method_int(acpi_handle handle, const char *method, int *val)
{
	acpi_status status;
	unsigned long long result;

	status = acpi_evaluate_integer(handle, (char *)method, NULL, &result);
	if (ACPI_FAILURE(status)) {
		*val = -1;
		return -1;
	} else {
		*val = result;
		return 0;
	}
}

static int method_vpcr(acpi_handle handle, int cmd, int *ret)
{
	acpi_status status;
	unsigned long long result;
	struct acpi_object_list params;
	union acpi_object in_obj;

	params.count = 1;
	params.pointer = &in_obj;
	in_obj.type = ACPI_TYPE_INTEGER;
	in_obj.integer.value = cmd;

	status = acpi_evaluate_integer(handle, "VPCR", &params, &result);

	if (ACPI_FAILURE(status)) {
		*ret = -1;
		return -1;
	} else {
		*ret = result;
		return 0;
	}
}

static int method_vpcw(acpi_handle handle, int cmd, int data)
{
	struct acpi_object_list params;
	union acpi_object in_obj[2];
	acpi_status status;

	params.count = 2;
	params.pointer = in_obj;
	in_obj[0].type = ACPI_TYPE_INTEGER;
	in_obj[0].integer.value = cmd;
	in_obj[1].type = ACPI_TYPE_INTEGER;
	in_obj[1].integer.value = data;

	status = acpi_evaluate_object(handle, "VPCW", &params, NULL);
	if (status != AE_OK)
		return -1;
	return 0;
}

static int read_ec_data(acpi_handle handle, int cmd, unsigned long *data)
{
	int val;
	unsigned long int end_jiffies;

	if (method_vpcw(handle, 1, cmd))
		return -1;

	for (end_jiffies = jiffies+(HZ)*IDEAPAD_EC_TIMEOUT/1000+1;
	     time_before(jiffies, end_jiffies);) {
		schedule();
		if (method_vpcr(handle, 1, &val))
			return -1;
		if (val == 0) {
			if (method_vpcr(handle, 0, &val))
				return -1;
			*data = val;
			return 0;
		}
	}
	pr_err("timeout in read_ec_cmd\n");
	return -1;
}

static int write_ec_cmd(acpi_handle handle, int cmd, unsigned long data)
{
	int val;
	unsigned long int end_jiffies;

	if (method_vpcw(handle, 0, data))
		return -1;
	if (method_vpcw(handle, 1, cmd))
		return -1;

	for (end_jiffies = jiffies+(HZ)*IDEAPAD_EC_TIMEOUT/1000+1;
	     time_before(jiffies, end_jiffies);) {
		schedule();
		if (method_vpcr(handle, 1, &val))
			return -1;
		if (val == 0)
			return 0;
	}
	pr_err("timeout in write_ec_cmd\n");
	return -1;
}

/*
 * sysfs
 */
static ssize_t show_ideapad_cam(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	unsigned long result;

	if (read_ec_data(ideapad_handle, 0x1D, &result))
		return sprintf(buf, "-1\n");
	return sprintf(buf, "%lu\n", result);
}

static ssize_t store_ideapad_cam(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int ret, state;

	if (!count)
		return 0;
	if (sscanf(buf, "%i", &state) != 1)
		return -EINVAL;
	ret = write_ec_cmd(ideapad_handle, 0x1E, state);
	if (ret < 0)
		return ret;
	return count;
}

static DEVICE_ATTR(camera_power, 0644, show_ideapad_cam, store_ideapad_cam);

static ssize_t show_ideapad_cfg(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	struct ideapad_private *priv = dev_get_drvdata(dev);

	return sprintf(buf, "0x%.8lX\n", priv->cfg);
}

static DEVICE_ATTR(cfg, 0444, show_ideapad_cfg, NULL);


/******** mike 20110901 *********/

static size_t device_read( struct device *dev , struct device_attribute *attr , char *buf )
{
	struct ideapad_private *priv = dev_get_drvdata(dev);
	int cfg = priv->cfg;	

        if (test_bit(socket_cmd[EVENT_WIRELESS].cfgbit, (unsigned long *)&cfg))
        	device_exist |= 0x01;
	          
        if (test_bit(socket_cmd[EVENT_3G].cfgbit, (unsigned long *)&cfg))
                device_exist |= 0x02;

	if (test_bit(socket_cmd[EVENT_BLUETOOTH].cfgbit, (unsigned long *)&cfg))
                device_exist |= 0x04;       
	
        return sprintf( buf , "%d\n" , device_exist );
}

static DEVICE_ATTR(device, 0644, device_read, NULL);

static size_t enable_read( struct device *dev , struct device_attribute *attr , char *buf )
{
	unsigned long vdat;
	int i;

        for ( i = EVENT_WIRELESS ; i <= EVENT_BLUETOOTH ; i++ )
        {
                if (read_ec_data( ideapad_handle , socket_cmd[i].r_cmd , &vdat))
                {
                        printk("read cmd %x failure!\n", (socket_cmd[i].r_cmd) );
                }

                device_enable = ( device_enable & ~( 1  << (i-2)))  |  ( vdat << (i-2)) ;
        }
	
	return sprintf( buf , "%d\n" , device_enable );
}

static size_t enable_write( struct device *dev , struct device_attribute *attr , char *buf , size_t count )
{
	unsigned long vdat;
	int i;
	int data;
	struct ideapad_private *priv = dev_get_drvdata(&acpi_device_global->dev);

	sscanf( buf , "%d\n" , &device_enable );

	//printk("value input = %d\n" , device_enable);
	//printk("wmi handle = %d\n", ideapad_handle->handle );

	for ( i = EVENT_WIRELESS ; i <= EVENT_BLUETOOTH ; i++ )
	{
		if ( test_bit( i-2 , (unsigned long *)&device_enable) )
			data = 1;
		else
			data = 0;

		printk(" before: bit %d = %d, data = %d\n", i - 2 , test_bit( i-2 , (unsigned long *)&device_enable) , data);

		
		if (write_ec_cmd( ideapad_handle , (socket_cmd[i].r_cmd+1) , data ))
                {
                        printk("write cmd %x failure!\n", (socket_cmd[i].r_cmd+1) );
                }

		if (read_ec_data( ideapad_handle , socket_cmd[i].r_cmd , &vdat))
        	{	
                	printk("read cmd %x failure!\n", (socket_cmd[i].r_cmd) );
                }
		
		device_enable = ( device_enable & ~( 1  << (i-2)))  |  ( vdat << (i-2)) ;

		printk(" after: device_enable =  %d , vdat =  %d\n", device_enable , vdat);	

		//sync rfkill state
		if( i == EVENT_WIRELESS )
			rfkill_set_hw_state(priv->rfk[0], !vdat);
   
	}

	return count;
}

static DEVICE_ATTR(enable, 0666, enable_read , enable_write);


static struct attribute *ideapad_attributes[] = {
	&dev_attr_camera_power.attr,
	&dev_attr_cfg.attr,
	&dev_attr_device.attr,  //mike add
	&dev_attr_enable.attr,  //mike add
	NULL
};

/************ end ***************/

static mode_t ideapad_is_visible(struct kobject *kobj,
				 struct attribute *attr,
				 int idx)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct ideapad_private *priv = dev_get_drvdata(dev);
	bool supported;

	if (attr == &dev_attr_camera_power.attr)
		supported = test_bit(CFG_CAMERA_BIT, &(priv->cfg));
	else
		supported = true;

	return supported ? attr->mode : 0;
}

static struct attribute_group ideapad_attribute_group = {
	.is_visible = ideapad_is_visible,
	.attrs = ideapad_attributes
};

/*
 * Rfkill
 */
struct ideapad_rfk_data {
	char *name;
	int cfgbit;
	int opcode;
	int type;
};

const struct ideapad_rfk_data ideapad_rfk_data[] = {
	{ "ideapad_wlan",      CFG_WIFI_BIT, 0x15, RFKILL_TYPE_WLAN },
	{ "ideapad_bluetooth", CFG_BT_BIT,   0x17, RFKILL_TYPE_BLUETOOTH },
	{ "ideapad_3g",        CFG_3G_BIT,   0x20, RFKILL_TYPE_WWAN },
};

static int ideapad_rfk_set(void *data, bool blocked)
{
	unsigned long opcode = (unsigned long)data;

	return write_ec_cmd(ideapad_handle, opcode, !blocked);
}

static struct rfkill_ops ideapad_rfk_ops = {
	.set_block = ideapad_rfk_set,
};

static void ideapad_sync_rfk_state(struct acpi_device *adevice)
{
	struct ideapad_private *priv = dev_get_drvdata(&adevice->dev);
	unsigned long hw_blocked;
	int i;

	if (read_ec_data(ideapad_handle, 0x23, &hw_blocked))
		return;
	hw_blocked = !hw_blocked;

	for (i = 0; i < IDEAPAD_RFKILL_DEV_NUM; i++)
		if (priv->rfk[i])
			rfkill_set_hw_state(priv->rfk[i], hw_blocked);
}

static int __devinit ideapad_register_rfkill(struct acpi_device *adevice,
					     int dev)
{
	struct ideapad_private *priv = dev_get_drvdata(&adevice->dev);
	int ret;
	unsigned long sw_blocked;

	if (no_bt_rfkill &&
	    (ideapad_rfk_data[dev].type == RFKILL_TYPE_BLUETOOTH)) {
		/* Force to enable bluetooth when no_bt_rfkill=1 */
		write_ec_cmd(ideapad_handle,
			     ideapad_rfk_data[dev].opcode, 1);
		printk("@@@ bluetooth @@@\n");
		return 0;
	}

	priv->rfk[dev] = rfkill_alloc(ideapad_rfk_data[dev].name, &adevice->dev,
				      ideapad_rfk_data[dev].type, &ideapad_rfk_ops,
				      (void *)(long)dev);
	if (!priv->rfk[dev])
		return -ENOMEM;

	if (read_ec_data(ideapad_handle, ideapad_rfk_data[dev].opcode-1,
			 &sw_blocked)) {
		rfkill_init_sw_state(priv->rfk[dev], 0);
	} else {
		sw_blocked = !sw_blocked;
		rfkill_init_sw_state(priv->rfk[dev], sw_blocked);
	}

	ret = rfkill_register(priv->rfk[dev]);
	if (ret) {
		rfkill_destroy(priv->rfk[dev]);
		return ret;
	}
	return 0;
}

static void ideapad_unregister_rfkill(struct acpi_device *adevice, int dev)
{
	struct ideapad_private *priv = dev_get_drvdata(&adevice->dev);

	if (!priv->rfk[dev])
		return;

	rfkill_unregister(priv->rfk[dev]);
	rfkill_destroy(priv->rfk[dev]);
}

/*
 * Platform device
 */
static int __devinit ideapad_platform_init(struct ideapad_private *priv)
{
	int result;

	priv->platform_device = platform_device_alloc("ideapad", -1);
	if (!priv->platform_device)
		return -ENOMEM;
	platform_set_drvdata(priv->platform_device, priv);

	result = platform_device_add(priv->platform_device);
	if (result)
		goto fail_platform_device;

	result = sysfs_create_group(&priv->platform_device->dev.kobj,
				    &ideapad_attribute_group);
	if (result)
		goto fail_sysfs;
	return 0;

fail_sysfs:
	platform_device_del(priv->platform_device);
fail_platform_device:
	platform_device_put(priv->platform_device);
	return result;
}

static void ideapad_platform_exit(struct ideapad_private *priv)
{
	sysfs_remove_group(&priv->platform_device->dev.kobj,
			   &ideapad_attribute_group);
	platform_device_unregister(priv->platform_device);
}

/*
 * input device
 */
static const struct key_entry ideapad_keymap[] = {
	{ KE_KEY, 0x06, { KEY_SWITCHVIDEOMODE } },
	{ KE_KEY, 0x0D, { KEY_WLAN } },
	{ KE_END, 0 },
};

static int __devinit ideapad_input_init(struct ideapad_private *priv)
{
	struct input_dev *inputdev;
	int error;

	inputdev = input_allocate_device();
	if (!inputdev) {
		pr_info("Unable to allocate input device\n");
		return -ENOMEM;
	}

	inputdev->name = "Ideapad extra buttons";
	inputdev->phys = "ideapad/input0";
	inputdev->id.bustype = BUS_HOST;
	inputdev->dev.parent = &priv->platform_device->dev;

	error = sparse_keymap_setup(inputdev, ideapad_keymap, NULL);
	if (error) {
		pr_err("Unable to setup input device keymap\n");
		goto err_free_dev;
	}

	error = input_register_device(inputdev);
	if (error) {
		pr_err("Unable to register input device\n");
		goto err_free_keymap;
	}

	priv->inputdev = inputdev;
	return 0;

err_free_keymap:
	sparse_keymap_free(inputdev);
err_free_dev:
	input_free_device(inputdev);
	return error;
}

static void ideapad_input_exit(struct ideapad_private *priv)
{
	sparse_keymap_free(priv->inputdev);
	input_unregister_device(priv->inputdev);
	priv->inputdev = NULL;
}

static void ideapad_input_report(struct ideapad_private *priv,
				 unsigned long scancode)
{
	sparse_keymap_report_event(priv->inputdev, scancode, 1, true);
}

/*
 * backlight
 */
static int ideapad_backlight_get_brightness(struct backlight_device *blightdev)
{
	unsigned long now;

	if (read_ec_data(ideapad_handle, 0x12, &now))
		return -EIO;
	return now;
}

static int ideapad_backlight_update_status(struct backlight_device *blightdev)
{
	if (write_ec_cmd(ideapad_handle, 0x13, blightdev->props.brightness))
		return -EIO;
	if (write_ec_cmd(ideapad_handle, 0x33,
			 blightdev->props.power == FB_BLANK_POWERDOWN ? 0 : 1))
		return -EIO;

	return 0;
}

static const struct backlight_ops ideapad_backlight_ops = {
	.get_brightness = ideapad_backlight_get_brightness,
	.update_status = ideapad_backlight_update_status,
};

static int ideapad_backlight_init(struct ideapad_private *priv)
{
	struct backlight_device *blightdev;
	struct backlight_properties props;
	unsigned long max, now, power;

	if (read_ec_data(ideapad_handle, 0x11, &max))
		return -EIO;
	if (read_ec_data(ideapad_handle, 0x12, &now))
		return -EIO;
	if (read_ec_data(ideapad_handle, 0x18, &power))
		return -EIO;

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = max;
	props.type = BACKLIGHT_PLATFORM;
	blightdev = backlight_device_register("ideapad",
					      &priv->platform_device->dev,
					      priv,
					      &ideapad_backlight_ops,
					      &props);
	if (IS_ERR(blightdev)) {
		pr_err("Could not register backlight device\n");
		return PTR_ERR(blightdev);
	}

	priv->blightdev = blightdev;
	blightdev->props.brightness = now;
	blightdev->props.power = power ? FB_BLANK_UNBLANK : FB_BLANK_POWERDOWN;
	backlight_update_status(blightdev);

	return 0;
}

static void ideapad_backlight_exit(struct ideapad_private *priv)
{
	if (priv->blightdev)
		backlight_device_unregister(priv->blightdev);
	priv->blightdev = NULL;
}

static void ideapad_backlight_notify_power(struct ideapad_private *priv)
{
	unsigned long power;
	struct backlight_device *blightdev = priv->blightdev;

	if (read_ec_data(ideapad_handle, 0x18, &power))
		return;
	blightdev->props.power = power ? FB_BLANK_UNBLANK : FB_BLANK_POWERDOWN;
}

static void ideapad_backlight_notify_brightness(struct ideapad_private *priv)
{
	unsigned long now;

	/* if we control brightness via acpi video driver */
	if (priv->blightdev == NULL) {
		read_ec_data(ideapad_handle, 0x12, &now);
		return;
	}

	backlight_force_update(priv->blightdev, BACKLIGHT_UPDATE_HOTKEY);
}

/******** mike 20110901 *********/

static int read_status( acpi_handle handle  , int dev_type )
{
	unsigned long vdat;
	struct sk_buff *skb = NULL;
        struct nlmsghdr *nlh;
	char key_str[16];

	if (read_ec_data(handle, socket_cmd[dev_type].r_cmd , &vdat))
        	return -1;        
	
	printk("\nBefore: %d status is %d", dev_type ,vdat);
	skb = alloc_skb(NLMSG_SPACE(1024), GFP_ATOMIC);
        if(!skb)
	{
		printk(KERN_ERR"\n@@@ allocate skb fail!");
		return -1;
	}	
	nlh = NLMSG_PUT(skb , 0 , 0 , NLMSG_DONE , NLMSG_SPACE(1024));


	if( vdat == 0)
	{
		sprintf(key_str, "%s", socket_cmd[dev_type].on_cmd );
		strncpy(NLMSG_DATA(nlh) , key_str , socket_cmd[dev_type].on_cmd_len + 1 );
	}
	else
	{
		sprintf(key_str, "%s", socket_cmd[dev_type].off_cmd );
                strncpy(NLMSG_DATA(nlh) , key_str , socket_cmd[dev_type].off_cmd_len + 1 );
	}	


	switch(dev_type)
	{
		case EVENT_WIRELESS:
			if( vdat == 0 )
			{
	                	//if (write_ec_cmd( handle , socket_cmd[EVENT_WIRELESS].r_cmd+1 , 1))
                        	//	return -1;

				//if (write_ec_cmd( handle , socket_cmd[EVENT_BLUETOOTH].r_cmd+1 , 1))
                                //	return -1;
				sprintf(key_str, "wireless on" );
				strncpy(NLMSG_DATA(nlh) , key_str , 12);
			}
			else
			{
                                //if (write_ec_cmd( handle , socket_cmd[EVENT_WIRELESS].r_cmd+1 , 0))
                                //        return -1;
                                //if (write_ec_cmd( handle , socket_cmd[EVENT_BLUETOOTH].r_cmd+1 , 0))
                                //        return -1;
 				sprintf(key_str, "wireless off" );
				strncpy(NLMSG_DATA(nlh) , key_str , 13);
			}
			break;

		case EVENT_CAMARA:
			if( vdat == 0 )
			{
				sprintf(key_str, "camara off" );
				strncpy(NLMSG_DATA(nlh) , key_str , 11 );
			}
			else
			{
				sprintf(key_str, "camara on" );
				strncpy(NLMSG_DATA(nlh) , key_str , 10);
			}
			break;

		case EVENT_TOUCHPAD:   //inverse
			if ( vdat == 0 )
			{
				sprintf(key_str, "touchpad on" );
				strncpy(NLMSG_DATA(nlh) , key_str , 12 );
			}
			else
			{
				sprintf(key_str, "touchpad off" );
				strncpy(NLMSG_DATA(nlh) , key_str , 13 );
			}
			break;

		case EVENT_RESOL_CHANGE:
			sprintf(key_str, "resol change" );
                        strncpy(NLMSG_DATA(nlh) , key_str , 13 );

		case EVENT_BRIGHTNESS:
			sprintf(key_str, "brightness" );
                        strncpy(NLMSG_DATA(nlh) , key_str , 11 );


		default:
			break;
	}

	printk("\nAfter: %d status is %d", dev_type , vdat);

	NETLINK_CB(skb).dst_group = 1;

	if (lenovo_sock == NULL)
		printk(KERN_ERR"\n@@@ lenovo_sock is null!");
	else
		netlink_broadcast(lenovo_sock , skb , 0 , 1 , GFP_KERNEL );
	
	if (read_ec_data( handle , socket_cmd[dev_type].r_cmd , &vdat ))
        	return -1;

	return 0;

nlmsg_failure:
	printk(KERN_ERR"\n@@@ mike!");
}
/************ end ***************/


/*
 * module init/exit
 */
static const struct acpi_device_id ideapad_device_ids[] = {
	{ "VPC2004", 0},
	{ "", 0},
};
MODULE_DEVICE_TABLE(acpi, ideapad_device_ids);

static int __devinit ideapad_acpi_add(struct acpi_device *adevice)
{
	int ret, i;
	unsigned long cfg;
	struct ideapad_private *priv;

	if (read_method_int(adevice->handle, "_CFG", (int *)&cfg))
		return -ENODEV;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	dev_set_drvdata(&adevice->dev, priv);
	ideapad_handle = adevice->handle;
	priv->cfg = cfg;

	//set acpi device global , mike add
	acpi_device_global = adevice;

	ret = ideapad_platform_init(priv);
	if (ret)
		goto platform_failed;

	ret = ideapad_input_init(priv);
	if (ret)
		goto input_failed;

	for (i = 0; i < IDEAPAD_RFKILL_DEV_NUM; i++) {
		if (test_bit(ideapad_rfk_data[i].cfgbit, &cfg))
			ideapad_register_rfkill(adevice, i);
		else
			priv->rfk[i] = NULL;
	}
	ideapad_sync_rfk_state(adevice);

	if (!acpi_video_backlight_support()) {
		ret = ideapad_backlight_init(priv);
		if (ret && ret != -ENODEV)
			goto backlight_failed;
	}

	return 0;

backlight_failed:
	for (i = 0; i < IDEAPAD_RFKILL_DEV_NUM; i++)
		ideapad_unregister_rfkill(adevice, i);
	ideapad_input_exit(priv);   

input_failed:
	ideapad_platform_exit(priv);
platform_failed:
	kfree(priv);
	return ret;
}

static int __devexit ideapad_acpi_remove(struct acpi_device *adevice, int type)
{
	struct ideapad_private *priv = dev_get_drvdata(&adevice->dev);
	int i;

	ideapad_backlight_exit(priv);
	for (i = 0; i < IDEAPAD_RFKILL_DEV_NUM; i++)
		ideapad_unregister_rfkill(adevice, i);

	ideapad_input_exit(priv);
	ideapad_platform_exit(priv);
	dev_set_drvdata(&adevice->dev, NULL);
	kfree(priv);

	return 0;
}

static void ideapad_acpi_notify(struct acpi_device *adevice, u32 event)
{
	struct ideapad_private *priv = dev_get_drvdata(&adevice->dev);
	acpi_handle handle = adevice->handle;
	unsigned long vpc1, vpc2, vpc_bit;

	if (read_ec_data(handle, 0x10, &vpc1))
		return;
	if (read_ec_data(handle, 0x1A, &vpc2))
		return;

	vpc1 = (vpc2 << 8) | vpc1;
	for (vpc_bit = 0; vpc_bit < 16; vpc_bit++) {
		if (test_bit(vpc_bit, &vpc1)) {

/******** mike 20110901 *********/

			switch( vpc_bit )
			{
				
				case 0:
					printk("\none key!");
					break;
                                case 1:
                                        printk("\ngeneral!");
                                        break;
				
				case 2:
					//ideapad_backlight_notify_power(priv);
                                        printk("\nInverter: Fn+F2 press!");
                                        break;
				case 3:
					printk("\nNovo!");
					break;
				case 4:
					ideapad_backlight_notify_brightness(priv);
					read_status( ideapad_handle , EVENT_BRIGHTNESS );
					printk("\nBrightness: Fn+up or down  press!");	
					break;	
				case 5:
					printk("\nTouchPad: Fn+F6 press!");
					read_status( ideapad_handle , EVENT_TOUCHPAD );
					break;
				case 6:
					printk("\nDisplay!");
					break;		
				case 7:	
					printk("\nCamera: Fn+Esc press!");
					read_status( ideapad_handle , EVENT_CAMARA );
					
					break;		
				case 9:
					ideapad_sync_rfk_state(adevice);
					printk("\nKill sw!");
					break;
				
				case 10:
					printk("\nUser self define!");
					break;
				case 11:
					printk("\nswitch display resolution: Fn+F4 press!");
					read_status( ideapad_handle , EVENT_RESOL_CHANGE );
                                        break;
                                case 12:
                                        printk("\nswitch EQ");
				        break;	                                
				case 13:
                                        printk("\nApp control RF!");
					read_status( ideapad_handle , EVENT_WIRELESS );
                                        break;
				
				default:
					ideapad_input_report(priv, vpc_bit);
					break;
/************ end ***************/
			}
		}
	}
}

static struct acpi_driver ideapad_acpi_driver = {
	.name = "ideapad_acpi",
	.class = "IdeaPad",
	.ids = ideapad_device_ids,
	.ops.add = ideapad_acpi_add,
	.ops.remove = ideapad_acpi_remove,
	.ops.notify = ideapad_acpi_notify,
	.owner = THIS_MODULE,
};

static int __init ideapad_acpi_module_init(void)
{

/******** mike 20110901 *********/

        lenovo_sock = netlink_kernel_create ( &init_net , NETLINK_LITE_HOTKEY , 0 , NULL, NULL, THIS_MODULE );
        if(lenovo_sock == NULL)
	{
                printk(KERN_ERR"\n@@@@ create lenovo netlink sock fail!");
	}
	else
	{	
		printk(KERN_ERR"\n@@@@ create lenovo netlink sock success!");
	}

/************ end ***************/
	return acpi_bus_register_driver(&ideapad_acpi_driver);
}

static void __exit ideapad_acpi_module_exit(void)
{
	acpi_bus_unregister_driver(&ideapad_acpi_driver);
	/******** mike 2011-10-21 *********/	
	sock_release(lenovo_sock->sk_socket);
	/************ end ***************/
}

MODULE_AUTHOR("David Woodhouse <dwmw2@infradead.org>");
MODULE_DESCRIPTION("IdeaPad ACPI Extras");
MODULE_LICENSE("GPL");

module_init(ideapad_acpi_module_init);
module_exit(ideapad_acpi_module_exit);
