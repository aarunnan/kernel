// SPDX-License-Identifier: GPL-2.0
/*
 * Qualcomm MSM Camera Subsystem - CSIPHY Module 3phase v1.0, C-PHY mode
 *
 * Copyright (C) 2026 Linaro Ltd.
 */

#include <linux/delay.h>
#include <linux/io.h>

#include "phy-qcom-mipi-csi2.h"

#define CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(offset, n)	((offset) + 0x4 * (n))
#define CSIPHY_3PH_CMN_CSI_COMMON_CTRL0_PHY_SW_RESET	BIT(0)
#define CSIPHY_3PH_CMN_CSI_COMMON_CTRL6_SHOW_REV_ID	BIT(1)
#define CSIPHY_3PH_CMN_CSI_COMMON_STATUSn(offset, n)	((offset) + 0xb0 + 0x4 * (n))

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
	u8 settle_cnt = 0;

	/* Base C-PHY lane_regs init sequence (empty until real data lands). */
	phy_qcom_mipi_csi2_cphy_write_regs(csi2phy, regs->init_seq,
					   regs->lane_array_size, settle_cnt);

	/* Bandwidth-tier overrides on top, if this soc_cfg has any. */
	tier = phy_qcom_mipi_csi2_cphy_pick_datarate(regs, cfg->link_freq);
	if (tier)
		phy_qcom_mipi_csi2_cphy_write_regs(csi2phy, tier->reg_array,
						   tier->reg_array_size, settle_cnt);

	return 0;
}

static void
phy_qcom_mipi_csi2_cphy_lanes_disable(struct mipi_csi2phy_device *csi2phy,
				      struct mipi_csi2phy_stream_cfg *cfg)
{
	const struct mipi_csi2phy_device_regs *regs = csi2phy_dev_to_cphy_regs(csi2phy);

	writel(0, csi2phy->base +
	       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->common_regs_offset, 6));
}

const struct mipi_csi2phy_hw_ops phy_qcom_mipi_csi2_ops_3ph_cphy = {
	.hw_version_read = phy_qcom_mipi_csi2_cphy_hw_version_read,
	.reset = phy_qcom_mipi_csi2_cphy_reset,
	.lanes_enable = phy_qcom_mipi_csi2_cphy_lanes_enable,
	.lanes_disable = phy_qcom_mipi_csi2_cphy_lanes_disable,
};
