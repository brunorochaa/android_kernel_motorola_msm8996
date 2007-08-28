/*
    On Screen Display cx23415 Framebuffer driver

    This module presents the cx23415 OSD (onscreen display) framebuffer memory
    as a standard Linux /dev/fb style framebuffer device. The framebuffer has
    support for 8, 16 & 32 bpp packed pixel formats with alpha channel. In 16bpp
    mode, there is a choice of a three color depths (12, 15 or 16 bits), but no
    local alpha. The colorspace is selectable between rgb & yuv.
    Depending on the TV standard configured in the ivtv module at load time,
    the initial resolution is either 640x400 (NTSC) or 640x480 (PAL) at 8bpp.
    Video timings are locked to ensure a vertical refresh rate of 50Hz (PAL)
    or 59.94 (NTSC)

    Copyright (c) 2003 Matt T. Yourst <yourst@yourst.com>

    Derived from drivers/video/vesafb.c
    Portions (c) 1998 Gerd Knorr <kraxel@goldbach.in-berlin.de>

    2.6 kernel port:
    Copyright (C) 2004 Matthias Badaire

    Copyright (C) 2004  Chris Kennedy <c@groovy.org>

    Copyright (C) 2006  Ian Armstrong <ian@iarmst.demon.co.uk>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fb.h>
#include <linux/ivtvfb.h>

#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#include "ivtv-driver.h"
#include "ivtv-udma.h"
#include "ivtv-mailbox.h"

/* card parameters */
static int ivtvfb_card_id = -1;
static int ivtvfb_debug = 0;
static int osd_laced;
static int osd_compat;
static int osd_depth;
static int osd_upper;
static int osd_left;
static int osd_yres;
static int osd_xres;

module_param(ivtvfb_card_id, int, 0444);
module_param_named(debug,ivtvfb_debug, int, 0644);
module_param(osd_laced, bool, 0444);
module_param(osd_compat, bool, 0444);
module_param(osd_depth, int, 0444);
module_param(osd_upper, int, 0444);
module_param(osd_left, int, 0444);
module_param(osd_yres, int, 0444);
module_param(osd_xres, int, 0444);

MODULE_PARM_DESC(ivtvfb_card_id,
		 "Only use framebuffer of the specified ivtv card (0-31)\n"
		 "\t\t\tdefault -1: initialize all available framebuffers");

MODULE_PARM_DESC(debug,
		 "Debug level (bitmask). Default: errors only\n"
		 "\t\t\t(debug = 3 gives full debugging)");

MODULE_PARM_DESC(osd_compat,
		 "Compatibility mode - Display size is locked (use for old X drivers)\n"
		 "\t\t\t0=off\n"
		 "\t\t\t1=on\n"
		 "\t\t\tdefault off");

/* Why upper, left, xres, yres, depth, laced ? To match terminology used
   by fbset.
   Why start at 1 for left & upper coordinate ? Because X doesn't allow 0 */

MODULE_PARM_DESC(osd_laced,
		 "Interlaced mode\n"
		 "\t\t\t0=off\n"
		 "\t\t\t1=on\n"
		 "\t\t\tdefault off");

MODULE_PARM_DESC(osd_depth,
		 "Bits per pixel - 8, 16, 32\n"
		 "\t\t\tdefault 8");

MODULE_PARM_DESC(osd_upper,
		 "Vertical start position\n"
		 "\t\t\tdefault 0 (Centered)");

MODULE_PARM_DESC(osd_left,
		 "Horizontal start position\n"
		 "\t\t\tdefault 0 (Centered)");

MODULE_PARM_DESC(osd_yres,
		 "Display height\n"
		 "\t\t\tdefault 480 (PAL)\n"
		 "\t\t\t        400 (NTSC)");

MODULE_PARM_DESC(osd_xres,
		 "Display width\n"
		 "\t\t\tdefault 640");

MODULE_AUTHOR("Kevin Thayer, Chris Kennedy, Hans Verkuil, John Harvey, Ian Armstrong");
MODULE_LICENSE("GPL");

/* --------------------------------------------------------------------- */

#define IVTVFB_DBGFLG_WARN  (1 << 0)
#define IVTVFB_DBGFLG_INFO  (1 << 1)

