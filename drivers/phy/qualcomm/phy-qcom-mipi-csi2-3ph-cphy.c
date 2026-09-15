// SPDX-License-Identifier: GPL-2.0
/*
 * Qualcomm MSM Camera Subsystem - CSIPHY Module 3phase v1.0, C-PHY mode
 *
 * Copyright (C) 2026 Linaro Ltd.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/time64.h>

#include "phy-qcom-mipi-csi2.h"

#define CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(offset, n)	((offset) + 0x4 * (n))
#define CSIPHY_3PH_CMN_CSI_COMMON_CTRL0_PHY_SW_RESET	BIT(0)
#define CSIPHY_3PH_CMN_CSI_COMMON_CTRL6_COMMON_PWRDN_B	BIT(0)
#define CSIPHY_3PH_CMN_CSI_COMMON_CTRL6_SHOW_REV_ID	BIT(1)
#define CSIPHY_3PH_CMN_CSI_COMMON_STATUSn(offset, n)	((offset) + 0xb0 + 0x4 * (n))

/*
 * 3 phase CSI has 19 common status regs with only 0-10 being used
 * and 11-18 being reserved.
 */
#define CSI_COMMON_STATUS_NUM				11
/*
 * There are a number of common control registers. The offset to clear the
 * CSIPHY IRQ status starts @ 22, so CSI_COMMON_STATUS0 is cleared via
 * CSI_COMMON_CONTROL22, STATUS1 via CONTROL23 and so on.
 */
#define CSI_CTRL_STATUS_INDEX				22

/*
 * C-PHY common control values, per the CSIPHY v1.3.0 3-phase C-PHY
 * programming sequence (differ from the D-PHY values for the same registers).
 */
#define CSIPHY_3PH_CMN_CSI_COMMON_CTRL7_CPHY		0x5a
#define CSIPHY_3PH_CMN_CSI_COMMON_CTRL0_CPHY		0x0e

#define CSIPHY_DEFAULT_PARAMS				0
#define CSIPHY_SETTLE_CNT_LOWER_BYTE			2
#define CSIPHY_SETTLE_CNT_HIGHER_BYTE			3

static inline const struct mipi_csi2phy_device_regs *
csi2phy_dev_to_cphy_regs(struct mipi_csi2phy_device *csi2phy)
{
	return &csi2phy->soc_cfg->reg_info_cphy;
}

static void phy_qcom_mipi_csi2_cphy_hw_version_read(struct mipi_csi2phy_device *csi2phy)
{
	const struct mipi_csi2phy_device_regs *regs = csi2phy_dev_to_cphy_regs(csi2phy);
	u32 tmp;

	writel(CSIPHY_3PH_CMN_CSI_COMMON_CTRL6_SHOW_REV_ID, csi2phy->base +
	       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->common_regs_offset, 6));

	tmp = readl_relaxed(csi2phy->base +
			    CSIPHY_3PH_CMN_CSI_COMMON_STATUSn(regs->common_regs_offset, 12));
	csi2phy->hw_version = tmp;

	tmp = readl_relaxed(csi2phy->base +
			    CSIPHY_3PH_CMN_CSI_COMMON_STATUSn(regs->common_regs_offset, 13));
	csi2phy->hw_version |= (tmp << 8) & 0xFF00;

	tmp = readl_relaxed(csi2phy->base +
			    CSIPHY_3PH_CMN_CSI_COMMON_STATUSn(regs->common_regs_offset, 14));
	csi2phy->hw_version |= (tmp << 16) & 0xFF0000;

	tmp = readl_relaxed(csi2phy->base +
			    CSIPHY_3PH_CMN_CSI_COMMON_STATUSn(regs->common_regs_offset, 15));
	csi2phy->hw_version |= (tmp << 24) & 0xFF000000;

	dev_dbg_once(csi2phy->dev, "CSIPHY 3PH C-PHY HW Version = 0x%08x\n", csi2phy->hw_version);
}

