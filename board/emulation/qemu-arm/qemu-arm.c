// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2017 Tuomas Tynkkynen
 */

#include <common.h>
#include <cpu_func.h>
#include <dm.h>
#include <efi.h>
#include <efi_loader.h>
#include <fdtdec.h>
#include <init.h>
#include <log.h>
#include <virtio_types.h>
#include <virtio.h>

#include <linux/kernel.h>
#include <linux/sizes.h>

#if IS_ENABLED(CONFIG_FIRMWARE_HANDOFF)
#include <transfer_list.h>
#endif

/* GUIDs for capsule updatable firmware images */
#define QEMU_ARM_UBOOT_IMAGE_GUID \
	EFI_GUID(0xf885b085, 0x99f8, 0x45af, 0x84, 0x7d, \
		 0xd5, 0x14, 0x10, 0x7a, 0x4a, 0x2c)

#define QEMU_ARM64_UBOOT_IMAGE_GUID \
	EFI_GUID(0x058b7d83, 0x50d5, 0x4c47, 0xa1, 0x95, \
		 0x60, 0xd8, 0x6a, 0xd3, 0x41, 0xc4)

#ifdef CONFIG_ARM64
#include <asm/armv8/mmu.h>

#if IS_ENABLED(CONFIG_EFI_HAVE_CAPSULE_SUPPORT)
struct efi_fw_image fw_images[] = {
#if defined(CONFIG_TARGET_QEMU_ARM_32BIT)
	{
		.image_type_id = QEMU_ARM_UBOOT_IMAGE_GUID,
		.fw_name = u"Qemu-Arm-UBOOT",
		.image_index = 1,
	},
#elif defined(CONFIG_TARGET_QEMU_ARM_64BIT)
	{
		.image_type_id = QEMU_ARM64_UBOOT_IMAGE_GUID,
		.fw_name = u"Qemu-Arm-UBOOT",
		.image_index = 1,
	},
#endif
};

struct efi_capsule_update_info update_info = {
	.images = fw_images,
};

u8 num_image_type_guids = ARRAY_SIZE(fw_images);
#endif /* EFI_HAVE_CAPSULE_SUPPORT */

static struct mm_region qemu_arm64_mem_map[] = {
	{
		/* Flash */
		.virt = 0x00000000UL,
		.phys = 0x00000000UL,
		.size = 0x08000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* Lowmem peripherals */
		.virt = 0x08000000UL,
		.phys = 0x08000000UL,
		.size = 0x38000000,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* RAM */
		.virt = 0x40000000UL,
		.phys = 0x40000000UL,
		.size = 255UL * SZ_1G,
		.attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
			 PTE_BLOCK_INNER_SHARE
	}, {
		/* Highmem PCI-E ECAM memory area */
		.virt = 0x4010000000ULL,
		.phys = 0x4010000000ULL,
		.size = 0x10000000,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* Highmem PCI-E MMIO memory area */
		.virt = 0x8000000000ULL,
		.phys = 0x8000000000ULL,
		.size = 0x8000000000ULL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE |
			 PTE_BLOCK_PXN | PTE_BLOCK_UXN
	}, {
		/* List terminator */
		0,
	}
};

struct mm_region *mem_map = qemu_arm64_mem_map;
#endif

#if IS_ENABLED(CONFIG_FIRMWARE_HANDOFF)
/* Boot parameters saved from lowlevel_init.S */
struct {
	unsigned long arg1;
	unsigned long arg3;
} qemu_saved_args __section(".data");
#define REGISTER_CONVENTION_VERSION_MASK (1 << 24)
#endif

int board_init(void)
{
	return 0;
}

int board_late_init(void)
{
	/*
	 * Make sure virtio bus is enumerated so that peripherals
	 * on the virtio bus can be discovered by their drivers
	 */
	virtio_init();

	return 0;
}

int dram_init(void)
{
	if (fdtdec_setup_mem_size_base() != 0)
		return -EINVAL;

	return 0;
}

int dram_init_banksize(void)
{
	fdtdec_setup_memory_banksize();

	return 0;
}

