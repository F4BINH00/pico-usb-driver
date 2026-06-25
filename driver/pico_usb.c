#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/kref.h>

#define USB_PICO_VID 0xCAFE
#define USB_PICO_PID 0x4001

#define DRIVER_NAME "pico_usb"
#define MAX_XFER    512

static int bulk_timeout_ms = 2000;
module_param(bulk_timeout_ms, int, 0644);
MODULE_PARM_DESC(bulk_timeout_ms, "Timeout (ms) for bulk USB transfers");

struct pico_usb_dev {
	struct usb_device       *udev;
	struct usb_interface    *interface;

	unsigned char            bulk_in_ep;
	unsigned char            bulk_out_ep;
	u16                      bulk_in_maxpktsz;

	u8                      *in_buf;
	size_t                   in_buf_len;

	struct mutex             io_mutex;
	struct kref              kref;
};

#define to_pico_usb_dev(d) container_of(d, struct pico_usb_dev, kref)

static struct usb_driver pico_usb_driver;

static void pico_usb_delete(struct kref *kref)
{
	struct pico_usb_dev *dev = to_pico_usb_dev(kref);

	usb_put_dev(dev->udev);
	kfree(dev->in_buf);
	kfree(dev);
}

static int pico_usb_open(struct inode *inode, struct file *file)
{
	struct usb_interface *interface;
	struct pico_usb_dev *dev;
	int subminor;

	subminor = iminor(inode);
	interface = usb_find_interface(&pico_usb_driver, subminor);
	if (!interface)
		return -ENODEV;

	dev = usb_get_intfdata(interface);
	if (!dev)
		return -ENODEV;

	kref_get(&dev->kref);
	file->private_data = dev;

	return 0;
}

static int pico_usb_release(struct inode *inode, struct file *file)
{
	struct pico_usb_dev *dev = file->private_data;

	if (dev)
		kref_put(&dev->kref, pico_usb_delete);

	return 0;
}

static ssize_t pico_usb_read(struct file *file, char __user *buffer,
			     size_t count, loff_t *ppos)
{
	struct pico_usb_dev *dev = file->private_data;
	int ret;
	int actual = 0;
	size_t xfer;

	if (!dev || !dev->udev)
		return -ENODEV;

	if (count == 0)
		return 0;

	xfer = min_t(size_t, count, min_t(size_t, dev->in_buf_len, (size_t)MAX_XFER));

	/* Only one in-flight transfer at a time. */
	if (mutex_lock_interruptible(&dev->io_mutex))
		return -ERESTARTSYS;

	ret = usb_bulk_msg(dev->udev,
			   usb_rcvbulkpipe(dev->udev, dev->bulk_in_ep),
			   dev->in_buf,
			   xfer,
			   &actual,
			   bulk_timeout_ms);

	mutex_unlock(&dev->io_mutex);

	if (ret) {
		/* Common: -ETIMEDOUT if no data (device didn't send). */
		return ret;
	}

	if (actual == 0)
		return 0;

	if (copy_to_user(buffer, dev->in_buf, actual))
		return -EFAULT;

	return actual;
}

static ssize_t pico_usb_write(struct file *file, const char __user *user_buffer,
			      size_t count, loff_t *ppos)
{
	struct pico_usb_dev *dev = file->private_data;
	u8 *buf;
	int ret;
	int actual = 0;
	size_t xfer;

	if (!dev || !dev->udev)
		return -ENODEV;

	if (count == 0)
		return 0;

	xfer = min_t(size_t, count, (size_t)MAX_XFER);

	buf = kmalloc(xfer, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, user_buffer, xfer)) {
		kfree(buf);
		return -EFAULT;
	}

	if (mutex_lock_interruptible(&dev->io_mutex)) {
		kfree(buf);
		return -ERESTARTSYS;
	}

	ret = usb_bulk_msg(dev->udev,
			   usb_sndbulkpipe(dev->udev, dev->bulk_out_ep),
			   buf,
			   xfer,
			   &actual,
			   bulk_timeout_ms);

	mutex_unlock(&dev->io_mutex);
	kfree(buf);

	if (ret)
		return ret;

	return actual;
}

static const struct file_operations pico_usb_fops = {
	.owner   = THIS_MODULE,
	.open    = pico_usb_open,
	.release = pico_usb_release,
	.read    = pico_usb_read,
	.write   = pico_usb_write,
	.llseek  = noop_llseek,
};

static struct usb_class_driver pico_usb_class = {
	/* udev will create /dev/pico_usb0, /dev/pico_usb1, ... */
	.name       = "pico_usb%d",
	.fops       = &pico_usb_fops,
	.minor_base = 192,
};

static int pico_usb_probe(struct usb_interface *interface,
			  const struct usb_device_id *id)
{
	struct pico_usb_dev *dev;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int i, retval = -ENOMEM;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	kref_init(&dev->kref);
	mutex_init(&dev->io_mutex);

	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;

	/* Parse endpoints. Expect 2 bulk endpoints: one IN, one OUT. */
	iface_desc = interface->cur_altsetting;

	for (i = 0; i < iface_desc->desc.bNumEndpoints; i++) {
		endpoint = &iface_desc->endpoint[i].desc;

		if (usb_endpoint_is_bulk_in(endpoint) && !dev->bulk_in_ep) {
			dev->bulk_in_ep = endpoint->bEndpointAddress;
			dev->bulk_in_maxpktsz = usb_endpoint_maxp(endpoint);
			dev->in_buf_len = max_t(size_t, dev->bulk_in_maxpktsz, (size_t)MAX_XFER);
			dev->in_buf = kmalloc(dev->in_buf_len, GFP_KERNEL);
			if (!dev->in_buf)
				goto error;
		}

		if (usb_endpoint_is_bulk_out(endpoint) && !dev->bulk_out_ep) {
			dev->bulk_out_ep = endpoint->bEndpointAddress;
		}
	}

	if (!dev->bulk_in_ep || !dev->bulk_out_ep) {
		retval = -ENODEV;
		goto error;
	}

	usb_set_intfdata(interface, dev);

	retval = usb_register_dev(interface, &pico_usb_class);
	if (retval) {
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	dev_info(&interface->dev,
		 "pico_usb connected: /dev/pico_usb%d (bulk in=0x%02x out=0x%02x)\n",
		 interface->minor - pico_usb_class.minor_base,
		 dev->bulk_in_ep, dev->bulk_out_ep);

	return 0;

error:
	if (dev)
		kref_put(&dev->kref, pico_usb_delete);
	return retval;
}

static void pico_usb_disconnect(struct usb_interface *interface)
{
	struct pico_usb_dev *dev = usb_get_intfdata(interface);

	usb_set_intfdata(interface, NULL);
	usb_deregister_dev(interface, &pico_usb_class);

	/*
	 * Drop the reference from probe(). open() holds its own kref.
	 */
	if (dev)
		kref_put(&dev->kref, pico_usb_delete);

	dev_info(&interface->dev, "pico_usb disconnected\n");
}

static const struct usb_device_id pico_usb_table[] = {
	{ USB_DEVICE(USB_PICO_VID, USB_PICO_PID) },
	{ } /* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, pico_usb_table);

static struct usb_driver pico_usb_driver = {
	.name       = DRIVER_NAME,
	.probe      = pico_usb_probe,
	.disconnect = pico_usb_disconnect,
	.id_table   = pico_usb_table,
};

module_usb_driver(pico_usb_driver);

MODULE_LICENSE("GPL");
