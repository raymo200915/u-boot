// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * The 'smbios' command displays information from the SMBIOS table.
 *
 * Copyright (c) 2023, Heinrich Schuchardt <heinrich.schuchardt@canonical.com>
 */

#include <command.h>
#include <hexdump.h>
#include <mapmem.h>
#include <smbios.h>
#include <tables_csum.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

static const char * const wakeup_type_strings[] = {
	"Reserved",		/* 0x00 */
	"Other",		/* 0x01 */
	"Unknown",		/* 0x02 */
	"APM Timer",		/* 0x03 */
	"Modem Ring",		/* 0x04 */
	"Lan Remote",		/* 0x05 */
	"Power Switch",		/* 0x06 */
	"PCI PME#",		/* 0x07 */
	"AC Power Restored",	/* 0x08 */
};

static const char * const err_corr_type_strings[] = {
	"Reserved",		/* 0x00 */
	"Other",		/* 0x01 */
	"Unknown",		/* 0x02 */
	"None",			/* 0x03 */
	"Parity",		/* 0x04 */
	"Single-bit ECC",	/* 0x05 */
	"Multi-bit ECC",	/* 0x06 */
};

static const char * const sys_cache_type_strings[] = {
	"Reserved",		/* 0x00 */
	"Other",		/* 0x01 */
	"Unknown",		/* 0x02 */
	"Instruction",		/* 0x03 */
	"Data",			/* 0x04 */
	"Unified",		/* 0x05 */
};

static const char * const associativity_strings[] = {
	"Reserved",			/* 0x00 */
	"Other",			/* 0x01 */
	"Unknown",			/* 0x02 */
	"Direct Mapped",		/* 0x03 */
	"2-way Set-Associative",	/* 0x04 */
	"4-way Set-Associative",	/* 0x05 */
	"Fully Associative",		/* 0x06 */
	"8-way Set-Associative",	/* 0x07 */
	"16-way Set-Associative",	/* 0x08 */
	"12-way Set-Associative",	/* 0x09 */
	"24-way Set-Associative",	/* 0x0a */
	"32-way Set-Associative",	/* 0x0b */
	"48-way Set-Associative",	/* 0x0c */
	"64-way Set-Associative",	/* 0x0d */
	"20-way Set-Associative",	/* 0x0e */
};

/**
 * smbios_get_string() - get SMBIOS string from table
 *
 * @table:	SMBIOS table
 * @index:	index of the string
 * Return:	address of string, may point to empty string
 */
static const char *smbios_get_string(void *table, int index)
{
	const char *str = (char *)table +
			  ((struct smbios_header *)table)->length;
	static const char fallback[] = "Not Specified";

	if (!index)
		return fallback;

	if (!*str)
		++str;
	for (--index; *str && index; --index)
		str += strlen(str) + 1;

	return str;
}

static struct smbios_header *next_table(struct smbios_header *table)
{
	const char *str;

	if (table->type == SMBIOS_END_OF_TABLE)
		return NULL;

	str = smbios_get_string(table, -1);
	return (struct smbios_header *)(++str);
}

static void smbios_print_generic(struct smbios_header *table)
{
	char *str = (char *)table + table->length;

	if (CONFIG_IS_ENABLED(HEXDUMP)) {
		printf("Header and Data:\n");
		print_hex_dump("\t", DUMP_PREFIX_OFFSET, 16, 1,
			       table, table->length, false);
	}
	if (*str) {
		printf("Strings:\n");
		for (int index = 1; *str; ++index) {
			printf("\tString %u: %s\n", index, str);
			str += strlen(str) + 1;
		}
	}
}

void smbios_print_str(const char *label, void *table, u8 index)
{
	printf("\t%s: %s\n", label, smbios_get_string(table, index));
}

const char *smbios_wakeup_type_str(u8 wakeup_type)
{
	if (wakeup_type >= ARRAY_SIZE(wakeup_type_strings))
		/* Values over 0x08 are reserved. */
		wakeup_type = 0;
	return wakeup_type_strings[wakeup_type];
}

static void smbios_print_type1(struct smbios_type1 *table)
{
	printf("System Information\n");
	smbios_print_str("Manufacturer", table, table->manufacturer);
	smbios_print_str("Product Name", table, table->product_name);
	smbios_print_str("Version", table, table->version);
	smbios_print_str("Serial Number", table, table->serial_number);
	if (table->length >= 0x19) {
		printf("\tUUID: %pUl\n", table->uuid);
		printf("\tWake-up Type: %s\n",
		       smbios_wakeup_type_str(table->wakeup_type));
	}
	if (table->length >= 0x1b) {
		smbios_print_str("SKU Number", table, table->sku_number);
		smbios_print_str("Family", table, table->family);
	}
}

static void smbios_print_type2(struct smbios_type2 *table)
{
	u16 *handle;

	printf("Base Board Information\n");
	smbios_print_str("Manufacturer", table, table->manufacturer);
	smbios_print_str("Product Name", table, table->product_name);
	smbios_print_str("Version", table, table->version);
	smbios_print_str("Serial Number", table, table->serial_number);
	smbios_print_str("Asset Tag", table, table->asset_tag_number);
	printf("\tFeature Flags: 0x%04x\n", table->feature_flags);
	smbios_print_str("Chassis Location", table, table->chassis_location);
	printf("\tChassis Handle: 0x%04x\n", table->chassis_handle);
	smbios_print_str("Board Type", table, table->board_type);
	printf("\tContained Object Handles: ");
	handle = (void *)table->eos;
	for (int i = 0; i < table->number_contained_objects; ++i)
		printf("0x%04x ", handle[i]);
	printf("\n");
}

