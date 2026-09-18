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
#define CSIPHY_3PH_CMN_CSI_COMMON_CTRL6_COMMON_PWRDN_B	BIT(0)

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
	return csi2phy->soc_cfg->reg_info_cphy;
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
	.hw_version_read = phy_qcom_mipi_csi2_hw_version_read,
	.reset = phy_qcom_mipi_csi2_reset,
	.lanes_enable = phy_qcom_mipi_csi2_cphy_lanes_enable,
	.lanes_disable = phy_qcom_mipi_csi2_cphy_lanes_disable,
};

/*
 * sa8775p C-PHY (3-phase) register tables.
 *
 * Ported verbatim from the legacy in-tree CAMSS C-PHY driver
 * (drivers/media/platform/qcom/camss/camss-csiphy-3ph-1-0.c:
 * lane_regs_sa8775p_3ph[] and data_rate_settings_sa8775p_3ph[]). The
 * per-lane-block macro addresses (CSIPHY_LNx_CSI_3PH_CTRLn_ADDR(n) =
 * base + n * 4, base 0x0100/0x0300/0x0500/0x0900/0x0a00/0x0b00 for
 * LN1/3/5/9/10/11) are expanded to the literal flat offsets used by
 * this driver's mipi_csi2phy_lane_regs format.
 */
static const struct mipi_csi2phy_lane_regs lane_regs_sa8775p_cphy[] = {
	{.reg_addr = 0x0168, .reg_data = 0x80, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x016c, .reg_data = 0x25, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x015c, .reg_data = 0xc8, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0104, .reg_data = 0x06, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x010c, .reg_data = 0x08, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x0108, .reg_data = 0x00, .param_type = CSIPHY_SETTLE_CNT_HIGHER_BYTE},
	{.reg_addr = 0x0114, .reg_data = 0x20, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0150, .reg_data = 0x00, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0188, .reg_data = 0x00, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x018c, .reg_data = 0x7f, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0190, .reg_data = 0x7f, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0118, .reg_data = 0x3e, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x011c, .reg_data = 0x41, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0120, .reg_data = 0x41, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0124, .reg_data = 0x7f, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0128, .reg_data = 0x00, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x012c, .reg_data = 0x00, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0144, .reg_data = 0xb2, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0160, .reg_data = 0x02, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x01cc, .reg_data = 0x41, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0164, .reg_data = 0x33, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x01dc, .reg_data = 0x50, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0368, .reg_data = 0x80, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x036c, .reg_data = 0x25, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x035c, .reg_data = 0xc8, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0304, .reg_data = 0x06, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x030c, .reg_data = 0x08, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x0308, .reg_data = 0x00, .param_type = CSIPHY_SETTLE_CNT_HIGHER_BYTE},
	{.reg_addr = 0x0314, .reg_data = 0x20, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0350, .reg_data = 0x00, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0388, .reg_data = 0x00, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x038c, .reg_data = 0x7f, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0390, .reg_data = 0x7f, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0318, .reg_data = 0x3e, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x031c, .reg_data = 0x41, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0320, .reg_data = 0x41, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0324, .reg_data = 0x7f, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0328, .reg_data = 0x00, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x032c, .reg_data = 0x00, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0344, .reg_data = 0xb2, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0360, .reg_data = 0x02, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x03cc, .reg_data = 0x41, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0364, .reg_data = 0x33, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x03dc, .reg_data = 0x50, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0568, .reg_data = 0x80, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x056c, .reg_data = 0x25, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x055c, .reg_data = 0xc8, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0504, .reg_data = 0x06, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x050c, .reg_data = 0x08, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x0508, .reg_data = 0x00, .param_type = CSIPHY_SETTLE_CNT_HIGHER_BYTE},
	{.reg_addr = 0x0514, .reg_data = 0x20, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0550, .reg_data = 0x00, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0588, .reg_data = 0x00, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x058c, .reg_data = 0x7f, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0590, .reg_data = 0x7f, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0518, .reg_data = 0x3e, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x051c, .reg_data = 0x41, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0520, .reg_data = 0x41, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0524, .reg_data = 0x7f, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0528, .reg_data = 0x00, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x052c, .reg_data = 0x00, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0544, .reg_data = 0xb2, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0560, .reg_data = 0x02, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x05cc, .reg_data = 0x41, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0564, .reg_data = 0x33, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x05dc, .reg_data = 0x50, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0984, .reg_data = 0x20, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0988, .reg_data = 0x05, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0980, .reg_data = 0x61, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x09b0, .reg_data = 0x01, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x09b4, .reg_data = 0x08, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0a84, .reg_data = 0x20, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0a88, .reg_data = 0x05, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0a80, .reg_data = 0x61, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0ab0, .reg_data = 0x01, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0ab4, .reg_data = 0x08, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0b84, .reg_data = 0x20, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0b88, .reg_data = 0x05, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0b80, .reg_data = 0x61, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0bb0, .reg_data = 0x01, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0bb4, .reg_data = 0x08, .param_type = CSIPHY_DEFAULT_PARAMS},
};

static const struct mipi_csi2phy_lane_regs datarate_sa8775p_cphy_1p5gsps[] = {
	{.reg_addr = 0x015c, .reg_data = 0x46, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0168, .reg_data = 0x80, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x016c, .reg_data = 0x25, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x010c, .reg_data = 0x24, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x035c, .reg_data = 0x46, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0368, .reg_data = 0x80, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x036c, .reg_data = 0x25, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x030c, .reg_data = 0x24, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x055c, .reg_data = 0x46, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0568, .reg_data = 0x80, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x056c, .reg_data = 0x25, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x050c, .reg_data = 0x24, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x09b4, .reg_data = 0x08, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0ab4, .reg_data = 0x08, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0bb4, .reg_data = 0x08, .param_type = CSIPHY_DEFAULT_PARAMS},
};

