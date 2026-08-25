// SPDX-License-Identifier: GPL-2.0
/*
 * camss-csiphy-3ph-1-0.c
 *
 * Qualcomm MSM Camera Subsystem - CSIPHY Module 3phase v1.0
 *
 * Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
 * Copyright (C) 2016-2018 Linaro Ltd.
 */

#include "phy-qcom-csiphy.h"

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>

#define CSIPHY_3PH_LNn_CFG1(n)			(0x000 + 0x100 * (n))
#define CSIPHY_3PH_LNn_CFG1_SWI_REC_DLY_PRG	(BIT(7) | BIT(6))
#define CSIPHY_3PH_LNn_CFG2(n)			(0x004 + 0x100 * (n))
#define CSIPHY_3PH_LNn_CFG2_LP_REC_EN_INT	BIT(3)
#define CSIPHY_3PH_LNn_CFG3(n)			(0x008 + 0x100 * (n))
#define CSIPHY_3PH_LNn_CFG4(n)			(0x00c + 0x100 * (n))
#define CSIPHY_3PH_LNn_CFG4_T_HS_CLK_MISS	0xa4
#define CSIPHY_3PH_LNn_CFG4_T_HS_CLK_MISS_660	0xa5
#define CSIPHY_3PH_LNn_CFG5(n)			(0x010 + 0x100 * (n))
#define CSIPHY_3PH_LNn_CFG5_T_HS_DTERM		0x02
#define CSIPHY_3PH_LNn_CFG5_HS_REC_EQ_FQ_INT	0x50
#define CSIPHY_3PH_LNn_TEST_IMP(n)		(0x01c + 0x100 * (n))
#define CSIPHY_3PH_LNn_TEST_IMP_HS_TERM_IMP	0xa
#define CSIPHY_3PH_LNn_MISC1(n)			(0x028 + 0x100 * (n))
#define CSIPHY_3PH_LNn_MISC1_IS_CLKLANE		BIT(2)
#define CSIPHY_3PH_LNn_CFG6(n)			(0x02c + 0x100 * (n))
#define CSIPHY_3PH_LNn_CFG6_SWI_FORCE_INIT_EXIT	BIT(0)
#define CSIPHY_3PH_LNn_CFG7(n)			(0x030 + 0x100 * (n))
#define CSIPHY_3PH_LNn_CFG7_SWI_T_INIT		0x2
#define CSIPHY_3PH_LNn_CFG8(n)			(0x034 + 0x100 * (n))
#define CSIPHY_3PH_LNn_CFG8_SWI_SKIP_WAKEUP	BIT(0)
#define CSIPHY_3PH_LNn_CFG8_SKEW_FILTER_ENABLE	BIT(1)
#define CSIPHY_3PH_LNn_CFG9(n)			(0x038 + 0x100 * (n))
#define CSIPHY_3PH_LNn_CFG9_SWI_T_WAKEUP	0x1
#define CSIPHY_3PH_LNn_CSI_LANE_CTRL15(n)	(0x03c + 0x100 * (n))
#define CSIPHY_3PH_LNn_CSI_LANE_CTRL15_SWI_SOT_SYMBOL	0xb8

#define CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(offset, n)	((offset) + 0x4 * (n))
#define CSIPHY_3PH_CMN_CSI_COMMON_CTRL5_CLK_ENABLE	BIT(7)
#define CSIPHY_3PH_CMN_CSI_COMMON_CTRL6_COMMON_PWRDN_B	BIT(0)
#define CSIPHY_3PH_CMN_CSI_COMMON_CTRL6_SHOW_REV_ID	BIT(1)
#define CSIPHY_3PH_CMN_CSI_COMMON_STATUSn(offset, common_status_offset, n) \
	((offset) + (common_status_offset) + 0x4 * (n))

#define CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(n) \
	(0x0100 + ((n) * 0x4))
#define CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(n) \
	(0x0300 + ((n) * 0x4))
#define CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(n) \
	(0x0500 + ((n) * 0x4))
