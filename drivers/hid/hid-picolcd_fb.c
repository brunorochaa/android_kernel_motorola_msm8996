/***************************************************************************
 *   Copyright (C) 2010-2012 by Bruno Prémont <bonbons@linux-vserver.org>  *
 *                                                                         *
 *   Based on Logitech G13 driver (v0.4)                                   *
 *     Copyright (C) 2009 by Rick L. Vinyard, Jr. <rvinyard@cs.nmsu.edu>   *
 *                                                                         *
 *   This program is free software: you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, version 2 of the License.               *
 *                                                                         *
 *   This driver is distributed in the hope that it will be useful, but    *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU      *
 *   General Public License for more details.                              *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this software. If not see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/

#include <linux/hid.h>
#include <linux/vmalloc.h>
#include "usbhid/usbhid.h"
#include <linux/usb.h>

#include <linux/fb.h>
#include <linux/module.h>

#include "hid-picolcd.h"

/* Framebuffer
 *
 * The PicoLCD use a Topway LCD module of 256x64 pixel
 * This display area is tiled over 4 controllers with 8 tiles
 * each. Each tile has 8x64 pixel, each data byte representing
 * a 1-bit wide vertical line of the tile.
 *
 * The display can be updated at a tile granularity.
 *
 *       Chip 1           Chip 2           Chip 3           Chip 4
 * +----------------+----------------+----------------+----------------+
 * |     Tile 1     |     Tile 1     |     Tile 1     |     Tile 1     |
 * +----------------+----------------+----------------+----------------+
 * |     Tile 2     |     Tile 2     |     Tile 2     |     Tile 2     |
 * +----------------+----------------+----------------+----------------+
 *                                  ...
 * +----------------+----------------+----------------+----------------+
 * |     Tile 8     |     Tile 8     |     Tile 8     |     Tile 8     |
 * +----------------+----------------+----------------+----------------+
 */
#define PICOLCDFB_NAME "picolcdfb"
#define PICOLCDFB_WIDTH (256)
#define PICOLCDFB_HEIGHT (64)
#define PICOLCDFB_SIZE (PICOLCDFB_WIDTH * PICOLCDFB_HEIGHT / 8)

#define PICOLCDFB_UPDATE_RATE_LIMIT   10
#define PICOLCDFB_UPDATE_RATE_DEFAULT  2

/* Framebuffer visual structures */
static const struct fb_fix_screeninfo picolcdfb_fix = {
	.id          = PICOLCDFB_NAME,
	.type        = FB_TYPE_PACKED_PIXELS,
	.visual      = FB_VISUAL_MONO01,
	.xpanstep    = 0,
	.ypanstep    = 0,
	.ywrapstep   = 0,
	.line_length = PICOLCDFB_WIDTH / 8,
	.accel       = FB_ACCEL_NONE,
};

static const struct fb_var_screeninfo picolcdfb_var = {
	.xres           = PICOLCDFB_WIDTH,
	.yres           = PICOLCDFB_HEIGHT,
	.xres_virtual   = PICOLCDFB_WIDTH,
	.yres_virtual   = PICOLCDFB_HEIGHT,
	.width          = 103,
	.height         = 26,
	.bits_per_pixel = 1,
	.grayscale      = 1,
	.red            = {
		.offset = 0,
		.length = 1,
		.msb_right = 0,
	},
	.green          = {
		.offset = 0,
		.length = 1,
		.msb_right = 0,
	},
	.blue           = {
		.offset = 0,
		.length = 1,
		.msb_right = 0,
	},
	.transp         = {
		.offset = 0,
		.length = 0,
		.msb_right = 0,
	},
};

