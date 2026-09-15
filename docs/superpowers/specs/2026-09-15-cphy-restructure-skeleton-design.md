# C-PHY restructuring skeleton for phy-qcom-mipi-csi2

## Context

We already restructured lemans/sa8775p D-PHY off the legacy in-tree
`camss-csiphy-3ph-1-0.c` and onto the generic-PHY provider model
(`drivers/phy/qualcomm/phy-qcom-mipi-csi2-*`), landing 5 commits on
`dphy-branch`: `phy: qcom-mipi-csi2: make power-domains optional`,
`... add sa8775p D-PHY support`, the lemans camss consumer, the DT
provider nodes, and the imx577 overlay wiring.

We now want the same restructuring for **C-PHY** on the same
lemans/sa8775p target, eventually streaming an **imx686** sensor. A
prior exploratory attempt (`cphy-rebase` branch — not used as a code
source, only as architectural reference) already worked through this
once, including register data ported from an authoritative reference
branch. This design defines the **skeleton only**: everything that
must exist structurally before real register tables and the imx686
driver are dropped in. No real lane_regs/datarate data, no imx686
driver, no DT camera nodes yet.

## Key discovery: shared compatible with D-PHY

lemans's physical CSIPHY v1.3.0 IP is the same hardware block in both
phase modes — the prior C-PHY attempt bound its sa8775p C-PHY soc_cfg
to `qcom,sa8775p-csi2-phy`, the **exact same compatible** our D-PHY
work just claimed. Rather than introduce a colliding or redundant
compatible, we keep one compatible for sa8775p and let the existing
`#phy-cells = <1>` mode argument (`PHY_QCOM_CSI2_MODE_DPHY` = 0,
`PHY_QCOM_CSI2_MODE_CPHY` = 1 — both already defined in
`include/dt-bindings/phy/phy-qcom-mipi-csi2.h`) pick behavior at
`phy_get()` time via `qcom_csi2_phy_xlate()`, which today only accepts
mode 0 and must be extended to accept mode 1 too.

This means `struct mipi_csi2phy_soc_cfg` itself must carry **both**
mode variants' `ops`/`reg_info` (either may be absent if a given SoC
only implements one mode), and every `-core.c` call site that reads
`soc_cfg->ops` / `soc_cfg->reg_info` today must instead dispatch on
`csi2phy->phy_mode`. Clocks, supplies, and genpd stay shared — one
physical PHY instance, one set of power rails, regardless of which
mode is active.

## Layers, bottom-up

**1. phy-core prerequisite** (generic kernel API, not Qualcomm-specific)

- `include/linux/phy/phy.h`: add `PHY_MODE_MIPI_CPHY` to `enum
  phy_mode` (the only missing piece — per-lane map fields
  `lane_positions`/`lane_polarities`/`clock_lane_position`/`clock_lane_polarity`
  already exist upstream in `phy-mipi-dphy.h` on this base, confirmed
  by inspection; no need to re-add them).
- New `include/linux/phy/phy-mipi-cphy.h`: `struct
  phy_configure_opts_mipi_cphy` mirroring `phy_configure_opts_mipi_dphy`'s
  shape but for C-PHY: trio count instead of lane count, symbol rate
  instead of bit-clock rate, per-trio position/polarity. Plus
  `phy_mipi_cphy_config_validate()`.
- `include/linux/phy/phy.h`: add `mipi_cphy` member to `union
  phy_configure_opts`.

**2. Driver-internal struct changes** (`phy-qcom-mipi-csi2.h`)

- `mipi_csi2phy_soc_cfg` gains `ops_cphy` / `reg_info_cphy` alongside
  the existing `ops` / `reg_info` fields, which keep their current
  names and remain the D-PHY pair — this minimizes the diff against
  the already-landed D-PHY commits (no rename of fields every existing
  soc_cfg already initializes).
- `mipi_csi2phy_device_regs` gains `datarate_regs` /
  `num_datarate_regs` (bandwidth-keyed override arrays, mirrors legacy
  `data_rate_reg_info`) — NULL/0 for every existing D-PHY soc_cfg, so
  x1e80100 and our sa8775p D-PHY entry are completely unaffected.
- New `mipi_csi2phy_datarate_regs` struct: `{ bandwidth, reg_array,
  reg_array_size }`.
- `mipi_csi2phy_device.phy_mode` (already exists, currently unused
  beyond the `xlate()` reject) becomes the live dispatch key.

**3. Core dispatch** (`phy-qcom-mipi-csi2-core.c`)

- `qcom_csi2_phy_xlate()`: accept `PHY_QCOM_CSI2_MODE_CPHY` (mode 1)
  in addition to mode 0, instead of `-EOPNOTSUPP`.