#define CSIPHY_LN9_CSI_3PH_CTRLn_ADDR(n) \
	(0x0900 + ((n) * 0x4))
#define CSIPHY_LN10_CSI_3PH_CTRLn_ADDR(n) \
	(0x0A00 + ((n) * 0x4))
#define CSIPHY_LN11_CSI_3PH_CTRLn_ADDR(n) \
	(0x0B00 + ((n) * 0x4))

#define CSIPHY_DEFAULT_PARAMS		0
#define CSIPHY_LANE_ENABLE		1
#define CSIPHY_SETTLE_CNT_LOWER_BYTE	2
#define CSIPHY_SETTLE_CNT_HIGHER_BYTE	3
#define CSIPHY_DNP_PARAMS		4
#define CSIPHY_2PH_REGS			5
#define CSIPHY_3PH_REGS			6
#define CSIPHY_SKEW_CAL			7

#define CSIPHY_CPHY_DATA_RATE_DEFAULT_IDX	0

struct csiphy_lane_regs {
	s32 reg_addr;
	s32 reg_data;
	u32 delay_us;
	u32 csiphy_param_type;
};

/* GEN2 1.0 2PH */
static const struct
csiphy_lane_regs lane_regs_sdm845[] = {
	{0x0004, 0x0C, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x002C, 0x01, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0034, 0x0F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x001C, 0x0A, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0014, 0x60, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0028, 0x00, 0x00, CSIPHY_DNP_PARAMS},
	{0x003C, 0xB8, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0000, 0x91, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0008, 0x00, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{0x000c, 0x00, 0x00, CSIPHY_DNP_PARAMS},
	{0x0010, 0x52, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0038, 0xFE, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0060, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0064, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0704, 0x0C, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x072C, 0x01, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0734, 0x0F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x071C, 0x0A, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0714, 0x60, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0728, 0x04, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x073C, 0xB8, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0700, 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0708, 0x14, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{0x070C, 0xA5, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0710, 0x52, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0738, 0x1F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0760, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0764, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0204, 0x0C, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x022C, 0x01, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0234, 0x0F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x021C, 0x0A, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0214, 0x60, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0228, 0x00, 0x00, CSIPHY_DNP_PARAMS},
	{0x023C, 0xB8, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0200, 0x91, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0208, 0x00, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{0x020C, 0x00, 0x00, CSIPHY_DNP_PARAMS},
	{0x0210, 0x52, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0238, 0xFE, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0260, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0264, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0404, 0x0C, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x042C, 0x01, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0434, 0x0F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x041C, 0x0A, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0414, 0x60, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0428, 0x00, 0x00, CSIPHY_DNP_PARAMS},
	{0x043C, 0xB8, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0400, 0x91, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0408, 0x00, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{0x040C, 0x00, 0x00, CSIPHY_DNP_PARAMS},
	{0x0410, 0x52, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0438, 0xFE, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0460, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0464, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0604, 0x0C, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x062C, 0x01, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0634, 0x0F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x061C, 0x0A, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0614, 0x60, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0628, 0x00, 0x00, CSIPHY_DNP_PARAMS},
	{0x063C, 0xB8, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0600, 0x91, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0608, 0x00, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{0x060C, 0x00, 0x00, CSIPHY_DNP_PARAMS},
	{0x0610, 0x52, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0638, 0xFE, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0660, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{0x0664, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
};

/* GEN2 1.0 3PH */
/* 3 lanes (C-PHY trio) */
static const struct
csiphy_lane_regs lane_regs_sdm845_3ph[] = {
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(23), 0x63, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(26), 0xac, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(27), 0xa5, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(1),  0x06, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(3),  0x12, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(2),  0x00, 0x00, CSIPHY_SETTLE_CNT_HIGHER_BYTE},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(5),  0x20, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(20), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(6),  0x3e, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(7),  0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(8),  0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(9),  0x7f, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(10), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(11), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(17), 0x12, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(24), 0x02, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(51), 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(25), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(55), 0x51, 0x00, CSIPHY_DEFAULT_PARAMS},

	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(23), 0x63, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(26), 0xac, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(27), 0xa5, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(1),  0x06, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(3),  0x12, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(2),  0x00, 0x00, CSIPHY_SETTLE_CNT_HIGHER_BYTE},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(5),  0x20, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(20), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(6),  0x3e, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(7),  0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(8),  0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(9),  0x7f, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(10), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(11), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(17), 0x12, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(24), 0x02, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(51), 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(25), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(55), 0x51, 0x00, CSIPHY_DEFAULT_PARAMS},

	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(23), 0x63, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(26), 0xac, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(27), 0xa5, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(1),  0x06, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(3),  0x12, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(2),  0x00, 0x00, CSIPHY_SETTLE_CNT_HIGHER_BYTE},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(5),  0x20, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(20), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(6),  0x3e, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(7),  0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(8),  0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(9),  0x7f, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(10), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(11), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(17), 0x12, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(24), 0x02, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(51), 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(25), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(55), 0x51, 0x00, CSIPHY_DEFAULT_PARAMS},
};

/* GEN3 3PH sa8775p base (C-PHY) */
static const struct
csiphy_lane_regs lane_regs_sa8775p_3ph[] = {
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(26), 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(27), 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(23), 0xC8, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(3),  0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(34), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(35), 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(36), 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(6),  0x3E, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(7),  0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(9),  0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(11), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(17), 0xB2, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(51), 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(25), 0x33, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(55), 0x50, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(26), 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(27), 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(23), 0xC8, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(1),  0x06, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(3),  0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(2),  0x00, 0x00, CSIPHY_SETTLE_CNT_HIGHER_BYTE},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(5),  0x20, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(20), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(34), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(35), 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(36), 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(6),  0x3E, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(7),  0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(8),  0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(9),  0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(10), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(11), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(17), 0xB2, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(24), 0x02, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(51), 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(25), 0x33, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(55), 0x50, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(26), 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(27), 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(23), 0xC8, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(1),  0x06, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(3),  0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(2),  0x00, 0x00, CSIPHY_SETTLE_CNT_HIGHER_BYTE},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(5),  0x20, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(20), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(34), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(35), 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(36), 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(6),  0x3E, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(7),  0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(8),  0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(9),  0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(10), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(11), 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(17), 0xB2, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(24), 0x02, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(51), 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(25), 0x33, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(55), 0x50, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN9_CSI_3PH_CTRLn_ADDR(33), 0x20, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN9_CSI_3PH_CTRLn_ADDR(34), 0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN9_CSI_3PH_CTRLn_ADDR(32), 0x61, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN9_CSI_3PH_CTRLn_ADDR(44), 0x01, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN9_CSI_3PH_CTRLn_ADDR(45), 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN10_CSI_3PH_CTRLn_ADDR(33), 0x20, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN10_CSI_3PH_CTRLn_ADDR(34), 0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN10_CSI_3PH_CTRLn_ADDR(32), 0x61, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN10_CSI_3PH_CTRLn_ADDR(44), 0x01, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN10_CSI_3PH_CTRLn_ADDR(45), 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN11_CSI_3PH_CTRLn_ADDR(33), 0x20, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN11_CSI_3PH_CTRLn_ADDR(34), 0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN11_CSI_3PH_CTRLn_ADDR(32), 0x61, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN11_CSI_3PH_CTRLn_ADDR(44), 0x01, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN11_CSI_3PH_CTRLn_ADDR(45), 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
};

