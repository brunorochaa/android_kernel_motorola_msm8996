/*
 * Copyright 2008 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 * Copyright 2009 Jerome Glisse.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alex Deucher
 *          Jerome Glisse
 */
#include "drmP.h"
#include "radeon_reg.h"
#include "radeon.h"
#include "atom.h"

#include <linux/vga_switcheroo.h>
#include <linux/slab.h>
/*
 * BIOS.
 */

/* If you boot an IGP board with a discrete card as the primary,
 * the IGP rom is not accessible via the rom bar as the IGP rom is
 * part of the system bios.  On boot, the system bios puts a
 * copy of the igp rom at the start of vram if a discrete card is
 * present.
 */
static bool igp_read_bios_from_vram(struct radeon_device *rdev)
{
	uint8_t __iomem *bios;
	resource_size_t vram_base;
	resource_size_t size = 256 * 1024; /* ??? */

	rdev->bios = NULL;
	vram_base = drm_get_resource_start(rdev->ddev, 0);
	bios = ioremap(vram_base, size);
	if (!bios) {
		return false;
	}

	if (size == 0 || bios[0] != 0x55 || bios[1] != 0xaa) {
		iounmap(bios);
		return false;
	}
	rdev->bios = kmalloc(size, GFP_KERNEL);
	if (rdev->bios == NULL) {
		iounmap(bios);
		return false;
	}
	memcpy_fromio(rdev->bios, bios, size);
	iounmap(bios);
	return true;
}

static bool radeon_read_bios(struct radeon_device *rdev)
{
	uint8_t __iomem *bios;
	size_t size;

	rdev->bios = NULL;
	/* XXX: some cards may return 0 for rom size? ddx has a workaround */
	bios = pci_map_rom(rdev->pdev, &size);
	if (!bios) {
		return false;
	}

	if (size == 0 || bios[0] != 0x55 || bios[1] != 0xaa) {
		pci_unmap_rom(rdev->pdev, bios);
		return false;
	}
	rdev->bios = kmemdup(bios, size, GFP_KERNEL);
	if (rdev->bios == NULL) {
		pci_unmap_rom(rdev->pdev, bios);
		return false;
	}
	pci_unmap_rom(rdev->pdev, bios);
	return true;
}

/* ATRM is used to get the BIOS on the discrete cards in
 * dual-gpu systems.
 */