#define IVTVFB_DEBUG(x, type, fmt, args...) \
	do { \
		if ((x) & ivtvfb_debug) \
			printk(KERN_INFO "ivtvfb%d " type ": " fmt, itv->num , ## args); \
	} while (0)
#define IVTVFB_DEBUG_WARN(fmt, args...)  IVTVFB_DEBUG(IVTVFB_DBGFLG_WARN, "warning", fmt , ## args)
#define IVTVFB_DEBUG_INFO(fmt, args...)  IVTVFB_DEBUG(IVTVFB_DBGFLG_INFO, "info", fmt , ## args)

/* Standard kernel messages */
#define IVTVFB_ERR(fmt, args...)   printk(KERN_ERR  "ivtvfb%d: " fmt, itv->num , ## args)
#define IVTVFB_WARN(fmt, args...)  printk(KERN_WARNING  "ivtvfb%d: " fmt, itv->num , ## args)
#define IVTVFB_INFO(fmt, args...)  printk(KERN_INFO "ivtvfb%d: " fmt, itv->num , ## args)

/* --------------------------------------------------------------------- */

#define IVTV_OSD_MAX_WIDTH  720
#define IVTV_OSD_MAX_HEIGHT 576

#define IVTV_OSD_BPP_8      0x00
#define IVTV_OSD_BPP_16_444 0x03
#define IVTV_OSD_BPP_16_555 0x02
#define IVTV_OSD_BPP_16_565 0x01
#define IVTV_OSD_BPP_32     0x04

struct osd_info {
	/* Physical base address */
	unsigned long video_pbase;
	/* Relative base address (relative to start of decoder memory) */
	u32 video_rbase;
	/* Mapped base address */
	volatile char __iomem *video_vbase;
	/* Buffer size */
	u32 video_buffer_size;

#ifdef CONFIG_MTRR
	/* video_base rounded down as required by hardware MTRRs */
	unsigned long fb_start_aligned_physaddr;
	/* video_base rounded up as required by hardware MTRRs */
	unsigned long fb_end_aligned_physaddr;
#endif

	/* Current osd mode */
	int osd_mode;

	/* Store the buffer offset */
	int set_osd_coords_x;
	int set_osd_coords_y;

	/* Current dimensions (NOT VISIBLE SIZE!) */
	int display_width;
	int display_height;
	int display_byte_stride;

	/* Current bits per pixel */
	int bits_per_pixel;
	int bytes_per_pixel;

	/* Frame buffer stuff */
	struct fb_info ivtvfb_info;
	struct fb_var_screeninfo ivtvfb_defined;
	struct fb_fix_screeninfo ivtvfb_fix;
};

struct ivtv_osd_coords {
	unsigned long offset;
	unsigned long max_offset;
	int pixel_stride;
	int lines;
	int x;
	int y;
};

/* --------------------------------------------------------------------- */

/* ivtv API calls for framebuffer related support */

static int ivtvfb_get_framebuffer(struct ivtv *itv, u32 *fbbase,
				       u32 *fblength)
{
	u32 data[CX2341X_MBOX_MAX_DATA];
	int rc;

	rc = ivtv_vapi_result(itv, data, CX2341X_OSD_GET_FRAMEBUFFER, 0);
	*fbbase = data[0];
	*fblength = data[1];
	return rc;
}

static int ivtvfb_get_osd_coords(struct ivtv *itv,
				      struct ivtv_osd_coords *osd)
{
	struct osd_info *oi = itv->osd_info;
	u32 data[CX2341X_MBOX_MAX_DATA];

	ivtv_vapi_result(itv, data, CX2341X_OSD_GET_OSD_COORDS, 0);

	osd->offset = data[0] - oi->video_rbase;
	osd->max_offset = oi->display_width * oi->display_height * 4;
	osd->pixel_stride = data[1];
	osd->lines = data[2];
	osd->x = data[3];
	osd->y = data[4];
	return 0;
}

static int ivtvfb_set_osd_coords(struct ivtv *itv, const struct ivtv_osd_coords *osd)
{
	struct osd_info *oi = itv->osd_info;

	oi->display_width = osd->pixel_stride;
	oi->display_byte_stride = osd->pixel_stride * oi->bytes_per_pixel;
	oi->set_osd_coords_x += osd->x;
	oi->set_osd_coords_y = osd->y;

	return ivtv_vapi(itv, CX2341X_OSD_SET_OSD_COORDS, 5,
			osd->offset + oi->video_rbase,
			osd->pixel_stride,
			osd->lines, osd->x, osd->y);
}

static int ivtvfb_set_display_window(struct ivtv *itv, struct v4l2_rect *ivtv_window)
{
	int osd_height_limit = itv->is_50hz ? 576 : 480;

	/* Only fail if resolution too high, otherwise fudge the start coords. */
	if ((ivtv_window->height > osd_height_limit) || (ivtv_window->width > IVTV_OSD_MAX_WIDTH))
		return -EINVAL;

	/* Ensure we don't exceed display limits */
	if (ivtv_window->top + ivtv_window->height > osd_height_limit) {
		IVTVFB_DEBUG_WARN("ivtv_ioctl_fb_set_display_window - Invalid height setting (%d, %d)\n",
			ivtv_window->top, ivtv_window->height);
		ivtv_window->top = osd_height_limit - ivtv_window->height;
	}

	if (ivtv_window->left + ivtv_window->width > IVTV_OSD_MAX_WIDTH) {
		IVTVFB_DEBUG_WARN("ivtv_ioctl_fb_set_display_window - Invalid width setting (%d, %d)\n",
			ivtv_window->left, ivtv_window->width);
		ivtv_window->left = IVTV_OSD_MAX_WIDTH - ivtv_window->width;
	}

	/* Set the OSD origin */
	write_reg((ivtv_window->top << 16) | ivtv_window->left, 0x02a04);

	/* How much to display */
	write_reg(((ivtv_window->top+ivtv_window->height) << 16) | (ivtv_window->left+ivtv_window->width), 0x02a08);

	/* Pass this info back the yuv handler */
	itv->yuv_info.osd_vis_w = ivtv_window->width;
	itv->yuv_info.osd_vis_h = ivtv_window->height;
	itv->yuv_info.osd_x_offset = ivtv_window->left;
	itv->yuv_info.osd_y_offset = ivtv_window->top;

	return 0;
}

static int ivtvfb_prep_dec_dma_to_device(struct ivtv *itv,
				  unsigned long ivtv_dest_addr, void __user *userbuf,
				  int size_in_bytes)
{
	DEFINE_WAIT(wait);
	int ret = 0;
	int got_sig = 0;

	mutex_lock(&itv->udma.lock);
	/* Map User DMA */
	if (ivtv_udma_setup(itv, ivtv_dest_addr, userbuf, size_in_bytes) <= 0) {
		mutex_unlock(&itv->udma.lock);
		IVTVFB_WARN("ivtvfb_prep_dec_dma_to_device, "
			       "Error with get_user_pages: %d bytes, %d pages returned\n",
			       size_in_bytes, itv->udma.page_count);

		/* get_user_pages must have failed completely */
		return -EIO;
	}

	IVTVFB_DEBUG_INFO("ivtvfb_prep_dec_dma_to_device, %d bytes, %d pages\n",
		       size_in_bytes, itv->udma.page_count);

	ivtv_udma_prepare(itv);
	prepare_to_wait(&itv->dma_waitq, &wait, TASK_INTERRUPTIBLE);
	/* if no UDMA is pending and no UDMA is in progress, then the DMA
	   is finished */
	while (itv->i_flags & (IVTV_F_I_UDMA_PENDING | IVTV_F_I_UDMA)) {
		/* don't interrupt if the DMA is in progress but break off
		   a still pending DMA. */
		got_sig = signal_pending(current);
		if (got_sig && test_and_clear_bit(IVTV_F_I_UDMA_PENDING, &itv->i_flags))
			break;
		got_sig = 0;
		schedule();
	}
	finish_wait(&itv->dma_waitq, &wait);

	/* Unmap Last DMA Xfer */
	ivtv_udma_unmap(itv);
	mutex_unlock(&itv->udma.lock);
	if (got_sig) {
		IVTV_DEBUG_INFO("User stopped OSD\n");
		return -EINTR;
	}

	return ret;
}

static int ivtvfb_prep_frame(struct ivtv *itv, int cmd, void __user *source,
			      unsigned long dest_offset, int count)
{
	DEFINE_WAIT(wait);
	struct osd_info *oi = itv->osd_info;

	/* Nothing to do */
	if (count == 0) {
		IVTVFB_DEBUG_WARN("ivtvfb_prep_frame: Nothing to do. count = 0\n");
		return -EINVAL;
	}

	/* Check Total FB Size */
	if ((dest_offset + count) > oi->video_buffer_size) {
		IVTVFB_WARN("ivtvfb_prep_frame: Overflowing the framebuffer %ld, only %d available\n",
			dest_offset + count, oi->video_buffer_size);
		return -E2BIG;
	}

	/* Not fatal, but will have undesirable results */
	if ((unsigned long)source & 3)
		IVTVFB_WARN("ivtvfb_prep_frame: Source address not 32 bit aligned (0x%08lx)\n",
			(unsigned long)source);

	if (dest_offset & 3)
		IVTVFB_WARN("ivtvfb_prep_frame: Dest offset not 32 bit aligned (%ld)\n", dest_offset);

	if (count & 3)
		IVTVFB_WARN("ivtvfb_prep_frame: Count not a multiple of 4 (%d)\n", count);

	/* Check Source */
	if (!access_ok(VERIFY_READ, source + dest_offset, count)) {
		IVTVFB_WARN("Invalid userspace pointer 0x%08lx\n",
			(unsigned long)source);

		IVTVFB_DEBUG_WARN("access_ok() failed for offset 0x%08lx source 0x%08lx count %d\n",
			dest_offset, (unsigned long)source,
			count);
		return -EINVAL;
	}

	/* OSD Address to send DMA to */
	dest_offset += IVTV_DECODER_OFFSET + oi->video_rbase;

	/* Fill Buffers */
	return ivtvfb_prep_dec_dma_to_device(itv, dest_offset, source, count);
}

static int ivtvfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	DEFINE_WAIT(wait);
	struct ivtv *itv = (struct ivtv *)info->par;
	int rc = 0;

	switch (cmd) {
		case FBIOGET_VBLANK: {
			struct fb_vblank vblank;
			u32 trace;

			vblank.flags = FB_VBLANK_HAVE_COUNT |FB_VBLANK_HAVE_VCOUNT |
					FB_VBLANK_HAVE_VSYNC;
			trace = read_reg(0x028c0) >> 16;
			if (itv->is_50hz && trace > 312) trace -= 312;
			else if (itv->is_60hz && trace > 262) trace -= 262;
			if (trace == 1) vblank.flags |= FB_VBLANK_VSYNCING;
			vblank.count = itv->last_vsync_field;
			vblank.vcount = trace;
			vblank.hcount = 0;
			if (copy_to_user((void __user *)arg, &vblank, sizeof(vblank)))
				return -EFAULT;
			return 0;
		}

		case FBIO_WAITFORVSYNC:
			prepare_to_wait(&itv->vsync_waitq, &wait, TASK_INTERRUPTIBLE);
			if (!schedule_timeout(msecs_to_jiffies(50))) rc = -ETIMEDOUT;
			finish_wait(&itv->vsync_waitq, &wait);
			return rc;

		case IVTVFB_IOC_DMA_FRAME: {
			struct ivtvfb_dma_frame args;

			IVTVFB_DEBUG_INFO("IVTVFB_IOC_DMA_FRAME\n");
			if (copy_from_user(&args, (void __user *)arg, sizeof(args)))
				return -EFAULT;

			return ivtvfb_prep_frame(itv, cmd, args.source, args.dest_offset, args.count);
		}

		default:
			IVTVFB_DEBUG_INFO("Unknown ioctl %08x\n", cmd);
			return -EINVAL;
	}
	return 0;
}

