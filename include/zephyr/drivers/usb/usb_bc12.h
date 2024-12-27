/*
 * Copyright (c) 2022 Google LLC
 * Copyright (c) 2025 Jianxiong Gu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public APIs for the USB BC1.2 battery charging detect drivers.
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_USB_USB_BC12_H_
#define ZEPHYR_INCLUDE_DRIVERS_USB_USB_BC12_H_

#include <zephyr/device.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BC1.2 driver APIs
 * @defgroup b12_interface BC1.2 driver APIs
 * @ingroup io_interfaces
 * @{
 */

/* FIXME - make these Kconfig options */

/**
 * @brief Maximum value for DCP Shut Down Voltage.
 *        A PD shall not pull the output voltage of a Charging Port below VDCP_SHTDWN max.
 */
#define BC12_V_DCP_SHTDWN_MAX_MV 2000

/**
 * @brief OTG Session Valid Voltage
 *        Each PD shall detects when VBUS is greater than its internal session valid threshold.
 */
#define BC12_V_OTG_SESS_VLD_MAX_MV 4000
#define BC12_V_OTG_SESS_VLD_MIN_MV  800

/**
 * @name BC1.2, Table 5-5 Times
 * @{
 */

/**
 * @brief Maximum time for IDP_SINK to be disabled after connect.
 *        The CDP shall disable IDP_SINK within TCON_IDPSNK_DIS of detecting the connect.
 */
#define BC12_T_CON_IDPSNK_DIS_MAX_MS 10

/**
 * @brief Time for VDM_SRC to be disabled after connect.
 *        This is the time a CDP waits to disable VDM_SRC after a connect.
 */
#define BC12_T_CP_VDM_DIS_MAX_MS 10

/**
 * @brief Time for VDM_SRC to be enabled after disconnect.
 *        This is the time a CDP waits to enable VDM_SRC after a disconnect.
 */
#define BC12_T_CP_VDM_EN_MAX_MS 200

/**
 * @brief Time for VDP_SRC to be enabled after attach during Dead Battery Provision (DBP).
 *        This is the time a PD waits to enable VDP_SRC after attach.
 */
#define BC12_T_DBP_ATT_VDPSRC_MAX_MS 1000

/**
 * @brief Time for a PD to reach full USB functionality under Dead Battery Provision (DBP).
 *        This is the time a PD waits to provide full USB functionality after attach.
 */
#define BC12_T_DBP_FUL_FNCTN_MS 15 * 60 * 1000

/**
 * @brief Time for a PD to inform the user that it is charging.
 *        This is the time a PD waits to inform the user that it is charging after attach.
 */
#define BC12_T_DBP_INFORM_MAX_MS 60 * 1000

/**
 * @brief Time for VDP_SRC to be disabled before connect during Dead Battery Provision (DBP).
 *        This is the time a PD waits to disable VDP_SRC before connect.
 */
#define BC12_T_DBP_VDPSRC_CON_MAX_MS 1000

/**
 * @brief Debounce time for Data Contact Detect (DCD).
 *        This is the time the PD waits to confirm data pin contact.
 */
#define BC12_T_DCD_DBNC_MIN_MS 10

/**
 * @brief Maximum time for Data Contact Detect (DCD) timeout.
 *        This is the time the PD waits for data pin contact detection.
 */
#define BC12_T_DCD_TIMEOUT_MAX_MS 900

/**
 * @brief Minimum time for Data Contact Detect (DCD) timeout.
 *        This is the time the PD waits for data pin contact detection.
 */
#define BC12_T_DCD_TIMEOUT_MIN_MS 300

/**
 * @brief Minimum time between load steps for a Dedicated Charging Port (DCP).
 *        This is the time a DCP waits between load steps to ensure stable operation.
 */
#define BC12_T_DCP_LD_STP_MIN_MS 20

/**
 * @brief Maximum undershoot time for a Dedicated Charging Port (DCP).
 *        This is the maximum time a DCP allows for undershoot during load transitions.
 */
#define BC12_T_DCP_UNDSHT_MAX_MS 10

/**
 * @brief Maximum recovery time after shutdown for a Charging Port.
 *        This is the time a Charging Port takes to recover after a shutdown event.
 */
#define BC12_T_SHTDWN_REC_MAX_MS 2 * 60 * 1000

/**
 * @brief Time for a PD to connect after VBUS is detected.
 *        This is the time a PD waits to connect after VBUS is detected.
 */
#define BC12_T_SVLD_CON_PWD_MAX_MS 1000

/**
 * @brief Time for a PD with Dead or Weak Battery to connect after VBUS is detected.
 *        This is the time a PD with Dead or Weak Battery waits to connect after VBUS is detected.
 */
