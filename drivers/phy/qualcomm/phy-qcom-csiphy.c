// SPDX-License-Identifier: GPL-2.0
/*
 * Qualcomm CSIPHY MIPI CSI-2 receiver PHY driver
 *
 * Copyright (c) 2016-2018 Linaro Ltd.
 * Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
 * Copyright (c) 2026 Qualcomm Innovation Center, Inc.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>

#include "phy-qcom-csiphy.h"

static int qcom_csiphy_phy_init(struct phy *phy)
{
	struct qcom_csiphy *csiphy = phy_get_drvdata(phy);
	int ret;

	ret = csiphy->soc->hw_ops->init(csiphy);
	if (ret)
		return ret;

	return 0;
}

static int qcom_csiphy_configure(struct phy *phy,
				 union phy_configure_opts *opts)
{
	struct qcom_csiphy *csiphy = phy_get_drvdata(phy);
	struct csiphy_lanes_cfg *lane_cfg = &csiphy->csi2_identity.lane_cfg;
	enum phy_mode mode = phy_get_mode(phy);
	struct phy_configure_opts_mipi_lane *lane_map;
	unsigned int num_lanes;
	bool have_map = false;
	int ret;
	int i;

	if (mode == PHY_MODE_MIPI_CPHY) {
		ret = phy_mipi_cphy_config_validate(&opts->mipi_cphy);
		if (ret)
			return ret;

		csiphy->opts = *opts;
		csiphy->link_freq = opts->mipi_cphy.lane_rate;
		lane_cfg->phy_cfg = PHY_MODE_MIPI_CPHY;
		num_lanes = opts->mipi_cphy.lanes;
	} else {
		ret = phy_mipi_dphy_config_validate(&opts->mipi_dphy);
		if (ret)
			return ret;

		csiphy->opts = *opts;
		csiphy->link_freq = opts->mipi_dphy.hs_clk_rate;
		lane_cfg->phy_cfg = PHY_MODE_MIPI_DPHY;
		num_lanes = opts->mipi_dphy.lanes;
	}

	/*
	 * Lane cfg: honor the real per-lane pos/pol the consumer placed in
	 * opts->...lane_map[] when present. An all-zero lane_map means the
	 * consumer did not fill it, so fall back to a straight identity
	 * mapping (pos = i, pol = 0). A real multi-lane identity map has
	 * non-zero entries for lanes 1..N, so it is correctly detected as
	 * present; the only all-zero map is a single lane at pos 0, which is
	 * itself identity, so the fallback is byte-identical there.
	 */
	lane_map = (mode == PHY_MODE_MIPI_CPHY) ? opts->mipi_cphy.lane_map
						: opts->mipi_dphy.lane_map;

	lane_cfg->num_data = min_t(unsigned int, num_lanes,
				   CSIPHY_MAX_LANES);

	for (i = 0; i < lane_cfg->num_data; i++)
		if (lane_map[i].pos || lane_map[i].pol)
			have_map = true;

	for (i = 0; i < lane_cfg->num_data; i++) {
		lane_cfg->data[i].pos = have_map ? lane_map[i].pos : i;
		lane_cfg->data[i].pol = have_map ? lane_map[i].pol : 0;
	}

	csiphy->cfg.csi2 = &csiphy->csi2_identity;

	return 0;
}

