#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/usb/ch9.h>
#include <linux/usbdevice_fs.h>

#include "omron.h"

static int omron_fd;

#define TIMEOUT_MILLISECS	0

#define OMRON_OUT_ENDPT	0x02
#define OMRON_IN_ENDPT  0x81

#define INPUT_LEN	8


/* Is there some standard header file for HID protocol information? */
#define HID_REPORT_TYPE_INPUT	1
#define HID_REPORT_TYPE_OUTPUT	2
#define HID_REPORT_TYPE_FEATURE	3

#define	USB_REQ_HID_GET_REPORT	0x01
#define	USB_REQ_HID_SET_REPORT	0x09

static const unsigned int omron_interface_num = 0;

static unsigned char mk_printable(unsigned char ch)
{
	return (ch >= ' ' && ch < 0x80) ? ch : '.';
}

static void hexdump(const char *prefix, void *data_orig, int len_orig)
{
	void *data = data_orig;
	int len = len_orig;

#if 0				/* for debugging */
	printf("%s",prefix);
	while (len--) {
		printf(" %02x", *(unsigned char*) data);
		data++;
	}

	printf("  ");
	data = data_orig;
	len = len_orig;
	while (len--) {
		putchar(mk_printable(*(unsigned char*) data));
		data++;
	}
	printf("\n");
	fflush(stdout);
#endif
}

int omron_get_count(int VID, int PID)
{
	return 1;
}

int omron_open(omron_device* dev, int VID, int PID, unsigned int device_index)
{
	int err;
	const char *dev_string;

	dev_string = getenv("OMRON_DEV");
	if (dev_string == NULL) {
	     fprintf(stderr, "Need OMRON_DEV environment variable to be set to something like /dev/bus/usb/001/004.  Do lsusb to see exactly which device.\n");
	     errno = ENODEV;
	     return -1;
	}

	omron_fd = open(dev_string, O_RDWR);
	if (omron_fd >= 0) {
		const struct usbdevfs_ioctl disconnect_ioctl = {
			.ifno		= 0,
			.ioctl_code	= USBDEVFS_DISCONNECT,
			.data		= NULL,
		};
		/* OK for this ioctl to fail if there is no driver attached: */
		ioctl(omron_fd, USBDEVFS_IOCTL, &disconnect_ioctl);

		err = ioctl(omron_fd, USBDEVFS_CLAIMINTERFACE,
			    &omron_interface_num);
		if (err < 0)
			perror("omron_open USBDEVFS_CLAIMINTERFACE");

	}
	return omron_fd;
}

int omron_close(omron_device* dev)
{
	int err;

	err = ioctl(omron_fd, USBDEVFS_RELEASEINTERFACE, &omron_interface_num);
	if (err < 0)
			perror("USBDEVFS_RELEASEINTERFACE");

	return close(omron_fd);
}

int omron_set_mode(omron_device* dev, omron_mode mode)
{
	char feature_report[2] = {(mode & 0xff00) >> 8, (mode & 0x00ff)};
	const int feature_report_id = 0;
	const int feature_interface_num = 0;
	int err;

	const struct usbdevfs_ctrltransfer set_mode_cmd = {
		/* FIXME: What is the 0x80 here symbolically? */
		.bRequestType	= USB_TYPE_CLASS | USB_RECIP_INTERFACE,
		.bRequest	= USB_REQ_HID_SET_REPORT,
		.wValue		= (HID_REPORT_TYPE_FEATURE << 8) | feature_report_id,
		.wIndex		= feature_interface_num,
		.wLength	= sizeof(feature_report),
		.timeout	= TIMEOUT_MILLISECS,
		.data		= feature_report,
		
	};

	err = ioctl(omron_fd, USBDEVFS_CONTROL, &set_mode_cmd);
	if (err < 0) {
		perror("omron_set_mode USBDEVFS_CONTROL");
		exit(1);
	}
	return 0;
}

int omron_read_data(omron_device* dev, unsigned char *input_report)
{
	int result;

	struct usbdevfs_bulktransfer xfer = {
		.ep	= OMRON_IN_ENDPT,
		.len	= INPUT_LEN,
		.timeout= TIMEOUT_MILLISECS,
		.data	= input_report,
	};

	result = ioctl(omron_fd, USBDEVFS_BULK, &xfer);
	if (result < 0) {
		perror("omron_read_data USBDEVFS_BULK");
		exit(1);
	}

	//hexdump("<", xfer.data, xfer.len);
	hexdump("<", xfer.data, ((unsigned char*) xfer.data)[0]+1);
	return result;
}

#if 0
int omron_read_data_from_control(omron_device* dev, unsigned char *input_report)
{
	const int input_report_id = 0;
	const int feature_interface_num = 0;
	int err;

	const struct usbdevfs_ctrltransfer get_report_cmd = {
		/* FIXME: What is the 0x80 here symbolically? */
		.bRequestType	= USB_TYPE_CLASS | USB_RECIP_INTERFACE | 0x80,
		.bRequest	= USB_REQ_HID_GET_REPORT,
		.wValue		= (HID_REPORT_TYPE_INPUT << 8) | input_report_id,
		.wIndex		= feature_interface_num,
		.wLength	= INPUT_LEN,
		.timeout	= TIMEOUT_MILLISECS,
		.data		= input_report, 
	};

	memset(input_report, 0xde, 9);
	err = ioctl(omron_fd, USBDEVFS_CONTROL, &get_report_cmd);
	hexdump("<",input_report, 9);
	if (err < 0) {
		perror("omron_read_data USBDEVFS_CONTROL");
		exit(1);
	}
	return INPUT_LEN;
}
#endif


int omron_write_data(omron_device* dev, unsigned char *output_report)
{
	int result;

	struct usbdevfs_bulktransfer xfer = {
		.ep	= OMRON_OUT_ENDPT,
		.len	= 8,	/* FIXME: Make a #define */
		.timeout= TIMEOUT_MILLISECS,
		.data	= output_report,
	};

	hexdump(">",xfer.data, xfer.len);
	result = ioctl(omron_fd, USBDEVFS_BULK, &xfer);
	if (result < 0) {
		perror("omron_write_data USBDEVFS_BULK");
		exit(1);
	}

	return result;
	
}
