/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Qualcomm Pin control
 *
 * (C) Copyright 2018 Ramon Fried <ramon.fried@gmail.com>
 *
 */
#ifndef _PINCTRL_SNAPDRAGON_H
#define _PINCTRL_SNAPDRAGON_H

struct msm_pinctrl_data {
	int pin_count;
	int functions_count;
	const char *(*get_function_name)(struct udevice *dev,
					 unsigned int selector);
	unsigned int (*get_function_mux)(unsigned int selector);
	const char *(*get_pin_name)(struct udevice *dev,
				    unsigned int selector);
};

struct pinctrl_function {
	const char *name;
	int val;
};

extern struct msm_pinctrl_data ipq4019_data;

#endif