static const struct mipi_csi2phy_lane_regs datarate_sa8775p_cphy_1p7gsps[] = {
	{.reg_addr = 0x015c, .reg_data = 0x56, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0168, .reg_data = 0xae, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x016c, .reg_data = 0x65, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x010c, .reg_data = 0x12, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x035c, .reg_data = 0x56, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0368, .reg_data = 0xae, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x036c, .reg_data = 0x65, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x030c, .reg_data = 0x12, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x055c, .reg_data = 0x56, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0568, .reg_data = 0xae, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x056c, .reg_data = 0x65, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x050c, .reg_data = 0x12, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x09b4, .reg_data = 0x08, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0ab4, .reg_data = 0x08, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0bb4, .reg_data = 0x08, .param_type = CSIPHY_DEFAULT_PARAMS},
};

static const struct mipi_csi2phy_lane_regs datarate_sa8775p_cphy_2p5gsps[] = {
	{.reg_addr = 0x0168, .reg_data = 0x80, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x015c, .reg_data = 0xc8, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x016c, .reg_data = 0x25, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x010c, .reg_data = 0x08, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x0368, .reg_data = 0x80, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x035c, .reg_data = 0xc8, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x036c, .reg_data = 0x25, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x030c, .reg_data = 0x08, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x0568, .reg_data = 0x80, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x055c, .reg_data = 0xc8, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x056c, .reg_data = 0x25, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x050c, .reg_data = 0x08, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x09b4, .reg_data = 0x08, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0988, .reg_data = 0x05, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0ab4, .reg_data = 0x08, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0a88, .reg_data = 0x05, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0bb4, .reg_data = 0x08, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0b88, .reg_data = 0x05, .param_type = CSIPHY_DEFAULT_PARAMS},
};

static const struct mipi_csi2phy_lane_regs datarate_sa8775p_cphy_3p5gsps[] = {
	{.reg_addr = 0x0168, .reg_data = 0x80, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x015c, .reg_data = 0x46, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x016c, .reg_data = 0x25, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x010c, .reg_data = 0x08, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x0368, .reg_data = 0x80, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x035c, .reg_data = 0x46, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x036c, .reg_data = 0x25, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x030c, .reg_data = 0x08, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x0568, .reg_data = 0x80, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x055c, .reg_data = 0x46, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x056c, .reg_data = 0x25, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x050c, .reg_data = 0x08, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x09b4, .reg_data = 0x05, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0988, .reg_data = 0x05, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0ab4, .reg_data = 0x05, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0a88, .reg_data = 0x05, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0bb4, .reg_data = 0x05, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0b88, .reg_data = 0x05, .param_type = CSIPHY_DEFAULT_PARAMS},
};

static const struct mipi_csi2phy_lane_regs datarate_sa8775p_cphy_4p5gsps[] = {
	{.reg_addr = 0x0168, .reg_data = 0x80, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x015c, .reg_data = 0x46, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x016c, .reg_data = 0x25, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x010c, .reg_data = 0x08, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x0368, .reg_data = 0x80, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x035c, .reg_data = 0x46, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x036c, .reg_data = 0x25, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x030c, .reg_data = 0x08, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x0568, .reg_data = 0x80, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x055c, .reg_data = 0x46, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x056c, .reg_data = 0x25, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x050c, .reg_data = 0x08, .param_type = CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{.reg_addr = 0x09b4, .reg_data = 0x02, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0988, .reg_data = 0x05, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0ab4, .reg_data = 0x02, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0a88, .reg_data = 0x05, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0bb4, .reg_data = 0x02, .param_type = CSIPHY_DEFAULT_PARAMS},
	{.reg_addr = 0x0b88, .reg_data = 0x05, .param_type = CSIPHY_DEFAULT_PARAMS},
};

static const struct mipi_csi2phy_datarate_regs datarate_regs_sa8775p_cphy[] = {
	{
		.bandwidth = 1500000000,
		.reg_array = datarate_sa8775p_cphy_1p5gsps,
		.reg_array_size = ARRAY_SIZE(datarate_sa8775p_cphy_1p5gsps),
	},
	{
		.bandwidth = 1700000000,
		.reg_array = datarate_sa8775p_cphy_1p7gsps,
		.reg_array_size = ARRAY_SIZE(datarate_sa8775p_cphy_1p7gsps),
	},
	{
		.bandwidth = 2500000000,
		.reg_array = datarate_sa8775p_cphy_2p5gsps,
		.reg_array_size = ARRAY_SIZE(datarate_sa8775p_cphy_2p5gsps),
	},
	{
		.bandwidth = 3500000000,
		.reg_array = datarate_sa8775p_cphy_3p5gsps,
		.reg_array_size = ARRAY_SIZE(datarate_sa8775p_cphy_3p5gsps),
	},
	{
		.bandwidth = 4500000000,
		.reg_array = datarate_sa8775p_cphy_4p5gsps,
		.reg_array_size = ARRAY_SIZE(datarate_sa8775p_cphy_4p5gsps),
	},
};

const struct mipi_csi2phy_device_regs mipi_csi2_3ph_cphy_regs_sa8775p = {
	.init_seq = lane_regs_sa8775p_cphy,
	.lane_array_size = ARRAY_SIZE(lane_regs_sa8775p_cphy),
	.common_regs_offset = 0x800,
	.datarate_regs = datarate_regs_sa8775p_cphy,
	.num_datarate_regs = ARRAY_SIZE(datarate_regs_sa8775p_cphy),
};
