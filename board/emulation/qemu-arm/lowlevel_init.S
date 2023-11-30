/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2023, Linaro Limited
 */

#include <config.h>

.global save_boot_params
save_boot_params:
#ifdef CONFIG_ARM64
	adr x9, qemu_saved_args
	stp x0, x1, [x9]
	/* Increment the address by 16 bytes for the next pair of values */
	stp x2, x3, [x9, #16]
#else
	ldr r12, =qemu_saved_args
	stm r12, {r0, r1, r2, r3}
#endif
	b	save_boot_params_ret
