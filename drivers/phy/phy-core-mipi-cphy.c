/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Qualcomm Innovation Center, Inc.
 */

#include <linux/errno.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/time64.h>

#include <linux/phy/phy.h>
#include <linux/phy/phy-mipi-cphy.h>

/*
 * Minimum C-PHY timings based on the MIPI C-PHY specification global
 * operation timing parameters. C-PHY transports 16 payload bits per 7
 * transmitted symbols, so the symbol rate is 7/16 of the raw bit rate.
 */
#define CPHY_BITS_PER_GROUP	16
#define CPHY_SYMBOLS_PER_GROUP	7

int phy_mipi_cphy_get_default_config(unsigned long pixel_clock,
				     unsigned int bpp,
				     unsigned int lanes,
				     struct phy_configure_opts_mipi_cphy *cfg)
{
	unsigned long long bit_rate, symbol_rate;

	if (!cfg)
		return -EINVAL;

	if (!lanes)
		return -EINVAL;

	bit_rate = (unsigned long long)pixel_clock * bpp;
	do_div(bit_rate, lanes);

	/* symbol_rate = bit_rate * 7 / 16 (16 bits carried per 7 symbols) */
	symbol_rate = bit_rate * CPHY_SYMBOLS_PER_GROUP;
	do_div(symbol_rate, CPHY_BITS_PER_GROUP);

	/* lane_rate is expressed in ksps */
	do_div(symbol_rate, 1000);

	cfg->t3_prepare = 38000;
	cfg->t3_lpx = 38000;
	cfg->ths_settle = 38000;
	cfg->ths_exit = 100000;
	cfg->twakeup = 1000;
	cfg->tinit = 100;

	cfg->lane_rate = symbol_rate;
	cfg->lanes = lanes;

	return 0;
}
EXPORT_SYMBOL(phy_mipi_cphy_get_default_config);

/*
 * Validate C-PHY configuration against the MIPI C-PHY specification
 * global operation timing parameter minimums.
 */
int phy_mipi_cphy_config_validate(struct phy_configure_opts_mipi_cphy *cfg)
{
	if (!cfg)
		return -EINVAL;

	if (!cfg->lanes)
		return -EINVAL;

	if (!cfg->lane_rate)
		return -EINVAL;

	if (cfg->t3_prepare < 38000)
		return -EINVAL;

	if (cfg->t3_lpx < 38000)
		return -EINVAL;

	if (cfg->ths_settle < 38000)
		return -EINVAL;

	if (cfg->ths_exit < 100000)
		return -EINVAL;

	if (cfg->twakeup < 1000)
		return -EINVAL;

	if (cfg->tinit < 100)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(phy_mipi_cphy_config_validate);
