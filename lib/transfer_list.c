// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2023, Linaro Limited
 */

#include <log.h>
#include <inttypes.h>
#include <transfer_list.h>
#include <string.h>

#define ADD_OVERFLOW(a, b, res) __builtin_add_overflow((a), (b), (res))

#define SUB_OVERFLOW(a, b, res) __builtin_sub_overflow((a), (b), (res))

#define MUL_OVERFLOW(a, b, res) __builtin_mul_overflow((a), (b), (res))

#define ROUNDUP_OVERFLOW(v, size, res) (__extension__({ \
	typeof(res) __res = res; \
	typeof(*(__res)) __roundup_tmp = 0; \
	typeof(v) __roundup_mask = (typeof(v))(size) - 1; \
	\
	ADD_OVERFLOW((v), __roundup_mask, &__roundup_tmp) ? 1 : \
		(void)(*(__res) = __roundup_tmp & ~__roundup_mask), 0; \
}))

#define ALIGN_MASK(p) ((1 << (p)) - 1)

#define IS_ALIGNED(x, a) (((x) & ALIGN_MASK((typeof(x))(a))) == 0)

#define ALIGN(x, a) (__extension__({ \
	typeof(x) __a = (a); \
	\
	((x) + ALIGN_MASK((__a))) & ~ALIGN_MASK((__a)); \
}))

/**
 * transfer_list_init
 *
 * Creating a transfer list in a reserved memory region specified
 * Compliant to 2.4.5 of Firmware handoff specification (v0.9)
 *
 * @addr:	Base memory address where to place the new transfer list
 * @max_size:	Available size in bytes to reserve for the transfer List
 *
 * Return:	Pointer to the created transfer list
 */
struct transfer_list_header *transfer_list_init(void *addr, size_t max_size)
{
	struct transfer_list_header *tl = addr;

	if (!addr || max_size == 0)
		return NULL;

	if (!IS_ALIGNED((uintptr_t)addr, TRANSFER_LIST_INIT_MAX_ALIGN) ||
	    !IS_ALIGNED(max_size, TRANSFER_LIST_INIT_MAX_ALIGN) ||
	    max_size < sizeof(*tl))
		return NULL;

	memset(tl, 0, max_size);
	tl->signature = TRANSFER_LIST_SIGNATURE;
	tl->version = TRANSFER_LIST_VERSION;
	tl->hdr_size = sizeof(*tl);
	tl->alignment = TRANSFER_LIST_INIT_MAX_ALIGN; // initial max align
	tl->size = sizeof(*tl); // initial size is the size of header
	tl->max_size = max_size;
	tl->flags = TL_FLAGS_HAS_CHECKSUM;
	transfer_list_update_checksum(tl);
	return tl;
}

/**
 * transfer_list_relocate
 *
 * Relocating a transfer list to a reserved memory region specified
 * Compliant to 2.4.6 of Firmware handoff specification (v0.9)
 *
 * @tl:		Pointer to the transfer list
 * @addr:	Pointer to the base memory address for relocating
 * @max_size:	Available size in bytes to reserve for relocating
 *
 * Return:	Pointer to the relocated transfer list
 */
struct transfer_list_header *transfer_list_relocate(struct transfer_list_header *tl,
						    void *addr, size_t max_size)
{
	uintptr_t aligned_addr;
	struct transfer_list_header *new_tl;
	u32 new_max_size;

	if (!tl || !addr || max_size == 0)
		return NULL;

	aligned_addr = ALIGN((uintptr_t)addr, tl->alignment);

	if (aligned_addr < (uintptr_t)addr)
		aligned_addr += (1 << tl->alignment);

	new_max_size = max_size - (aligned_addr - (uintptr_t)addr);
	// the new space is not sufficient for the tl
	if (tl->size > new_max_size)
		return NULL;

	new_tl = (struct transfer_list_header *)aligned_addr;
	memmove(new_tl, tl, tl->size);
	new_tl->max_size = new_max_size;
	transfer_list_update_checksum(new_tl);

	return new_tl;
}

