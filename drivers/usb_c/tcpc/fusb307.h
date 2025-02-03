/*
 * Copyright (c) 2025 Jianxiong Gu <jianxiong.gu@outlook.com>
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_USBC_TCPC_FUSB307_H_
#define ZEPHYR_DRIVERS_USBC_TCPC_FUSB307_H_

#define FUSB307_VENDOR_ID  0x0779
#define FUSB307_PRODUCT_ID 0x0133

#define FUSB307_REG_VCONN_OCP	    0xA0
#define FUSB307_REG_VCONN_OCP_RANGE BIT(3)
#define FUSB307_REG_VCONN_OCP_CUR   GENMASK(2, 0)

#define FUSB307_REG_RESET	 0xA2
#define FUSB307_REG_RESET_PD_RST BIT(1)
#define FUSB307_REG_RESET_SW_RST BIT(0)

#define FUSB307_REG_GPIO1_CFG		 0xA4
#define FUSB307_REG_GPIO2_CFG		 0xA5
#define FUSB307_REG_GPIO_CFG_GPO_VAL	 BIT(2)
#define FUSB307_REG_GPIO_CFG_GPI_EN	 BIT(1)
#define FUSB307_REG_GPIO_CFG_GPO_EN	 BIT(0)
#define FUSB307_REG_GPIO2_CFG_FR_SWAP_FN BIT(3)

#define FUSB307_REG_GPIO_STAT	       0xA6
#define FUSB307_REG_GPIO_STAT_GPI2_VAL BIT(1)
#define FUSB307_REG_GPIO_STAT_GPI1_VAL BIT(0)

#define FUSB307_REG_DRPTOGGLE		0xA7
#define FUSB307_REG_DRPTOGGLE_DRPTOGGLE BIT(0)

#define FUSB307_REG_SINK_TRANSMIT	     0xB0
#define FUSB307_REG_SINK_TRANSMIT_DIS_SNK_TX BIT(6)
#define FUSB307_REG_SINK_TRANSMIT_RETRY_CNT  GENMASK(5, 4)
#define FUSB307_REG_SINK_TRANSMIT_TXSOP	     GENMASK(2, 0)

#define FUSB307_REG_SRC_FRSWAP			0xB1
#define FUSB307_REG_SRC_FRSWAP_FRSWAP_SNK_DELAY	GENMASK(3, 2)
#define FUSB307_REG_SRC_FRSWAP_MANUAL_SNK_EN	BIT(1)
#define FUSB307_REG_SRC_FRSWAP_FR_SWAP		BIT(0)

#define FUSB307_REG_SNK_FRSWAP		      0xB2
#define FUSB307_REG_SNK_FRSWAP_EN_FRSWAP_DTCT BIT(0)

#define FUSB307_REG_ALERT_VD		0xB3
#define FUSB307_REG_ALERT_VD_DISCH_SUCC	BIT(6)
#define FUSB307_REG_ALERT_VD_GPI2	BIT(5)
#define FUSB307_REG_ALERT_VD_GPI1	BIT(4)
#define FUSB307_REG_ALERT_VD_VDD_DTCT	BIT(3)
#define FUSB307_REG_ALERT_VD_OTP	BIT(2)
#define FUSB307_REG_ALERT_VD_SWAP_TX	BIT(1)
#define FUSB307_REG_ALERT_VD_SWAP_RX	BIT(0)

#define FUSB307_REG_ALERT_VD_MASK      0xB4

/* Sampling period of VBUS Measurement, at least greater than 3ms */
#define FUSB307_tVBUSsample 4 /* ms */

#endif /* ZEPHYR_DRIVERS_USBC_TCPC_FUSB307_H_ */
