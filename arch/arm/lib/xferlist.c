// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 Linaro Limited
 * Author: Raymond Mao <raymond.mao@linaro.org>
 */
#include <linux/types.h>
#include <errno.h>
#include <bloblist.h>
#include "xferlist.h"

int xferlist_from_boot_arg(ulong addr, ulong size)
{
	int ret;

	ret = bloblist_check(saved_args[3], size);
	if (ret)
		return ret;

	/* Check the register conventions */
	ret = bloblist_check_reg_conv(saved_args[0], saved_args[2],
				      saved_args[1]);
	if (!ret)
		/* Relocate the bloblist to the fixed address */
		ret = bloblist_reloc((void *)addr, size);

	return ret;
}
