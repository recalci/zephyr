/*
 * Copyright (c) 2025 Jianxiong Gu <jianxiong.gu@outlook.com>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_utils.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/usb_c/tcpci.h>

#include <zephyr/drivers/mfd/fusb307.h>

#define DT_DRV_COMPAT onnn_fusb307_gpio
LOG_MODULE_REGISTER(gpio_fusb307, CONFIG_GPIO_LOG_LEVEL);

/* Driver config */
struct gpio_fusb307_config {
	/* gpio_driver_config needs to be first */
	struct gpio_driver_config common;
	/* FUSB307 chip device */
	const struct device *mfd_dev;
};

/* Driver data */
struct gpio_fusb307_data {
	/* gpio_driver_data needs to be first */
	struct gpio_driver_data common;
	/* GPIO callback list */
	sys_slist_t cb_list_gpio;
};

/* FUSB307 only has GPIO1, GPIO2 */
enum fusb307_gpio_pin {
	FUSB307_GPIO1 = 1U,
	FUSB307_GPIO2,
	FUSB307_GPIO_NUM,
};

static int gpio_fusb307_pin_config(const struct device *dev, enum fusb307_gpio_pin pin,
				   gpio_flags_t flags)
{
	const struct gpio_fusb307_config *const config = dev->config;
	const struct mfd_fusb307_config *parent_cfg =
		(struct mfd_fusb307_config *)(config->mfd_dev->config);
	uint8_t addr;
	uint8_t reg = 0;

	/* Don't support simultaneous in/out mode */
	if ((flags & GPIO_INPUT) && (flags & GPIO_OUTPUT)) {
		return -ENOTSUP;
	}

	/* Don't support "open source" mode */
	if ((flags & GPIO_LINE_OPEN_DRAIN) && !(flags & GPIO_SINGLE_ENDED)) {
		return -ENOTSUP;
	}

	/* Don't support pull-up/pull-down resistors */
	if (flags & (GPIO_PULL_UP | GPIO_PULL_DOWN)) {
		return -ENOTSUP;
	}

	if (pin >= FUSB307_GPIO_NUM) {
		LOG_ERR("Invalid GPIO pin number: %d", pin);
		return -EINVAL;
	}

	if (flags & GPIO_INPUT) {
		/* Set GPIO as input */
		reg |= FUSB307_REG_GPIO_CFG_GPI_EN;
	} else if (flags & GPIO_OUTPUT) {
		/* Set GPIO as output */
		reg |= FUSB307_REG_GPIO_CFG_GPO_EN;

		/* Set init state */
		if (flags & GPIO_OUTPUT_INIT_HIGH) {
			reg |= FUSB307_REG_GPIO_CFG_GPO_VAL;
		}
	}

	switch (pin) {
	case FUSB307_GPIO1:
		addr = FUSB307_REG_GPIO1_CFG;
		break;
	case FUSB307_GPIO2:
		addr = FUSB307_REG_GPIO2_CFG;
		break;
	default:
		return -EINVAL;
	}

	return i2c_reg_write_byte_dt(&parent_cfg->i2c, addr, reg);
}

static int gpio_fusb307_port_get_raw(const struct device *dev, gpio_port_value_t *value)
{
	const struct gpio_fusb307_config *const config = dev->config;
	const struct mfd_fusb307_config *parent_cfg =
		(struct mfd_fusb307_config *)(config->mfd_dev->config);
	uint8_t reg;
	int ret;

	ret = i2c_reg_read_byte_dt(&parent_cfg->i2c, FUSB307_REG_GPIO_STAT, &reg);
	if (ret) {
		LOG_DBG("read %x got %d", *value, ret);
	}

	*value = (reg & (FUSB307_REG_GPIO_STAT_GPI1_VAL | FUSB307_REG_GPIO_STAT_GPI2_VAL)) << 1;

	return ret;
}

