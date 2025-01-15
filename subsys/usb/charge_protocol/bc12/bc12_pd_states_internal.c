/*
 * Copyright (c) 2025 Jianxiong Gu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/smf.h>
#include <zephyr/drivers/usb/usb_bc12.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(usb_charge_protocol CONFIG_USB_BC12_LOG_LEVEL);

#include "bc12_pd_states_internal.h"


/** @brief BC1.2 DP state. */
enum bc12_dp_state {
	/** BC1.2 peripheral disconnect from D+ line. */
	BC12_DP_OPEN,
	/** turn on VDP_SRC */
	BC12_DP_VDP_SRC,
	/** turn on IDP_SRC */
	BC12_DP_IDP_SRC,
	/** turn on IDP_SINK */
	BC12_DP_IDP_SINK,
	/** turn on RDP_UP */
	BC12_DP_RDP_UP,
	/** turn on RDP_DWN */
	BC12_DP_RDP_DWN,
	/** short D+ to D- through a resistance of RDCP_DATA */
	BC12_DP_SHORT_DM,
};

/** @brief BC1.2 DM state. */
enum bc12_dm_state {
	/** BC1.2 peripheral disconnect from D- line. */
	BC12_DM_OPEN,
	/** turn on VDM_SRC */
	BC12_DM_VDM_SRC,
	/** turn on IDM_SINK */
	BC12_DM_IDM_SINK,
	/** turn on RDM_DWN */
	BC12_DM_RDM_DWN,
	BC12_DM_SHORT_DP,
};

/** @brief BC1.2 DP DM voltage level. */
enum bc12_dpdm_voltage_level {
	/** Higher than VLGC (Min: 0.8V, Max: 2.0V) */
	BC12_LOGIC_HIGH,
	/** Higher than VDAT_REF (Min: 0.25V, Max: 0.4V) */
	BC12_ABOVE_VDAT_REF,
	BC12_BELOW_VDAT_REF,
};

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
	/** The D+ state */
	enum bc12_dp_state dp_state;
	/** The D- state */
	enum bc12_dm_state dm_state;
	/** Current D+ voltage level */
	enum bc12_dpdm_voltage_level dp_level;
	/** Current D- voltage level */
	enum bc12_dpdm_voltage_level dm_level;

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

static const struct smf_state bc12_states[BC12_STATE_COUNT];

static void bc12_disconnect_entry(void *obj)
{
	struct bc12_sm_t *bc = (struct bc12_sm_t *)obj;
	const struct device *dev = bc->dev;
	struct usb_port_data *data = dev->data;
	const struct device *bc12 = data->bc12;
	int ret;

	bc->dp_state = BC12_DP_OPEN;
	bc->dm_state = BC12_DM_OPEN;

	ret = bc12_set_dpdm(bc12, BC12_DP_OPEN, BC12_DM_OPEN);
	if (ret != 0) {
		LOG_ERR("Couldn't set Data lines disconnect: %d", ret);
		bc12_set_state(dev, BC12_STATE_VBUS_DETECT);
	}
}

static void bc12_vbus_detect_run(void *obj)
{
	struct bc12_sm_t *bc = (struct bc12_sm_t *)obj;
	const struct device *dev = bc->dev;
	struct usb_port_data *data = dev->data;
	const struct device *vbus = data->vbus;

	if (usb_vbus_check_level(vbus, TC_VBUS_SAFE0V)) {
		bc12_set_state(dev, BC12_STATE_DATA_CONTACT_DETECT);
	}
}

static void bc12_data_contact_detect_entry(void *obj)
{
	struct bc12_sm_t *bc = (struct bc12_sm_t *)obj;
	const struct device *dev = bc->dev;
	struct usb_port_data *data = dev->data;
	const struct device *bc12 = data->bc12;
	int ret;

	bc->dp_state = BC12_DP_IDP_SRC;
	bc->dm_state = BC12_DM_RDM_DWN;

	ret = bc12_set_dpdm(bc12, BC12_DP_IDP_SRC, BC12_DM_RDM_DWN);
	if (ret != 0) {
		LOG_ERR("Couldn't set Data lines DCD: %d", ret);
		bc12_timer_start(&bc->bc12_t_dcd);
		bc12_timer_start(&bc->bc12_t_vdpsrc_on);
	}
}

