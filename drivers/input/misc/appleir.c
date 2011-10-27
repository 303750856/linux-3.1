/*
 * appleir: USB driver for the apple ir device
 *
 * Original driver written by James McKenzie
 * Ported to recent 2.6 kernel versions by Greg Kroah-Hartman <gregkh@suse.de>
 *
 * Copyright (C) 2006 James McKenzie
 * Copyright (C) 2008 Greg Kroah-Hartman <greg@kroah.com>
 * Copyright (C) 2008 Novell Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, version 2.
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/usb/input.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/usb/input.h>
#include <asm/unaligned.h>
#include <asm/byteorder.h>

#define DRIVER_VERSION "v1.2"
#define DRIVER_AUTHOR "James McKenzie"
#define DRIVER_DESC "Apple infrared receiver driver"
#define DRIVER_LICENSE "GPL"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);

#define USB_VENDOR_ID_APPLE			0x05ac
#define USB_DEVICE_ID_APPLE_IRCONTROL		0x8240
#define USB_DEVICE_ID_APPLE_IRCONTROL2		0x1440
#define USB_DEVICE_ID_APPLE_IRCONTROL3		0x8241
#define USB_DEVICE_ID_APPLE_IRCONTROL4		0x8242
#define USB_DEVICE_ID_APPLE_IRCONTROL5		0x8243

#define URB_SIZE	32

#define MAX_KEYS	9
#define MAX_KEYS_MASK	(MAX_KEYS - 1)

#define dbginfo(dev, format, arg...) do { if (debug) dev_info(dev , format , ## arg); } while (0)

static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Enable extra debug messages and information");

/* I have two devices both of which report the following */
/* 25 87 ee 83 0a  	+  */
/* 25 87 ee 83 0c  	-  */
/* 25 87 ee 83 09	<< */
/* 25 87 ee 83 06	>> */
/* 25 87 ee 83 05	>" */
/* 25 87 ee 83 03	menu */
/* 26 00 00 00 00	for key repeat*/

/* Thomas Glanzmann reports the following responses */
/* 25 87 ee ca 0b	+  */
/* 25 87 ee ca 0d	-  */
/* 25 87 ee ca 08	<< */
/* 25 87 ee ca 07	>> */
/* 25 87 ee ca 04	>" */
/* 25 87 ee ca 02 	menu */
/* 26 00 00 00 00       for key repeat*/
/* He also observes the following event sometimes */
/* sent after a key is release, which I interpret */
/* as a flat battery message */
/* 25 87 e0 ca 06	flat battery */

/* Alexandre Karpenko reports the following responses for Device ID 0x8242 */
/* 25 87 ee 47 0b	+  */
/* 25 87 ee 47 0d	-  */
/* 25 87 ee 47 08	<< */
/* 25 87 ee 47 07	>> */
/* 25 87 ee 47 04	>" */
/* 25 87 ee 47 02 	menu */
/* 26 87 ee 47 ** 	for key repeat (** is the code of the key being held) */

/* Bastien Nocera's "new" remote */
/* 25 87 ee 91 5f	followed by
 * 25 87 ee 91 05	gives you >"
 *
 * 25 87 ee 91 5c	followed by
 * 25 87 ee 91 05	gives you the middle button */

static const unsigned short appleir_key_table[] = {
	KEY_RESERVED,
	KEY_MENU,
	KEY_PLAYPAUSE,
	KEY_FORWARD,
	KEY_BACK,
	KEY_VOLUMEUP,
	KEY_VOLUMEDOWN,
	KEY_ENTER,
	KEY_RESERVED,
};

struct appleir {
	struct input_dev *input_dev;
	unsigned short keymap[ARRAY_SIZE(appleir_key_table)];
	u8 *data;
	dma_addr_t dma_buf;
	struct usb_device *usbdev;
	unsigned int flags;
	struct urb *urb;
	struct timer_list key_up_timer;
	int current_key;
	int prev_key_idx;
	char phys[32];
};

