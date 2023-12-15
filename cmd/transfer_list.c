// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2023, Linaro Limited
 */

#include <common.h>
#include <command.h>
#include <transfer_list.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

#define DATA_DUMP_ALIGN 32

static void dump_buf(const u8 *buffer, size_t size)
{
	int i;

	if (!buffer || size == 0)
		return;

	for (i = 0; i < size; i++) {
		if (i % DATA_DUMP_ALIGN == 0)
			printf("\n");
		printf("%02x", buffer[i]);
	}
	printf("\n");
}

static int do_tl(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	// get Transfer List from the saved global data
	struct transfer_list_header *tl = gd->transfer_list;

	if (argc < 2)
		return CMD_RET_USAGE;
	if (!tl) {
		printf("No Transfer list from previous boot stage found\n");
		return CMD_RET_FAILURE;
	}

	if (strncmp(argv[1], "list", 4) == 0) {
		struct transfer_list_entry *te = NULL;

		printf("Transfer list from address 0x%lx:\n", (uintptr_t)tl);
		printf("signature  0x%x\n", tl->signature);
		printf("checksum   0x%x\n", tl->checksum);
		printf("version    0x%x\n", tl->version);
		printf("hdr_size   0x%x\n", tl->hdr_size);
		printf("alignment  0x%x\n", tl->alignment);
		printf("size       0x%x\n", tl->size);
		printf("max_size   0x%x\n", tl->max_size);
		printf("flags      0x%x\n", tl->flags);
		while (true) {
			te = transfer_list_next(tl, te);
			if (!te)
				break;
			printf("\nEntry:\n");
			printf("tag_id     0x%x\n", te->tag_id);
			printf("hdr_size   0x%x\n", te->hdr_size);
			printf("data_size  0x%x\n", te->data_size);
			u8 *data_addr = transfer_list_data(te);

			printf("data_addr  0x%lx\n",
			       (unsigned long)data_addr);
			printf("data:\n");
			dump_buf(data_addr, te->data_size);
		}

		return CMD_RET_SUCCESS;
	}

	/* Unrecognized command */
	return CMD_RET_USAGE;
}

#ifdef CONFIG_SYS_LONGHELP
static char tl_help_text[] =
	"list - List all entries in the Firmware Transfer list\n";
#endif
U_BOOT_CMD(tl,	255,	0,	do_tl,
	   "Firmware transfer list utility commands", tl_help_text
);
