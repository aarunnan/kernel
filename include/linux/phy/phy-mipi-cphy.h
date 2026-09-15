/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __PHY_MIPI_CPHY_H_
#define __PHY_MIPI_CPHY_H_

/**
 * struct phy_configure_opts_mipi_cphy - MIPI C-PHY configuration set
 * @lanes: number of active C-PHY trios (data lanes)
 * @hs_clk_rate: high speed symbol rate in symbols/s derived from link rate
 *
 * C-PHY encodes 16 bits per 7 symbols across a 3-wire trio. Consumers fill
 * this from the sensor link frequency and hand it to phy_configure().
 */
struct phy_configure_opts_mipi_cphy {
	unsigned int	lanes;
	unsigned long	hs_clk_rate;
};

int phy_mipi_cphy_get_default_config(unsigned long pixel_clock,
				     unsigned int bpp, unsigned int lanes,
				     struct phy_configure_opts_mipi_cphy *cfg);
int phy_mipi_cphy_config_validate(struct phy_configure_opts_mipi_cphy *cfg);

#endif /* __PHY_MIPI_CPHY_H_ */
