#include <smbios.h>
// #include <sysinfo.h>
#include <stdio.h>

void sysinfo_get_cache_info(u8 level, struct smbios_type7 *cache_info) {
	unsigned long clidr_el1, ccsidr_el1;
	unsigned csselr_el1;
	printf("=====================> arm64 sysinfo_get_cache_info\n");

	// Read CLIDR_EL1
	asm volatile("mrs %0, clidr_el1" : "=r" (clidr_el1));
	printf("CLIDR_EL1: 0x%lx\n", clidr_el1);

	int cache_type = (clidr_el1 >> (3 * level)) & 0x7;
	if (!cache_type)
		return;  // No more cache levels

	// Select cache level
	csselr_el1 = (level << 1);  // Select data/unified cache (bit 0 = 0) and level
	asm volatile("msr csselr_el1, %0" : : "r" (csselr_el1));

	// Read CCSIDR_EL1
	asm volatile("mrs %0, ccsidr_el1" : "=r" (ccsidr_el1));
	printf("CCSIDR_EL1 (Level %d): 0x%lx\n", level, ccsidr_el1);

	// Extract cache size and associativity
	uint32_t line_size = (ccsidr_el1 & 0x7) + 4;  // Line size is in terms of log2(words) - 2
	uint32_t associativity = ((ccsidr_el1 >> 3) & 0x3FF) + 1;
	uint32_t num_sets = ((ccsidr_el1 >> 13) & 0x7FFF) + 1;
	uint32_t cache_size_kb = (associativity * num_sets * (1 << line_size)) / 1024;  // Size in KB
	printf("Level %d Cache: line_size=%u, associativity=%u, num_sets=%u, size_kb=%u\n", 
		level, line_size, associativity, num_sets, cache_size_kb);

	// Fill in the SMBIOS type 7 structure, skip the header
	// cache_info->type = SMBIOS_CACHE_INFORMATION;
	// cache_info->length = sizeof(struct smbios_type7);
	// cache_info->handle = 0xFFFF;  // This should be set to a unique handle value
	cache_info->config = (level << 8) | (1 << 7);  // Cache level and enabled
	cache_info->max_size = cache_size_kb > 2047 ? 0xFFFF : cache_size_kb;
	cache_info->inst_size = cache_info->max_size;
	cache_info->supp_sram_type = 0x0001;  // SRAM type: Other (example value)
	cache_info->curr_sram_type = 0x0001;    // SRAM type: Other (example value)
	cache_info->speed = 0;               // Unknown or not provided
	cache_info->err_corr_type = 0x03;  // Example: ECC
	cache_info->sys_cache_type = cache_type == 1 ? 0x04 : cache_type == 2 ? 0x05 : 0x03;  // Data, Instruction, Unified
	cache_info->associativity = associativity;	// Actual associativity value

	if (cache_size_kb > 2047) {
		cache_info->max_size2 = cache_size_kb;
		cache_info->inst_size2 = cache_size_kb;
	} else {
		cache_info->max_size2 = 0;
		cache_info->inst_size2 = 0;
	}
}
