/*
 * Copyright (c) 2024 Jianxiong Gu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(usb_bc12, CONFIG_USB_BC12_LOG_LEVEL);

#include <zephyr/smf.h>
#include <zephyr/drivers/usb/usb_bc12.h>
#include "bc12.h"

/**
 * @brief Port data
 */
struct pd_port_data {
	/** This port's thread */
	k_tid_t port_thread;
	/** This port thread's data */
	struct k_thread thread_data;

	/** BC1.2 state machine object */
	struct bc12_sm_t *pd;
	/** Enables or Disables the BC1.2 state machine */
	bool bc12_enabled;

	/** The BC1.2 Port Controller on this port */
	const struct device *bc12;
	/** VBUS Measurement and control device on this port */
	const struct device *vbus;
};

static const struct smf_state bc12_states[BC12_STATE_COUNT];

/**
 * @brief Initializes the state machine and enters the Disabled state
 */
void bc12_subsys_init(const struct device *dev)
{
	struct pd_port_data *data = dev->data;
	struct bc12_sm_t *pd = data->pd;

	/* Save the port device object so states can access it */
	pd->dev = dev;

	/* Initialize the state machine */
	smf_set_initial(SMF_CTX(pd), &bc12_pd_states[BC12_DISABLED_STATE]);
}

static void bc12_pd_open_data_lines(void *obj)
{
	struct bc12_sm_t *pd = (struct bc12_sm_t *)obj;
	const struct device *dev = pd->dev;
	struct pd_port_data *data = dev->data;
	const struct device *bc12 = data->bc12;
	int ret;

	ret = set_dpdm(tcpc, BC12_DP_OPEN, BC12_DM_OPEN);
	if (ret != 0) {
		LOG_ERR("Couldn't set CC lines to open: %d", ret);
		tc_set_state(dev, TC_ERROR_RECOVERY_STATE);
	}
}

/**
 * @brief VBUS Detect
 */
void bc12_pd_vbus_detect_entry(void *obj)
{
	struct tc_sm_t *tc = (struct tc_sm_t *)obj;
	const struct device *dev = tc->dev;
	struct usbc_port_data *data = dev->data;
	const struct device *tcpc = data->tcpc;
	int ret;

	ret = tcpc_set_cc(tcpc, TC_CC_RD);
	if (ret != 0) {
		LOG_ERR("Couldn't set CC lines to Rd: %d", ret);
		tc_set_state(dev, TC_ERROR_RECOVERY_STATE);
	}
}

void bc12_pd_vbus_detect_run(void *obj);
{
	struct tc_sm_t *tc = (struct tc_sm_t *)obj;
	const struct device *dev = tc->dev;

	/*
	 * Transition to AttachWait.SNK when the SNK.Rp state is present
	 * on at least one of its CC pins.
	 */
	if (tcpc_is_cc_rp(tc->cc1) || tcpc_is_cc_rp(tc->cc2)) {
		tc_set_state(dev, TC_ATTACH_WAIT_SNK_STATE);
	}
}

/**
 * @brief BC1.2 State Table
 */
static const struct smf_state bc12_pd_states[BC12_STATE_COUNT] = {
	[BC12_DATA_INIT_STATE] = SMF_CREATE_STATE(
		bc12_pd_open_data_lines,
		NULL,
		NULL,
		NULL,
		NULL),
	[BC12_VBUS_DETECT_STATE] = SMF_CREATE_STATE(
		bc12_pd_vbus_detect_entry,
		bc12_pd_vbus_detect_run,
		NULL,
		&bc12_pd_states[BC12_DATA_INIT_STATE],
		NULL),
	[BC12_DATA_CONTACT_DETECT_STATE] = SMF_CREATE_STATE(
		bc12_pd_data_contact_detect_entry,
		bc12_pd_data_contact_detect_run,
		bc12_pd_data_contact_detect_exit,
		&bc12_pd_states[BC12_DATA_INIT_STATE],
		NULL),
	[BC12_PRIMARY_DETECTION_STATE] = SMF_CREATE_STATE(
		bc12_pd_primary_detection_entry,
		bc12_pd_primary_detection_run,
		bc12_pd_primary_detection_exit,
		&bc12_pd_states[BC12_DATA_INIT_STATE],
		NULL),
	[BC12_SECONDARY_DETECTION_STATE] = SMF_CREATE_STATE(
		bc12_pd_secondary_detection_entry,
		bc12_pd_secondary_detection_run,
		bc12_pd_secondary_detection_exit,
		&bc12_pd_states[BC12_DATA_INIT_STATE],
		NULL),
	[BC12_CHARGING_STATE] = SMF_CREATE_STATE(
		bc12_pd_secondary_detection_entry,
		bc12_pd_secondary_detection_run,
		NULL,
		&bc12_pd_states[BC12_DATA_INIT_STATE],
		NULL),
	[BC12_DISABLED_STATE] = SMF_CREATE_STATE(
		tc_disabled_entry,
		tc_disabled_run,
		NULL,
		&tc_states[BC12_DATA_INIT_STATE],
		NULL),
	[BC12_ERROR_RECOVERY_STATE] = SMF_CREATE_STATE(
		tc_error_recovery_entry,
		tc_error_recovery_run,
		NULL,
		&tc_states[BC12_DATA_INIT_STATE],
		NULL),
};
BUILD_ASSERT(ARRAY_SIZE(bc12_pd_states) == BC12_STATE_COUNT);