static bool radeon_atrm_get_bios(struct radeon_device *rdev)
{
	int ret;
	int size = 64 * 1024;
	int i;

	if (!radeon_atrm_supported(rdev->pdev))
		return false;

	rdev->bios = kmalloc(size, GFP_KERNEL);
	if (!rdev->bios) {
		DRM_ERROR("Unable to allocate bios\n");
		return false;
	}

	for (i = 0; i < size / ATRM_BIOS_PAGE; i++) {
		ret = radeon_atrm_get_bios_chunk(rdev->bios,
						 (i * ATRM_BIOS_PAGE),
						 ATRM_BIOS_PAGE);
		if (ret <= 0)
			break;
	}

	if (i == 0 || rdev->bios[0] != 0x55 || rdev->bios[1] != 0xaa) {
		kfree(rdev->bios);
		return false;
	}
	return true;
}
static bool r700_read_disabled_bios(struct radeon_device *rdev)
{
	uint32_t viph_control;
	uint32_t bus_cntl;
	uint32_t d1vga_control;
	uint32_t d2vga_control;
	uint32_t vga_render_control;
	uint32_t rom_cntl;
	uint32_t cg_spll_func_cntl = 0;
	uint32_t cg_spll_status;
	bool r;

	viph_control = RREG32(RADEON_VIPH_CONTROL);
	bus_cntl = RREG32(RADEON_BUS_CNTL);
	d1vga_control = RREG32(AVIVO_D1VGA_CONTROL);
	d2vga_control = RREG32(AVIVO_D2VGA_CONTROL);
	vga_render_control = RREG32(AVIVO_VGA_RENDER_CONTROL);
	rom_cntl = RREG32(R600_ROM_CNTL);

	/* disable VIP */
	WREG32(RADEON_VIPH_CONTROL, (viph_control & ~RADEON_VIPH_EN));
	/* enable the rom */
	WREG32(RADEON_BUS_CNTL, (bus_cntl & ~RADEON_BUS_BIOS_DIS_ROM));
	/* Disable VGA mode */
	WREG32(AVIVO_D1VGA_CONTROL,
	       (d1vga_control & ~(AVIVO_DVGA_CONTROL_MODE_ENABLE |
		AVIVO_DVGA_CONTROL_TIMING_SELECT)));
	WREG32(AVIVO_D2VGA_CONTROL,
	       (d2vga_control & ~(AVIVO_DVGA_CONTROL_MODE_ENABLE |
		AVIVO_DVGA_CONTROL_TIMING_SELECT)));
	WREG32(AVIVO_VGA_RENDER_CONTROL,
	       (vga_render_control & ~AVIVO_VGA_VSTATUS_CNTL_MASK));

	if (rdev->family == CHIP_RV730) {
		cg_spll_func_cntl = RREG32(R600_CG_SPLL_FUNC_CNTL);

		/* enable bypass mode */
		WREG32(R600_CG_SPLL_FUNC_CNTL, (cg_spll_func_cntl |
						R600_SPLL_BYPASS_EN));

		/* wait for SPLL_CHG_STATUS to change to 1 */
		cg_spll_status = 0;
		while (!(cg_spll_status & R600_SPLL_CHG_STATUS))
			cg_spll_status = RREG32(R600_CG_SPLL_STATUS);

		WREG32(R600_ROM_CNTL, (rom_cntl & ~R600_SCK_OVERWRITE));
	} else
		WREG32(R600_ROM_CNTL, (rom_cntl | R600_SCK_OVERWRITE));

	r = radeon_read_bios(rdev);

	/* restore regs */
	if (rdev->family == CHIP_RV730) {
		WREG32(R600_CG_SPLL_FUNC_CNTL, cg_spll_func_cntl);

		/* wait for SPLL_CHG_STATUS to change to 1 */
		cg_spll_status = 0;
		while (!(cg_spll_status & R600_SPLL_CHG_STATUS))
			cg_spll_status = RREG32(R600_CG_SPLL_STATUS);
	}
	WREG32(RADEON_VIPH_CONTROL, viph_control);
	WREG32(RADEON_BUS_CNTL, bus_cntl);
	WREG32(AVIVO_D1VGA_CONTROL, d1vga_control);
	WREG32(AVIVO_D2VGA_CONTROL, d2vga_control);
	WREG32(AVIVO_VGA_RENDER_CONTROL, vga_render_control);
	WREG32(R600_ROM_CNTL, rom_cntl);
	return r;
}

