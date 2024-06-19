/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2024 Linaro Limited
 * Author: Raymond Mao <raymond.mao@linaro.org>
 */
#ifndef __SMBIOS_PLAT_H
#define __SMBIOS_PLAT_H

#include <smbios.h>

/*
 * TODO:
 * sysinfo_plat and all sub data structure should be moved to <asm/sysinfo.h>
 * if we have this defined for each arch.
 */
struct __packed cache_info {
	union cache_config config;
	u32 line_size;
	u32 associativity;
	u32 max_size;
	u32 inst_size;
	u8 cache_type;
	union cache_sram_type supp_sram_type;
	union cache_sram_type curr_sram_type;
	u8 speed;
	u8 err_corr_type;
};

struct sysinfo_plat {
	struct cache_info *cache;
	/* add other sysinfo structure here */
};

#define SYSINFO_CACHE_LVL_MAX 3
#define SYSINFO_CACHE_SIZE_DWORD_THRES_KB 2047 * 1024 /* 2047 MiB */

#if CONFIG_IS_ENABLED(SYSINFO_SMBIOS)
int sysinfo_get_cache_info(u8 level, struct cache_info *cache_info);
void sysinfo_cache_info_default(struct cache_info *ci);
#else
static inline int sysinfo_get_cache_info(u8 level,
					  struct cache_info *cache_info)
{
	return 0;
}
static inline void sysinfo_cache_info_default(struct cache_info *ci)
{
}
#endif

#endif	/* __SMBIOS_PLAT_H */