static struct csiphy_lane_regs datarate_sa8775p_3ph_1p5Gsps[] = {
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(23), 0x46, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(26), 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(27), 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(3),  0x24, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(23), 0x46, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(26), 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(27), 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(3),  0x24, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(23), 0x46, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(26), 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(27), 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(3),  0x24, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN9_CSI_3PH_CTRLn_ADDR(45), 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN10_CSI_3PH_CTRLn_ADDR(45), 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN11_CSI_3PH_CTRLn_ADDR(45), 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
};

static struct csiphy_lane_regs datarate_sa8775p_3ph_1p7Gsps[] = {
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(23), 0x56, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(26), 0xAE, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(27), 0x65, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(3),  0x12, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(23), 0x56, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(26), 0xAE, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(27), 0x65, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(3),  0x12, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(23), 0x56, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(26), 0xAE, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(27), 0x65, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(3),  0x12, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN9_CSI_3PH_CTRLn_ADDR(45),  0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN10_CSI_3PH_CTRLn_ADDR(45), 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN11_CSI_3PH_CTRLn_ADDR(45), 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
};

static struct csiphy_lane_regs datarate_sa8775p_3ph_2p5Gsps[] = {
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(26), 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(23), 0xC8, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(27), 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(3),  0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(26), 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(23), 0xC8, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(27), 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(3),  0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(26), 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(23), 0xC8, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(27), 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(3),  0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN9_CSI_3PH_CTRLn_ADDR(45),  0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN9_CSI_3PH_CTRLn_ADDR(34),  0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN10_CSI_3PH_CTRLn_ADDR(45), 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN10_CSI_3PH_CTRLn_ADDR(34), 0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN11_CSI_3PH_CTRLn_ADDR(45), 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN11_CSI_3PH_CTRLn_ADDR(34), 0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
};