static void bc12_data_contact_detect_run(void *obj)
{
	struct bc12_sm_t *bc = (struct bc12_sm_t *)obj;
	const struct device *dev = bc->dev;
	struct usb_port_data *data = dev->data;
	const struct device *bc12 = data->bc12;
	const struct device *vbus = data->vbus;
	int ret;

	if (!usb_vbus_check_level(vbus, TC_VBUS_SAFE0V)) {
		bc12_set_state(dev, BC12_STATE_VBUS_DETECT);
	}

	ret = get_dpdm(bc12, &bc->dp_level, &bc->dm_level);
	if (bc->dp_level == BC12_BELOW_VDAT_REF)


	if (bc->dp_level != BC12_BELOW_VDAT_REF && bc->dm_level != BC12_BELOW_VDAT_REF) {
		if (!usbc_timer_running(&bc->bc12_t_dcd_dbnc)) {
			bc12_timer_start(&bc->bc12_t_dcd_dbnc);
		}
	} else {
		if (usbc_timer_running(&bc->bc12_t_dcd_dbnc)) {
			bc12_timer_stop(&bc->bc12_t_dcd_dbnc);
		}
	}

	if (usb_timer_expired(&bc->bc12_t_vdpsrc_on)) {
		bc12_timer_stop(&bc->bc12_t_vdpsrc_on);

		if (usb_timer_expired(&bc->bc12_t_dcd_dbnc)) {
			bc12_timer_stop(&bc->bc12_t_dcd_dbnc);
			bc12_set_state(dev, BC12_STATE_PRIMARY_DETECTION);
		} else {
			data->bc12_type = BC12_TYPE_SDP;
			bc12_timer_stop(&bc->bc12_t_dcd);
			bc12_set_state(dev, BC12_STATE_FINISH);
		}
	}
}

static void bc12_data_contact_detect_exit(void *obj)
{
	bc->dp_state = BC12_DP_OPEN;
	bc->dm_state = BC12_DM_OPEN;

	bc12_set_dpdm(bc12, BC12_DP_OPEN, BC12_DM_OPEN);
}

static void bc12_primary_detection_entry(void *obj)
{
	struct bc12_sm_t *bc = (struct bc12_sm_t *)obj;
	const struct device *dev = bc->dev;
	struct usb_port_data *data = dev->data;
	const struct device *bc12 = data->bc12;
	int ret;

	bc->dp_state = BC12_DP_IDP_SRC;
	bc->dm_state = BC12_DM_IDM_SINK;

	ret = bc12_set_dpdm(bc12, BC12_DP_IDP_SRC, BC12_DM_IDM_SINK);
	if (ret != 0) {
		bc12_timer_start(&bc->bc12_t_vdmsrc_on);
	}
}

/**
 * @brief BC1.2 State Table
 */
static const struct smf_state bc12_states[BC12_STATE_COUNT] = {
	[BC12_STATE_DISCONNECT] = SMF_CREATE_STATE(
		bc12_disconnect_entry,
		NULL,
		NULL,
		NULL,
		NULL),
	[BC12_STATE_VBUS_DETECT] = SMF_CREATE_STATE(
		NULL,
		bc12_vbus_detect_run,
		NULL,
		NULL,
		NULL),
	[BC12_STATE_DATA_CONTACT_DETECT] = SMF_CREATE_STATE(
		bc12_data_contact_detect_entry,
		bc12_data_contact_detect_run,
		bc12_data_contact_detect_exit,
		NULL,
		NULL),
	[BC12_STATE_PRIMARY_DETECTION] = SMF_CREATE_STATE(
		bc12_primary_detection_entry,
		bc12_primary_detection_run,
		bc12_primary_detection_exit,
		NULL,
		NULL),
	[BC12_STATE_SECONDARY_DETECTION] = SMF_CREATE_STATE(
		bc12_secondary_detection_entry,
		bc12_secondary_detection_run,
		bc12_secondary_detection_exit,
		NULL,
		NULL),
	[BC12_STATE_FINISH] = SMF_CREATE_STATE(
		NULL,
		bc12_finish_run,
		NULL,
		NULL,
		NULL),
	[BC12_STATE_ERROR] = SMF_CREATE_STATE(
		tc_error_recovery_entry,
		tc_error_recovery_run,
		NULL,
		NULL,
		NULL),
};
BUILD_ASSERT(ARRAY_SIZE(bc12_states) == BC12_STATE_COUNT);