static int qcom_csiphy_power_on(struct phy *phy)
{
	struct qcom_csiphy *csiphy = phy_get_drvdata(phy);
	struct csiphy_config *cfg = &csiphy->cfg;
	u8 lane_mask;
	int ret;

	ret = regulator_bulk_enable(csiphy->num_supplies, csiphy->supplies);
	if (ret)
		return ret;

	if (csiphy->timer_clk_idx >= 0 && csiphy->timer_clk_rate) {
		ret = clk_set_rate(csiphy->clks[csiphy->timer_clk_idx].clk,
				   csiphy->timer_clk_rate);
		if (ret)
			goto err_regulator;
	}

	ret = clk_bulk_prepare_enable(csiphy->num_clks, csiphy->clks);
	if (ret)
		goto err_regulator;

	if (csiphy->timer_clk_idx >= 0)
		csiphy->timer_clk_rate =
			clk_get_rate(csiphy->clks[csiphy->timer_clk_idx].clk);

	enable_irq(csiphy->irq);

	csiphy->soc->hw_ops->reset(csiphy);

	csiphy->soc->hw_ops->hw_version_read(csiphy, csiphy->dev);

	lane_mask = csiphy->soc->hw_ops->get_lane_mask(&cfg->csi2->lane_cfg);
	csiphy->soc->hw_ops->lanes_enable(csiphy, cfg, csiphy->link_freq,
					  lane_mask);
	return 0;

err_regulator:
	regulator_bulk_disable(csiphy->num_supplies, csiphy->supplies);
	return ret;
}

static int qcom_csiphy_power_off(struct phy *phy)
{
	struct qcom_csiphy *csiphy = phy_get_drvdata(phy);

	csiphy->soc->hw_ops->lanes_disable(csiphy, &csiphy->cfg);
	disable_irq(csiphy->irq);
	clk_bulk_disable_unprepare(csiphy->num_clks, csiphy->clks);
	regulator_bulk_disable(csiphy->num_supplies, csiphy->supplies);

	return 0;
}

static const struct phy_ops qcom_csiphy_ops = {
	.init		= qcom_csiphy_phy_init,
	.power_on	= qcom_csiphy_power_on,
	.power_off	= qcom_csiphy_power_off,
	.configure	= qcom_csiphy_configure,
	.owner		= THIS_MODULE,
};

static int qcom_csiphy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy_provider *phy_provider;
	struct qcom_csiphy *csiphy;
	int ret;

	csiphy = devm_kzalloc(dev, sizeof(*csiphy), GFP_KERNEL);
	if (!csiphy)
		return -ENOMEM;

	csiphy->dev = dev;
	csiphy->soc = of_device_get_match_data(dev);
	if (!csiphy->soc)
		return dev_err_probe(dev, -EINVAL, "failed to get match data\n");

	csiphy->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(csiphy->base))
		return PTR_ERR(csiphy->base);

	csiphy->irq = platform_get_irq(pdev, 0);
	if (csiphy->irq < 0)
		return csiphy->irq;

	snprintf(csiphy->irq_name, sizeof(csiphy->irq_name), "%s",
		 dev_name(dev));

	csiphy->num_clks = devm_clk_bulk_get_all(dev, &csiphy->clks);
	if (csiphy->num_clks < 0)
		return csiphy->num_clks;

	csiphy->clk_rate = devm_kcalloc(dev, csiphy->num_clks,
					sizeof(*csiphy->clk_rate), GFP_KERNEL);
	if (!csiphy->clk_rate)
		return -ENOMEM;

	/*
	 * Identify the rate-settable timer clock by clock-name "csiphy_timer".
	 *
	 * Clock model for sa8775p (lemans): the legacy CAMSS csiphy_res_8775p
	 * declares THREE clocks per PHY - "csiphy_rx" (shared RX symbol clock,
	 * 400 MHz), "csiphy{N}" (0/leave-alone) and "csiphy{N}_timer" (400 MHz).
	 * This provider node declares only two, "csiphy" and "csiphy_timer",
	 * and enables both. The shared "csiphy_rx" RX symbol clock is
	 * deliberately LEFT OWNED by the CAMSS/CSID RX path (it is a per-SoC
	 * shared RX clock, not a per-PHY resource) and is NOT requested here.
	 * If bring-up/stream debugging (Task 8) shows the RX clock ungated,
	 * revisit by adding "csiphy_rx" as a third optional clock on the
	 * provider node.
	 */
	csiphy->timer_clk_idx = -1;
	{
		struct device_node *np = dev->of_node;
		int i;
		const char *name;

		for (i = 0; i < csiphy->num_clks; i++) {
			if (of_property_read_string_index(np, "clock-names", i, &name))
				continue;
			if (!strcmp(name, "csiphy_timer")) {
				csiphy->timer_clk_idx = i;
				/*
				 * sa8775p timer clock rate, from the legacy
				 * csiphy_res_8775p.clock_rate for the *_timer
				 * clock (400 MHz).
				 */
				csiphy->timer_clk_rate = 400000000;
				break;
			}
		}
	}

	{
		static const char * const supply_names[] = {
			"vdda-phy", "vdda-pll",
		};
		int i;

		csiphy->num_supplies = ARRAY_SIZE(supply_names);
		csiphy->supplies = devm_kcalloc(dev, csiphy->num_supplies,
						sizeof(*csiphy->supplies),
						GFP_KERNEL);
		if (!csiphy->supplies)
			return -ENOMEM;
		for (i = 0; i < csiphy->num_supplies; i++)
			csiphy->supplies[i].supply = supply_names[i];

		ret = devm_regulator_bulk_get(dev, csiphy->num_supplies,
					      csiphy->supplies);
		if (ret)
			return dev_err_probe(dev, ret, "failed to get supplies\n");
	}

	csiphy->identity_lane_data = devm_kcalloc(dev, CSIPHY_MAX_LANES,
						  sizeof(*csiphy->identity_lane_data),
						  GFP_KERNEL);
	if (!csiphy->identity_lane_data)
		return -ENOMEM;
	csiphy->csi2_identity.lane_cfg.data = csiphy->identity_lane_data;

	platform_set_drvdata(pdev, csiphy);

	ret = devm_request_irq(dev, csiphy->irq, csiphy->soc->hw_ops->isr,
			       IRQF_TRIGGER_RISING | IRQF_NO_AUTOEN,
			       csiphy->irq_name, csiphy);
	if (ret)
		return dev_err_probe(dev, ret, "failed to request irq\n");

	csiphy->phy = devm_phy_create(dev, NULL, &qcom_csiphy_ops);
	if (IS_ERR(csiphy->phy))
		return dev_err_probe(dev, PTR_ERR(csiphy->phy),
				     "failed to create PHY\n");

	phy_set_drvdata(csiphy->phy, csiphy);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);
	if (IS_ERR(phy_provider))
		return dev_err_probe(dev, PTR_ERR(phy_provider),
				     "failed to register PHY provider\n");

	return 0;
}