static struct csiphy_lane_regs datarate_sa8775p_3ph_3p5Gsps[] = {
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(26), 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(23), 0x46, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(27), 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(3),  0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(26), 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(23), 0x46, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(27), 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(3),  0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(26), 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(23), 0x46, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(27), 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(3),  0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN9_CSI_3PH_CTRLn_ADDR(45),  0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN9_CSI_3PH_CTRLn_ADDR(34),  0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN10_CSI_3PH_CTRLn_ADDR(45), 0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN10_CSI_3PH_CTRLn_ADDR(34), 0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN11_CSI_3PH_CTRLn_ADDR(45), 0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN11_CSI_3PH_CTRLn_ADDR(34), 0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
};

static struct csiphy_lane_regs datarate_sa8775p_3ph_4p5Gsps[] = {
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(26), 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(23), 0x46, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(27), 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN1_CSI_3PH_CTRLn_ADDR(3),  0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(26), 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(23), 0x46, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(27), 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN3_CSI_3PH_CTRLn_ADDR(3),  0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(26), 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(23), 0x46, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(27), 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN5_CSI_3PH_CTRLn_ADDR(3),  0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
	{CSIPHY_LN9_CSI_3PH_CTRLn_ADDR(45),  0x02, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN9_CSI_3PH_CTRLn_ADDR(34),  0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN10_CSI_3PH_CTRLn_ADDR(45), 0x02, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN10_CSI_3PH_CTRLn_ADDR(34), 0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN11_CSI_3PH_CTRLn_ADDR(45), 0x02, 0x00, CSIPHY_DEFAULT_PARAMS},
	{CSIPHY_LN11_CSI_3PH_CTRLn_ADDR(34), 0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
};

static struct data_rate_reg_info data_rate_settings_sa8775p_3ph[] = {
	{
		/* 1.5 GSpS */
		.bandwidth = 1500000000,
		.data_rate_reg_array_size = ARRAY_SIZE(datarate_sa8775p_3ph_1p5Gsps),
		.data_rate_reg_array = datarate_sa8775p_3ph_1p5Gsps,
	},
	{
		/* 1.7 GSpS */
		.bandwidth = 1700000000,
		.data_rate_reg_array_size = ARRAY_SIZE(datarate_sa8775p_3ph_1p7Gsps),
		.data_rate_reg_array = datarate_sa8775p_3ph_1p7Gsps,
	},
	{
		/* 2.5 GSpS */
		.bandwidth = 2500000000,
		.data_rate_reg_array_size = ARRAY_SIZE(datarate_sa8775p_3ph_2p5Gsps),
		.data_rate_reg_array = datarate_sa8775p_3ph_2p5Gsps,
	},
	{
		/* 3.5 GSpS */
		.bandwidth = 3500000000,
		.data_rate_reg_array_size = ARRAY_SIZE(datarate_sa8775p_3ph_3p5Gsps),
		.data_rate_reg_array = datarate_sa8775p_3ph_3p5Gsps,
	},
	{
		/* 4.5 GSpS */
		.bandwidth = 4500000000,
		.data_rate_reg_array_size = ARRAY_SIZE(datarate_sa8775p_3ph_4p5Gsps),
		.data_rate_reg_array = datarate_sa8775p_3ph_4p5Gsps,
	},
};

static void csiphy_hw_version_read(struct qcom_csiphy *csiphy,
				   struct device *dev)
{
	struct csiphy_device_regs *regs = csiphy->regs;
	u32 hw_version;