/* Framebuffer device handling */

static int ivtvfb_set_var(struct ivtv *itv, struct fb_var_screeninfo *var)
{
	struct osd_info *oi = itv->osd_info;
	struct ivtv_osd_coords ivtv_osd;
	struct v4l2_rect ivtv_window;
	int osd_mode = -1;

	IVTVFB_DEBUG_INFO("ivtvfb_set_var\n");

	/* Select color space */
	if (var->nonstd) /* YUV */
		write_reg(read_reg(0x02a00) | 0x0002000, 0x02a00);
	else /* RGB  */
		write_reg(read_reg(0x02a00) & ~0x0002000, 0x02a00);

	/* Set the color mode */
	switch (var->bits_per_pixel) {
		case 8:
			osd_mode = IVTV_OSD_BPP_8;
			break;
		case 32:
			osd_mode = IVTV_OSD_BPP_32;
			break;
		case 16:
			switch (var->green.length) {
			case 4:
				osd_mode = IVTV_OSD_BPP_16_444;
				break;
			case 5:
				osd_mode = IVTV_OSD_BPP_16_555;
				break;
			case 6:
				osd_mode = IVTV_OSD_BPP_16_565;
				break;
			default:
				IVTVFB_DEBUG_WARN("ivtvfb_set_var - Invalid bpp\n");
			}
			break;
		default:
			IVTVFB_DEBUG_WARN("ivtvfb_set_var - Invalid bpp\n");
	}

	/* Change osd mode if needed.
	   Although rare, things can go wrong. The extra mode
	   change seems to help... */
	if (osd_mode != -1 && osd_mode != oi->osd_mode) {
		ivtv_vapi(itv, CX2341X_OSD_SET_PIXEL_FORMAT, 1, 0);
		ivtv_vapi(itv, CX2341X_OSD_SET_PIXEL_FORMAT, 1, osd_mode);
		oi->osd_mode = osd_mode;
	}

	oi->bits_per_pixel = var->bits_per_pixel;
	oi->bytes_per_pixel = var->bits_per_pixel / 8;

	/* Set the flicker filter */
	switch (var->vmode & FB_VMODE_MASK) {
		case FB_VMODE_NONINTERLACED: /* Filter on */
			ivtv_vapi(itv, CX2341X_OSD_SET_FLICKER_STATE, 1, 1);
			break;
		case FB_VMODE_INTERLACED: /* Filter off */
			ivtv_vapi(itv, CX2341X_OSD_SET_FLICKER_STATE, 1, 0);
			break;
		default:
			IVTVFB_DEBUG_WARN("ivtvfb_set_var - Invalid video mode\n");
	}

	/* Read the current osd info */
	ivtvfb_get_osd_coords(itv, &ivtv_osd);

	/* Now set the OSD to the size we want */
	ivtv_osd.pixel_stride = var->xres_virtual;
	ivtv_osd.lines = var->yres_virtual;
	ivtv_osd.x = 0;
	ivtv_osd.y = 0;
	ivtvfb_set_osd_coords(itv, &ivtv_osd);

	/* Can't seem to find the right API combo for this.
	   Use another function which does what we need through direct register access. */
	ivtv_window.width = var->xres;
	ivtv_window.height = var->yres;

	/* Minimum margin cannot be 0, as X won't allow such a mode */
	if (!var->upper_margin) var->upper_margin++;
	if (!var->left_margin) var->left_margin++;
	ivtv_window.top = var->upper_margin - 1;
	ivtv_window.left = var->left_margin - 1;

	ivtvfb_set_display_window(itv, &ivtv_window);

	/* Force update of yuv registers */
	itv->yuv_info.yuv_forced_update = 1;

	IVTVFB_DEBUG_INFO("Display size: %dx%d (virtual %dx%d) @ %dbpp\n",
		      var->xres, var->yres,
		      var->xres_virtual, var->yres_virtual,
		      var->bits_per_pixel);

	IVTVFB_DEBUG_INFO("Display position: %d, %d\n",
		      var->left_margin, var->upper_margin);

	IVTVFB_DEBUG_INFO("Display filter: %s\n",
			(var->vmode & FB_VMODE_MASK) == FB_VMODE_NONINTERLACED ? "on" : "off");
	IVTVFB_DEBUG_INFO("Color space: %s\n", var->nonstd ? "YUV" : "RGB");

	return 0;
}