const char *smbios_cache_err_corr_type_str(u8 err_corr_type)
{
	if (err_corr_type >= ARRAY_SIZE(err_corr_type_strings))
		err_corr_type = 0;
	return err_corr_type_strings[err_corr_type];
}

const char *smbios_cache_sys_cache_type_str(u8 sys_cache_type)
{
	if (sys_cache_type >= ARRAY_SIZE(sys_cache_type_strings))
		sys_cache_type = 0;
	return sys_cache_type_strings[sys_cache_type];
}

const char *smbios_cache_associativity_str(u8 associativity)
{
	if (associativity >= ARRAY_SIZE(associativity_strings))
		associativity = 0;
	return associativity_strings[associativity];
}

static void smbios_print_type7(struct smbios_type7 *table)
{
	printf("Cache Information:\n");
	printf("\tCache Configuration: 0x%04x\n", table->config.data);
	printf("\tMaximum Cache Size: 0x%04x\n", table->max_size.data);
	printf("\tInstalled Size: 0x%04x\n", table->inst_size.data);
	printf("\tSupported SRAM Type: 0x%04x\n", table->supp_sram_type.data);
	printf("\tCurrent SRAM Type: 0x%04x\n", table->curr_sram_type.data);
	printf("\tCache Speed: 0x%04x\n", table->speed);
	printf("\tError Correction Type: %s\n",
		smbios_cache_err_corr_type_str(table->err_corr_type));
	printf("\tSystem Cache Type: %s\n",
		smbios_cache_sys_cache_type_str(table->sys_cache_type));
	printf("\tAssociativity: %s\n",
		smbios_cache_associativity_str(table->associativity));
	printf("\tMaximum Cache Size 2: 0x%04x\n", table->max_size2.data);
	printf("\tInstalled Cache Size 2: 0x%04x\n", table->inst_size2.data);
}

static void smbios_print_type127(struct smbios_type127 *table)
{
	printf("End Of Table\n");
}

static int do_smbios(struct cmd_tbl *cmdtp, int flag, int argc,
		     char *const argv[])
{
	ulong addr;
	void *entry;
	u32 size;
	char version[12];
	struct smbios_header *table;
	static const char smbios_sig[] = "_SM_";
	static const char smbios3_sig[] = "_SM3_";
	size_t count = 0;
	u32 table_maximum_size;

	addr = gd_smbios_start();
	if (!addr) {
		log_warning("SMBIOS not available\n");
		return CMD_RET_FAILURE;
	}
	entry = map_sysmem(addr, 0);
	if (!memcmp(entry, smbios3_sig, sizeof(smbios3_sig) - 1)) {
		struct smbios3_entry *entry3 = entry;

		table = (void *)(uintptr_t)entry3->struct_table_address;
		snprintf(version, sizeof(version), "%d.%d.%d",
			 entry3->major_ver, entry3->minor_ver, entry3->doc_rev);
		table = (void *)(uintptr_t)entry3->struct_table_address;
		size = entry3->length;
		table_maximum_size = entry3->table_maximum_size;
	} else if (!memcmp(entry, smbios_sig, sizeof(smbios_sig) - 1)) {
		struct smbios_entry *entry2 = entry;

		snprintf(version, sizeof(version), "%d.%d",
			 entry2->major_ver, entry2->minor_ver);
		table = (void *)(uintptr_t)entry2->struct_table_address;
		size = entry2->length;
		table_maximum_size = entry2->struct_table_length;
	} else {
		log_err("Unknown SMBIOS anchor format\n");
		return CMD_RET_FAILURE;
	}
	if (table_compute_checksum(entry, size)) {
		log_err("Invalid anchor checksum\n");
		return CMD_RET_FAILURE;
	}
	printf("SMBIOS %s present.\n", version);

	for (struct smbios_header *pos = table; pos; pos = next_table(pos))
		++count;
	printf("%zd structures occupying %d bytes\n", count, table_maximum_size);
	printf("Table at 0x%llx\n", (unsigned long long)map_to_sysmem(table));

	for (struct smbios_header *pos = table; pos; pos = next_table(pos)) {
		printf("\nHandle 0x%04x, DMI type %d, %d bytes at 0x%llx\n",
		       pos->handle, pos->type, pos->length,
		       (unsigned long long)map_to_sysmem(pos));
		switch (pos->type) {
		case 1:
			smbios_print_type1((struct smbios_type1 *)pos);
			break;
		case 2:
			smbios_print_type2((struct smbios_type2 *)pos);
			break;
		case 7:
			smbios_print_type7((struct smbios_type7 *)pos);
			break;
		case 127:
			smbios_print_type127((struct smbios_type127 *)pos);
			break;
		default:
			smbios_print_generic(pos);
			break;
		}
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_LONGHELP(smbios, "- display SMBIOS information");

U_BOOT_CMD(smbios, 1, 0, do_smbios, "display SMBIOS information",
	   smbios_help_text);