	writel(CSIPHY_3PH_CMN_CSI_COMMON_CTRL6_SHOW_REV_ID, csiphy->base +
	       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, 6));

	hw_version = readl_relaxed(csiphy->base +
		CSIPHY_3PH_CMN_CSI_COMMON_STATUSn(regs->offset,
						  regs->common_status_offset, 12));
	hw_version |= readl_relaxed(csiphy->base +
		CSIPHY_3PH_CMN_CSI_COMMON_STATUSn(regs->offset,
						  regs->common_status_offset, 13)) << 8;
	hw_version |= readl_relaxed(csiphy->base +
		CSIPHY_3PH_CMN_CSI_COMMON_STATUSn(regs->offset,
						  regs->common_status_offset, 14)) << 16;
	hw_version |= readl_relaxed(csiphy->base +
		CSIPHY_3PH_CMN_CSI_COMMON_STATUSn(regs->offset,
						  regs->common_status_offset, 15)) << 24;

	dev_dbg(dev, "CSIPHY 3PH HW Version = 0x%08x\n", hw_version);
}

/*
 * csiphy_reset - Perform software reset on CSIPHY module
 * @csiphy: CSIPHY device
 */
static void csiphy_reset(struct qcom_csiphy *csiphy)
{
	struct csiphy_device_regs *regs = csiphy->regs;

	writel_relaxed(0x1, csiphy->base +
		      CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, 0));
	usleep_range(5000, 8000);
}

static irqreturn_t csiphy_isr(int irq, void *dev)
{
	struct qcom_csiphy *csiphy = dev;
	struct csiphy_device_regs *regs = csiphy->regs;
	int i;

	for (i = 0; i < 11; i++) {
		int c = i + 22;
		u8 val = readl_relaxed(csiphy->base +
			CSIPHY_3PH_CMN_CSI_COMMON_STATUSn(regs->offset,
							  regs->common_status_offset, i));

		writel_relaxed(val, csiphy->base +
			       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, c));
	}

	writel_relaxed(0x1, csiphy->base +
		       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, 10));
	writel_relaxed(0x0, csiphy->base +
		       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, 10));

	for (i = 22; i < 33; i++) {
		writel_relaxed(0x0, csiphy->base +
			       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, i));
	}

	return IRQ_HANDLED;
}

/*
 * csiphy_settle_cnt_calc - Calculate settle count value
 *
 * Helper function to calculate settle count value. This is
 * based on the CSI2 T_hs_settle parameter which in turn
 * is calculated based on the CSI2 transmitter link frequency.
 *
 * Return settle count value or 0 if the CSI2 link frequency
 * is not available
 */
static u8 csiphy_settle_cnt_calc(s64 link_freq, u32 timer_clk_rate)
{
	u32 ui; /* ps */
	u32 timer_period; /* ps */
	u32 t_hs_prepare_max; /* ps */
	u32 t_hs_settle; /* ps */
	u8 settle_cnt;

	if (link_freq <= 0)
		return 0;

	ui = div_u64(1000000000000LL, link_freq);
	ui /= 2;
	t_hs_prepare_max = 85000 + 6 * ui;
	t_hs_settle = t_hs_prepare_max;

	timer_period = div_u64(1000000000000LL, timer_clk_rate);
	settle_cnt = t_hs_settle / timer_period - 6;

	return settle_cnt;
}

