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

/*
 * Minimum C-PHY timings based on the MIPI Alliance Specification for
 * C-PHY (v1.0), Table 18 "Global Operation Timing Parameters". These
 * parameters are not data rate dependent (see sections 6.14.3 and
 * 6.14.4 of that specification), so unlike the D-PHY equivalent this
 * default-config helper does not need the symbol rate to compute them.
 */
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

	cfg->t3_prepare = 38000;
	cfg->t3_term_en = 0;
	cfg->t3_settle = 95000;
	cfg->t3_hs_exit = 100000;
	cfg->lpx = 50000;
	cfg->init = 100;
	cfg->ta_get = 5 * cfg->lpx;
	cfg->ta_go = 4 * cfg->lpx;
	cfg->ta_sure = cfg->lpx;
	cfg->wakeup = 1000;

	return 0;
}
EXPORT_SYMBOL_GPL(phy_mipi_cphy_get_default_config);

/*
 * Validate C-PHY configuration according to the MIPI Alliance
 * Specification for C-PHY (v1.0), Table 18 "Global Operation Timing
 * Parameters".
 */
int phy_mipi_cphy_config_validate(struct phy_configure_opts_mipi_cphy *cfg)
{
	if (!cfg || !cfg->lanes)
		return -EINVAL;

	if (cfg->t3_prepare < 38000 || cfg->t3_prepare > 95000)
		return -EINVAL;

	if (cfg->t3_term_en > 38000)
		return -EINVAL;

	if (cfg->t3_settle < 95000 || cfg->t3_settle > 300000)
		return -EINVAL;

	if (cfg->t3_hs_exit < 100000)
		return -EINVAL;

	if (cfg->lpx < 50000)
		return -EINVAL;

	if (cfg->init < 100)
		return -EINVAL;

	if (cfg->ta_get != 5 * cfg->lpx)
		return -EINVAL;

	if (cfg->ta_go != 4 * cfg->lpx)
		return -EINVAL;

	if (cfg->ta_sure < cfg->lpx || cfg->ta_sure > (2 * cfg->lpx))
		return -EINVAL;

	if (cfg->wakeup < 1000)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL_GPL(phy_mipi_cphy_config_validate);