- `phy_qcom_mipi_csi2_configure()`: branch on `phy_get_mode(phy)` —
  D-PHY path validates `&opts->mipi_dphy` (existing, unchanged),
  C-PHY path validates the new `&opts->mipi_cphy` and populates
  `stream_cfg` from it (trio positions, symbol rate → `link_freq`
  equivalent).
- `phy_qcom_mipi_csi2_power_on()`: pick `ops = phy_mode ==
  PHY_QCOM_CSI2_MODE_CPHY ? soc_cfg->ops_cphy : soc_cfg->ops` before
  calling `reset()`/`hw_version_read()`/`lanes_enable()`.
- Bandwidth-tier register override selection happens **inside** the
  C-PHY `lanes_enable` hw_op itself (see layer 4), not in `-core.c` —
  `-core.c` only picks which `ops`/`reg_info` pair is active.

**4. New C-PHY hw_ops file** (`phy-qcom-mipi-csi2-3ph-cphy.c`)

- `phy_qcom_mipi_csi2_ops_3ph_cphy`: `hw_version_read`/`reset` reuse
  the same common-ctrl register class as the D-PHY ops (same offsets,
  same bit meanings — legacy driver confirms this), so these two
  functions can be near-identical to their D-PHY counterparts.
- `lanes_enable`: apply `reg_info->init_seq` (base lane_regs, empty
  placeholder for now), then if `reg_info->datarate_regs` is non-NULL,
  select the first tier whose `bandwidth >= link_freq`-derived rate
  (mirrors legacy `csiphy_cphy_data_rate_config()`'s selection logic
  and the `qmp-combo.c` `switch(link_rate)` idiom) and apply that
  override array on top. Placeholder: `datarate_regs = NULL` until
  real tables are provided, so this code path is present but inert.
- `lanes_disable`: mirrors D-PHY's.
- No `mipi_csi2_cphy_sa8775p` soc_cfg data yet — this file only
  defines the `hw_ops` struct and is wired into the Makefile, but
  nothing references it until sa8775p's `ops_cphy` field is populated
  in a later commit.

**5. camss + DT stubs**

- No new camss.c consumer code yet — the existing lemans D-PHY
  consumer (`csiphy_res_lemans[]`, `lemans_resources`) is untouched.
  C-PHY sensors would reuse the same `phys`/`phy-names` consumer model
  already in place; camss.c doesn't need mode-specific resource
  tables (mode is a property of the PHY node config, not the camss
  consumer side).
- No new DT nodes yet. `lemans.dtsi`'s `csiphy0..3` nodes stay
  D-PHY-only in practice (no consumer requests mode 1 yet); the
  insertion point for a C-PHY-mode consumer (once imx686 + real
  register data exist) is a `phys = <&csiphyN PHY_QCOM_CSI2_MODE_CPHY>`
  reference from a future camera overlay, exactly like the D-PHY
  imx577 overlay does with mode 0 today.
- No imx686 sensor driver, no camera DT overlay — explicitly deferred
  until real inputs are provided.

## What "done" looks like for this skeleton

- Kernel builds clean with `CONFIG_PHY_QCOM_MIPI_CSI2=m` and
  `CONFIG_VIDEO_QCOM_CAMSS=m`, no functional change to any existing
  D-PHY consumer (x1e80100, sa8775p D-PHY/lemans) — verified by
  confirming zero diff in those soc_cfgs' `ops`/`reg_info` fields
  after the rename/restructure.
- `phy_set_mode(phy, PHY_MODE_MIPI_CPHY)` +
  `phy_configure(phy, &opts)` with `opts.mipi_cphy` populated
  succeeds structurally (validates, dispatches to the C-PHY hw_ops)
  even though `lanes_enable` writes an empty register list — i.e. the
  plumbing is provably wired end-to-end with inert data, ready for
  real tables to be dropped into `phy-qcom-mipi-csi2-3ph-cphy.c` and a
  real `ops_cphy`/`reg_info_cphy` pair added to the sa8775p soc_cfg.

## Explicitly out of scope (next steps, pending your inputs)

- Real `lane_regs` base C-PHY register sequence for sa8775p.
- Real bandwidth-tier override tables (1.5/1.7/2.5/3.5/4.5 Gsps or
  whatever tiers apply).
- `mipi_csi2_cphy_sa8775p` C-PHY register data populated into the
  shared sa8775p `soc_cfg`'s `ops_cphy`/`reg_info_cphy`.
- imx686 sensor driver (`drivers/media/i2c/imx686.c` or similar).
- lemans-evk imx686 camera DT overlay.
- Any camss.c changes specific to C-PHY streaming (current belief is
  none are needed, since the phy-consumer model is mode-agnostic on
  the camss side — to be confirmed once real C-PHY streaming is
  tested).
