/*
 * Copyright (c) 2025 Jianxiong Gu
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT wch_ch32v_bc12

#include <kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/usb/usb_bc12.h>
#include <logging/log.h>

LOG_MODULE_REGISTER(bc12_ch32v, CONFIG_USB_BC12_LOG_LEVEL);

#include <soc.h>

struct ch32v_bc12_config {
	struct gpio_dt_spec dp_gpio;
	struct gpio_dt_spec dm_gpio;
	// enum bc12_type charging_mode;

	uint32_t output_ohm;
	uint32_t full_ohm;
	struct adc_dt_spec adc_channel;
};

struct ch32v_bc12_data {
	const struct device *dev;
	struct k_work work;
	struct bc12_partner_state partner_state;
	struct gpio_callback gpio_cb;

	// enum bc12_dpdm_voltage_level dp;
	// enum bc12_dpdm_voltage_level dm;

	bc12_callback_t result_cb;
	void *result_cb_data;

	struct k_timer timer;
	enum bc12_role current_role;

	enum {
		BC12_STATE_IDLE,
		BC12_STATE_VBUS_DETECT,
		BC12_STATE_DATA_CONTACT_DETECT,
		BC12_STATE_PRIMARY_DETECTION,
		BC12_STATE_SECONDARY_DETECTION,
		BC12_STATE_FINISH,
	} detection_state;

	uint16_t adc_raw;
	struct adc_sequence adc_seq;
};

static int ch32v_bc12_set_dpdm(const struct device *dev, enum bc12_dp_state dp, enum bc12_dm_state dm)
{
	uint32_t reg = 0;

	if (dp == BC12_DP_SHORT_DM && dm == BC12_DM_SHORT_DP) {
		EXTEN->CTLR1 = UDU_SHRT;
		return 0;
	}

	switch (dp) {
	case BC12_DP_OPEN:
		break;
	case BC12_DP_VDP_SRC:
		/** DAC programmable voltage mode */
		reg |= UDP_PDE | UDP_PUE;
		/** output to the pin through the buffer */
		reg |= UDP_AE | UDP_BUFOE;
		/** DAC voltage 8*0.075V */
		reg |= 8 << UDP_DAC_SHIFT;
		break;
	case BC12_DP_IDP_SRC:
		/** pull-up 10uA */
		reg |= UDP_PCS_1;
		break;
	case BC12_DP_IDP_SINK:
		/** DAC programmable voltage mode */
		reg |= UDP_PDE | UDP_PUE;
		/** compared with the DAC voltage */
		reg |= UDP_AE;
		/** DAC voltage 4*0.075V */
		reg |= 4 << UDP_DAC_SHIFT;
		/** pull-down 80uA */
		reg |= UDP_PCS;
		break;
	case BC12_DP_RDP_UP:
		/** programmable pull-up resistor mode */
		reg |= UDP_PUE;
		/** pull-up resistor 1*1kohm */
		reg |= 1u << UDP_DAC_SHIFT;;
		break;
	case BC12_DP_RDP_DWN:
		/** programmable pull-down resistor mode */
		reg |= UDP_PDE;
		/** pull-down resistor 20*1kohm */
		reg |= 20u << UDP_DAC_SHIFT;
		break;
	default:
		LOG_ERR("Unsupported DP state: %d", dp);
		return -EINVAL;
	}

	switch (dm) {
	case BC12_DM_OPEN:
		break;
	case BC12_DM_VDM_SRC:
		/** DAC programmable voltage mode */
		reg |= UDM_PDE | UDM_PUE;
		/** output to the pin through the buffer */
		reg |= UDM_AE | UDM_BUFOE;
		/** DAC voltage 8*0.075V */
		reg |= 8 << UDM_DAC_SHIFT;
		break;
	case BC12_DM_IDM_SINK:
		/** DAC programmable voltage mode */
		reg |= UDM_PDE | UDM_PUE;
		/** compared with the DAC voltage */
		reg |= UDM_AE;
		/** DAC voltage 4*0.075V */
		reg |= 4 << UDM_DAC_SHIFT;
		/** pull-down 80uA */
		reg |= UDM_PCS;
		break;
	case BC12_DM_RDM_DWN:
		/** programmable pull-down resistor mode */
		reg |= UDM_PDE;
		/** pull-down resistor 20*1kohm */
		reg |= 20u << UDM_DAC_SHIFT;
		break;
	default:
		LOG_ERR("Unsupported DM state: %d", dm);
		return -EINVAL;
	}

	EXTEN->CTLR1 = reg;
	return 0;
}