static int ivtvfb_get_fix(struct ivtv *itv, struct fb_fix_screeninfo *fix)
{
	struct osd_info *oi = itv->osd_info;

	IVTVFB_DEBUG_INFO("ivtvfb_get_fix\n");
	memset(fix, 0, sizeof(struct fb_fix_screeninfo));
	strcpy(fix->id, "cx23415 TV out");
	fix->smem_start = oi->video_pbase;
	fix->smem_len = oi->video_buffer_size;
	fix->type = FB_TYPE_PACKED_PIXELS;
	fix->visual = (oi->bits_per_pixel == 8) ? FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;
	fix->xpanstep = 1;
	fix->ypanstep = 1;
	fix->ywrapstep = 0;
	fix->line_length = oi->display_byte_stride;
	fix->accel = FB_ACCEL_NONE;
	return 0;
}

/* Check the requested display mode, returning -EINVAL if we can't
   handle it. */

static int _ivtvfb_check_var(struct fb_var_screeninfo *var, struct ivtv *itv)
{
	struct osd_info *oi = itv->osd_info;
	int osd_height_limit;
	u32 pixclock, hlimit, vlimit;

	IVTVFB_DEBUG_INFO("ivtvfb_check_var\n");

	/* Set base references for mode calcs. */
	if (itv->is_50hz) {
		pixclock = 84316;
		hlimit = 776;
		vlimit = 591;
		osd_height_limit = 576;
	}
	else {
		pixclock = 83926;
		hlimit = 776;
		vlimit = 495;
		osd_height_limit = 480;
	}

	/* Check the bits per pixel */
	if (osd_compat) {
		if (var->bits_per_pixel != 32) {
			IVTVFB_DEBUG_WARN("Invalid colour mode: %d\n", var->bits_per_pixel);
			return -EINVAL;
		}
	}

	if (var->bits_per_pixel == 8 || var->bits_per_pixel == 32) {
		var->transp.offset = 24;
		var->transp.length = 8;
		var->red.offset = 16;
		var->red.length = 8;
		var->green.offset = 8;
		var->green.length = 8;
		var->blue.offset = 0;
		var->blue.length = 8;
	}
	else if (var->bits_per_pixel == 16) {
		/* To find out the true mode, check green length */
		switch (var->green.length) {
			case 4:
				var->red.offset = 8;
				var->red.length = 4;
				var->green.offset = 4;
				var->green.length = 4;
				var->blue.offset = 0;
				var->blue.length = 4;
				var->transp.offset = 12;
				var->transp.length = 1;
				break;
			case 5:
				var->red.offset = 10;
				var->red.length = 5;
				var->green.offset = 5;
				var->green.length = 5;
				var->blue.offset = 0;
				var->blue.length = 5;
				var->transp.offset = 15;
				var->transp.length = 1;
				break;
			default:
				var->red.offset = 11;
				var->red.length = 5;
				var->green.offset = 5;
				var->green.length = 6;
				var->blue.offset = 0;
				var->blue.length = 5;
				var->transp.offset = 0;
				var->transp.length = 0;
				break;
		}
	}
	else {
		IVTVFB_DEBUG_WARN("Invalid colour mode: %d\n", var->bits_per_pixel);
		return -EINVAL;
	}

	/* Check the resolution */
	if (osd_compat) {
		if (var->xres != oi->ivtvfb_defined.xres ||
		    var->yres != oi->ivtvfb_defined.yres ||
		    var->xres_virtual != oi->ivtvfb_defined.xres_virtual ||
		    var->yres_virtual != oi->ivtvfb_defined.yres_virtual) {
			IVTVFB_DEBUG_WARN("Invalid resolution: %dx%d (virtual %dx%d)\n",
				var->xres, var->yres, var->xres_virtual, var->yres_virtual);
			return -EINVAL;
		}
	}
	else {
		if (var->xres > IVTV_OSD_MAX_WIDTH || var->yres > osd_height_limit) {
			IVTVFB_DEBUG_WARN("Invalid resolution: %dx%d\n",
					var->xres, var->yres);
			return -EINVAL;
		}

		/* Max horizontal size is 1023 @ 32bpp, 2046 & 16bpp, 4092 @ 8bpp */
		if (var->xres_virtual > 4095 / (var->bits_per_pixel / 8) ||
		    var->xres_virtual * var->yres_virtual * (var->bits_per_pixel / 8) > oi->video_buffer_size ||
		    var->xres_virtual < var->xres ||
		    var->yres_virtual < var->yres) {
			IVTVFB_DEBUG_WARN("Invalid virtual resolution: %dx%d\n",
				var->xres_virtual, var->yres_virtual);
			return -EINVAL;
		}
	}

	/* Some extra checks if in 8 bit mode */
	if (var->bits_per_pixel == 8) {
		/* Width must be a multiple of 4 */
		if (var->xres & 3) {
			IVTVFB_DEBUG_WARN("Invalid resolution for 8bpp: %d\n", var->xres);
			return -EINVAL;
		}
		if (var->xres_virtual & 3) {
			IVTVFB_DEBUG_WARN("Invalid virtual resolution for 8bpp: %d)\n", var->xres_virtual);
			return -EINVAL;
		}
	}
	else if (var->bits_per_pixel == 16) {
		/* Width must be a multiple of 2 */
		if (var->xres & 1) {
			IVTVFB_DEBUG_WARN("Invalid resolution for 16bpp: %d\n", var->xres);
			return -EINVAL;
		}
		if (var->xres_virtual & 1) {
			IVTVFB_DEBUG_WARN("Invalid virtual resolution for 16bpp: %d)\n", var->xres_virtual);
			return -EINVAL;
		}
	}

	/* Now check the offsets */
	if (var->xoffset >= var->xres_virtual || var->yoffset >= var->yres_virtual) {
		IVTVFB_DEBUG_WARN("Invalid offset: %d (%d) %d (%d)\n",
			var->xoffset, var->xres_virtual, var->yoffset, var->yres_virtual);
		return -EINVAL;
	}

	/* Check pixel format */
	if (var->nonstd > 1) {
		IVTVFB_DEBUG_WARN("Invalid nonstd % d\n", var->nonstd);
		return -EINVAL;
	}

	/* Check video mode */
	if (((var->vmode & FB_VMODE_MASK) != FB_VMODE_NONINTERLACED) &&
		((var->vmode & FB_VMODE_MASK) != FB_VMODE_INTERLACED)) {
		IVTVFB_DEBUG_WARN("Invalid video mode: %d\n", var->vmode & FB_VMODE_MASK);
		return -EINVAL;
	}

	/* Check the left & upper margins
	   If the margins are too large, just center the screen
	   (enforcing margins causes too many problems) */

	if (var->left_margin + var->xres > IVTV_OSD_MAX_WIDTH + 1) {
		var->left_margin = 1 + ((IVTV_OSD_MAX_WIDTH - var->xres) / 2);
	}
	if (var->upper_margin + var->yres > (itv->is_50hz ? 577 : 481)) {
		var->upper_margin = 1 + (((itv->is_50hz ? 576 : 480) - var->yres) / 2);
	}

	/* Maintain overall 'size' for a constant refresh rate */
	var->right_margin = hlimit - var->left_margin - var->xres;
	var->lower_margin = vlimit - var->upper_margin - var->yres;

	/* Fixed sync times */
	var->hsync_len = 24;
	var->vsync_len = 2;

	/* Non-interlaced / interlaced mode is used to switch the OSD filter
	   on or off. Adjust the clock timings to maintain a constant
	   vertical refresh rate. */
	if ((var->vmode & FB_VMODE_MASK) == FB_VMODE_NONINTERLACED)
		var->pixclock = pixclock / 2;
	else
		var->pixclock = pixclock;

	IVTVFB_DEBUG_INFO("Display size: %dx%d (virtual %dx%d) @ %dbpp\n",
		      var->xres, var->yres,
		      var->xres_virtual, var->yres_virtual,
		      var->bits_per_pixel);

	IVTVFB_DEBUG_INFO("Display position: %d, %d\n",
		      var->left_margin, var->upper_margin);

	IVTVFB_DEBUG_INFO("Display filter: %s\n",
			(var->vmode & FB_VMODE_MASK) == FB_VMODE_NONINTERLACED ? "on" : "off");
	IVTVFB_DEBUG_INFO("Color space: %s\n", var->nonstd ? "YUV" : "RGB");
	return 0;
}