/* Send a given tile to PicoLCD */
static int picolcd_fb_send_tile(struct hid_device *hdev, int chip, int tile)
{
	struct picolcd_data *data = hid_get_drvdata(hdev);
	struct hid_report *report1 = picolcd_out_report(REPORT_LCD_CMD_DATA, hdev);
	struct hid_report *report2 = picolcd_out_report(REPORT_LCD_DATA, hdev);
	unsigned long flags;
	u8 *tdata;
	int i;

	if (!report1 || report1->maxfield != 1 || !report2 || report2->maxfield != 1)
		return -ENODEV;

	spin_lock_irqsave(&data->lock, flags);
	hid_set_field(report1->field[0],  0, chip << 2);
	hid_set_field(report1->field[0],  1, 0x02);
	hid_set_field(report1->field[0],  2, 0x00);
	hid_set_field(report1->field[0],  3, 0x00);
	hid_set_field(report1->field[0],  4, 0xb8 | tile);
	hid_set_field(report1->field[0],  5, 0x00);
	hid_set_field(report1->field[0],  6, 0x00);
	hid_set_field(report1->field[0],  7, 0x40);
	hid_set_field(report1->field[0],  8, 0x00);
	hid_set_field(report1->field[0],  9, 0x00);
	hid_set_field(report1->field[0], 10,   32);

	hid_set_field(report2->field[0],  0, (chip << 2) | 0x01);
	hid_set_field(report2->field[0],  1, 0x00);
	hid_set_field(report2->field[0],  2, 0x00);
	hid_set_field(report2->field[0],  3,   32);

	tdata = data->fb_vbitmap + (tile * 4 + chip) * 64;
	for (i = 0; i < 64; i++)
		if (i < 32)
			hid_set_field(report1->field[0], 11 + i, tdata[i]);
		else
			hid_set_field(report2->field[0], 4 + i - 32, tdata[i]);

	usbhid_submit_report(data->hdev, report1, USB_DIR_OUT);
	usbhid_submit_report(data->hdev, report2, USB_DIR_OUT);
	spin_unlock_irqrestore(&data->lock, flags);
	return 0;
}

/* Translate a single tile*/
static int picolcd_fb_update_tile(u8 *vbitmap, const u8 *bitmap, int bpp,
		int chip, int tile)
{
	int i, b, changed = 0;
	u8 tdata[64];
	u8 *vdata = vbitmap + (tile * 4 + chip) * 64;

	if (bpp == 1) {
		for (b = 7; b >= 0; b--) {
			const u8 *bdata = bitmap + tile * 256 + chip * 8 + b * 32;
			for (i = 0; i < 64; i++) {
				tdata[i] <<= 1;
				tdata[i] |= (bdata[i/8] >> (i % 8)) & 0x01;
			}
		}
	} else if (bpp == 8) {
		for (b = 7; b >= 0; b--) {
			const u8 *bdata = bitmap + (tile * 256 + chip * 8 + b * 32) * 8;
			for (i = 0; i < 64; i++) {
				tdata[i] <<= 1;
				tdata[i] |= (bdata[i] & 0x80) ? 0x01 : 0x00;
			}
		}
	} else {
		/* Oops, we should never get here! */
		WARN_ON(1);
		return 0;
	}

	for (i = 0; i < 64; i++)
		if (tdata[i] != vdata[i]) {
			changed = 1;
			vdata[i] = tdata[i];
		}
	return changed;
}

void picolcd_fb_refresh(struct picolcd_data *data)
{
	if (data->fb_info)
		schedule_delayed_work(&data->fb_info->deferred_work, 0);
}

/* Reconfigure LCD display */
int picolcd_fb_reset(struct picolcd_data *data, int clear)
{
	struct hid_report *report = picolcd_out_report(REPORT_LCD_CMD, data->hdev);
	int i, j;
	unsigned long flags;
	static const u8 mapcmd[8] = { 0x00, 0x02, 0x00, 0x64, 0x3f, 0x00, 0x64, 0xc0 };

	if (!report || report->maxfield != 1)
		return -ENODEV;

	spin_lock_irqsave(&data->lock, flags);
	for (i = 0; i < 4; i++) {
		for (j = 0; j < report->field[0]->maxusage; j++)
			if (j == 0)
				hid_set_field(report->field[0], j, i << 2);
			else if (j < sizeof(mapcmd))
				hid_set_field(report->field[0], j, mapcmd[j]);
			else
				hid_set_field(report->field[0], j, 0);
		usbhid_submit_report(data->hdev, report, USB_DIR_OUT);
	}

	data->status |= PICOLCD_READY_FB;
	spin_unlock_irqrestore(&data->lock, flags);

	if (data->fb_bitmap) {
		if (clear) {
			memset(data->fb_vbitmap, 0, PICOLCDFB_SIZE);
			memset(data->fb_bitmap, 0, PICOLCDFB_SIZE*data->fb_bpp);
		}
		data->fb_force = 1;
	}

	/* schedule first output of framebuffer */
	picolcd_fb_refresh(data);

	return 0;
}