void *board_fdt_blob_setup(int *err)
{
	*err = 0;
	/*
	 * QEMU loads a generated DTB for us at the start of RAM. Either
	 * use this DTB or use the one handoff from previous boot stage.
	 */
	void *fdt = (void *)CFG_SYS_SDRAM_BASE;

	if (IS_ENABLED(CONFIG_FIRMWARE_HANDOFF)) {
		struct transfer_list_header *tl = NULL;
		struct transfer_list_entry *te_fdt = NULL;
		void *new_fdt;

		gd->transfer_list = NULL;
		/*
		 * If it looks like there's a valid DTB at the start of RAM save
		 * that as a backup DTB and use the location right after for the
		 * as the a temporary location of the new DTB.
		 */
		if (!fdt_check_header(fdt)) {
			new_fdt = (uint8_t *)fdt + roundup(fdt_totalsize(fdt), 0x1000);
			log_debug("A valid DTB already exists at 0x%lx\n", (uintptr_t)fdt);
		} else {
			new_fdt = fdt;
		}

		log_debug("saved args: arg1=0x%lx, arg3=0x%lx\n", qemu_saved_args.arg1,
			  qemu_saved_args.arg3);

		if (qemu_saved_args.arg1 == TRANSFER_LIST_SIGNATURE |
		    REGISTER_CONVENTION_VERSION_MASK &&
		    qemu_saved_args.arg3 &&
		    transfer_list_check_header((struct transfer_list_header *)
					       qemu_saved_args.arg3)) {
			tl = (struct transfer_list_header *)qemu_saved_args.arg3;
		} else {
			log_err("No valid Transfer List detected from the saved args\n");
			return fdt;
		}

		log_notice("Transfer List detected from 0x%lx, size:%d\n",
			   (uintptr_t)tl, tl->size);

		// save the Transfer List to global data
		gd->transfer_list = tl;

		// check if a DTB exists in TEs
		te_fdt = transfer_list_find(tl, TL_TAG_FDT);
		if (!te_fdt) {
			log_err("No valid DTB Transfer Entries detected from the Transfer List\n");
			return fdt;
		}

		unsigned long max_size = roundup(te_fdt->data_size, 1 << tl->alignment);

		log_notice("Extract DTB handoff from previous boot stage to 0x%lx, size:%ld\n",
			   (uintptr_t)new_fdt, max_size);

		if (fdt_open_into(transfer_list_data(te_fdt), new_fdt, max_size)) {
			log_err("Invalid DTB data extracted from the Transfer Entry\n");
			return fdt;
		}

		if (new_fdt != fdt) {
			log_notice("Copy DTB handoff from previous boot stage to 0x%lx\n",
				   (uintptr_t)fdt);
			fdt_move(new_fdt, fdt, fdt_totalsize(new_fdt));
		}
	}

	return fdt;
}

void enable_caches(void)
{
	 icache_enable();
	 dcache_enable();
}

#ifdef CONFIG_ARM64
#define __W	"w"
#else
#define __W
#endif

u8 flash_read8(void *addr)
{
	u8 ret;

	asm("ldrb %" __W "0, %1" : "=r"(ret) : "m"(*(u8 *)addr));
	return ret;
}

u16 flash_read16(void *addr)
{
	u16 ret;

	asm("ldrh %" __W "0, %1" : "=r"(ret) : "m"(*(u16 *)addr));
	return ret;
}

u32 flash_read32(void *addr)
{
	u32 ret;

	asm("ldr %" __W "0, %1" : "=r"(ret) : "m"(*(u32 *)addr));
	return ret;
}

void flash_write8(u8 value, void *addr)
{
	asm("strb %" __W "1, %0" : "=m"(*(u8 *)addr) : "r"(value));
}

void flash_write16(u16 value, void *addr)
{
	asm("strh %" __W "1, %0" : "=m"(*(u16 *)addr) : "r"(value));
}

void flash_write32(u32 value, void *addr)
{
	asm("str %" __W "1, %0" : "=m"(*(u32 *)addr) : "r"(value));
}
