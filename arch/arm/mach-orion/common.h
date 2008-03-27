#ifndef __ARCH_ORION_COMMON_H__
#define __ARCH_ORION_COMMON_H__

/*
 * Basic Orion init functions used early by machine-setup.
 */

void __init orion_map_io(void);
void __init orion_init_irq(void);
void __init orion_init(void);
extern struct sys_timer orion_timer;

/*
 * Enumerations and functions for Orion windows mapping. Used by Orion core
 * functions to map its interfaces and by the machine-setup to map its on-
 * board devices. Details in /mach-orion/addr-map.c
 */
extern struct mbus_dram_target_info orion_mbus_dram_info;
void orion_setup_cpu_mbus_bridge(void);
void orion_setup_dev_boot_win(u32 base, u32 size);
void orion_setup_dev0_win(u32 base, u32 size);
void orion_setup_dev1_win(u32 base, u32 size);
void orion_setup_dev2_win(u32 base, u32 size);
void orion_setup_pcie_wa_win(u32 base, u32 size);
void orion_setup_eth_wins(void);

/*
 * Shared code used internally by other Orion core functions.
 * (/mach-orion/pci.c)
 */

struct pci_sys_data;
struct pci_bus;

void orion_pcie_id(u32 *dev, u32 *rev);
int orion_pcie_local_bus_nr(void);
int orion_pci_local_bus_nr(void);
int orion_pci_sys_setup(int nr, struct pci_sys_data *sys);
struct pci_bus *orion_pci_sys_scan_bus(int nr, struct pci_sys_data *sys);

/*
 * Valid GPIO pins according to MPP setup, used by machine-setup.
 * (/mach-orion/gpio.c).
 */

void __init orion_gpio_set_valid_pins(u32 pins);
void gpio_display(void);	/* debug */

/*
 * Pull in Orion Ethernet platform_data, used by machine-setup
 */

struct mv643xx_eth_platform_data;

void __init orion_eth_init(struct mv643xx_eth_platform_data *eth_data);

/*
 * Orion Sata platform_data, used by machine-setup
 */

struct mv_sata_platform_data;

void __init orion_sata_init(struct mv_sata_platform_data *sata_data);

struct machine_desc;
struct meminfo;
struct tag;
extern void __init tag_fixup_mem32(struct machine_desc *, struct tag *,
				   char **, struct meminfo *);

#endif /* __ARCH_ORION_COMMON_H__ */