static DEFINE_MUTEX(appleir_mutex);

enum {
	APPLEIR_OPENED = 0x1,
	APPLEIR_SUSPENDED = 0x2,
};

static struct usb_device_id appleir_ids[] = {
	{ USB_DEVICE(USB_VENDOR_ID_APPLE, USB_DEVICE_ID_APPLE_IRCONTROL) },
	{ USB_DEVICE(USB_VENDOR_ID_APPLE, USB_DEVICE_ID_APPLE_IRCONTROL2) },
	{ USB_DEVICE(USB_VENDOR_ID_APPLE, USB_DEVICE_ID_APPLE_IRCONTROL3) },
	{ USB_DEVICE(USB_VENDOR_ID_APPLE, USB_DEVICE_ID_APPLE_IRCONTROL4) },
	{ USB_DEVICE(USB_VENDOR_ID_APPLE, USB_DEVICE_ID_APPLE_IRCONTROL5) },
	{}
};
MODULE_DEVICE_TABLE(usb, appleir_ids);

static void dump_packet(struct appleir *appleir, char *msg, u8 *data, int len)
{
	int i;

	printk(KERN_ERR "appleir: %s (%d bytes)", msg, len);

	for (i = 0; i < len; ++i)
		printk(" %02x", data[i]);
	printk(" (should be command %d)\n", (data[4] >> 1) & MAX_KEYS_MASK);
}

static int get_key(int data)
{
	switch (data) {
	case 0x02:
	case 0x03:
		/* menu */
		return 1;
	case 0x04:
	case 0x05:
		/* >" */
		return 2;
	case 0x06:
	case 0x07:
		/* >> */
		return 3;
	case 0x08:
	case 0x09:
		/* << */
		return 4;
	case 0x0a:
	case 0x0b:
		/* + */
		return 5;
	case 0x0c:
	case 0x0d:
		/* - */
		return 6;
	case 0x5c:
		/* Middle button, on newer remotes,
		 * part of a 2 packet-command */
		return -7;
	default:
		return -1;
	}
}

static void key_up(struct appleir *appleir, int key)
{
	dbginfo(&appleir->input_dev->dev, "key %d up\n", key);
	input_report_key(appleir->input_dev, key, 0);
	input_sync(appleir->input_dev);
}

static void key_down(struct appleir *appleir, int key)
{
	dbginfo(&appleir->input_dev->dev, "key %d down\n", key);
	input_report_key(appleir->input_dev, key, 1);
	input_sync(appleir->input_dev);
}

static void battery_flat(struct appleir *appleir)
{
	dev_err(&appleir->input_dev->dev, "possible flat battery?\n");
}

static void key_up_tick(unsigned long data)
{
	struct appleir *appleir = (struct appleir *)data;

	if (appleir->current_key) {
		key_up(appleir, appleir->current_key);
		appleir->current_key = 0;
	}
}

static void new_data(struct appleir *appleir, u8 *data, int len)
{
	static const u8 keydown[] = { 0x25, 0x87, 0xee };
	static const u8 keyrepeat[] = { 0x26, };
	static const u8 flatbattery[] = { 0x25, 0x87, 0xe0 };

	if (debug)
		dump_packet(appleir, "received", data, len);

	if (len != 5)
		return;

	if (!memcmp(data, keydown, sizeof(keydown))) {
		int index;

		/* If we already have a key down, take it up before marking
		   this one down */
		if (appleir->current_key)
			key_up(appleir, appleir->current_key);

		/* Handle dual packet commands */
		if (appleir->prev_key_idx > 0)
			index = appleir->prev_key_idx;
		else
			index = get_key(data[4]);

		if (index > 0) {
			appleir->current_key = appleir->keymap[index];

			key_down(appleir, appleir->current_key);
			/* Remote doesn't do key up, either pull them up, in the test
			   above, or here set a timer which pulls them up after 1/8 s */
			mod_timer(&appleir->key_up_timer, jiffies + HZ / 8);
			appleir->prev_key_idx = 0;
			return;
		} else if (index == -7) {
			/* Remember key for next packet */
			appleir->prev_key_idx = 0 - index;
			return;
		}
	}

	appleir->prev_key_idx = 0;

	if (!memcmp(data, keyrepeat, sizeof(keyrepeat))) {
		key_down(appleir, appleir->current_key);
		/* Remote doesn't do key up, either pull them up, in the test
		   above, or here set a timer which pulls them up after 1/8 s */
		mod_timer(&appleir->key_up_timer, jiffies + HZ / 8);
		return;
	}

	if (!memcmp(data, flatbattery, sizeof(flatbattery))) {
		battery_flat(appleir);
		/* Fall through */
	}

	dump_packet(appleir, "unknown packet", data, len);
}