/* Update fb_vbitmap from the screen_base and send changed tiles to device */
static void picolcd_fb_update(struct fb_info *info)
{
	int chip, tile, n;
	unsigned long flags;
	struct picolcd_data *data;

	mutex_lock(&info->lock);
	data = info->par;
	if (!data)
		goto out;

	spin_lock_irqsave(&data->lock, flags);
	if (!(data->status & PICOLCD_READY_FB)) {
		spin_unlock_irqrestore(&data->lock, flags);
		picolcd_fb_reset(data, 0);
	} else {
		spin_unlock_irqrestore(&data->lock, flags);
	}

	/*
	 * Translate the framebuffer into the format needed by the PicoLCD.
	 * See display layout above.
	 * Do this one tile after the other and push those tiles that changed.
	 *
	 * Wait for our IO to complete as otherwise we might flood the queue!
	 */
	n = 0;
	for (chip = 0; chip < 4; chip++)
		for (tile = 0; tile < 8; tile++)
			if (picolcd_fb_update_tile(data->fb_vbitmap,
					data->fb_bitmap, data->fb_bpp, chip, tile) ||
				data->fb_force) {
				n += 2;
				if (n >= HID_OUTPUT_FIFO_SIZE / 2) {
					mutex_unlock(&info->lock);
					usbhid_wait_io(data->hdev);
					mutex_lock(&info->lock);
					data = info->par;
					if (!data)
						goto out;
					spin_lock_irqsave(&data->lock, flags);
					if (data->status & PICOLCD_FAILED) {
						spin_unlock_irqrestore(&data->lock, flags);
						goto out;
					}
					spin_unlock_irqrestore(&data->lock, flags);
					n = 0;
				}
				picolcd_fb_send_tile(data->hdev, chip, tile);
			}
	data->fb_force = false;
	if (n) {
		mutex_unlock(&info->lock);
		usbhid_wait_io(data->hdev);
		return;
	}
out:
	mutex_unlock(&info->lock);
}

/* Stub to call the system default and update the image on the picoLCD */
static void picolcd_fb_fillrect(struct fb_info *info,
		const struct fb_fillrect *rect)
{
	if (!info->par)
		return;
	sys_fillrect(info, rect);

	schedule_delayed_work(&info->deferred_work, 0);
}

/* Stub to call the system default and update the image on the picoLCD */
static void picolcd_fb_copyarea(struct fb_info *info,
		const struct fb_copyarea *area)
{
	if (!info->par)
		return;
	sys_copyarea(info, area);

	schedule_delayed_work(&info->deferred_work, 0);
}

/* Stub to call the system default and update the image on the picoLCD */
static void picolcd_fb_imageblit(struct fb_info *info, const struct fb_image *image)
{
	if (!info->par)
		return;
	sys_imageblit(info, image);

	schedule_delayed_work(&info->deferred_work, 0);
}

/*
 * this is the slow path from userspace. they can seek and write to
 * the fb. it's inefficient to do anything less than a full screen draw
 */
static ssize_t picolcd_fb_write(struct fb_info *info, const char __user *buf,
		size_t count, loff_t *ppos)
{
	ssize_t ret;
	if (!info->par)
		return -ENODEV;
	ret = fb_sys_write(info, buf, count, ppos);
	if (ret >= 0)
		schedule_delayed_work(&info->deferred_work, 0);
	return ret;
}

static int picolcd_fb_blank(int blank, struct fb_info *info)
{
	if (!info->par)
		return -ENODEV;
	/* We let fb notification do this for us via lcd/backlight device */
	return 0;
}

static void picolcd_fb_destroy(struct fb_info *info)
{
	/* make sure no work is deferred */
	fb_deferred_io_cleanup(info);

	vfree((u8 *)info->fix.smem_start);
	framebuffer_release(info);
	printk(KERN_DEBUG "picolcd_fb_destroy(%p)\n", info);
}

static int picolcd_fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	__u32 bpp      = var->bits_per_pixel;
	__u32 activate = var->activate;

	/* only allow 1/8 bit depth (8-bit is grayscale) */
	*var = picolcdfb_var;
	var->activate = activate;
	if (bpp >= 8) {
		var->bits_per_pixel = 8;
		var->red.length     = 8;
		var->green.length   = 8;
		var->blue.length    = 8;
	} else {
		var->bits_per_pixel = 1;
		var->red.length     = 1;
		var->green.length   = 1;
		var->blue.length    = 1;
	}
	return 0;
}

