#include <dm.h>
#include <smbios.h>
//#include <smbios_plat.h>
#include <sysinfo.h>

struct sysinfo_plat_priv {
	struct smbios_type7 t7[SYSINFO_MAX_CACHE_LEVEL];
	/*
	 * TODO: add other types here:
	 * Type 9 - System Slots
	 * Type 16 - Physical Memory Array
	 * Type 17 - Memory Device
	 * Type 19 - Memory Array Mapped Address
	 */
};

/* weak function for the platforms not yet supported */
__weak void sysinfo_get_cache_info(u8 level,
				   struct smbios_type7 *cache_info)
{
	printf("=====================> weak sysinfo_get_cache_info\n");
}

static void print_smbios_cache_info(struct smbios_type7 *cache_info) {
	printf("SMBIOS Type 7 (Cache Information):\n");
	// printf("  Type: %u\n", cache_info->type);
	// printf("  Length: %u\n", cache_info->length);
	// printf("  Handle: 0x%04x\n", cache_info->handle);
	printf("  Cache Configuration: 0x%04x\n", cache_info->config);
	printf("  Maximum Cache Size: %u KB\n", cache_info->max_size);
	printf("  Installed Size: %u KB\n", cache_info->inst_size);
	printf("  Supported SRAM Type: 0x%04x\n", cache_info->supp_sram_type);
	printf("  Current SRAM Type: 0x%04x\n", cache_info->curr_sram_type);
	printf("  Cache Speed: %u\n", cache_info->speed);
	printf("  Error Correction Type: %u\n", cache_info->err_corr_type);
	printf("  System Cache Type: %u\n", cache_info->sys_cache_type);
	printf("  Associativity: %u\n", cache_info->associativity);
	printf("  Maximum Cache Size 2: %u KB\n", cache_info->max_size2);
	printf("  Installed Cache Size 2: %u KB\n", cache_info->inst_size2);
}

static int sysinfo_plat_detect(struct udevice *dev)
{
	struct sysinfo_plat_priv *priv = dev_get_priv(dev);
	u8 level;

	printf("=====================> sysinfo_plat_detect\n");

	for (level = 0; level < SYSINFO_MAX_CACHE_LEVEL; level++) {
		sysinfo_get_cache_info(level, &priv->t7[level]);
		if (!priv->t7[level].config)
			break;
		print_smbios_cache_info(&priv->t7[level]);
	}
	if (!level) /* no cache detected */
		return -ENOTSUPP;

	return 0;
}

static int sysinfo_plat_get_str(struct udevice *dev, int id,
				    size_t size, char *val)
{
	struct sysinfo_plat_priv *priv = dev_get_priv(dev);
	const char *str = NULL;

	switch (id) {
		/*
		 * TODO:
		 * add those SYSINFO_ID_SMBIOS_ here, which need getting
		 * strings from the platform-specific driver
		 */
		default:
			break;
	}

	if (!str)
		return -ENOTSUPP;

	strlcpy(val, str, size);

	return  0;
}

static int sysinfo_plat_get_int(struct udevice *dev, int id, int *val)
{
	struct sysinfo_plat_priv *priv = dev_get_priv(dev);

	switch (id) {
		case SYSINFO_CACHE_LEVEL:
			u8 level;

			for (level = 0; level < SYSINFO_MAX_CACHE_LEVEL; level++) {
				if (!priv->t7[level].config)
					break;
			}
			if (!level) /* no cache detected */
				return -ENOTSUPP;

			*val = level - 1;
			break;
		default:
			break;
	}

	return  0;
}


static int sysinfo_plat_probe(struct udevice *dev)
{
	printf("=====================> sysinfo_plat_probe\n");
	return 0;
}

// static const struct udevice_id sysinfo_smbios_ids[] = {
// 	{ .compatible = "u-boot,sysinfo-smbios" },
// 	{ /* sentinel */ }
// };

static const struct sysinfo_ops sysinfo_smbios_ops = {
	.detect = sysinfo_plat_detect,
	.get_str = sysinfo_plat_get_str,
	.get_int = sysinfo_plat_get_int,
};

U_BOOT_DRIVER(sysinfo_smbios_plat) = {
	.name           = "sysinfo_smbios_plat",
	.id             = UCLASS_SYSINFO,
	// .of_match       = sysinfo_smbios_ids,
	.ops		= &sysinfo_smbios_ops,
	.priv_auto	= sizeof(struct sysinfo_plat_priv),
	.probe		= sysinfo_plat_probe,
};