#define BC12_T_SVLD_CON_WKB_MS 45 * 60 * 1000

/**
 * @brief Maximum time for VBUS voltage averaging.
 *        This is the time over which the VBUS voltage is averaged to ensure stable readings.
 */
#define BC12_T_VBUS_AVG_MAX_MS 250

/**
 * @brief Time for VBUS to be reapplied after detection renegotiation.
 *        This is the time a downstream port waits to reapply VBUS after detection renegotiation.
 */
#define BC12_T_VBUS_REAPP_MIN_MS 100

/**
 * @brief Maximum time for VDM_SRC to be disabled.
 *        This is the time a Charging Port takes to disable VDM_SRC after a connect event.
 */
#define BC12_T_VDMSRC_DIS_MAX_MS 20

/**
 * @brief Maximum time for VDM_SRC to be enabled.
 *        This is the time a Charging Port takes to enable VDM_SRC after a disconnect event.
 */
#define BC12_T_VDMSRC_EN_MAX_MS 20

/**
 * @brief Minimum time for VDP_SRC to be on during Primary Detection.
 *        This is the time a PD keeps VDP_SRC on during Primary Detection.
 */
#define BC12_T_VDPSRC_ON_MIN_MS 40

/**
 * @brief Minimum time for VDM_SRC to be on during Secondary Detection.
 *        This is the time a PD keeps VDM_SRC on during Secondary Detection.
 */
#define BC12_T_VDMSRC_ON_MIN_MS 40

/**
 * @brief Time for VBUS to decay to VBUS_LKG after removal.
 *        This is the time VBUS takes to decay to VBUS_LKG after removal.
 */
#define BC12_T_VLD_VLKG_MAX_MS 500

/** @} */

/**
 * @name BC1.2 constants
 * @{
 */

/** BC1.2 USB charger voltage. */
#define BC12_CHARGER_VOLTAGE_UV  5000 * 1000
/**
 * BC1.2 USB charger minimum current. Set to match the Isusp of 2.5 mA parameter.
 * This is returned by the driver when either BC1.2 detection fails, or the
 * attached partner is a SDP (standard downstream port).
 *
 * The application may increase the current draw after determining the USB device
 * state of suspended/unconfigured/configured.
 *   Suspended: 2.5 mA
 *   Unconfigured: 100 mA
 *   Configured: 500 mA (USB 2.0)
 */
#define BC12_CHARGER_MIN_CURR_UA 2500
/** BC1.2 USB charger maximum current. */
#define BC12_CHARGER_MAX_CURR_UA 1500 * 1000

/** @} */

/** @cond INTERNAL_HIDDEN
 * @brief Helper macro for setting a BC1.2 current limit
 *
 * @param val Current limit value, in uA.
 * @return A valid BC1.2 current limit, in uA, clamped between the BC1.2 minimum
 * and maximum values.
 */
#define BC12_CURR_UA(val) CLAMP(val, BC12_CHARGER_MIN_CURR_UA, BC12_CHARGER_MAX_CURR_UA)
/** @endcond  */

/** @brief BC1.2 device role. */
enum bc12_role {
	BC12_DISCONNECTED,
	BC12_PORTABLE_DEVICE,
	BC12_CHARGING_PORT,
};

/** @brief BC1.2 charging partner type. */
enum bc12_type {
	/**  No partner connected. */
	BC12_TYPE_NONE,
	/** Standard Downstream Port */
	BC12_TYPE_SDP,
	/** Dedicated Charging Port */
	BC12_TYPE_DCP,
	/** Charging Downstream Port */
	BC12_TYPE_CDP,
	/** Proprietary charging port */
	BC12_TYPE_PROPRIETARY,
	/** Unknown charging port, BC1.2 detection failed. */
	BC12_TYPE_UNKNOWN,
	/** Count of valid BC12 types. */
	BC12_TYPE_COUNT,
};