static int ivtvfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct ivtv *itv = (struct ivtv *) info->par;
	IVTVFB_DEBUG_INFO("ivtvfb_check_var\n");
	return _ivtvfb_check_var(var, itv);
}

static int ivtvfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	u32 osd_pan_index;
	struct ivtv *itv = (struct ivtv *) info->par;

	osd_pan_index = (var->xoffset + (var->yoffset * var->xres_virtual))*var->bits_per_pixel/8;
	write_reg(osd_pan_index, 0x02A0C);

	/* Pass this info back the yuv handler */
	itv->yuv_info.osd_x_pan = var->xoffset;
	itv->yuv_info.osd_y_pan = var->yoffset;
	/* Force update of yuv registers */
	itv->yuv_info.yuv_forced_update = 1;
	return 0;
}

static int ivtvfb_set_par(struct fb_info *info)
{
	int rc = 0;
	struct ivtv *itv = (struct ivtv *) info->par;

	IVTVFB_DEBUG_INFO("ivtvfb_set_par\n");

	rc = ivtvfb_set_var(itv, &info->var);
	ivtvfb_pan_display(&info->var, info);
	ivtvfb_get_fix(itv, &info->fix);
	return rc;
}

static int ivtvfb_setcolreg(unsigned regno, unsigned red, unsigned green,
				unsigned blue, unsigned transp,
				struct fb_info *info)
{
	u32 color, *palette;
	struct ivtv *itv = (struct ivtv *)info->par;