static int gpio_fusb307_port_set_masked_raw(const struct device *dev, gpio_port_pins_t mask,
					    gpio_port_value_t value)
{
	const struct gpio_fusb307_config *const config = dev->config;
	const struct mfd_fusb307_config *parent_cfg =
		(struct mfd_fusb307_config *)(config->mfd_dev->config);
	uint8_t addr;
	uint8_t reg;
	int ret;

	for (int pin = FUSB307_GPIO1; pin < FUSB307_GPIO_NUM; pin++) {
		if (!(mask & BIT(pin))) {
			continue;
		}

		switch (pin) {
		case FUSB307_GPIO1:
			addr = FUSB307_REG_GPIO1_CFG;
			break;
		case FUSB307_GPIO2:
			addr = FUSB307_REG_GPIO2_CFG;
			break;
		default:
			return -EINVAL;
		}

		ret = i2c_reg_read_byte_dt(&parent_cfg->i2c, addr, &reg);
		if (ret) {
			return ret;
		}

		if (value & BIT(pin)) {
			reg |= FUSB307_REG_GPIO_CFG_GPO_VAL;
		} else {
			reg &= ~FUSB307_REG_GPIO_CFG_GPO_VAL;
		}
		ret = i2c_reg_write_byte_dt(&parent_cfg->i2c, addr, reg);
		if (ret) {
			return ret;
		}
	}

	return 0;
}

static int gpio_fusb307_port_set_bits_raw(const struct device *dev, gpio_port_pins_t mask)
{
	const struct gpio_fusb307_config *const config = dev->config;
	const struct mfd_fusb307_config *parent_cfg =
		(struct mfd_fusb307_config *)(config->mfd_dev->config);
	uint8_t addr;
	int ret;

	for (int pin = FUSB307_GPIO1; pin < FUSB307_GPIO_NUM; pin++) {
		if (!(mask & BIT(pin))) {
			continue;
		}

		switch (pin) {
		case FUSB307_GPIO1:
			addr = FUSB307_REG_GPIO1_CFG;
			break;
		case FUSB307_GPIO2:
			addr = FUSB307_REG_GPIO2_CFG;
			break;
		default:
			return -EINVAL;
		}

		ret = i2c_reg_update_byte_dt(&parent_cfg->i2c, addr,
					     FUSB307_REG_GPIO_CFG_GPO_VAL,
					     FUSB307_REG_GPIO_CFG_GPO_VAL);
		if (ret) {
			return ret;
		}
	}

	return 0;
}

static int gpio_fusb307_port_clear_bits_raw(const struct device *dev, gpio_port_pins_t mask)
{
	const struct gpio_fusb307_config *const config = dev->config;
	const struct mfd_fusb307_config *parent_cfg =
		(struct mfd_fusb307_config *)(config->mfd_dev->config);
	uint8_t addr;
	int ret;

	for (int pin = FUSB307_GPIO1; pin < FUSB307_GPIO_NUM; pin++) {
		if (!(mask & BIT(pin))) {
			continue;
		}

		switch (pin) {
		case FUSB307_GPIO1:
			addr = FUSB307_REG_GPIO1_CFG;
			break;
		case FUSB307_GPIO2:
			addr = FUSB307_REG_GPIO2_CFG;
			break;
		default:
			return -EINVAL;
		}

		ret = i2c_reg_update_byte_dt(&parent_cfg->i2c, addr,
					     FUSB307_REG_GPIO_CFG_GPO_VAL, 0);
		if (ret) {
			return ret;
		}
	}

	return 0;
}

static int gpio_fusb307_port_toggle_bits(const struct device *dev, gpio_port_pins_t mask)
{
	const struct gpio_fusb307_config *const config = dev->config;
	const struct mfd_fusb307_config *parent_cfg =
		(struct mfd_fusb307_config *)(config->mfd_dev->config);
	uint8_t addr;
	uint8_t reg;
	int ret;

	for (int pin = FUSB307_GPIO1; pin < FUSB307_GPIO_NUM; pin++) {
		if (!(mask & BIT(pin))) {
			continue;
		}

		switch (pin) {
		case FUSB307_GPIO1:
			addr = FUSB307_REG_GPIO1_CFG;
			break;
		case FUSB307_GPIO2:
			addr = FUSB307_REG_GPIO2_CFG;
			break;
		default:
			return -EINVAL;
		}

		ret = i2c_reg_read_byte_dt(&parent_cfg->i2c, addr, &reg);
		if (ret) {
			return ret;
		}

		reg = reg ^ FUSB307_REG_GPIO_CFG_GPO_VAL;
		ret = i2c_reg_write_byte_dt(&parent_cfg->i2c, addr, reg);
		if (ret) {
			return ret;
		}
	}

	return 0;
}