static void phy_qcom_mipi_csi2_cphy_reset(struct mipi_csi2phy_device *csi2phy)
{
	const struct mipi_csi2phy_device_regs *regs = csi2phy_dev_to_cphy_regs(csi2phy);

	writel(CSIPHY_3PH_CMN_CSI_COMMON_CTRL0_PHY_SW_RESET,
	       csi2phy->base + CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->common_regs_offset, 0));
	usleep_range(5000, 8000);
	writel(0x0, csi2phy->base +
	       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->common_regs_offset, 0));
}

/*
 * phy_qcom_mipi_csi2_cphy_settle_cnt_calc - Calculate settle count value
 *
 * Helper function to calculate settle count value. This is based on the
 * CSI2 T_hs_settle parameter which in turn is calculated based on the CSI2
 * transmitter link frequency. The calculation is identical to the D-PHY
 * one; the CSIPHY settle-count timer is not phase-mode specific.
 *
 * Return settle count value or 0 if the CSI2 link frequency is not available.
 */
static u8 phy_qcom_mipi_csi2_cphy_settle_cnt_calc(s64 link_freq, u32 timer_clk_rate)
{
	u32 t_hs_prepare_max_ps;
	u32 timer_period_ps;
	u32 t_hs_settle_ps;
	u8 settle_cnt;
	u32 ui_ps;

	if (link_freq <= 0)
		return 0;

	ui_ps = div_u64(PSEC_PER_SEC, link_freq);
	ui_ps /= 2;
	t_hs_prepare_max_ps = 85000 + 6 * ui_ps;
	t_hs_settle_ps = t_hs_prepare_max_ps;

	timer_period_ps = div_u64(PSEC_PER_SEC, timer_clk_rate);
	settle_cnt = t_hs_settle_ps / timer_period_ps - 6;

	return settle_cnt;
}

/*
 * phy_qcom_mipi_csi2_cphy_lane_mask - Compute the C-PHY lane enable mask
 *
 * Unlike D-PHY (which sets the CTRL5 clock-lane enable bit and uses even
 * bit positions), C-PHY has no separate clock lane and enables each active
 * trio at an odd bit position (data lane logical position * 2 + 1).
 */
static u8 phy_qcom_mipi_csi2_cphy_lane_mask(struct mipi_csi2phy_stream_cfg *cfg)
{
	struct mipi_csi2phy_lanes_cfg *lane_cfg = &cfg->lane_cfg;
	u8 lane_mask = 0;
	int i;

	for (i = 0; i < cfg->num_data_lanes; i++)
		lane_mask |= BIT((lane_cfg->data[i].pos * 2) + 1);

	return lane_mask;
}

/*
 * phy_qcom_mipi_csi2_cphy_pick_datarate - select the bandwidth-tier override
 *
 * Selects the first entry whose bandwidth is >= link_freq, falling back to
 * the highest entry if none qualifies, or the first entry if link_freq is
 * not yet known (0). Returns NULL if @regs has no datarate_regs table.
 */
static const struct mipi_csi2phy_datarate_regs *
phy_qcom_mipi_csi2_cphy_pick_datarate(const struct mipi_csi2phy_device_regs *regs,
				     s64 link_freq)
{
	size_t idx;

	if (!regs->datarate_regs || !regs->num_datarate_regs)
		return NULL;

	if (!link_freq)
		return &regs->datarate_regs[0];

	for (idx = 0; idx < regs->num_datarate_regs; idx++) {
		if (regs->datarate_regs[idx].bandwidth >= link_freq)
			return &regs->datarate_regs[idx];
	}

	/* No tier reaches link_freq: fall back to the highest one. */
	return &regs->datarate_regs[regs->num_datarate_regs - 1];
}