static int ch32v_get_dpdm(const struct device *dev, enum bc12_dpdm_voltage_level *dp,
			  enum bc12_dpdm_voltage_level *dm)
{
	const struct ch32v_bc12_config *cfg = dev->config;
	const uint32_t reg = EXTEN->CTLR1;

	if (gpio_pin_get_dt(&cfg->dp_gpio)) {
		*dp = BC12_LOGIC_HIGH;
	} else if (reg & UDP_AI) {
		*dp = BC12_ABOVE_VDAT_REF;
	} else {
		*dp = BC12_BELOW_VDAT_REF;
	}

	if (gpio_pin_get_dt(&cfg->dm_gpio)) {
		*dm = BC12_LOGIC_HIGH;
	} else if (reg & UDM_AI) {
		*dm = BC12_ABOVE_VDAT_REF;
	} else {
		*dm = BC12_BELOW_VDAT_REF;
	}

	return 0;
}

static void ch32v_bc12_gpio_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    struct ch32v_bc12_data *data = CONTAINER_OF(cb, struct ch32v_bc12_data, gpio_cb);
    k_work_submit(&data->work);
}

static void ch32v_bc12_timer_handler(struct k_timer *timer)
{
    struct ch32v_bc12_data *data = CONTAINER_OF(timer, struct ch32v_bc12_data, timer);
    const struct ch32v_bc12_config *config = data->dev->config;

    if (data->current_role == BC12_PORTABLE_DEVICE && data->detection_state != BC12_STATE_IDLE) {
        // Trigger ADC sampling
        adc_read_async(config->adc_channel.dev, &data->adc_seq, NULL);
    }

    k_work_submit(&data->work);
}

static void ch32v_bc12_adc_callback(const struct device *dev, struct adc_sequence *seq, int err)
{
	struct ch32v_bc12_data *data = CONTAINER_OF(seq, struct ch32v_bc12_data, adc_seq);
	const struct ch32v_bc12_config *config = data->dev->config;

	if (err) {
		return;
	}

	uint32_t value = data->adc_raw;

	if (config->full_ohm > 0) {
		value = (value * 1000) / ((config->output_ohm * 1000) / config->full_ohm);
	}

	if (value < 2000u) {
		data->detection_state = BC12_STATE_IDLE;
	} else {
		if (data->detection_state == BC12_STATE_VBUS_DETECT) {

			ch32v_set_dpdm(dev, BC12_DP_IDP_SRC, BC12_DM_RDM_DWN);
			k_timer_start(&data->timer, K_MSEC(BC12_T_DCD_TIMEOUT_MIN_MS), K_NO_WAIT);

			data->detection_state = BC12_STATE_DATA_CONTACT_DETECT;
		}
	}

	k_work_submit(&data->work);
}