/**
 * transfer_list_check_header
 *
 * Verifying the header of a transfer list
 * Compliant to 2.4.1 of Firmware handoff specification (v0.9)
 *
 * @tl:		Pointer to the transfer list
 *
 * Return:	true on success, false on error
 */
bool transfer_list_check_header(const struct transfer_list_header *tl)
{
	if (!tl)
		return false;

	if (tl->signature != TRANSFER_LIST_SIGNATURE) {
		log_err("Bad transfer list signature %#x\n",
			tl->signature);
		return false;
	}
	if (tl->version != TRANSFER_LIST_VERSION) {
		log_err("Unsupported transfer list version %#x\n",
			tl->version);
		return false;
	}
	if (!tl->max_size) {
		log_err("Bad transfer list max size %#x\n",
			tl->max_size);
		return false;
	}
	if (tl->size > tl->max_size) {
		log_err("Bad transfer list size %#x\n", tl->size);
		return false;
	}
	if (tl->hdr_size != sizeof(struct transfer_list_header)) {
		log_err("Bad transfer list header size %#x\n", tl->hdr_size);
		return false;
	}
	if (!transfer_list_verify_checksum(tl)) {
		log_err("Bad transfer list checksum %#x\n", tl->checksum);
		return false;
	}
	return true;
}

/**
 * transfer_list_next
 *
 * Enumerate the next transfer entry
 *
 * @tl:		Pointer to the transfer list
 * @last:	Pointer to the last enumerated transfer entry
 *
 * Return:	Pointer to the next transfer entry
 */
struct transfer_list_entry *transfer_list_next(struct transfer_list_header *tl,
					       struct transfer_list_entry *last)
{
	struct transfer_list_entry *te = NULL;
	uintptr_t tl_ev = 0;
	uintptr_t va = 0;
	uintptr_t ev = 0;
	size_t sz = 0;

	if (!tl)
		return NULL;

	tl_ev = (uintptr_t)tl + tl->size;
	if (last) {
		va = (uintptr_t)last;
		if (ADD_OVERFLOW(last->hdr_size, last->data_size, &sz) ||
		    ADD_OVERFLOW(va, sz, &va) ||
		    ROUNDUP_OVERFLOW(va, (1 << tl->alignment), &va))
			return NULL;
	} else {
		va = (uintptr_t)(tl + 1);
	}
	te = (struct transfer_list_entry *)va;

	if (va + sizeof(*te) > tl_ev || te->hdr_size < sizeof(*te) ||
	    ADD_OVERFLOW(va, te->hdr_size, &ev) ||
	    ADD_OVERFLOW(ev, te->data_size, &ev) ||
	    ev > tl_ev)
		return NULL;

	return te;
}

/**
 * calc_checksum
 *
 * Calculate the checksum of a transfer list
 *
 * @tl:		Pointer to the transfer list
 *
 * Return:	Checksum of the transfer list
 */
static u8 calc_checksum(const struct transfer_list_header *tl)
{
	u8 *b = (u8 *)tl;
	u8 cs = 0;
	size_t n = 0;

	if (!tl)
		return 0;

	for (n = 0; n < tl->size; n++)
		cs += b[n];

	return cs;
}

/**
 * transfer_list_update_checksum
 *
 * Update the checksum of a transfer list
 *
 * @tl:		Pointer to the transfer list
 *
 * Return:	Updated checksum of the transfer list
 */
void transfer_list_update_checksum(struct transfer_list_header *tl)
{
	u8 cs;

	if (!tl)
		return;

	cs = calc_checksum(tl);
	cs -= tl->checksum;
	cs = 256 - cs;
	tl->checksum = cs;
	assert((cs = calc_checksum(tl)) == 0 &&
	       transfer_list_verify_checksum(tl));
}

