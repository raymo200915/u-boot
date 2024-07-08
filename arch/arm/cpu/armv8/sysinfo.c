// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2024 Linaro Limited
 * Author: Raymond Mao <raymond.mao@linaro.org>
 */
#include <dm.h>
#include <smbios_plat.h>
#include <stdio.h>

union ccsidr_el1 {
	struct {
		u64 linesize:3;
		u64 associativity:10;
		u64 numsets:15;
		u64 unknown:4;
		u64 reserved:32;
	} no_ccidx;
	struct {
		u64 linesize:3;
		u64 associativity:21;
		u64 reserved1:8;
		u64 numsets:24;
		u64 reserved2:8;
	} ccidx_aarch64;
	struct {
		u64 linesize:3;
		u64 associativity:21;
		u64 reserved:8;
		u64 unallocated:32;
	} ccidx_aarch32;
	u64 data;
};

enum {
	CACHE_NONE,
	CACHE_INST_ONLY,
	CACHE_DATA_ONLY,
	CACHE_INST_WITH_DATA,
	CACHE_UNIFIED,
};

enum {
        CACHE_ASSOC_DIRECT_MAPPED = 1,
        CACHE_ASSOC_2WAY = 2,
        CACHE_ASSOC_4WAY = 4,
        CACHE_ASSOC_8WAY = 8,
        CACHE_ASSOC_16WAY = 16,
        CACHE_ASSOC_12WAY = 12,
        CACHE_ASSOC_24WAY = 24,
        CACHE_ASSOC_32WAY = 32,
        CACHE_ASSOC_48WAY = 48,
        CACHE_ASSOC_64WAY = 64,
        CACHE_ASSOC_20WAY = 20,
};

struct cache_info cache_info_armv8[SYSINFO_CACHE_LVL_MAX];

/*
 * TODO:
 * To support ARMv8.3, we need to read "CCIDX, bits [23:20]" from
 * ID_AA64MMFR2_EL1 to get the format of CCSIDR_EL1:
 *
 * 0b0000 - 32-bit format implemented for all levels of the CCSIDR_EL1.
 * 0b0001 - 64-bit format implemented for all levels of the CCSIDR_EL1.
 * 
 * Here we assume to use CCSIDR_EL1 in no CCIDX layout:
 * NumSets, bits [27:13]: (Number of sets in cache) - 1
 * Associativity, bits [12:3]: (Associativity of cache) - 1
 * LineSize, bits [2:0]: (Log2(Number of bytes in cache line)) - 4
 */
