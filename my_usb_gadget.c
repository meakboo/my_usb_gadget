#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/usb/composite.h>

#include "g_zero.h"

struct f_gadget_trans
{
	struct usb_function	function;

	struct usb_ep		*in_ep;
	struct usb_ep		*out_ep;

	unsigned                qlen;
	unsigned                buflen;
};

static struct usb_interface_descriptor gadget_trans_intf = {
	.bLength =		sizeof(gadget_trans_intf),
	.bDescriptorType =	USB_DT_INTERFACE,

	.bNumEndpoints =	2,
	.bInterfaceClass =	USB_CLASS_VENDOR_SPEC,
	/* .iInterface = DYNAMIC */
};

/* full speed support: */

static struct usb_endpoint_descriptor fs_trans_source_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor fs_trans_sink_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *fs_trans_descs[] = {
	(struct usb_descriptor_header *) &gadget_trans_intf,
	(struct usb_descriptor_header *) &fs_trans_sink_desc,
	(struct usb_descriptor_header *) &fs_trans_source_desc,
	NULL,
};

/* high speed support: */

static struct usb_endpoint_descriptor hs_trans_source_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor hs_trans_sink_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_descriptor_header *hs_trans_descs[] = {
	(struct usb_descriptor_header *) &gadget_trans_intf,
	(struct usb_descriptor_header *) &hs_trans_source_desc,
	(struct usb_descriptor_header *) &hs_trans_sink_desc,
	NULL,
};

/* super speed support: */

static struct usb_endpoint_descriptor ss_trans_source_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_trans_source_comp_desc = {
	.bLength =		USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst =		0,
	.bmAttributes =		0,
	.wBytesPerInterval =	0,
};

static struct usb_endpoint_descriptor ss_trans_sink_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor ss_trans_sink_comp_desc = {
	.bLength =		USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType =	USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst =		0,
	.bmAttributes =		0,
	.wBytesPerInterval =	0,
};

static struct usb_descriptor_header *ss_trans_descs[] = {
	(struct usb_descriptor_header *) &gadget_trans_intf,
	(struct usb_descriptor_header *) &ss_trans_source_desc,
	(struct usb_descriptor_header *) &ss_trans_source_comp_desc,
	(struct usb_descriptor_header *) &ss_trans_sink_desc,
	(struct usb_descriptor_header *) &ss_trans_sink_comp_desc,
	NULL,
};

static struct usb_string strings_gadgettrans[] = {
	[0].s = "gadget trans data",
	{ }
};

static struct usb_gadget_strings stringtab_gadgettrans = {
	.language 	 = 0x0409,
	.strings     = strings_gadgettrans,
};

static struct usb_gadget_strings *gadgettrans_strings[] = {
	&stringtab_gadgettrans,
	NULL,
};

static inline struct f_gadget_trans *func_to_trans(struct usb_function *f)
{
	return container_of(f, struct f_gadget_trans, function);
}

static inline struct usb_request *gt_alloc_ep_req(struct usb_ep *ep, int len)
{
	return alloc_ep_req(ep, len);
}

static void disable_ep(struct usb_composite_dev *cdev, struct usb_ep *ep)
{
	int	value;

	value = usb_ep_disable(ep);
	if (value < 0)
	{
		DBG(cdev, "disable %s --> %d\n", ep->name, value);
	}

	return;
}

static void disable_endpoints(struct usb_composite_dev *cdev,
						struct usb_ep *in, struct usb_ep *out)
{
	disable_ep(cdev, in);
	disable_ep(cdev, out);

	return;
}