/**
 * transfer_list_verify_checksum
 *
 * Verify the checksum of a transfer list
 *
 * @tl:		Pointer to the transfer list
 *
 * Return:	true if verified, false if not
 */
bool transfer_list_verify_checksum(const struct transfer_list_header *tl)
{
	return !calc_checksum(tl);
}

/**
 * transfer_list_set_data_size
 *
 * Update the data size of a transfer entry
 *
 * @tl:			Pointer to the transfer list
 * @te:			Pointer to the transfer entry
 * @new_data_size:	New data size of the transfer entry
 *
 * Return:		true on success, false on error
 */
bool transfer_list_set_data_size(struct transfer_list_header *tl,
				 struct transfer_list_entry *te,
				 u32 new_data_size)
{
	uintptr_t tl_max_ev, tl_old_ev, new_ev, old_ev;

	if (!tl || !te)
		return false;

	tl_max_ev = (uintptr_t)tl + tl->max_size;
	tl_old_ev = (uintptr_t)tl + tl->size;
	new_ev = (uintptr_t)te;
	old_ev = (uintptr_t)te;

	if (ADD_OVERFLOW(old_ev, te->hdr_size, &old_ev) ||
	    ADD_OVERFLOW(old_ev, te->data_size, &old_ev) ||
	    ROUNDUP_OVERFLOW(old_ev, (1 << tl->alignment), &old_ev) ||
	    ADD_OVERFLOW(new_ev, te->hdr_size, &new_ev) ||
	    ADD_OVERFLOW(new_ev, new_data_size, &new_ev) ||
	    ROUNDUP_OVERFLOW(new_ev, (1 << tl->alignment), &new_ev))
		return false;

	if (new_ev > tl_max_ev)
		return false;

	te->data_size = new_data_size;

	if (new_ev > old_ev)
		tl->size += new_ev - old_ev;
	else
		tl->size -= old_ev - new_ev;

	if (new_ev != old_ev)
		memmove((void *)new_ev, (void *)old_ev, tl_old_ev - old_ev);

	transfer_list_update_checksum(tl);
	return true;
}

/**
 * transfer_list_grow_to_max_data_size
 *
 * Set the data size of a transfer entry to the maximum available value
 *
 * @tl:		Pointer to the transfer list
 * @te:		Pointer to the transfer entry
 *
 * Return:	true on success, false on error
 */
bool transfer_list_grow_to_max_data_size(struct transfer_list_header *tl,
					 struct transfer_list_entry *te)
{
	u32 sz;

	if (!tl || !te)
		return false;

	sz = tl->max_size - tl->size +
	     round_up(te->data_size, (1 << tl->alignment));

	return transfer_list_set_data_size(tl, te, sz);
}

/**
 * transfer_list_rem
 *
 * Remove a specified transfer entry from a transfer list
 *
 * @tl:		Pointer to the transfer list
 * @te:		Pointer to the transfer entry
 *
 * Return:	true on success, false on error
 */
bool transfer_list_rem(struct transfer_list_header *tl, struct transfer_list_entry *te)
{
	uintptr_t tl_ev, ev;

	if (!tl || !te)
		return false;

	tl_ev = (uintptr_t)tl + tl->size;
	ev = (uintptr_t)te;

	if (ADD_OVERFLOW(ev, te->hdr_size, &ev) ||
	    ADD_OVERFLOW(ev, te->data_size, &ev) ||
	    ROUNDUP_OVERFLOW(ev, (1 << tl->alignment), &ev) || ev > tl_ev)
		assert(0);

	memmove(te, (void *)ev, tl_ev - ev);
	tl->size -= ev - (uintptr_t)te;
	transfer_list_update_checksum(tl);
	return true;
}