static bool r600_read_disabled_bios(struct radeon_device *rdev)
{
	uint32_t viph_control;
	uint32_t bus_cntl;
	uint32_t d1vga_control;
	uint32_t d2vga_control;
	uint32_t vga_render_control;
	uint32_t rom_cntl;
	uint32_t general_pwrmgt;
	uint32_t low_vid_lower_gpio_cntl;
	uint32_t medium_vid_lower_gpio_cntl;
	uint32_t high_vid_lower_gpio_cntl;
	uint32_t ctxsw_vid_lower_gpio_cntl;
	uint32_t lower_gpio_enable;
	bool r;

	viph_control = RREG32(RADEON_VIPH_CONTROL);
	bus_cntl = RREG32(RADEON_BUS_CNTL);
	d1vga_control = RREG32(AVIVO_D1VGA_CONTROL);
	d2vga_control = RREG32(AVIVO_D2VGA_CONTROL);
	vga_render_control = RREG32(AVIVO_VGA_RENDER_CONTROL);
	rom_cntl = RREG32(R600_ROM_CNTL);
	general_pwrmgt = RREG32(R600_GENERAL_PWRMGT);
	low_vid_lower_gpio_cntl = RREG32(R600_LOW_VID_LOWER_GPIO_CNTL);
	medium_vid_lower_gpio_cntl = RREG32(R600_MEDIUM_VID_LOWER_GPIO_CNTL);
	high_vid_lower_gpio_cntl = RREG32(R600_HIGH_VID_LOWER_GPIO_CNTL);
	ctxsw_vid_lower_gpio_cntl = RREG32(R600_CTXSW_VID_LOWER_GPIO_CNTL);
	lower_gpio_enable = RREG32(R600_LOWER_GPIO_ENABLE);

	/* disable VIP */
	WREG32(RADEON_VIPH_CONTROL, (viph_control & ~RADEON_VIPH_EN));
	/* enable the rom */
	WREG32(RADEON_BUS_CNTL, (bus_cntl & ~RADEON_BUS_BIOS_DIS_ROM));
	/* Disable VGA mode */
	WREG32(AVIVO_D1VGA_CONTROL,
	       (d1vga_control & ~(AVIVO_DVGA_CONTROL_MODE_ENABLE |
		AVIVO_DVGA_CONTROL_TIMING_SELECT)));
	WREG32(AVIVO_D2VGA_CONTROL,
	       (d2vga_control & ~(AVIVO_DVGA_CONTROL_MODE_ENABLE |
		AVIVO_DVGA_CONTROL_TIMING_SELECT)));
	WREG32(AVIVO_VGA_RENDER_CONTROL,
	       (vga_render_control & ~AVIVO_VGA_VSTATUS_CNTL_MASK));

	WREG32(R600_ROM_CNTL,
	       ((rom_cntl & ~R600_SCK_PRESCALE_CRYSTAL_CLK_MASK) |
		(1 << R600_SCK_PRESCALE_CRYSTAL_CLK_SHIFT) |
		R600_SCK_OVERWRITE));

	WREG32(R600_GENERAL_PWRMGT, (general_pwrmgt & ~R600_OPEN_DRAIN_PADS));
	WREG32(R600_LOW_VID_LOWER_GPIO_CNTL,
	       (low_vid_lower_gpio_cntl & ~0x400));
	WREG32(R600_MEDIUM_VID_LOWER_GPIO_CNTL,
	       (medium_vid_lower_gpio_cntl & ~0x400));
	WREG32(R600_HIGH_VID_LOWER_GPIO_CNTL,
	       (high_vid_lower_gpio_cntl & ~0x400));
	WREG32(R600_CTXSW_VID_LOWER_GPIO_CNTL,
	       (ctxsw_vid_lower_gpio_cntl & ~0x400));
	WREG32(R600_LOWER_GPIO_ENABLE, (lower_gpio_enable | 0x400));

	r = radeon_read_bios(rdev);

	/* restore regs */
	WREG32(RADEON_VIPH_CONTROL, viph_control);
	WREG32(RADEON_BUS_CNTL, bus_cntl);
	WREG32(AVIVO_D1VGA_CONTROL, d1vga_control);
	WREG32(AVIVO_D2VGA_CONTROL, d2vga_control);
	WREG32(AVIVO_VGA_RENDER_CONTROL, vga_render_control);
	WREG32(R600_ROM_CNTL, rom_cntl);
	WREG32(R600_GENERAL_PWRMGT, general_pwrmgt);
	WREG32(R600_LOW_VID_LOWER_GPIO_CNTL, low_vid_lower_gpio_cntl);
	WREG32(R600_MEDIUM_VID_LOWER_GPIO_CNTL, medium_vid_lower_gpio_cntl);
	WREG32(R600_HIGH_VID_LOWER_GPIO_CNTL, high_vid_lower_gpio_cntl);
	WREG32(R600_CTXSW_VID_LOWER_GPIO_CNTL, ctxsw_vid_lower_gpio_cntl);
	WREG32(R600_LOWER_GPIO_ENABLE, lower_gpio_enable);
	return r;
}