static void
phy_qcom_mipi_csi2_cphy_write_regs(struct mipi_csi2phy_device *csi2phy,
				   const struct mipi_csi2phy_lane_regs *r,
				   size_t count, u8 settle_cnt)
{
	u32 val;
	size_t i;

	for (i = 0; i < count; i++, r++) {
		switch (r->param_type) {
		case CSIPHY_SETTLE_CNT_LOWER_BYTE:
			val = settle_cnt & 0xff;
			break;
		case CSIPHY_SETTLE_CNT_HIGHER_BYTE:
			val = (settle_cnt >> 8) & 0xff;
			break;
		default:
			val = r->reg_data;
			break;
		}
		writel(val, csi2phy->base + r->reg_addr);
		if (r->delay_us)
			udelay(r->delay_us);
	}
}

static int phy_qcom_mipi_csi2_cphy_lanes_enable(struct mipi_csi2phy_device *csi2phy,
						struct mipi_csi2phy_stream_cfg *cfg)
{
	const struct mipi_csi2phy_device_regs *regs = csi2phy_dev_to_cphy_regs(csi2phy);
	const struct mipi_csi2phy_datarate_regs *tier;
	u8 settle_cnt;
	u8 val;
	int i;

	settle_cnt = phy_qcom_mipi_csi2_cphy_settle_cnt_calc(cfg->link_freq,
							     csi2phy->timer_clk_rate);

	/* Enable the active C-PHY trios (odd bit positions, no clock lane). */
	val = phy_qcom_mipi_csi2_cphy_lane_mask(cfg);
	writel(val, csi2phy->base +
	       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->common_regs_offset, 5));

	val = CSIPHY_3PH_CMN_CSI_COMMON_CTRL6_COMMON_PWRDN_B;
	writel(val, csi2phy->base +
	       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->common_regs_offset, 6));

	val = CSIPHY_3PH_CMN_CSI_COMMON_CTRL7_CPHY;
	writel(val, csi2phy->base +
	       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->common_regs_offset, 7));

	val = CSIPHY_3PH_CMN_CSI_COMMON_CTRL0_CPHY;
	writel(val, csi2phy->base +
	       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->common_regs_offset, 0));

	val = 0x00;
	writel(val, csi2phy->base +
	       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->common_regs_offset, 0));

	/* Base C-PHY lane_regs init sequence. */
	phy_qcom_mipi_csi2_cphy_write_regs(csi2phy, regs->init_seq,
					   regs->lane_array_size, settle_cnt);

	/* Bandwidth-tier overrides on top, if this soc_cfg has any. */
	tier = phy_qcom_mipi_csi2_cphy_pick_datarate(regs, cfg->link_freq);
	if (tier)
		phy_qcom_mipi_csi2_cphy_write_regs(csi2phy, tier->reg_array,
						   tier->reg_array_size, settle_cnt);

	/* IRQ_MASK registers - disable all interrupts */
	for (i = CSI_COMMON_STATUS_NUM; i < CSI_CTRL_STATUS_INDEX; i++) {
		writel(0, csi2phy->base +
		       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->common_regs_offset, i));
	}

	return 0;
}

static void
phy_qcom_mipi_csi2_cphy_lanes_disable(struct mipi_csi2phy_device *csi2phy,
				      struct mipi_csi2phy_stream_cfg *cfg)
{
	const struct mipi_csi2phy_device_regs *regs = csi2phy_dev_to_cphy_regs(csi2phy);

	writel(0, csi2phy->base +
	       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->common_regs_offset, 5));

	writel(0, csi2phy->base +
	       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->common_regs_offset, 6));
}

const struct mipi_csi2phy_hw_ops phy_qcom_mipi_csi2_ops_3ph_cphy = {
	.hw_version_read = phy_qcom_mipi_csi2_cphy_hw_version_read,
	.reset = phy_qcom_mipi_csi2_cphy_reset,
	.lanes_enable = phy_qcom_mipi_csi2_cphy_lanes_enable,
	.lanes_disable = phy_qcom_mipi_csi2_cphy_lanes_disable,
};