int sysinfo_get_cache_info(u8 level, struct cache_info *cinfo) {
	u64 clidr_el1;
	u32 csselr_el1;
	u32 num_sets;
	union ccsidr_el1 creg;
	int cache_type;

	sysinfo_cache_info_default(cinfo);

	/* Read CLIDR_EL1 */
	asm volatile("mrs %0, clidr_el1" : "=r" (clidr_el1));
	log_debug("CLIDR_EL1: 0x%llx\n", clidr_el1);

	cache_type = (clidr_el1 >> (3 * level)) & 0x7;
	log_debug("cache_type:%d\n", cache_type);

	if (cache_type == CACHE_NONE) /* level does not exist */
		return -1;

	switch (cache_type) {
	case CACHE_INST_ONLY:
		cinfo->cache_type = SMBIOS_CACHE_SYSCACHE_TYPE_INSTRUCTION;
		break;
	case CACHE_DATA_ONLY:
		cinfo->cache_type = SMBIOS_CACHE_SYSCACHE_TYPE_DATA;
		break;
	case CACHE_UNIFIED:
		cinfo->cache_type = SMBIOS_CACHE_SYSCACHE_TYPE_UNIFIED;
		break;
	case CACHE_INST_WITH_DATA:
		cinfo->cache_type = SMBIOS_CACHE_SYSCACHE_TYPE_OTHER;
		break;
	default:
		cinfo->cache_type = SMBIOS_CACHE_SYSCACHE_TYPE_UNKNOWN;
		break;
	}

	/* Select cache level */
	csselr_el1 = (level << 1);
	asm volatile("msr csselr_el1, %0" : : "r" (csselr_el1));

	/* Read CCSIDR_EL1 */
	asm volatile("mrs %0, ccsidr_el1" : "=r" (creg.data));
	log_debug("CCSIDR_EL1 (Level %d): 0x%llx\n", level + 1, creg.data);

	/* Extract cache size and associativity */
	cinfo->line_size = 1 << (creg.no_ccidx.linesize + 4);

	/* Map the associativity value */
	switch (creg.no_ccidx.associativity + 1) {
	case CACHE_ASSOC_DIRECT_MAPPED:
		cinfo->associativity = SMBIOS_CACHE_ASSOC_DMAPPED;
		break;
	case CACHE_ASSOC_2WAY:
		cinfo->associativity = SMBIOS_CACHE_ASSOC_2WAY;
		break;
	case CACHE_ASSOC_4WAY:
		cinfo->associativity = SMBIOS_CACHE_ASSOC_4WAY;
		break;
	case CACHE_ASSOC_8WAY:
		cinfo->associativity = SMBIOS_CACHE_ASSOC_8WAY;
		break;
	case CACHE_ASSOC_16WAY:
		cinfo->associativity = SMBIOS_CACHE_ASSOC_16WAY;
		break;
	case CACHE_ASSOC_12WAY:
		cinfo->associativity = SMBIOS_CACHE_ASSOC_12WAY;
		break;
	case CACHE_ASSOC_24WAY:
		cinfo->associativity = SMBIOS_CACHE_ASSOC_24WAY;
		break;
	case CACHE_ASSOC_32WAY:
		cinfo->associativity = SMBIOS_CACHE_ASSOC_32WAY;
		break;
	case CACHE_ASSOC_48WAY:
		cinfo->associativity = SMBIOS_CACHE_ASSOC_48WAY;
		break;
	case CACHE_ASSOC_64WAY:
		cinfo->associativity = SMBIOS_CACHE_ASSOC_64WAY;
		break;
	case CACHE_ASSOC_20WAY:
		cinfo->associativity = SMBIOS_CACHE_ASSOC_20WAY;
		break;
	default:
		cinfo->associativity = SMBIOS_CACHE_ASSOC_UNKNOWN;
		break;
	}

	num_sets = creg.no_ccidx.numsets + 1;
	/* Size in KB */
	cinfo->max_size = (cinfo->associativity * num_sets * cinfo->line_size) /
			  1024;

	log_debug("L%d Cache:\n", level + 1);
	log_debug("Number of bytes in cache line:%u\n", cinfo->line_size);
	log_debug("Associativity of cache:%u\n", cinfo->associativity);
	log_debug("Number of sets in cache:%u\n", num_sets);
	log_debug("Cache size in KB:%u\n", cinfo->max_size);

	cinfo->inst_size = cinfo->max_size;

	/*
	 * Below fields are set with default values for Arm arch,
	 * More information are hardware platform specific.
	 */
	cinfo->config.fields.level = level;
	/* Generally, "Not-Socketed" is for integrated caches */
	cinfo->config.fields.bsocketed = SMBIOS_CACHE_UNSOCKETED;
	/* For modern SoCs, L1/L2 cache are usually internal */
	cinfo->config.fields.locate = SMBIOS_CACHE_LOCATE_INTERNAL;
	cinfo->config.fields.benabled = SMBIOS_CACHE_ENABLED;
	/* Generally, L1 and L2 caches are write-back */
	cinfo->config.fields.opmode = SMBIOS_CACHE_OP_WB;

	/*
	 * Other fields are typically specific to the implementation of the ARM
	 * processor by the silicon vendor:
	 * supp_sram_type, curr_sram_type, speed, err_corr_type
	 */

	return 0;
}

struct sysinfo_plat sysinfo_smbios_armv8 = {
	.sys.manufacturer = "arm",
	.sys.prod_name = "arm",
	.sys.version = "armv8",
	.sys.sn = "Not Specified",
	.sys.sku_num = "Not Specified",
	.sys.family = "arm",
	.board.manufacturer = "arm",
	.board.prod_name = "arm",
	.board.version = "armv8",
	.board.sn = "Not Specified",
	.board.asset_tag = "Not Specified",
	.board.chassis_locat = "Not Specified",
	.chassis.manufacturer = "arm",
	.chassis.version = "armv8",
	.chassis.sn = "Not Specified",
	.chassis.asset_tag = "Not Specified",
	.cache = &cache_info_armv8[0],
};

U_BOOT_DRVINFO(sysinfo_smbios_plat) = {
	.name = "sysinfo_smbios_plat",
	.plat = &sysinfo_smbios_armv8,
};