/**
 * transfer_list_add
 *
 * Add a new transfer entry into a transfer list
 * Compliant to 2.4.3 of Firmware handoff specification (v0.9)
 *
 * @tl:		Pointer to the transfer list
 * @tag_id:	Tag id of the new transfer entry
 * @data_size:	Data size in byte for the new transfer entry
 * @data:	Pointer to the data to be added into the new transfer entry
 *
 * Return:	Pointer to the added transfer entry
 */
struct transfer_list_entry *transfer_list_add(struct transfer_list_header *tl,
					      u16 tag_id, u32 data_size,
					      const void *data)
{
	uintptr_t max_tl_ev, tl_ev, ev;
	struct transfer_list_entry *te = NULL;
	u8 *te_data = NULL;

	if (!tl)
		return NULL;

	// check if the data size is less than the max data size
	if (data_size > tl->max_size - tl->size +
		round_up(data_size, (1 << tl->alignment)))
		return NULL;

	max_tl_ev = (uintptr_t)tl + tl->max_size;
	tl_ev = (uintptr_t)tl + tl->size;
	ev = tl_ev;

	// skip the step 1 (optional step)
	// new TE will be added into the tail
	if (ADD_OVERFLOW(ev, sizeof(*te), &ev) ||
	    ADD_OVERFLOW(ev, data_size, &ev) ||
	    ROUNDUP_OVERFLOW(ev, (1 << tl->alignment), &ev) || ev > max_tl_ev)
		return NULL;

	te = (struct transfer_list_entry *)tl_ev;
	memset(te, 0, ev - tl_ev);
	te->tag_id = tag_id;
	te->hdr_size = sizeof(*te);
	te->data_size = data_size;
	tl->size += ev - tl_ev;

	// get the data pointer of the TE
	te_data = transfer_list_data(te);
	if (!te_data)
		return NULL;

	// fill 0 to the TE data if input data is NULL
	if (!data)
		memset(te_data, 0, data_size);
	else
		memmove(te_data, data, data_size);
	transfer_list_update_checksum(tl);

	return te;
}

/**
 * transfer_list_add_with_align
 *
 * Add a new transfer entry into a transfer list with specified new data alignment requirement
 * Compliant to 2.4.4 of Firmware handoff specification (v0.9)
 *
 * @tl:		Pointer to the transfer list
 * @tag_id:	Tag id of the new transfer entry
 * @data_size:	Data size in byte for the new transfer entry
 * @data:	Pointer to the data to be added into the new transfer entry
 * @alignment:	New data alignment
 *
 * Return:	Pointer to the added transfer entry
 */
struct transfer_list_entry *transfer_list_add_with_align(struct transfer_list_header *tl,
							 u16 tag_id, u32 data_size,
							 const void *data, u8 alignment)
{
	struct transfer_list_entry *te = NULL;
	uintptr_t tl_ev;

	if (!tl)
		return NULL;

	// TE start address is not aligned to the new alignment
	// fill the gap with an empty TE as a placeholder before
	// adding the desire TE
	tl_ev = (uintptr_t)tl + tl->size;
	if (!IS_ALIGNED(tl_ev, alignment))
		if (!transfer_list_add(tl, TL_TAG_EMPTY,
				       ALIGN(tl_ev, alignment) - tl_ev,
				       NULL))
			return NULL;

	te = transfer_list_add(tl, tag_id, data_size, data);
	if (alignment > tl->alignment) {
		tl->alignment = alignment;
		transfer_list_update_checksum(tl);
	}

	return te;
}

/**
 * transfer_list_find
 *
 * Search for an existing transfer entry with the specified tag id from a transfer list
 *
 * @tl:		Pointer to the transfer list
 * @tag_id:	Tag id of the new transfer entry
 *
 * Return:	Pointer to the found transfer entry
 */
struct transfer_list_entry *transfer_list_find(struct transfer_list_header *tl,
					       u16 tag_id)
{
	struct transfer_list_entry *te = NULL;

	if (!tl)
		return NULL;

	do
		te = transfer_list_next(tl, te);
	while (te && te->tag_id != tag_id);

	return te;
}
