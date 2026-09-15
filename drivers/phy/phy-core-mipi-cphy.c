// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 Linaro Ltd.
 *
 * Generic MIPI C-PHY configuration helpers.
 */
#include <linux/phy/phy-mipi-cphy.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/math64.h>

/*
 * C-PHY carries 16 bits per 7 symbols per trio (lane).
 */
#define PHY_MIPI_CPHY_SYMBOLS_PER_GROUP		7
#define PHY_MIPI_CPHY_BITS_PER_GROUP		16

int phy_mipi_cphy_get_default_config(unsigned long pixel_clock,
				     unsigned int bpp, unsigned int lanes,
				     struct phy_configure_opts_mipi_cphy *cfg)
{
	unsigned long long sym_rate;

	if (!cfg || !lanes || !bpp)
		return -EINVAL;

	/* total payload bits/s = pixel_clock * bpp; spread over lanes trios */
	sym_rate = (unsigned long long)pixel_clock * bpp;
	sym_rate = DIV_ROUND_UP_ULL(sym_rate * PHY_MIPI_CPHY_SYMBOLS_PER_GROUP,
				    (unsigned long long)PHY_MIPI_CPHY_BITS_PER_GROUP * lanes);

	cfg->lanes = lanes;
	cfg->hs_clk_rate = sym_rate;

	return 0;
}
EXPORT_SYMBOL_GPL(phy_mipi_cphy_get_default_config);

int phy_mipi_cphy_config_validate(struct phy_configure_opts_mipi_cphy *cfg)
{
	if (!cfg || !cfg->lanes)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL_GPL(phy_mipi_cphy_config_validate);