static int gpio_fusb307_pin_interrupt_configure(const struct device *dev, enum fusb307_gpio_pin pin,
						enum gpio_int_mode mode, enum gpio_int_trig trig)
{
	const struct gpio_fusb307_config *const config = dev->config;
	const struct mfd_fusb307_config *parent_cfg =
		(struct mfd_fusb307_config *)(config->mfd_dev->config);
	uint8_t reg_int, reg_mask = 0;
	uint8_t mask;
	uint16_t alert_mask;
	int ret;

	switch (pin) {
	case FUSB307_GPIO1:
		mask = FUSB307_REG_ALERT_VD_MASK_GPI1;
		reg_int = FUSB307_REG_ALERT_VD_GPI1;
		break;
	case FUSB307_GPIO2:
		mask = FUSB307_REG_ALERT_VD_MASK_GPI2;
		reg_int = FUSB307_REG_ALERT_VD_GPI2;
		break;
	default:
		LOG_ERR("Invalid pin");
		return -EINVAL;
	}

	/* Device does not support level interrupts */
	if (mode == GPIO_INT_MODE_LEVEL) {
		return -ENOTSUP;
	}

	/* Only support edge interrupts */
	if (trig != GPIO_INT_TRIG_BOTH) {
		return -EINVAL;
	}

	ret = i2c_reg_read_byte_dt(&parent_cfg->i2c, FUSB307_REG_ALERT_VD_MASK, &reg_mask);
	if (ret) {
		return ret;
	}

	if (mode == GPIO_INT_MODE_DISABLED) {
		reg_mask &= ~mask;
	} else if (mode == GPIO_INT_MODE_EDGE) {
		reg_mask |= mask;

		/* Enable Vendor Defined Alert for GPIO interrupts */
		ret = i2c_burst_read_dt(&parent_cfg->i2c, TCPC_REG_ALERT_MASK,
					(uint8_t *)&alert_mask, sizeof(alert_mask));
		if (ret) {
			return ret;
		}

		if (!(alert_mask & TCPC_REG_ALERT_VENDOR_DEF)) {
			alert_mask |= TCPC_REG_ALERT_VENDOR_DEF;

			ret = i2c_burst_write_dt(&parent_cfg->i2c, TCPC_REG_ALERT_MASK,
						(uint8_t *)&alert_mask, sizeof(alert_mask));
			if (ret) {
				return ret;
			}
		}

		/* Clear pending interrupts  */
		ret = i2c_reg_write_byte_dt(&parent_cfg->i2c, FUSB307_REG_ALERT_VD, reg_int);
		if (ret) {
			return ret;
		}
	}

	return i2c_reg_write_byte_dt(&parent_cfg->i2c, FUSB307_REG_ALERT_VD_MASK, reg_mask);
}

static int gpio_fusb307_manage_callback(const struct device *dev, struct gpio_callback *callback,
					bool set)
{
	struct gpio_fusb307_data *const data = dev->data;

	return gpio_manage_callback(&data->cb_list_gpio, callback, set);
}

