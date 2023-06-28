/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2023, Linaro Limited
 */

#include <config.h>

.global save_boot_params
save_boot_params:
#ifdef CONFIG_ARM64
	adr	x9, qemu_saved_args
	stp	x1, x3, [x9]
#else
	ldr	r12, =qemu_saved_args
	stm	r12, {r1, r3}
#endif
	b	save_boot_params_ret