/** @brief BC1.2 DP state. */
enum bc12_dp_state {
	/** BC1.2 peripheral disconnect to D+ line. */
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
	/** BC1.2 peripheral disconnect to D- line. */
	BC12_DM_OPEN,
	/** turn on VDM_SRC */
	BC12_DM_VDM_SRC,
	/** turn on IDM_SRC */
	BC12_DM_IDM_SINK,
	/** turn on IDM_SINK */
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

/**
 * @brief BC1.2 detected partner state.
 *
 * @param bc12_role Current role of the BC1.2 device.
 * @param type Charging partner type. Valid when bc12_role is BC12_PORTABLE_DEVICE.
 * @param current_ma Current, in uA, that the charging partner provides. Valid when bc12_role is
 * BC12_PORTABLE_DEVICE.
 * @param voltage_mv Voltage, in uV, that the charging partner provides. Valid when bc12_role is
 * BC12_PORTABLE_DEVICE.
 * @param pd_partner_connected True if a PD partner is currently connected. Valid when bc12_role is
 * BC12_CHARGING_PORT.
 */
struct bc12_partner_state {
	enum bc12_role bc12_role;
	union {
		struct {
			enum bc12_type type;
			int current_ua;
			int voltage_uv;
		};
		struct {
			bool pd_partner_connected;
		};
	};
};

/**
 * @brief BC1.2 callback for charger configuration
 *
 * @param dev BC1.2 device which is notifying of the new charger state.
 * @param state Current state of the BC1.2 client, including BC1.2 type
 * detected, voltage, and current limits.
 * If NULL, then the partner charger is disconnected or the BC1.2 device is
 * operating in host mode.
 * @param user_data Requester supplied data which is passed along to the callback.
 */
typedef void (*bc12_callback_t)(const struct device *dev, struct bc12_partner_state *state,
				void *user_data);

/**
 * @cond INTERNAL_HIDDEN
 *
 * These are for internal use only, so skip these in public documentation.
 */
__subsystem struct bc12_driver_api {
	int (*set_role)(const struct device *dev, enum bc12_role role);
	int (*set_result_cb)(const struct device *dev, bc12_callback_t cb, void *user_data);
	int (*set_dpdm)(const struct device *dev, enum bc12_dp_state dp, enum bc12_dm_state dm);
	int (*get_dpdm)(const struct device *dev, enum bc12_dpdm_voltage_level *dp,
			enum bc12_dpdm_voltage_level *dm);
};
/**
 * @endcond
 */

/**
 * @brief Set the BC1.2 role.
 *
 * @param dev Pointer to the device structure for the BC1.2 driver instance.
 * @param role New role for the BC1.2 device.
 *
 * @retval 0 If successful.
 * @retval -EIO general input/output error.
 */
__syscall int bc12_set_role(const struct device *dev, enum bc12_role role);

static inline int z_impl_bc12_set_role(const struct device *dev, enum bc12_role role)
{
	const struct bc12_driver_api *api = (const struct bc12_driver_api *)dev->api;

	return api->set_role(dev, role);
}

/**
 * @brief Register a callback for BC1.2 results.
 *
 * @param dev Pointer to the device structure for the BC1.2 driver instance.
 * @param cb Function pointer for the result callback.
 * @param user_data Requester supplied data which is passed along to the callback.
 *
 * @retval 0 If successful.
 * @retval -EIO general input/output error.
 */
__syscall int bc12_set_result_cb(const struct device *dev, bc12_callback_t cb, void *user_data);

static inline int z_impl_bc12_set_result_cb(const struct device *dev, bc12_callback_t cb,
					    void *user_data)
{
	const struct bc12_driver_api *api = (const struct bc12_driver_api *)dev->api;

	return api->set_result_cb(dev, cb, user_data);
}

/**
 * @brief Set the DP and DM state for BC1.2 detection.
 *
 * @param dev Pointer to the device structure for the BC1.2 driver instance.
 * @param dp Current DP state.
 * @param dm Current DM state.
 *
 * @retval 0 If successful.
 * @retval -EIO General input/output error.
 */
__syscall int bc12_set_dpdm(const struct device *dev, enum bc12_dp_state dp, enum bc12_dm_state dm);

static inline int z_impl_bc12_set_dpdm(const struct device *dev, enum bc12_dp_state dp,
				       enum bc12_dm_state dm)
{
	const struct bc12_driver_api *api = (const struct bc12_driver_api *)dev->api;

	if (api->set_dpdm == NULL) {
		return -ENOSYS;
	}

	return api->set_dpdm(dev, dp, dm);
}

/**
 * @brief Get the voltage level on DP and DM.
 *
 * @param dev Pointer to the device structure for the BC1.2 driver instance.
 * @param dp Pointer where the DP voltage level is stored.
 * @param dm Pointer where the DM voltage level is stored.
 *
 * @retval 0 If successful.
 * @retval -EIO General input/output error.
 */
__syscall int bc12_get_dpdm(const struct device *dev, enum bc12_dpdm_voltage_level *dp,
			    enum bc12_dpdm_voltage_level *dm);

static inline int z_impl_bc12_get_dpdm(const struct device *dev, enum bc12_dpdm_voltage_level *dp,
				       enum bc12_dpdm_voltage_level *dm)
{
	const struct bc12_driver_api *api = (const struct bc12_driver_api *)dev->api;

	if (api->get_dpdm == NULL) {
		return -ENOSYS;
	}

	return api->get_dpdm(dev, dp, dm);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#include <zephyr/syscalls/usb_bc12.h>

#endif /* ZEPHYR_INCLUDE_DRIVERS_USB_USB_BC12_H_ */