static void ch32v_bc12_work_handler(struct k_work *work)
{
	struct ch32v_bc12_data *data = CONTAINER_OF(work, struct ch32v_bc12_data, work);
	const struct device *dev = data->dev;
	enum bc12_dpdm_voltage_level dp_level, dm_level;

	if (data->current_role != BC12_PORTABLE_DEVICE) {
		return;
	}

	ch32v_get_dpdm(dev, &dp_level, &dm_level);

	switch (data->detection_state) {
	case BC12_STATE_IDLE:
		/* Do nothing in IDLE state */
		break;
	case BC12_STATE_VBUS_DETECT:
		/* VBUS detection is handled in ADC callback */
		break;
	case BC12_STATE_DATA_CONTACT_DETECT:
		if (dp_level == BC12_LOGIC_HIGH || dm_level == BC12_LOGIC_HIGH) {
			data->partner_state.type = BC12_TYPE_UNKNOWN;
			data->detection_state = BC12_STATE_FINISH;
			break;
		}

		if (k_timer_status_get(&data->timer) > 0) {
			ch32v_set_dpdm(dev, BC12_DP_VDP_SRC, BC12_DM_IDM_SINK);
			k_timer_start(&data->timer, K_MSEC(BC12_T_VDPSRC_ON_MIN_MS), K_NO_WAIT);

			data->detection_state = BC12_STATE_PRIMARY_DETECTION;
		}

		break;
	case BC12_STATE_PRIMARY_DETECTION:
		if (dp_level == BC12_LOGIC_HIGH) {
			data->partner_state.type = BC12_TYPE_UNKNOWN;
			data->detection_state = BC12_STATE_FINISH;
			break;
		}

		/* If D- is less than VDAT_REF, then the PD is allowed to determine that it is
		 * attached to an SDP. A PD is optionally allowed to compare D- with VLGC as well,
		 * and determine that it is attached to an SDP if D- is greater than VLGC.
		 */
		if (dm_level == BC12_BELOW_VDAT_REF || dm_level == BC12_LOGIC_HIGH) {
			data->partner_state.type = BC12_TYPE_SDP;
			data->detection_state = BC12_STATE_FINISH;
			break;
		}

		/* A PD only determine that it is attached to a DCP or CDP if D- is greater than
		 * VDAT_REF, but less than VLGC.
		 */
		if (k_timer_status_get(&data->timer) > 0) {
			ch32v_set_dpdm(dev, BC12_DM_IDP_SINK, BC12_DP_VDM_SRC);
			k_timer_start(&data->timer, K_MSEC(BC12_T_VDMSRC_ON_MIN_MS), K_NO_WAIT);

			data->detection_state = BC12_STATE_SECONDARY_DETECTION;
		}

		break;
	case BC12_STATE_SECONDARY_DETECTION:
		/*
		 * If a PD detects that D+ is greater than VDAT_REF, it knows that it is attached
		 * to a DCP. It is then required to enable VDP_SRC or pull D+ to VDP_UP through
		 * RDP_UP.
		 */
		if (dp_level == BC12_ABOVE_VDAT_REF) {
			data->partner_state.type = BC12_TYPE_DCP;
			ch32v_set_dpdm(dev, BC12_DP_VDP_SRC, BC12_DM_IDM_SINK);
			data->detection_state = BC12_STATE_FINISH;
		} else if (dp_level == BC12_BELOW_VDAT_REF) {
			data->partner_state.type = BC12_TYPE_CDP;
			data->detection_state = BC12_STATE_FINISH;
		} else {
			data->partner_state.type = BC12_TYPE_UNKNOWN;
			data->detection_state = BC12_STATE_FINISH;
		}

		break;
	case BC12_STATE_FINISH:
		break;
	}

	if (data->detection_state == BC12_STATE_FINISH) {
		k_timer_stop(&data->timer);
		/* Only do VBUS detection */
		k_timer_start(&data->timer, K_MSEC(BC12_T_DCD_DBNC_MIN_MS), K_NO_WAIT);
	}
}

static int ch32v_bc12_set_role(const struct device *dev, const enum bc12_role role)
{
	struct ch32v_bc12_data *data = dev->data;

	switch (role) {
	case BC12_DISCONNECTED:
		ch32v_set_dpdm(dev, BC12_DP_OPEN, BC12_DM_OPEN);
		k_timer_stop(&data->timer);
		data->current_role = BC12_DISCONNECTED;
		data->detection_state = BC12_STATE_IDLE;
		break;
	case BC12_PORTABLE_DEVICE:
		data->current_role = BC12_PORTABLE_DEVICE;
		data->detection_state = BC12_STATE_VBUS_DETECT;
		// Start periodic ADC sampling
		k_timer_start(&data->timer, K_MSEC(BC12_T_DCD_DBNC_MIN_MS), K_NO_WAIT);
		break;
	default:
		LOG_ERR("Unsupported BC12 role: %d", role);
		return -EINVAL;
	}

	return 0;
}

static int ch32v_bc12_set_result_cb(const struct device *dev, bc12_callback_t cb, void *const user_data)
{
	struct ch32v_bc12_data *data = dev->data;

	data->result_cb = cb;
	data->result_cb_data = user_data;

	return 0;
}

static DEVICE_API(bc12, ch32v_bc12_driver_api) = {
	.set_role = ch32v_bc12_set_role,
	.set_result_cb = ch32v_bc12_set_result_cb,
};

