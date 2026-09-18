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
 * Fill in the C-PHY global operation timing parameters based on the MIPI
 * Alliance Specification for C-PHY (v1.0), Table 18 "Global Operation Timing
 * Parameters". These parameters are not data rate dependent (see sections
 * 6.14.3 and 6.14.4 of that specification), so unlike the D-PHY equivalent
 * this helper does not need the symbol rate to compute them.
 */
static int phy_mipi_cphy_calc_config(unsigned long pixel_clock,
				     unsigned int bpp, unsigned int lanes,
				     unsigned long long hs_clk_rate,
				     struct phy_configure_opts_mipi_cphy *cfg)
{
	if (!cfg || !lanes)
		return -EINVAL;

	if (!hs_clk_rate) {
		if (!bpp)
			return -EINVAL;

		/*
		 * Total payload bits/s = pixel_clock * bpp, spread over the
		 * active trios. C-PHY carries 16 bits per 7 symbols per trio,
		 * so the per-trio symbol rate is the bit rate scaled by 7/16.
		 */
		hs_clk_rate = (unsigned long long)pixel_clock * bpp;
		hs_clk_rate = DIV_ROUND_UP_ULL(hs_clk_rate * PHY_MIPI_CPHY_SYMBOLS_PER_GROUP,
					       (unsigned long long)PHY_MIPI_CPHY_BITS_PER_GROUP * lanes);
	}

	cfg->lanes = lanes;
	cfg->hs_clk_rate = hs_clk_rate;

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

int phy_mipi_cphy_get_default_config(unsigned long pixel_clock,
				     unsigned int bpp, unsigned int lanes,
				     struct phy_configure_opts_mipi_cphy *cfg)
{
	if (!bpp)
		return -EINVAL;

	return phy_mipi_cphy_calc_config(pixel_clock, bpp, lanes, 0, cfg);
}
EXPORT_SYMBOL_GPL(phy_mipi_cphy_get_default_config);

int phy_mipi_cphy_get_default_config_for_hsclk(unsigned long long hs_clk_rate,
					       unsigned int lanes,
					       struct phy_configure_opts_mipi_cphy *cfg)
{
	if (!hs_clk_rate)
		return -EINVAL;

	return phy_mipi_cphy_calc_config(0, 0, lanes, hs_clk_rate, cfg);
}
EXPORT_SYMBOL_GPL(phy_mipi_cphy_get_default_config_for_hsclk);

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