static bool avivo_read_disabled_bios(struct radeon_device *rdev)
{
	uint32_t seprom_cntl1;
	uint32_t viph_control;
	uint32_t bus_cntl;
	uint32_t d1vga_control;
	uint32_t d2vga_control;
	uint32_t vga_render_control;
	uint32_t gpiopad_a;
	uint32_t gpiopad_en;
	uint32_t gpiopad_mask;
	bool r;

	seprom_cntl1 = RREG32(RADEON_SEPROM_CNTL1);
	viph_control = RREG32(RADEON_VIPH_CONTROL);
	bus_cntl = RREG32(RADEON_BUS_CNTL);
	d1vga_control = RREG32(AVIVO_D1VGA_CONTROL);
	d2vga_control = RREG32(AVIVO_D2VGA_CONTROL);
	vga_render_control = RREG32(AVIVO_VGA_RENDER_CONTROL);
	gpiopad_a = RREG32(RADEON_GPIOPAD_A);
	gpiopad_en = RREG32(RADEON_GPIOPAD_EN);
	gpiopad_mask = RREG32(RADEON_GPIOPAD_MASK);

	WREG32(RADEON_SEPROM_CNTL1,
	       ((seprom_cntl1 & ~RADEON_SCK_PRESCALE_MASK) |
		(0xc << RADEON_SCK_PRESCALE_SHIFT)));
	WREG32(RADEON_GPIOPAD_A, 0);
	WREG32(RADEON_GPIOPAD_EN, 0);
	WREG32(RADEON_GPIOPAD_MASK, 0);

	/* disable VIP */
	WREG32(RADEON_VIPH_CONTROL, (viph_control & ~RADEON_VIPH_EN));

	/* enable the rom */
	WREG32(RADEON_BUS_CNTL, (bus_cntl & ~RADEON_BUS_BIOS_DIS_ROM));

	/* Disable VGA mode */
	WREG32(AVIVO_D1VGA_CONTROL,
	       (d1vga_control & ~(AVIVO_DVGA_CONTROL_MODE_ENABLE |
		AVIVO_DVGA_CONTROL_TIMING_SELECT)));
	WREG32(AVIVO_D2VGA_CONTROL,
	       (d2vga_control & ~(AVIVO_DVGA_CONTROL_MODE_ENABLE |
		AVIVO_DVGA_CONTROL_TIMING_SELECT)));
	WREG32(AVIVO_VGA_RENDER_CONTROL,
	       (vga_render_control & ~AVIVO_VGA_VSTATUS_CNTL_MASK));

	r = radeon_read_bios(rdev);

	/* restore regs */
	WREG32(RADEON_SEPROM_CNTL1, seprom_cntl1);
	WREG32(RADEON_VIPH_CONTROL, viph_control);
	WREG32(RADEON_BUS_CNTL, bus_cntl);
	WREG32(AVIVO_D1VGA_CONTROL, d1vga_control);
	WREG32(AVIVO_D2VGA_CONTROL, d2vga_control);
	WREG32(AVIVO_VGA_RENDER_CONTROL, vga_render_control);
	WREG32(RADEON_GPIOPAD_A, gpiopad_a);
	WREG32(RADEON_GPIOPAD_EN, gpiopad_en);
	WREG32(RADEON_GPIOPAD_MASK, gpiopad_mask);
	return r;
}