	if (regno >= info->cmap.len)
		return -EINVAL;

	color = ((transp & 0xFF00) << 16) |((red & 0xFF00) << 8) | (green & 0xFF00) | ((blue & 0xFF00) >> 8);
	if (info->var.bits_per_pixel <= 8) {
		write_reg(regno, 0x02a30);
		write_reg(color, 0x02a34);
		return 0;
	}
	if (regno >= 16)
		return -EINVAL;

	palette = info->pseudo_palette;
	if (info->var.bits_per_pixel == 16) {
		switch (info->var.green.length) {
			case 4:
				color = ((red & 0xf000) >> 4) |
					((green & 0xf000) >> 8) |
					((blue & 0xf000) >> 12);
				break;
			case 5:
				color = ((red & 0xf800) >> 1) |
					((green & 0xf800) >> 6) |
					((blue & 0xf800) >> 11);
				break;
			case 6:
				color = (red & 0xf800 ) |
					((green & 0xfc00) >> 5) |
					((blue & 0xf800) >> 11);
				break;
		}
	}
	palette[regno] = color;
	return 0;
}

/* We don't really support blanking. All this does is enable or
   disable the OSD. */
static int ivtvfb_blank(int blank_mode, struct fb_info *info)
{
	struct ivtv *itv = (struct ivtv *)info->par;

	IVTVFB_DEBUG_INFO("Set blanking mode : %d\n", blank_mode);
	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
		ivtv_vapi(itv, CX2341X_OSD_SET_STATE, 1, 1);
		break;
	case FB_BLANK_NORMAL:
	case FB_BLANK_HSYNC_SUSPEND:
	case FB_BLANK_VSYNC_SUSPEND:
	case FB_BLANK_POWERDOWN:
		ivtv_vapi(itv, CX2341X_OSD_SET_STATE, 1, 0);
		break;
	}
	return 0;
}

static struct fb_ops ivtvfb_ops = {
	.owner = THIS_MODULE,
	.fb_check_var   = ivtvfb_check_var,
	.fb_set_par     = ivtvfb_set_par,
	.fb_setcolreg   = ivtvfb_setcolreg,
	.fb_fillrect    = cfb_fillrect,
	.fb_copyarea    = cfb_copyarea,
	.fb_imageblit   = cfb_imageblit,
	.fb_cursor      = NULL,
	.fb_ioctl       = ivtvfb_ioctl,
	.fb_pan_display = ivtvfb_pan_display,
	.fb_blank       = ivtvfb_blank,
};

/* Initialization */


