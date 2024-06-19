#ifndef __SMBIOS_PLAT_H
#define __SMBIOS_PLAT_H

#define SYSINFO_MAX_CACHE_LEVEL 3

enum {
	SYSINFO_CACHE_LEVEL = 0,
};

#if CONFIG_IS_ENABLED(SYSINFO_SMBIOS)
void sysinfo_get_cache_info(u8 level, struct smbios_type7 *cache_info);
#else
static inline void sysinfo_get_cache_info(u8 level,
					  struct smbios_type7 *cache_info)
{
}
#endif

#endif	/* __SMBIOS_PLAT_H */