int fusb307_gpio_alert_handler(const struct device *dev)
{
	const struct gpio_fusb307_config *const config = dev->config;
	const struct mfd_fusb307_config *parent_cfg =
		(struct mfd_fusb307_config *)(config->mfd_dev->config);
	struct gpio_fusb307_data *const data = dev->data;
	uint8_t alert_vd_reg;
	uint8_t alert = 0;
	int ret;

	/* Read GPIO interrupt status */
	ret = i2c_reg_read_byte_dt(&parent_cfg->i2c, FUSB307_REG_ALERT_VD, &alert_vd_reg);
	if (ret) {
		return ret;
	}

	if (alert_vd_reg & FUSB307_REG_ALERT_VD_GPI1) {
		alert |= FUSB307_REG_ALERT_VD_GPI1;
		gpio_fire_callbacks(&data->cb_list_gpio, dev, FUSB307_GPIO1);
	}

	if (alert_vd_reg & FUSB307_REG_ALERT_VD_GPI2) {
		alert |= FUSB307_REG_ALERT_VD_GPI2;
		gpio_fire_callbacks(&data->cb_list_gpio, dev, FUSB307_GPIO2);
	}

	/* Clear GPIO interrupts */
	if (alert) {
		ret = i2c_reg_write_byte_dt(&parent_cfg->i2c, FUSB307_REG_ALERT_VD, alert);
	}

	return ret;
}

static const struct gpio_driver_api gpio_fusb307_driver_api = {
	.pin_configure		 = gpio_fusb307_pin_config,
	.port_get_raw		 = gpio_fusb307_port_get_raw,
	.port_set_masked_raw	 = gpio_fusb307_port_set_masked_raw,
	.port_set_bits_raw	 = gpio_fusb307_port_set_bits_raw,
	.port_clear_bits_raw	 = gpio_fusb307_port_clear_bits_raw,
	.port_toggle_bits	 = gpio_fusb307_port_toggle_bits,
	.pin_interrupt_configure = gpio_fusb307_pin_interrupt_configure,
	.manage_callback	 = gpio_fusb307_manage_callback,
};

static int gpio_fusb307_init(const struct device *dev)
{
	const struct gpio_fusb307_config *const config = dev->config;
	struct mfd_fusb307_data *parent_data = config->mfd_dev->data;
	const struct mfd_fusb307_config *parent_cfg =
		(struct mfd_fusb307_config *)(config->mfd_dev->config);
	uint8_t addr;
	int ret;

	if (!device_is_ready(config->mfd_dev)) {
		LOG_ERR("%s: parent dev not ready", dev->name);
		return -ENODEV;
	}

	for (int pin = FUSB307_GPIO1; pin < FUSB307_GPIO_NUM; pin++) {
		switch (pin) {
		case FUSB307_GPIO1:
			addr = FUSB307_REG_GPIO1_CFG;
			break;
		case FUSB307_GPIO2:
			addr = FUSB307_REG_GPIO2_CFG;
			break;
		default:
			ret = -EINVAL;
			goto out;
		}

		/* Disable Input and Output */
		ret = i2c_reg_update_byte_dt(&parent_cfg->i2c, addr,
				FUSB307_REG_GPIO_CFG_GPI_EN | FUSB307_REG_GPIO_CFG_GPO_EN, 0);
		if (ret) {
			goto out;
		}
	}

out:
	if (ret) {
		LOG_ERR("%s init failed: %d", dev->name, ret);
	} else {
		LOG_INF("%s init ok", dev->name);
		parent_data->child.gpio_dev = dev;
	}
	return ret;
}

#define GPIO_FUSB307_INIT(inst)                                                                    \
	static const struct gpio_fusb307_config gpio_fusb307_cfg_##inst = {                        \
		.common = {                                                                        \
			.port_pin_mask = GPIO_DT_INST_PORT_PIN_MASK_NGPIOS_EXC(inst,               \
						DT_INST_PROP(inst, ngpios))                        \
		},                                                                                 \
		.mfd_dev = DEVICE_DT_GET(DT_INST_PARENT(inst)),                                    \
	};                                                                                         \
	static struct gpio_fusb307_data gpio_fusb307_data_##inst;                                  \
	DEVICE_DT_INST_DEFINE(inst, gpio_fusb307_init, NULL, &gpio_fusb307_data_##inst,            \
			      &gpio_fusb307_cfg_##inst, POST_KERNEL,                               \
			      CONFIG_GPIO_FUSB307_INIT_PRIORITY, &gpio_fusb307_driver_api);

DT_INST_FOREACH_STATUS_OKAY(GPIO_FUSB307_INIT)