static bool legacy_read_disabled_bios(struct radeon_device *rdev)
{
	uint32_t seprom_cntl1;
	uint32_t viph_control;
	uint32_t bus_cntl;
	uint32_t crtc_gen_cntl;
	uint32_t crtc2_gen_cntl;
	uint32_t crtc_ext_cntl;
	uint32_t fp2_gen_cntl;
	bool r;

	seprom_cntl1 = RREG32(RADEON_SEPROM_CNTL1);
	viph_control = RREG32(RADEON_VIPH_CONTROL);
	bus_cntl = RREG32(RADEON_BUS_CNTL);
	crtc_gen_cntl = RREG32(RADEON_CRTC_GEN_CNTL);
	crtc2_gen_cntl = 0;
	crtc_ext_cntl = RREG32(RADEON_CRTC_EXT_CNTL);
	fp2_gen_cntl = 0;

	if (rdev->ddev->pci_device == PCI_DEVICE_ID_ATI_RADEON_QY) {
		fp2_gen_cntl = RREG32(RADEON_FP2_GEN_CNTL);
	}

	if (!(rdev->flags & RADEON_SINGLE_CRTC)) {
		crtc2_gen_cntl = RREG32(RADEON_CRTC2_GEN_CNTL);
	}

	WREG32(RADEON_SEPROM_CNTL1,
	       ((seprom_cntl1 & ~RADEON_SCK_PRESCALE_MASK) |
		(0xc << RADEON_SCK_PRESCALE_SHIFT)));

	/* disable VIP */
	WREG32(RADEON_VIPH_CONTROL, (viph_control & ~RADEON_VIPH_EN));

	/* enable the rom */
	WREG32(RADEON_BUS_CNTL, (bus_cntl & ~RADEON_BUS_BIOS_DIS_ROM));

	/* Turn off mem requests and CRTC for both controllers */
	WREG32(RADEON_CRTC_GEN_CNTL,
	       ((crtc_gen_cntl & ~RADEON_CRTC_EN) |
		(RADEON_CRTC_DISP_REQ_EN_B |
		 RADEON_CRTC_EXT_DISP_EN)));
	if (!(rdev->flags & RADEON_SINGLE_CRTC)) {
		WREG32(RADEON_CRTC2_GEN_CNTL,
		       ((crtc2_gen_cntl & ~RADEON_CRTC2_EN) |
			RADEON_CRTC2_DISP_REQ_EN_B));
	}
	/* Turn off CRTC */
	WREG32(RADEON_CRTC_EXT_CNTL,
	       ((crtc_ext_cntl & ~RADEON_CRTC_CRT_ON) |
		(RADEON_CRTC_SYNC_TRISTAT |
		 RADEON_CRTC_DISPLAY_DIS)));

	if (rdev->ddev->pci_device == PCI_DEVICE_ID_ATI_RADEON_QY) {
		WREG32(RADEON_FP2_GEN_CNTL, (fp2_gen_cntl & ~RADEON_FP2_ON));
	}

	r = radeon_read_bios(rdev);

	/* restore regs */
	WREG32(RADEON_SEPROM_CNTL1, seprom_cntl1);
	WREG32(RADEON_VIPH_CONTROL, viph_control);
	WREG32(RADEON_BUS_CNTL, bus_cntl);
	WREG32(RADEON_CRTC_GEN_CNTL, crtc_gen_cntl);
	if (!(rdev->flags & RADEON_SINGLE_CRTC)) {
		WREG32(RADEON_CRTC2_GEN_CNTL, crtc2_gen_cntl);
	}
	WREG32(RADEON_CRTC_EXT_CNTL, crtc_ext_cntl);
	if (rdev->ddev->pci_device == PCI_DEVICE_ID_ATI_RADEON_QY) {
		WREG32(RADEON_FP2_GEN_CNTL, fp2_gen_cntl);
	}
	return r;
}

static bool radeon_read_disabled_bios(struct radeon_device *rdev)
{
	if (rdev->flags & RADEON_IS_IGP)
		return igp_read_bios_from_vram(rdev);
	else if (rdev->family >= CHIP_RV770)
		return r700_read_disabled_bios(rdev);
	else if (rdev->family >= CHIP_R600)
		return r600_read_disabled_bios(rdev);
	else if (rdev->family >= CHIP_RS600)
		return avivo_read_disabled_bios(rdev);
	else
		return legacy_read_disabled_bios(rdev);
}


bool radeon_get_bios(struct radeon_device *rdev)
{
	bool r;
	uint16_t tmp;

	r = radeon_atrm_get_bios(rdev);
	if (r == false)
		r = igp_read_bios_from_vram(rdev);
	if (r == false)
		r = radeon_read_bios(rdev);
	if (r == false) {
		r = radeon_read_disabled_bios(rdev);
	}
	if (r == false || rdev->bios == NULL) {
		DRM_ERROR("Unable to locate a BIOS ROM\n");
		rdev->bios = NULL;
		return false;
	}
	if (rdev->bios[0] != 0x55 || rdev->bios[1] != 0xaa) {
		printk("BIOS signature incorrect %x %x\n", rdev->bios[0], rdev->bios[1]);
		goto free_bios;
	}

	tmp = RBIOS16(0x18);
	if (RBIOS8(tmp + 0x14) != 0x0) {
		DRM_INFO("Not an x86 BIOS ROM, not using.\n");
		goto free_bios;
	}

	rdev->bios_header_start = RBIOS16(0x48);
	if (!rdev->bios_header_start) {
		goto free_bios;
	}
	tmp = rdev->bios_header_start + 4;
	if (!memcmp(rdev->bios + tmp, "ATOM", 4) ||
	    !memcmp(rdev->bios + tmp, "MOTA", 4)) {
		rdev->is_atom_bios = true;
	} else {
		rdev->is_atom_bios = false;
	}

	DRM_DEBUG("%sBIOS detected\n", rdev->is_atom_bios ? "ATOM" : "COM");
	return true;
free_bios:
	kfree(rdev->bios);
	rdev->bios = NULL;
	return false;
}