static int ch32v_bc12_init(const struct device *dev)
{
	const struct ch32v_bc12_config *cfg = dev->config;
	struct ch32v_bc12_data *data = dev->data;
	int rv;

#ifdef CONFIG_SOC_CH641
#define VENDOR_CFG0_BASE	((uint32_t)0x1FFFF7D4)
#define CFG0_PD_C		(VENDOR_CFG0_BASE + 0x02)
	uint8_t tmp = 0;
	tmp = *( uint8_t * )CFG0_PD_C;

	/** adjusts the reference current based on specific conditions. */
	if (tmp != 0xFF && tmp & 0x01) {
		/** 12% increase in PD PHY and BC pin reference current */
		EXTEN->CTLR2 |= EXTEN_IREF_INC;
	}
#endif /** CONFIG_SOC_CH641 */

	data->dev = dev;

	k_work_init(&data->work, ch32v_bc12_work_handler);
	k_timer_init(&data->timer, ch32v_bc12_timer_handler, NULL);

	if (!gpio_is_ready_dt(&cfg->dp_gpio)) {
		LOG_ERR("D+ gpio is not ready.");
		return -EIO;
	}
	if (!gpio_is_ready_dt(&cfg->dm_gpio)) {
		LOG_ERR("D- gpio is not ready.");
		return -EIO;
	}

	ch32v_bc12_disconnect(dev);
	data->partner_state.bc12_role = BC12_DISCONNECTED;

	rv = gpio_pin_configure_dt(&cfg->dp_gpio, GPIO_INPUT);
	rv |= gpio_pin_configure_dt(&cfg->dm_gpio, GPIO_INPUT);
	if (rv < 0) {
		LOG_ERR("Failed to set gpio callback.");
		return rv;
	}

	gpio_init_callback(&data->gpio_cb, ch32v_bc12_gpio_callback,
			   BIT(cfg->dp_gpio.pin) | BIT(cfg->dm_gpio.pin));

	gpio_add_callback(cfg->dp_gpio.port, &data->gpio_cb);
	rv = gpio_pin_interrupt_configure_dt(&cfg->dp_gpio, GPIO_INT_EDGE_BOTH);
	if (rv < 0) {
		LOG_ERR("Failed to configure D+ interrupt");
		return rv;
	}

	gpio_add_callback(cfg->dm_gpio.port, &data->gpio_cb);
	rv = gpio_pin_interrupt_configure_dt(&cfg->dm_gpio, GPIO_INT_EDGE_BOTH);
	if (rv < 0) {
		LOG_ERR("Failed to configure D- interrupt");
		return rv;
	}

	if (!adc_is_ready_dt(&config->adc_channel)) {
		LOG_ERR("ADC controller device is not ready");
		return -ENODEV;
	}

	data->sequence.buffer = &data->sample;
	data->sequence.buffer_size = sizeof(data->sample);

	data->adc_seq = (struct adc_sequence){
		.channels = BIT(config->adc_channel.channel),
		.buffer = &data->adc_raw,
		.buffer_size = sizeof(data->adc_raw),
		.resolution = 10,
		.calibrate = true,
	};

	data->sequence.buffer = &data->sample;
	data->sequence.buffer_size = sizeof(data->sample);
	ret = adc_channel_setup_dt(&config->adc_channel);

	adc_channel_setup(config->adc_channel.dev, &config->adc_channel.channel_cfg);

	return 0;
}

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) > 0,
	     "No compatible CH32V BC1.2 instance found");

#define BC12_WCH_INIT(inst)                                                                        \
	static struct ch32v_bc12_data drv_data_##inst;                                             \
                                                                                                   \
	static const struct ch32v_bc12_config drv_config_##inst = {                                \
		.dp_gpio = GPIO_DT_SPEC_INST_GET(inst, udp_gpios),                                 \
		.dm_gpio = GPIO_DT_SPEC_INST_GET(inst, udm_gpios),                                 \
		.output_ohm = DT_INST_PROP(inst, output_ohms),				           \
		.full_ohm = DT_INST_PROP_OR(inst, full_ohms, 0),			           \
		.adc_channel = ADC_DT_SPEC_INST_GET(inst),				           \
	};                                                                                         \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(inst,                                                                \
			      ch32v_bc12_init,                                                     \
			      NULL,                                                                \
			      &drv_data_##inst,                                                    \
			      &drv_config_##inst,                                                  \
			      POST_KERNEL,                                                         \
			      USB_BC12_INIT_PRIORITY,                                              \
			      &ch32v_bc12_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BC12_WCH_INIT)