static int picolcd_set_par(struct fb_info *info)
{
	struct picolcd_data *data = info->par;
	u8 *tmp_fb, *o_fb;
	if (!data)
		return -ENODEV;
	if (info->var.bits_per_pixel == data->fb_bpp)
		return 0;
	/* switch between 1/8 bit depths */
	if (info->var.bits_per_pixel != 1 && info->var.bits_per_pixel != 8)
		return -EINVAL;

	o_fb   = data->fb_bitmap;
	tmp_fb = kmalloc(PICOLCDFB_SIZE*info->var.bits_per_pixel, GFP_KERNEL);
	if (!tmp_fb)
		return -ENOMEM;

	/* translate FB content to new bits-per-pixel */
	if (info->var.bits_per_pixel == 1) {
		int i, b;
		for (i = 0; i < PICOLCDFB_SIZE; i++) {
			u8 p = 0;
			for (b = 0; b < 8; b++) {
				p <<= 1;
				p |= o_fb[i*8+b] ? 0x01 : 0x00;
			}
			tmp_fb[i] = p;
		}
		memcpy(o_fb, tmp_fb, PICOLCDFB_SIZE);
		info->fix.visual = FB_VISUAL_MONO01;
		info->fix.line_length = PICOLCDFB_WIDTH / 8;
	} else {
		int i;
		memcpy(tmp_fb, o_fb, PICOLCDFB_SIZE);
		for (i = 0; i < PICOLCDFB_SIZE * 8; i++)
			o_fb[i] = tmp_fb[i/8] & (0x01 << (7 - i % 8)) ? 0xff : 0x00;
		info->fix.visual = FB_VISUAL_DIRECTCOLOR;
		info->fix.line_length = PICOLCDFB_WIDTH;
	}

	kfree(tmp_fb);
	data->fb_bpp      = info->var.bits_per_pixel;
	return 0;
}

/* Note this can't be const because of struct fb_info definition */
static struct fb_ops picolcdfb_ops = {
	.owner        = THIS_MODULE,
	.fb_destroy   = picolcd_fb_destroy,
	.fb_read      = fb_sys_read,
	.fb_write     = picolcd_fb_write,
	.fb_blank     = picolcd_fb_blank,
	.fb_fillrect  = picolcd_fb_fillrect,
	.fb_copyarea  = picolcd_fb_copyarea,
	.fb_imageblit = picolcd_fb_imageblit,
	.fb_check_var = picolcd_fb_check_var,
	.fb_set_par   = picolcd_set_par,
};


/* Callback from deferred IO workqueue */
static void picolcd_fb_deferred_io(struct fb_info *info, struct list_head *pagelist)
{
	picolcd_fb_update(info);
}

static const struct fb_deferred_io picolcd_fb_defio = {
	.delay = HZ / PICOLCDFB_UPDATE_RATE_DEFAULT,
	.deferred_io = picolcd_fb_deferred_io,
};


/*
 * The "fb_update_rate" sysfs attribute
 */
static ssize_t picolcd_fb_update_rate_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct picolcd_data *data = dev_get_drvdata(dev);
	unsigned i, fb_update_rate = data->fb_update_rate;
	size_t ret = 0;

	for (i = 1; i <= PICOLCDFB_UPDATE_RATE_LIMIT; i++)
		if (ret >= PAGE_SIZE)
			break;
		else if (i == fb_update_rate)
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "[%u] ", i);
		else
			ret += snprintf(buf+ret, PAGE_SIZE-ret, "%u ", i);
	if (ret > 0)
		buf[min(ret, (size_t)PAGE_SIZE)-1] = '\n';
	return ret;
}

static ssize_t picolcd_fb_update_rate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct picolcd_data *data = dev_get_drvdata(dev);
	int i;
	unsigned u;

	if (count < 1 || count > 10)
		return -EINVAL;

	i = sscanf(buf, "%u", &u);
	if (i != 1)
		return -EINVAL;

	if (u > PICOLCDFB_UPDATE_RATE_LIMIT)
		return -ERANGE;
	else if (u == 0)
		u = PICOLCDFB_UPDATE_RATE_DEFAULT;

	data->fb_update_rate = u;
	data->fb_info->fbdefio->delay = HZ / data->fb_update_rate;
	return count;
}

