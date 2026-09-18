/* SPDX-License-Identifier: GPL-2.0 */
/*
 *
 * Qualcomm MIPI CSI2 CPHY/DPHY driver
 *
 * Copyright (C) 2025 Linaro Ltd.
 */
#ifndef __PHY_QCOM_MIPI_CSI2_H__
#define __PHY_QCOM_MIPI_CSI2_H__

#include <linux/phy/phy.h>
#include <linux/pm_domain.h>

#define CSI2_MAX_DATA_LANES 4

struct mipi_csi2phy_lane {
	u8 pos;
	u8 pol;
};

struct mipi_csi2phy_lanes_cfg {
	struct mipi_csi2phy_lane data[CSI2_MAX_DATA_LANES];
	struct mipi_csi2phy_lane clk;
};

struct mipi_csi2phy_stream_cfg {
	s64 link_freq;
	u8 num_data_lanes;
	struct mipi_csi2phy_lanes_cfg lane_cfg;
};

struct mipi_csi2phy_device;

struct mipi_csi2phy_hw_ops {
	void (*hw_version_read)(struct mipi_csi2phy_device *csi2phy_dev);
	void (*reset)(struct mipi_csi2phy_device *csi2phy_dev);
	int (*lanes_enable)(struct mipi_csi2phy_device *csi2phy_dev,
			    struct mipi_csi2phy_stream_cfg *cfg);
	void (*lanes_disable)(struct mipi_csi2phy_device *csi2phy_dev,
			      struct mipi_csi2phy_stream_cfg *cfg);
};

struct mipi_csi2phy_lane_regs {
	const s32 reg_addr;
	const s32 reg_data;
	const u32 delay_us;
	const u32 param_type;
};

/**
 * struct mipi_csi2phy_datarate_regs - bandwidth-keyed register overrides
 * @bandwidth: data rate, in bits/s, this entry applies to
 * @reg_array: register writes to apply on top of the base init_seq
 * @reg_array_size: number of entries in reg_array
 *
 * Some C-PHY configurations need small register tweaks (impedance,
 * deskew, timing calibration) that vary with the link data rate. A
 * mipi_csi2phy_device_regs may carry an array of these, sorted by
 * ascending bandwidth; the hw_ops picks the first entry whose
 * bandwidth is >= the configured link rate (falling back to the
 * highest entry if none qualifies).
 */
struct mipi_csi2phy_datarate_regs {
	u64 bandwidth;
	const struct mipi_csi2phy_lane_regs *reg_array;
	size_t reg_array_size;
};

struct mipi_csi2phy_device_regs {
	const struct mipi_csi2phy_lane_regs *init_seq;
	const int lane_array_size;
	const u32 common_regs_offset;

	/* Optional: NULL/0 when no bandwidth-tier overrides apply. */
	const struct mipi_csi2phy_datarate_regs *datarate_regs;
	const size_t num_datarate_regs;
};

/**
 * struct mipi_csi2_genpd - named power-domain descriptor
 * @name: power-domain name, as declared in power-domain-names
 * @scaled: whether this domain takes an OPP-driven performance state
 *
 * The driver attaches to every named power-domain via
 * devm_pm_domain_attach_list(), but only the @scaled ones (e.g. the RPMHPD
 * voltage rails) get a performance state programmed from the OPP table; the
 * unscaled ones (e.g. a GDSC) are only kept enabled. SoCs with no dedicated
 * power-domains (e.g. sa8775p) declare an empty list.
 */
struct mipi_csi2_genpd {
	const char *name;
	bool scaled;
};

struct mipi_csi2phy_soc_cfg {
	/* D-PHY mode (PHY_QCOM_CSI2_MODE_DPHY). Always required. */
	const struct mipi_csi2phy_hw_ops *ops;
	const struct mipi_csi2phy_device_regs reg_info;

	/*
	 * C-PHY mode (PHY_QCOM_CSI2_MODE_CPHY). Both NULL on SoCs that only
	 * support D-PHY.
	 */
	const struct mipi_csi2phy_hw_ops *ops_cphy;
	const struct mipi_csi2phy_device_regs *reg_info_cphy;

	const char ** const supply_names;
	const unsigned int num_supplies;

	const unsigned int num_clk;

	const char * const opp_clk;
	const char * const timer_clk;

	const struct mipi_csi2_genpd *genpds;
	const unsigned int num_genpds;
};

struct mipi_csi2phy_device {
	struct device *dev;
	u8 phy_mode;

	struct phy *phy;
	void __iomem *base;

	struct clk_bulk_data *clks;
	struct clk *timer_clk;
	u32 timer_clk_rate;

	struct regulator_bulk_data *supplies;
	struct dev_pm_domain_list *pd_list;

	const struct mipi_csi2phy_soc_cfg *soc_cfg;
	struct mipi_csi2phy_stream_cfg stream_cfg;

	u32 hw_version;
};

extern const struct mipi_csi2phy_soc_cfg mipi_csi2_dphy_4nm_x1e;
extern const struct mipi_csi2phy_soc_cfg mipi_csi2_dphy_sa8775p;
extern const struct mipi_csi2phy_hw_ops phy_qcom_mipi_csi2_ops_3ph_1_0;
extern const struct mipi_csi2phy_hw_ops phy_qcom_mipi_csi2_ops_3ph_cphy;
extern const struct mipi_csi2phy_device_regs mipi_csi2_3ph_cphy_regs_sa8775p;

/*
 * Shared, phase-mode-agnostic common-control operations. The software reset
 * and HW-version read only touch the SoC's common-control register block
 * (common_regs_offset), which is identical for D-PHY and C-PHY, so the C-PHY
 * hw_ops reuse the D-PHY implementations rather than duplicating them.
 */
void phy_qcom_mipi_csi2_hw_version_read(struct mipi_csi2phy_device *csi2phy);
void phy_qcom_mipi_csi2_reset(struct mipi_csi2phy_device *csi2phy);

#endif /* __PHY_QCOM_MIPI_CSI2_H__ */
