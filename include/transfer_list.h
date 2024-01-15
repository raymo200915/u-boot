/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2023, Linaro Limited
 */

#ifndef __TRANSFER_LIST_H
#define __TRANSFER_LIST_H

#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>

#define	TRANSFER_LIST_SIGNATURE		(u32)(0x4a0fb10b)
#define TRANSFER_LIST_VERSION		(1)

// Init value of maximum alignment required by any TE in the TL
// specified as a power of two
#define TRANSFER_LIST_INIT_MAX_ALIGN	(3)
#define TL_FLAGS_HAS_CHECKSUM BIT(0)

#ifndef __ASSEMBLER__

enum transfer_list_tag_id {
	TL_TAG_EMPTY = 0,
	TL_TAG_FDT = 1,
	TL_TAG_HOB_BLOCK = 2,
	TL_TAG_HOB_LIST = 3,
	TL_TAG_ACPI_TABLE_AGGREGATE = 4,
};

struct transfer_list_header {
	u32	signature;
	u8	checksum;
	u8	version;
	u8	hdr_size;
	u8	alignment;
	u32	size; // TL header + all TEs
	u32	max_size;
	u32	flags;
	/*
	 * Commented out element used to visualize dynamic part of the
	 * data structure.
	 *
	 * Note that struct transfer_list_entry also is dynamic in size
	 * so the elements can't be indexed directly but instead must be
	 * traversed in order
	 *
	 * struct transfer_list_entry entries[];
	 */
};

struct transfer_list_entry {
	u16	tag_id;
	u8	reserved0;	// place holder
	u8	hdr_size;
	u32	data_size;
	/*
	 * Commented out element used to visualize dynamic part of the
	 * data structure.
	 *
	 * Note that padding is added at the end of @data to make to reach
	 * a 16-byte boundary.
	 *
	 * uint8_t	data[ROUNDUP(data_size, 1 << tl->alignment)];
	 */
};

struct transfer_list_header *transfer_list_init(void *addr, size_t max_size);

struct transfer_list_header *transfer_list_relocate(struct transfer_list_header *tl,
						    void *addr, size_t max_size);
bool transfer_list_check_header(const struct transfer_list_header *tl);

void transfer_list_update_checksum(struct transfer_list_header *tl);
bool transfer_list_verify_checksum(const struct transfer_list_header *tl);

bool transfer_list_set_data_size(struct transfer_list_header *tl,
				 struct transfer_list_entry *entry,
				 u32 new_data_size);

bool transfer_list_grow_to_max_data_size(struct transfer_list_header *tl,
					 struct transfer_list_entry *entry);

static inline void *transfer_list_data(struct transfer_list_entry *entry)
{
	return (u8 *)entry + entry->hdr_size;
}

bool transfer_list_rem(struct transfer_list_header *tl, struct transfer_list_entry *entry);

struct transfer_list_entry *transfer_list_add(struct transfer_list_header *tl,
					      u16 tag_id, u32 data_size,
					      const void *data);

struct transfer_list_entry *transfer_list_add_with_align(struct transfer_list_header *tl,
							 u16 tag_id, u32 data_size,
							 const void *data, u8 alignment);

struct transfer_list_entry *transfer_list_next(struct transfer_list_header *tl,
					       struct transfer_list_entry *last);

struct transfer_list_entry *transfer_list_find(struct transfer_list_header *tl,
					       u16 tag_id);

#endif /*__ASSEMBLER__*/
#endif /*__TRANSFER_LIST_H*/