/* Setup our initial video mode */
static int ivtvfb_init_vidmode(struct ivtv *itv)
{
	struct osd_info *oi = itv->osd_info;
	struct v4l2_rect start_window;
	int max_height;

	/* Color mode */

	if (osd_compat) osd_depth = 32;
	if (osd_depth != 8 && osd_depth != 16 && osd_depth != 32) osd_depth = 8;
	oi->bits_per_pixel = osd_depth;
	oi->bytes_per_pixel = oi->bits_per_pixel / 8;

	/* Invalidate current osd mode to force a mode switch later */
	oi->osd_mode = -1;

	/* Horizontal size & position */

	if (osd_xres > 720) osd_xres = 720;

	/* Must be a multiple of 4 for 8bpp & 2 for 16bpp */
	if (osd_depth == 8)
		osd_xres &= ~3;
	else if (osd_depth == 16)
		osd_xres &= ~1;

	if (osd_xres)
		start_window.width = osd_xres;
	else
		start_window.width = osd_compat ? 720: 640;

	/* Check horizontal start (osd_left). */
	if (osd_left && osd_left + start_window.width > 721) {
		IVTVFB_ERR("Invalid osd_left - assuming default\n");
		osd_left = 0;
	}

	/* Hardware coords start at 0, user coords start at 1. */
	osd_left--;

	start_window.left = osd_left >= 0 ? osd_left : ((IVTV_OSD_MAX_WIDTH - start_window.width) / 2);

	oi->display_byte_stride =
			start_window.width * oi->bytes_per_pixel;

	/* Vertical size & position */

	max_height = itv->is_50hz ? 576 : 480;

	if (osd_yres > max_height)
		osd_yres = max_height;

	if (osd_yres)
		start_window.height = osd_yres;
	else
		start_window.height = osd_compat ? max_height : (itv->is_50hz ? 480 : 400);

	/* Check vertical start (osd_upper). */
	if (osd_upper + start_window.height > max_height + 1) {
		IVTVFB_ERR("Invalid osd_upper - assuming default\n");
		osd_upper = 0;
	}

	/* Hardware coords start at 0, user coords start at 1. */
	osd_upper--;

	start_window.top = osd_upper >= 0 ? osd_upper : ((max_height - start_window.height) / 2);

	oi->display_width = start_window.width;
	oi->display_height = start_window.height;

	/* Generate a valid fb_var_screeninfo */

	oi->ivtvfb_defined.xres = oi->display_width;
	oi->ivtvfb_defined.yres = oi->display_height;
	oi->ivtvfb_defined.xres_virtual = oi->display_width;
	oi->ivtvfb_defined.yres_virtual = oi->display_height;
	oi->ivtvfb_defined.bits_per_pixel = oi->bits_per_pixel;
	oi->ivtvfb_defined.vmode = (osd_laced ? FB_VMODE_INTERLACED : FB_VMODE_NONINTERLACED);
	oi->ivtvfb_defined.left_margin = start_window.left + 1;
	oi->ivtvfb_defined.upper_margin = start_window.top + 1;
	oi->ivtvfb_defined.accel_flags = FB_ACCEL_NONE;
	oi->ivtvfb_defined.nonstd = 0;

	/* We've filled in the most data, let the usual mode check
	   routine fill in the rest. */
	_ivtvfb_check_var(&oi->ivtvfb_defined, itv);

	/* Generate valid fb_fix_screeninfo */

	ivtvfb_get_fix(itv, &oi->ivtvfb_fix);

	/* Generate valid fb_info */

	oi->ivtvfb_info.node = -1;
	oi->ivtvfb_info.flags = FBINFO_FLAG_DEFAULT;
	oi->ivtvfb_info.fbops = &ivtvfb_ops;
	oi->ivtvfb_info.par = itv;
	oi->ivtvfb_info.var = oi->ivtvfb_defined;
	oi->ivtvfb_info.fix = oi->ivtvfb_fix;
	oi->ivtvfb_info.screen_base = (u8 __iomem *)oi->video_vbase;
	oi->ivtvfb_info.fbops = &ivtvfb_ops;

	/* Supply some monitor specs. Bogus values will do for now */
	oi->ivtvfb_info.monspecs.hfmin = 8000;
	oi->ivtvfb_info.monspecs.hfmax = 70000;
	oi->ivtvfb_info.monspecs.vfmin = 10;
	oi->ivtvfb_info.monspecs.vfmax = 100;

	/* Allocate color map */
	if (fb_alloc_cmap(&oi->ivtvfb_info.cmap, 256, 1)) {
		IVTVFB_ERR("abort, unable to alloc cmap\n");
		return -ENOMEM;
	}

	/* Allocate the pseudo palette */
	oi->ivtvfb_info.pseudo_palette = kmalloc(sizeof(u32) * 16, GFP_KERNEL);

	if (!oi->ivtvfb_info.pseudo_palette) {
		IVTVFB_ERR("abort, unable to alloc pseudo pallete\n");
		return -ENOMEM;
	}

	return 0;
}

/* Find OSD buffer base & size. Add to mtrr. Zero osd buffer. */

static int ivtvfb_init_io(struct ivtv *itv)
{
	struct osd_info *oi = itv->osd_info;

	mutex_lock(&itv->serialize_lock);
	if (ivtv_init_on_first_open(itv)) {
		mutex_unlock(&itv->serialize_lock);
		IVTVFB_ERR("Failed to initialize ivtv\n");
		return -ENXIO;
	}
	mutex_unlock(&itv->serialize_lock);

	ivtvfb_get_framebuffer(itv, &oi->video_rbase, &oi->video_buffer_size);

	/* The osd buffer size depends on the number of video buffers allocated
	   on the PVR350 itself. For now we'll hardcode the smallest osd buffer
	   size to prevent any overlap. */
	oi->video_buffer_size = 1704960;

	oi->video_pbase = itv->base_addr + IVTV_DECODER_OFFSET + oi->video_rbase;
	oi->video_vbase = itv->dec_mem + oi->video_rbase;

	if (!oi->video_vbase) {
		IVTVFB_ERR("abort, video memory 0x%x @ 0x%lx isn't mapped!\n",
		     oi->video_buffer_size, oi->video_pbase);
		return -EIO;
	}

	IVTVFB_INFO("Framebuffer at 0x%lx, mapped to 0x%p, size %dk\n",
			oi->video_pbase, oi->video_vbase,
			oi->video_buffer_size / 1024);

#ifdef CONFIG_MTRR
	{
		/* Find the largest power of two that maps the whole buffer */
		int size_shift = 31;

		while (!(oi->video_buffer_size & (1 << size_shift))) {
			size_shift--;
		}
		size_shift++;
		oi->fb_start_aligned_physaddr = oi->video_pbase & ~((1 << size_shift) - 1);
		oi->fb_end_aligned_physaddr = oi->video_pbase + oi->video_buffer_size;
		oi->fb_end_aligned_physaddr += (1 << size_shift) - 1;
		oi->fb_end_aligned_physaddr &= ~((1 << size_shift) - 1);
		if (mtrr_add(oi->fb_start_aligned_physaddr,
			oi->fb_end_aligned_physaddr - oi->fb_start_aligned_physaddr,
			     MTRR_TYPE_WRCOMB, 1) < 0) {
			IVTVFB_INFO("disabled mttr\n");
			oi->fb_start_aligned_physaddr = 0;
			oi->fb_end_aligned_physaddr = 0;
		}
	}
#endif

	/* Blank the entire osd. */
	memset_io(oi->video_vbase, 0, oi->video_buffer_size);

	return 0;
}