static DEVICE_ATTR(fb_update_rate, 0666, picolcd_fb_update_rate_show,
		picolcd_fb_update_rate_store);

/* initialize Framebuffer device */
int picolcd_init_framebuffer(struct picolcd_data *data)
{
	struct device *dev = &data->hdev->dev;
	struct fb_info *info = NULL;
	int i, error = -ENOMEM;
	u8 *fb_vbitmap = NULL;
	u8 *fb_bitmap  = NULL;
	u32 *palette;

	fb_bitmap = vmalloc(PICOLCDFB_SIZE*8);
	if (fb_bitmap == NULL) {
		dev_err(dev, "can't get a free page for framebuffer\n");
		goto err_nomem;
	}

	fb_vbitmap = kmalloc(PICOLCDFB_SIZE, GFP_KERNEL);
	if (fb_vbitmap == NULL) {
		dev_err(dev, "can't alloc vbitmap image buffer\n");
		goto err_nomem;
	}

	data->fb_update_rate = PICOLCDFB_UPDATE_RATE_DEFAULT;
	/* The extra memory is:
	 * - 256*u32 for pseudo_palette
	 * - struct fb_deferred_io
	 */
	info = framebuffer_alloc(256 * sizeof(u32) +
			sizeof(struct fb_deferred_io), dev);
	if (info == NULL) {
		dev_err(dev, "failed to allocate a framebuffer\n");
		goto err_nomem;
	}

	info->fbdefio = info->par;
	*info->fbdefio = picolcd_fb_defio;
	palette  = info->par + sizeof(struct fb_deferred_io);
	for (i = 0; i < 256; i++)
		palette[i] = i > 0 && i < 16 ? 0xff : 0;
	info->pseudo_palette = palette;
	info->screen_base = (char __force __iomem *)fb_bitmap;
	info->fbops = &picolcdfb_ops;
	info->var = picolcdfb_var;
	info->fix = picolcdfb_fix;
	info->fix.smem_len   = PICOLCDFB_SIZE*8;
	info->fix.smem_start = (unsigned long)fb_bitmap;
	info->par = data;
	info->flags = FBINFO_FLAG_DEFAULT;

	data->fb_vbitmap = fb_vbitmap;
	data->fb_bitmap  = fb_bitmap;
	data->fb_bpp     = picolcdfb_var.bits_per_pixel;
	error = picolcd_fb_reset(data, 1);
	if (error) {
		dev_err(dev, "failed to configure display\n");
		goto err_cleanup;
	}
	error = device_create_file(dev, &dev_attr_fb_update_rate);
	if (error) {
		dev_err(dev, "failed to create sysfs attributes\n");
		goto err_cleanup;
	}
	fb_deferred_io_init(info);
	data->fb_info    = info;
	error = register_framebuffer(info);
	if (error) {
		dev_err(dev, "failed to register framebuffer\n");
		goto err_sysfs;
	}
	/* schedule first output of framebuffer */
	data->fb_force = 1;
	schedule_delayed_work(&info->deferred_work, 0);
	return 0;

err_sysfs:
	fb_deferred_io_cleanup(info);
	device_remove_file(dev, &dev_attr_fb_update_rate);
err_cleanup:
	data->fb_vbitmap = NULL;
	data->fb_bitmap  = NULL;
	data->fb_bpp     = 0;
	data->fb_info    = NULL;

err_nomem:
	framebuffer_release(info);
	vfree(fb_bitmap);
	kfree(fb_vbitmap);
	return error;
}

void picolcd_exit_framebuffer(struct picolcd_data *data)
{
	struct fb_info *info = data->fb_info;
	u8 *fb_vbitmap = data->fb_vbitmap;

	if (!info)
		return;

	device_remove_file(&data->hdev->dev, &dev_attr_fb_update_rate);
	info->par = NULL;
	unregister_framebuffer(info);
	data->fb_vbitmap = NULL;
	data->fb_bitmap  = NULL;
	data->fb_bpp     = 0;
	data->fb_info    = NULL;
	kfree(fb_vbitmap);
}