static void appleir_urb(struct urb *urb)
{
	struct appleir *appleir = urb->context;
	int status = urb->status;
	int retval;

	switch (status) {
	case 0:
		new_data(appleir, urb->transfer_buffer, urb->actual_length);
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		/* This urb is terminated, clean up */
		dbginfo(&appleir->input_dev->dev, "%s - urb shutting down with status: %d", __func__,
			urb->status);
		return;
	default:
		dbginfo(&appleir->input_dev->dev, "%s - nonzero urb status received: %d", __func__,
			urb->status);
	}

	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval)
		err("%s - usb_submit_urb failed with result %d", __func__,
		    retval);
}

static int appleir_open(struct input_dev *dev)
{
	struct appleir *appleir = input_get_drvdata(dev);
	struct usb_interface *intf = usb_ifnum_to_if(appleir->usbdev, 0);
	int r;

	r = usb_autopm_get_interface(intf);
	if (r) {
		dev_err(&intf->dev,
			"%s(): usb_autopm_get_interface() = %d\n", __func__, r);
		return r;
	}

	mutex_lock(&appleir_mutex);

	if (usb_submit_urb(appleir->urb, GFP_ATOMIC)) {
		r = -EIO;
		goto fail;
	}

	appleir->flags |= APPLEIR_OPENED;

	mutex_unlock(&appleir_mutex);

	usb_autopm_put_interface(intf);

	return 0;
fail:
	mutex_unlock(&appleir_mutex);
	usb_autopm_put_interface(intf);
	return r;
}

static void appleir_close(struct input_dev *dev)
{
	struct appleir *appleir = input_get_drvdata(dev);

	mutex_lock(&appleir_mutex);

	if (!(appleir->flags & APPLEIR_SUSPENDED)) {
		usb_kill_urb(appleir->urb);
		del_timer_sync(&appleir->key_up_timer);
	}

	appleir->flags &= ~APPLEIR_OPENED;

	mutex_unlock(&appleir_mutex);
}