static const struct qcom_csiphy_soc_data qcom_csiphy_sdm845 = {
	.id = QCOM_CSIPHY_SDM845,
	.hw_ops = &qcom_csiphy_ops_3ph_1_0,
};

/*
 * sa8775p has no D-PHY lane_regs table in the 3ph hw_ops (only the C-PHY
 * lane_regs_sa8775p_3ph[]/data_rate_settings_sa8775p_3ph[] exist), so a
 * D-PHY-mode instance on this SoC hits csiphy_lanes_enable()'s
 * WARN_ONCE("Missing lane_regs definition!"). Pre-existing gap, unrelated
 * to binding this compatible - no D-PHY sa8775p board uses this driver.
 */
static const struct qcom_csiphy_soc_data qcom_csiphy_sa8775p = {
	.id = QCOM_CSIPHY_SA8775P,
	.hw_ops = &qcom_csiphy_ops_3ph_1_0,
};

static const struct of_device_id qcom_csiphy_of_match[] = {
	{ .compatible = "qcom,sdm845-csiphy", .data = &qcom_csiphy_sdm845 },
	{ .compatible = "qcom,sa8775p-csiphy", .data = &qcom_csiphy_sa8775p },
	{ }
};
MODULE_DEVICE_TABLE(of, qcom_csiphy_of_match);

static struct platform_driver qcom_csiphy_driver = {
	.probe = qcom_csiphy_probe,
	.driver = {
		.name = "qcom-csiphy",
		.of_match_table = qcom_csiphy_of_match,
	},
};
module_platform_driver(qcom_csiphy_driver);

MODULE_DESCRIPTION("Qualcomm CSIPHY MIPI CSI-2 receiver PHY driver");
MODULE_LICENSE("GPL");
