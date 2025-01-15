/*
 * Copyright (c) 2025 Jianxiong Gu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/smf.h>
#include <zephyr/drivers/usb/usb_bc12.h>

/**
 * @brief BC1.2 States
 */
enum bc12_state_t {
	/** BC1.2 peripheral disconnect from data lines */
	BC12_STATE_DISCONNECT,

	/** BC1.2 detection procedure */
	BC12_STATE_VBUS_DETECT,
	BC12_STATE_DATA_CONTACT_DETECT,
	BC12_STATE_PRIMARY_DETECTION,
	BC12_STATE_SECONDARY_DETECTION,

	/** BC1.2 detection finish */
	BC12_STATE_FINISH,

	/** BC1.2 detection error */
	BC12_STATE_ERROR,

	/** Number of BC1.2 States */
	BC12_STATE_COUNT
};

/**
 * @brief BC1.2 Timer Object
 */
struct bc12_timer_t {
	/** kernel timer */
	struct k_timer timer;
	/** timeout value in ms */
	uint32_t timeout_ms;
	/** flags to track timer status */
	atomic_t flags;
};

/**
 * @brief BC1.2 State Machine Object
 */
struct bc12_sm_t {
	/** state machine context */
	struct smf_ctx ctx;
	/** Port device */
	const struct device *dev;
	/** VBUS measurement device */
	const struct device *vbus_dev;

	/* Timers */

	/** Data contact detect debounce timer */
	struct bc12_timer_t bc12_t_dcd_dbnc;
	/** Data contact detect timer */
	struct bc12_timer_t bc12_t_dcd;
	/** D+ voltage source on timer */
	struct bc12_timer_t bc12_t_vdpsrc_on;
	/** D- voltage source on timer */
	struct bc12_timer_t bc12_t_vdmsrc_on;
	/** D- voltage source enable timer */
	struct bc12_timer_t bc12_t_vdmsrc_en;
	/** D- voltage source disable timer */
	struct bc12_timer_t bc12_t_vdmsrc_dis;
};