/*
 * csiphy_cphy_data_rate_config - Apply data-rate specific C-PHY overrides
 *
 * Applied on top of the base lane_regs. Selects the first table entry whose
 * bandwidth >= the data rate derived from @link_freq (highest entry if none
 * qualifies, default entry if @link_freq is unknown) and writes its overrides.
 */
static void csiphy_cphy_data_rate_config(struct qcom_csiphy *csiphy,
					 struct data_rate_reg_info *settings,
					 size_t num_settings,
					 s64 link_freq, u8 settle_cnt)
{
	struct device *dev = csiphy->dev;
	const struct csiphy_lane_regs *r;
	size_t idx, i;
	u32 val;

	if (!settings || !num_settings)
		return;

	if (!link_freq) {
		/* Link frequency unknown; use the default (lowest) entry. */
		idx = CSIPHY_CPHY_DATA_RATE_DEFAULT_IDX;
	} else {
		/* First entry that satisfies the rate, else the highest. */
		for (idx = 0; idx < num_settings; idx++) {
			if (settings[idx].bandwidth >= link_freq)
				break;
		}
		if (idx == num_settings)
			idx = num_settings - 1;
	}

	dev_dbg(dev,
		"CSIPHY using specific bandwidth %llu bits/s (entry %zu) for link_freq %lld\n",
		settings[idx].bandwidth, idx, link_freq);

	r = settings[idx].data_rate_reg_array;
	for (i = 0; i < settings[idx].data_rate_reg_array_size; i++, r++) {
		switch (r->csiphy_param_type) {
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
		writel_relaxed(val, csiphy->base + r->reg_addr);
		if (r->delay_us)
			udelay(r->delay_us);
	}
}

static void csiphy_gen2_config_lanes(struct qcom_csiphy *csiphy,
				     u8 settle_cnt)
{
	const struct csiphy_lane_regs *r = csiphy->regs->lane_regs;
	int i, array_size = csiphy->regs->lane_array_size;
	u32 val;

	for (i = 0; i < array_size; i++, r++) {
		switch (r->csiphy_param_type) {
		case CSIPHY_SETTLE_CNT_LOWER_BYTE:
			val = settle_cnt & 0xff;
			break;
		case CSIPHY_SKEW_CAL:
			/* TODO: support application of skew from dt flag */
			continue;
		case CSIPHY_DNP_PARAMS:
			continue;
		default:
			val = r->reg_data;
			break;
		}
		writel_relaxed(val, csiphy->base + r->reg_addr);
		if (r->delay_us)
			udelay(r->delay_us);
	}
}

static u8 csiphy_get_lane_mask(struct csiphy_lanes_cfg *lane_cfg)
{
	u8 lane_mask = 0;
	u8 offset = 0;
	int i;

	switch (lane_cfg->phy_cfg) {
	case PHY_MODE_MIPI_CPHY:
		offset = 1;
		break;
	case PHY_MODE_MIPI_DPHY:
		lane_mask = CSIPHY_3PH_CMN_CSI_COMMON_CTRL5_CLK_ENABLE;
		break;
	default:
		break;
	}

	for (i = 0; i < lane_cfg->num_data; i++)
		lane_mask |= BIT((lane_cfg->data[i].pos * 2) + offset);

	return lane_mask;
}

static void csiphy_lanes_enable(struct qcom_csiphy *csiphy,
				struct csiphy_config *cfg,
				s64 link_freq, u8 lane_mask)
{
	struct csiphy_lanes_cfg *c = &cfg->csi2->lane_cfg;
	struct csiphy_device_regs *regs = csiphy->regs;
	struct data_rate_reg_info *data_rate_settings = NULL;
	size_t num_data_rate_settings = 0;
	u8 settle_cnt;
	u8 val;
	int i;

	switch (csiphy->soc->id) {
	case QCOM_CSIPHY_SDM845:
		if (c->phy_cfg == PHY_MODE_MIPI_DPHY) {
			regs->lane_regs = &lane_regs_sdm845[0];
			regs->lane_array_size = ARRAY_SIZE(lane_regs_sdm845);
		} else if (c->phy_cfg == PHY_MODE_MIPI_CPHY) {
			regs->lane_regs = &lane_regs_sdm845_3ph[0];
			regs->lane_array_size = ARRAY_SIZE(lane_regs_sdm845_3ph);
		}
		break;
	case QCOM_CSIPHY_SA8775P:
		if (c->phy_cfg == PHY_MODE_MIPI_CPHY) {
			regs->lane_regs = &lane_regs_sa8775p_3ph[0];
			regs->lane_array_size = ARRAY_SIZE(lane_regs_sa8775p_3ph);
			data_rate_settings = data_rate_settings_sa8775p_3ph;
			num_data_rate_settings = ARRAY_SIZE(data_rate_settings_sa8775p_3ph);
		}
		break;
	default:
		break;
	}

	if (!regs->lane_regs)
		WARN_ONCE(1, "Missing lane_regs definition!\n");

	settle_cnt = csiphy_settle_cnt_calc(link_freq, csiphy->timer_clk_rate);

	val = csiphy_get_lane_mask(c);
	writel_relaxed(val, csiphy->base +
		       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, 5));

	val = CSIPHY_3PH_CMN_CSI_COMMON_CTRL6_COMMON_PWRDN_B;
	writel_relaxed(val, csiphy->base +
		       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, 6));

	switch (csiphy->soc->id) {
	case QCOM_CSIPHY_SA8775P:
		if (c->phy_cfg == PHY_MODE_MIPI_CPHY) {
			val = 0x5A;
			writel_relaxed(val, csiphy->base +
				       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, 7));
			val = 0xE;
			writel_relaxed(val, csiphy->base +
				       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, 0));
		} else {
			val = 0x02;
			writel_relaxed(val, csiphy->base +
				       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, 7));
			writel_relaxed(val, csiphy->base +
				       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, 0));
		}
		break;
	case QCOM_CSIPHY_SDM845:
	default:
		val = 0x02;
		writel_relaxed(val, csiphy->base +
			       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, 7));
		val = 0x0;
		writel_relaxed(val, csiphy->base +
			       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, 0));
		break;
	}

	csiphy_gen2_config_lanes(csiphy, settle_cnt);

	/*
	 * For C-PHY platforms with a data-rate settings table, apply the
	 * data-rate specific overrides on top of the base lane_regs config.
	 */
	if (c->phy_cfg == PHY_MODE_MIPI_CPHY && data_rate_settings)
		csiphy_cphy_data_rate_config(csiphy, data_rate_settings,
					     num_data_rate_settings,
					     link_freq, settle_cnt);

	/* IRQ_MASK registers - disable all interrupts */
	for (i = 11; i < 22; i++) {
		writel_relaxed(0, csiphy->base +
			       CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, i));
	}
}

