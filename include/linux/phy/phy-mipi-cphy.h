/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2026 Qualcomm Innovation Center, Inc.
 */

#ifndef __PHY_MIPI_CPHY_H_
#define __PHY_MIPI_CPHY_H_

#include <linux/phy/phy-mipi-dphy.h>

/**
 * struct phy_configure_opts_mipi_cphy - MIPI C-PHY configuration set
 *
 * This structure is used to represent the configuration state of a
 * MIPI C-PHY phy. Unlike D-PHY, C-PHY has no separate clock lane: three
 * data lines form one "trio" carrying an embedded clock, and timing is
 * defined per trio. @lanes therefore counts trios, and @lane_rate is a
 * symbol rate, not a clock rate.
 */
struct phy_configure_opts_mipi_cphy {
	/**
	 * @t3_prepare:
	 *
	 * Time, in picoseconds, that the transmitter drives the Prepare
	 * state (the LP-to-HS entry sequence) on the trio immediately
	 * before starting HS symbol transmission.
	 *
	 * Minimum value: 38000 ps
	 */
	unsigned int		t3_prepare;

	/**
	 * @t3_lpx:
	 *
	 * Transmitted length, in picoseconds, of any Low-Power state
	 * period on the trio.
	 *
	 * Minimum value: 38000 ps
	 */
	unsigned int		t3_lpx;

	/**
	 * @ths_settle:
	 *
	 * Time interval, in picoseconds, during which the HS receiver
	 * shall ignore any trio HS transitions, starting from the end of
	 * @t3_prepare, to allow the line to settle before symbol capture.
	 *
	 * Minimum value: 38000 ps
	 */
	unsigned int		ths_settle;

	/**
	 * @ths_exit:
	 *
	 * Time, in picoseconds, that the transmitter drives the HS exit
	 * (disconnect) state following an HS burst before returning the
	 * trio to LP.
	 *
	 * Minimum value: 100000 ps
	 */
	unsigned int		ths_exit;

	/**
	 * @twakeup:
	 *
	 * Time, in microseconds, that the transmitter drives a Mark state
	 * prior to a Stop state in order to initiate an exit from ULPS.
	 *
	 * Minimum value: 1000 us
	 */
	unsigned int		twakeup;

	/**
	 * @tinit:
	 *
	 * Time, in microseconds, for the initialization period to
	 * complete.
	 *
	 * Minimum value: 100 us
	 */
	unsigned int		tinit;

	/**
	 * @lane_rate:
	 *
	 * Symbol rate, in kilosymbols per second (ksps), of the trio(s).
	 * C-PHY carries 16 payload bits per 7 transmitted symbols.
	 */
	unsigned long		lane_rate;

	/**
	 * @lanes:
	 *
	 * Number of active trios used for the transmission.
	 */
	unsigned char		lanes;

	/**
	 * @lane_map:
	 *
	 * Per-trio physical position/polarity. All-zero means unset;
	 * providers then use identity (pos = i, pol = 0).
	 */
	struct phy_configure_opts_mipi_lane	lane_map[PHY_MIPI_MAX_LANES];
};

int phy_mipi_cphy_get_default_config(unsigned long pixel_clock,
				     unsigned int bpp,
				     unsigned int lanes,
				     struct phy_configure_opts_mipi_cphy *cfg);
int phy_mipi_cphy_config_validate(struct phy_configure_opts_mipi_cphy *cfg);

#endif /* __PHY_MIPI_CPHY_H_ */
