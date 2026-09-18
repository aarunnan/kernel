/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __PHY_MIPI_CPHY_H_
#define __PHY_MIPI_CPHY_H_

#include <linux/types.h>

#define PHY_MIPI_CPHY_MAX_DATA_LANES	4

/**
 * struct phy_configure_opts_mipi_cphy - MIPI C-PHY configuration set
 *
 * This structure is used to represent the configuration state of a
 * MIPI C-PHY phy. The timing fields are named after their symbol in
 * the MIPI Alliance Specification for C-PHY (v1.0), Table 18 "Global
 * Operation Timing Parameters", and are all time values in picoseconds
 * (microseconds for @init and @wakeup) that are not data rate dependent
 * (see Table 18 and sections 6.14.3/6.14.4 of that specification).
 */
struct phy_configure_opts_mipi_cphy {
	/**
	 * @lanes:
	 *
	 * Number of active C-PHY trios (data lanes).
	 */
	unsigned int		lanes;

	/**
	 * @hs_clk_rate:
	 *
	 * High speed symbol rate in symbols/s derived from link rate.
	 *
	 * C-PHY encodes 16 bits per 7 symbols across a 3-wire trio.
	 * Consumers fill this from the sensor link frequency and hand
	 * it to phy_configure().
	 */
	unsigned long		hs_clk_rate;

	/**
	 * @lane_positions:
	 *
	 * Physical-to-logical trio (data lane) position mapping. Entry @i
	 * gives the physical lane position carrying logical trio @i. C-PHY
	 * has no clock lane, so all entries map data trios only.
	 */
	unsigned char		lane_positions[PHY_MIPI_CPHY_MAX_DATA_LANES];

	/**
	 * @lane_polarities:
	 *
	 * Polarity (0 = normal, 1 = swapped) of each logical trio. Entry
	 * @i corresponds to logical trio @i.
	 */
	bool			lane_polarities[PHY_MIPI_CPHY_MAX_DATA_LANES];

	/**
	 * @t3_prepare:
	 *
	 * Time, in picoseconds, that the transmitter drives the
	 * 3-wire LP-000 line state immediately before the HS_+x line
	 * state starting HS transmission.
	 *
	 * Minimum value: 38000 ps
	 * Maximum value: 95000 ps
	 */
	unsigned int		t3_prepare;

	/**
	 * @t3_term_en:
	 *
	 * Time, in picoseconds, for the slave to enable the HS line
	 * termination, starting from the time point when the A, B
	 * and C wires cross VIL_MAX.
	 *
	 * Maximum value: 38000 ps
	 */
	unsigned int		t3_term_en;

	/**
	 * @t3_settle:
	 *
	 * Time interval, in picoseconds, during which the HS receiver
	 * should ignore any HS transitions on the lane, starting from
	 * the beginning of @t3_prepare.
	 *
	 * Minimum value: 95000 ps
	 * Maximum value: 300000 ps
	 */
	unsigned int		t3_settle;

	/**
	 * @t3_hs_exit:
	 *
	 * Time, in picoseconds, that the transmitter drives LP-111
	 * following an HS burst.
	 *
	 * Minimum value: 100000 ps
	 */
	unsigned int		t3_hs_exit;

	/**
	 * @lpx:
	 *
	 * Transmitted length, in picoseconds, of any low-power state
	 * period.
	 *
	 * Minimum value: 50000 ps
	 */
	unsigned int		lpx;

	/**
	 * @init:
	 *
	 * Time, in microseconds, for the initialization period to
	 * complete.
	 *
	 * Minimum value: 100 us
	 */
	unsigned int		init;

	/**
	 * @ta_get:
	 *
	 * Time, in picoseconds, that the new transmitter drives the
	 * Bridge state (LP-000) after accepting control during a
	 * link turnaround.
	 *
	 * Value: 5 * @lpx
	 */
	unsigned int		ta_get;

	/**
	 * @ta_go:
	 *
	 * Time, in picoseconds, that the transmitter drives the
	 * Bridge state (LP-000) before releasing control during a
	 * link turnaround.
	 *
	 * Value: 4 * @lpx
	 */
	unsigned int		ta_go;

	/**
	 * @ta_sure:
	 *
	 * Time, in picoseconds, that the new transmitter waits after
	 * the LP-100 state before transmitting the Bridge state
	 * (LP-000) during a link turnaround.
	 *
	 * Minimum value: @lpx
	 * Maximum value: 2 * @lpx
	 */
	unsigned int		ta_sure;

	/**
	 * @wakeup:
	 *
	 * Time, in microseconds, that a transmitter drives a Mark-1
	 * state prior to a Stop state in order to initiate an exit
	 * from ULPS.
	 *
	 * Minimum value: 1000 us
	 */
	unsigned int		wakeup;
};

int phy_mipi_cphy_get_default_config(unsigned long pixel_clock,
				     unsigned int bpp, unsigned int lanes,
				     struct phy_configure_opts_mipi_cphy *cfg);
int phy_mipi_cphy_get_default_config_for_hsclk(unsigned long long hs_clk_rate,
					       unsigned int lanes,
					       struct phy_configure_opts_mipi_cphy *cfg);
int phy_mipi_cphy_config_validate(struct phy_configure_opts_mipi_cphy *cfg);

#endif /* __PHY_MIPI_CPHY_H_ */