static void csiphy_lanes_disable(struct qcom_csiphy *csiphy,
				 struct csiphy_config *cfg)
{
	struct csiphy_device_regs *regs = csiphy->regs;

	writel_relaxed(0, csiphy->base +
			  CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, 5));

	writel_relaxed(0, csiphy->base +
			  CSIPHY_3PH_CMN_CSI_COMMON_CTRLn(regs->offset, 6));
}

static int csiphy_init(struct qcom_csiphy *csiphy)
{
	struct device *dev = csiphy->dev;
	struct csiphy_device_regs *regs;

	regs = devm_kmalloc(dev, sizeof(*regs), GFP_KERNEL);
	if (!regs)
		return -ENOMEM;

	csiphy->regs = regs;
	regs->common_status_offset = 0xb0;

	switch (csiphy->soc->id) {
	default:
		regs->offset = 0x800;
		break;
	}

	return 0;
}

const struct csiphy_hw_ops qcom_csiphy_ops_3ph_1_0 = {
	.get_lane_mask = csiphy_get_lane_mask,
	.hw_version_read = csiphy_hw_version_read,
	.reset = csiphy_reset,
	.lanes_enable = csiphy_lanes_enable,
	.lanes_disable = csiphy_lanes_disable,
	.isr = csiphy_isr,
	.init = csiphy_init,
};