/* Release any memory we've grabbed & remove mtrr entry */
static void ivtvfb_release_buffers (struct ivtv *itv)
{
	struct osd_info *oi = itv->osd_info;

	/* Release cmap */
	if (oi->ivtvfb_info.cmap.len);
	fb_dealloc_cmap(&oi->ivtvfb_info.cmap);

	/* Release pseudo palette */
	if (oi->ivtvfb_info.pseudo_palette)
		kfree(oi->ivtvfb_info.pseudo_palette);

#ifdef CONFIG_MTRR
	if (oi->fb_end_aligned_physaddr) {
		mtrr_del(-1, oi->fb_start_aligned_physaddr,
			oi->fb_end_aligned_physaddr - oi->fb_start_aligned_physaddr);
	}
#endif

	kfree(oi);
	itv->osd_info = NULL;
}

/* Initialize the specified card */

static int ivtvfb_init_card(struct ivtv *itv)
{
	int rc;

	if (itv->osd_info) {
		IVTVFB_ERR("Card %d already initialised\n", ivtvfb_card_id);
		return -EBUSY;
	}

	itv->osd_info = kzalloc(sizeof(struct osd_info), GFP_ATOMIC);
	if (itv->osd_info == 0) {
		IVTVFB_ERR("Failed to allocate memory for osd_info\n");
		return -ENOMEM;
	}

	/* Find & setup the OSD buffer */
	if ((rc = ivtvfb_init_io(itv)))
		return rc;

	/* Set the startup video mode information */
	if ((rc = ivtvfb_init_vidmode(itv))) {
		ivtvfb_release_buffers(itv);
		return rc;
	}

	/* Register the framebuffer */
	if (register_framebuffer(&itv->osd_info->ivtvfb_info) < 0) {
		ivtvfb_release_buffers(itv);
		return -EINVAL;
	}

	itv->osd_video_pbase = itv->osd_info->video_pbase;

	/* Set the card to the requested mode */
	ivtvfb_set_par(&itv->osd_info->ivtvfb_info);

	/* Set color 0 to black */
	write_reg(0, 0x02a30);
	write_reg(0, 0x02a34);

	/* Enable the osd */
	ivtvfb_blank(FB_BLANK_UNBLANK, &itv->osd_info->ivtvfb_info);

	/* Note if we're running in compatibility mode */
	if (osd_compat)
		IVTVFB_INFO("Running in compatibility mode. Display resize & mode change disabled\n");

	/* Allocate DMA */
	ivtv_udma_alloc(itv);
	return 0;

}

static int __init ivtvfb_init(void)
{
	struct ivtv *itv;
	int i, registered = 0;

	if (ivtvfb_card_id < -1 || ivtvfb_card_id >= IVTV_MAX_CARDS) {
		printk(KERN_ERR "ivtvfb:  ivtvfb_card_id parameter is out of range (valid range: -1 - %d)\n",
		     IVTV_MAX_CARDS - 1);
		return -EINVAL;
	}

	/* Locate & initialise all cards supporting an OSD. */
	for (i = 0; i < ivtv_cards_active; i++) {
		if (ivtvfb_card_id != -1 && i != ivtvfb_card_id)
			continue;
		itv = ivtv_cards[i];
		if (itv && (itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT)) {
			if (ivtvfb_init_card(itv) == 0) {
				IVTVFB_INFO("Framebuffer registered on ivtv card id %d\n", i);
				registered++;
			}
		}
	}
	if (!registered) {
		printk(KERN_ERR "ivtvfb:  no cards found");
		return -ENODEV;
	}
	return 0;
}

static void ivtvfb_cleanup(void)
{
	struct ivtv *itv;
	int i;

	printk(KERN_INFO "ivtvfb:  Unloading framebuffer module\n");

	for (i = 0; i < ivtv_cards_active; i++) {
		itv = ivtv_cards[i];
		if (itv && (itv->v4l2_cap & V4L2_CAP_VIDEO_OUTPUT) && itv->osd_info) {
			IVTVFB_DEBUG_INFO("Unregister framebuffer %d\n", i);
			ivtvfb_blank(FB_BLANK_POWERDOWN, &itv->osd_info->ivtvfb_info);
			unregister_framebuffer(&itv->osd_info->ivtvfb_info);
			ivtvfb_release_buffers(itv);
			itv->osd_video_pbase = 0;
		}
	}
}

module_init(ivtvfb_init);
module_exit(ivtvfb_cleanup);