static int gadget_trans_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_gadget_trans  *gt = func_to_trans(f);
	int id;
	int ret;

	/* allocate interface ID(s) */
	id = usb_interface_id(c, f);
	if (id < 0)
	{
		return id;
	}
	gadget_trans_intf.bInterfaceNumber = id;

	/* allocate bulk endpoints */
	gt->in_ep = usb_ep_autoconfig(cdev->gadget, &fs_trans_source_desc);
	if(!gt->in_ep)
	{
autoconf_fail:
		ERROR(cdev, "%s: can't autoconfigure on %s\n",
			f->name, cdev->gadget->name);
		return -ENODEV;
	}
	gt->out_ep = usb_ep_autoconfig(cdev->gadget, &fs_trans_sink_desc);
	if(!gt->out_ep)
	{
		goto autoconf_fail;
	}

	/* support high speed hardware */
	hs_trans_source_desc.bEndpointAddress = fs_trans_source_desc.bEndpointAddress;
	hs_trans_sink_desc.bEndpointAddress = fs_trans_sink_desc.bEndpointAddress;

	/* support super speed hardware */
	ss_trans_source_desc.bEndpointAddress = fs_trans_source_desc.bEndpointAddress;
	ss_trans_sink_desc.bEndpointAddress = fs_trans_source_desc.bEndpointAddress;

	ret = usb_assign_descriptors(f, fs_trans_descs, hs_trans_descs, ss_trans_descs, NULL);
	if(ret)
	{
		return ret;
	}

	DBG(cdev, "%s speed %s: IN/%s, OUT/%s\n",
	    (gadget_is_superspeed(c->cdev->gadget) ? "super" :
	     (gadget_is_dualspeed(c->cdev->gadget) ? "dual" : "full")),
			f->name, gt->in_ep->name, gt->out_ep->name);	
	
	return 0;
}

static void disable_gadget_trans(struct f_gadget_trans *gt)
{
	struct usb_composite_dev	*cdev;

	dev = gt->function.config->cdev;
	disable_endpoints(cdev, gt->in_ep, gt->out_ep);

	VDBG(cdev, "%s disabled\n", gt->function.name);
	return;
}

static int enable_gadget_trans(struct usb_composite_dev *cdev, struct f_gadget_trans *gt, int alt)
{
	int ret = 0;
	struct usb_ep *ep;

	/* enable bulk endpoint write */
	ep = gt->in_ep;
	ret = config_ep_by_speed(cdev->gadget, &(gt->function), ep);
	if(ret)
	{
		return ret;
	}
	ret = usb_ep_enable(ep);
	if(ret < 0)
	{
		return ret;
	}
	ep->driver_data = gt;

	/* enable bulk endpoint read */
	ep = gt->out_ep;
	ret = config_ep_by_speed(cdev->gadget, &(gt->function), ep);
	if(ret)
	{
		return ret;
	}
	ret = usb_ep_enable(ep);
	if(ret < 0)
	{
		return ret;
	}
	ep->driver_data = gt;
	
	return 0;
}

static int gadgettrans_set_alt(struct usb_function *f, unsigned intf, unsigned alt)
{
	struct gadget_trans_intf  *gt = func_to_trans(f);
	struct usb_composite_dev	*cdev = f->config->cdev;

	disable_gadget_trans(gt);
	return enable_gadget_trans(cdev, gt, alt);
}

static void gadgettrans_get_alt(struct usb_function *f, unsigned intf)
{
	struct gadget_trans_intf  *gt = func_to_trans(f);

	return gt->cur_alt;
}

static void gadgettrans_disable(struct usb_function *f)
{
	struct gadget_trans_intf  *gt = func_to_trans(f);

	disable_gadget_trans(gt);
	return;
}

static void gadgettrans_free_func(struct usb_function *f)
{
	usb_free_all_descriptors(f);
	kfree(func_to_ss(f));
	
	return;
}

static int gadgettrans_setup(struct usb_function *f, const struct usb_ctrlrequest *ctrl)
{
	struct usb_configuration        *c = f->config;
	struct usb_request	*req = c->cdev->req;
	int			value = -EOPNOTSUPP;
	u16			w_index = le16_to_cpu(ctrl->wIndex);
	u16			w_value = le16_to_cpu(ctrl->wValue);
	u16			w_length = le16_to_cpu(ctrl->wLength);

	req->length = USB_COMP_EP0_BUFSIZ;

	

	return 0;
}

static struct usb_function *gadget_trans_alloc_func(struct usb_function_instance *fi)
{
	struct f_gadget_trans *gt;

	gt = kzalloc(sizeof(*gt), GFP_KERNEL);
	if(!gt)
	{
		return NULL;
	}

	gt->function.name = "gadget trans";
	gt->function.bind = gadget_trans_bind;
	gt->function.set_alt = gadgettrans_set_alt;
	gt->function.get_alt = gadgettrans_get_alt;
	gt->function.disable = gadgettrans_disable;
	gt->function.strings = gadgettrans_strings;
	gt->function.setup = gadgettrans_setup_plus;

	gt->function.free_func = gadgettrans_free_func;

	return &gt->function;
}