static int appleir_probe(struct usb_interface *intf,
			 const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_endpoint_descriptor *endpoint;
	struct appleir *appleir = NULL;
	struct input_dev *input_dev;
	int retval = -ENOMEM;
	int i;

	appleir = kzalloc(sizeof(struct appleir), GFP_KERNEL);
	if (!appleir)
		goto allocfail;

	appleir->data = usb_alloc_coherent(dev, URB_SIZE, GFP_KERNEL,
					 &appleir->dma_buf);
	if (!appleir->data)
		goto usbfail;

	appleir->urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!appleir->urb)
		goto urbfail;

	appleir->usbdev = dev;

	input_dev = input_allocate_device();
	if (!input_dev)
		goto inputfail;

	appleir->input_dev = input_dev;

	usb_make_path(dev, appleir->phys, sizeof(appleir->phys));
	strlcpy(appleir->phys, "/input0", sizeof(appleir->phys));

	input_dev->name = "Apple Infrared Remote Controller";
	input_dev->phys = appleir->phys;
	usb_to_input_id(dev, &input_dev->id);
	input_dev->dev.parent = &intf->dev;
	input_dev->keycode = appleir->keymap;
	input_dev->keycodesize = sizeof(unsigned short);
	input_dev->keycodemax = ARRAY_SIZE(appleir->keymap);

	input_dev->evbit[0] = BIT(EV_KEY) | BIT(EV_REP);

	memcpy(appleir->keymap, appleir_key_table, sizeof(appleir->keymap));
	for (i = 0; i < ARRAY_SIZE(appleir_key_table); i++)
		set_bit(appleir->keymap[i], input_dev->keybit);
	clear_bit(KEY_RESERVED, input_dev->keybit);

	input_set_drvdata(input_dev, appleir);
	input_dev->open = appleir_open;
	input_dev->close = appleir_close;

	endpoint = &intf->cur_altsetting->endpoint[0].desc;

	usb_fill_int_urb(appleir->urb, dev,
			 usb_rcvintpipe(dev, endpoint->bEndpointAddress),
			 appleir->data, 8,
			 appleir_urb, appleir, endpoint->bInterval);

	appleir->urb->transfer_dma = appleir->dma_buf;
	appleir->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	setup_timer(&appleir->key_up_timer,
		    key_up_tick, (unsigned long) appleir);

	retval = input_register_device(appleir->input_dev);
	if (retval)
		goto inputfail;

	usb_set_intfdata(intf, appleir);

	return 0;

inputfail:
	input_free_device(appleir->input_dev);

urbfail:
	usb_free_urb(appleir->urb);

usbfail:
	usb_free_coherent(dev, URB_SIZE, appleir->data,
			appleir->dma_buf);

allocfail:
	kfree(appleir);

	return retval;
}

static void appleir_disconnect(struct usb_interface *intf)
{
	struct appleir *appleir = usb_get_intfdata(intf);

	usb_set_intfdata(intf, NULL);
	input_unregister_device(appleir->input_dev);
	usb_free_urb(appleir->urb);
	usb_free_coherent(interface_to_usbdev(intf), URB_SIZE,
			appleir->data, appleir->dma_buf);
	kfree(appleir);
}

static int appleir_suspend(struct usb_interface *interface,
			   pm_message_t message)
{
	struct appleir *appleir = usb_get_intfdata(interface);

	mutex_lock(&appleir_mutex);
	if (appleir->flags & APPLEIR_OPENED)
		usb_kill_urb(appleir->urb);

	appleir->flags |= APPLEIR_SUSPENDED;

	mutex_unlock(&appleir_mutex);

	return 0;
}

static int appleir_resume(struct usb_interface *interface)
{
	struct appleir *appleir;
	int r = 0;

	appleir = usb_get_intfdata(interface);

	mutex_lock(&appleir_mutex);
	if (appleir->flags & APPLEIR_OPENED) {
		struct usb_endpoint_descriptor *endpoint;

		endpoint = &interface->cur_altsetting->endpoint[0].desc;
		usb_fill_int_urb(appleir->urb, appleir->usbdev,
				 usb_rcvintpipe(appleir->usbdev, endpoint->bEndpointAddress),
				 appleir->data, 8,
				 appleir_urb, appleir, endpoint->bInterval);
		appleir->urb->transfer_dma = appleir->dma_buf;
		appleir->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

		/* And reset the USB device */
		if (usb_submit_urb(appleir->urb, GFP_ATOMIC))
			r = -EIO;
	}

	appleir->flags &= ~APPLEIR_SUSPENDED;

	mutex_unlock(&appleir_mutex);

	return r;
}

static struct usb_driver appleir_driver = {
	.name                 = "appleir",
	.probe                = appleir_probe,
	.disconnect           = appleir_disconnect,
	.suspend              = appleir_suspend,
	.resume               = appleir_resume,
	.reset_resume         = appleir_resume,
	.id_table             = appleir_ids,
};

static int __init appleir_init(void)
{
	return usb_register(&appleir_driver);
}

static void __exit appleir_exit(void)
{
	usb_deregister(&appleir_driver);
}

module_init(appleir_init);
module_exit(appleir_exit);
