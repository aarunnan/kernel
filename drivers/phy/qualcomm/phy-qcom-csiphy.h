/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Qualcomm MSM Camera Subsystem - CSIPHY Module (generic PHY provider)
 *
 * Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016-2018 Linaro Ltd.
 */
#ifndef PHY_QCOM_CSIPHY_H
#define PHY_QCOM_CSIPHY_H

#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/phy/phy.h>
#include <linux/phy/phy-mipi-dphy.h>
#include <linux/phy/phy-mipi-cphy.h>
#include <linux/regulator/consumer.h>

#define CSIPHY_PAD_SINK 0
#define CSIPHY_PAD_SRC 1
#define CSIPHY_PADS_NUM 2

#define CSIPHY_MAX_LANES 4

struct csiphy_lane {
	u8 pos;
	u8 pol;
};

struct csiphy_lanes_cfg {
	enum phy_mode phy_cfg;
	int num_data;
	struct csiphy_lane *data;
	struct csiphy_lane clk;
};

struct csiphy_csi2_cfg {
	struct csiphy_lanes_cfg lane_cfg;
};

struct csiphy_config {
	u8 combo_mode;
	u8 csid_id;
	struct csiphy_csi2_cfg *csi2;
};

struct csiphy_format_info {
	u32 code;
	u8 bpp;
};

struct csiphy_formats {
	unsigned int nformats;
	const struct csiphy_format_info *formats;
};

struct qcom_csiphy;

struct csiphy_hw_ops {
	u8 (*get_lane_mask)(struct csiphy_lanes_cfg *lane_cfg);
	void (*hw_version_read)(struct qcom_csiphy *csiphy, struct device *dev);
	void (*reset)(struct qcom_csiphy *csiphy);
	void (*lanes_enable)(struct qcom_csiphy *csiphy,
			     struct csiphy_config *cfg,
			     s64 link_freq, u8 lane_mask);
	void (*lanes_disable)(struct qcom_csiphy *csiphy,
			      struct csiphy_config *cfg);
	irqreturn_t (*isr)(int irq, void *dev);
	int (*init)(struct qcom_csiphy *csiphy);
};

struct csiphy_lane_regs;

struct data_rate_reg_info {
	u64 bandwidth;
	ssize_t data_rate_reg_array_size;
	struct csiphy_lane_regs *data_rate_reg_array;
};

struct csiphy_device_regs {
	const struct csiphy_lane_regs *lane_regs;
	int lane_array_size;
	u32 offset;
	u32 common_status_offset;
};

/* Per-SoC id used to select register tables/offsets inside csiphy_init(),
 * replacing the former enum camss_version back-pointer.
 */
enum qcom_csiphy_soc_id {
	QCOM_CSIPHY_SDM845,
	QCOM_CSIPHY_SA8775P,
};

/* Per-SoC descriptor, selected via of_device_id .data. Replaces the
 * former switch (csiphy->camss->res->version) inside csiphy_init().
 */
struct qcom_csiphy_soc_data {
	enum qcom_csiphy_soc_id id;
	const struct csiphy_hw_ops *hw_ops;
};

struct qcom_csiphy {
	struct device *dev;
	struct phy *phy;
	const struct qcom_csiphy_soc_data *soc;

	void __iomem *base;
	void __iomem *base_clk_mux;	/* legacy SoCs only; NULL on sdm845 */
	int irq;
	char irq_name[32];

	struct clk_bulk_data *clks;
	int num_clks;
	unsigned long *clk_rate;	/* per-clk target rate, 0 = leave alone */
	int timer_clk_idx;		/* index of the rate-settable timer clk, -1 if none */
	u32 timer_clk_rate;

	struct regulator_bulk_data *supplies;
	int num_supplies;

	struct csiphy_device_regs *regs;

	struct csiphy_config cfg;		/* lane cfg + combo/csid, set via configure/DT */
	struct csiphy_csi2_cfg csi2_identity;	/* identity lane cfg built in .configure, SP2 only */
	struct csiphy_lane *identity_lane_data;	/* backing storage for csi2_identity, devm-allocated, max 4 lanes */
	union phy_configure_opts opts;		/* stashed by .configure (D-PHY or C-PHY) */
	s64 link_freq;				/* derived from the stashed opts */
};

extern const struct csiphy_hw_ops qcom_csiphy_ops_2ph_1_0;
extern const struct csiphy_hw_ops qcom_csiphy_ops_3ph_1_0;

#endif /* PHY_QCOM_CSIPHY_H */
