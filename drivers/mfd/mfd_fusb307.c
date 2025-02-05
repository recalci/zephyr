/*
 * Copyright (c) 2025 Jianxiong Gu <jianxiong.gu@outlook.com>
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT onnn_fusb307

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/usb_c/tcpci.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(mfd_fusb307, CONFIG_MFD_LOG_LEVEL);

#include <zephyr/drivers/mfd/fusb307.h>

static void fusb307_alert_worker(struct k_work *work)
{
	struct mfd_fusb307_data *const data = CONTAINER_OF(work, struct mfd_fusb307_data,
							   alert_worker);
	const struct device *const dev = data->dev;
	const struct mfd_fusb307_config *config = dev->config;
	uint16_t alert_reg = 0;
	uint16_t processed_alert = 0;
	int ret;

	/* Read alert register */
	ret =  i2c_burst_read_dt(&config->i2c, TCPC_REG_ALERT,
				 (uint8_t *)&alert_reg, sizeof(alert_reg));
	if (ret) {
		LOG_ERR("failed to read alert register");
		goto out;
	}

	if (alert_reg != 0) {
		LOG_DBG("alert: 0x%04x", alert_reg);

#ifdef CONFIG_GPIO_FUSB307
		/* TBD */
#endif /* CONFIG_GPIO_FUSB307 */

#ifdef CONFIG_USBC_TCPC_FUSB307
		/* TBD */
#endif /* CONFIG_USBC_TCPC_FUSB307 */

		/* Clear alert register */
		if (processed_alert != 0) {
			i2c_burst_write_dt(&config->i2c, TCPC_REG_ALERT,
					   (uint8_t *)&processed_alert, sizeof(processed_alert));
		}

		i2c_burst_read_dt(&config->i2c, TCPC_REG_ALERT,
				  (uint8_t *)&alert_reg, sizeof(alert_reg));
	}

out:
	/* If alert_reg is not 0 or the interrupt signal is still active */
	if (alert_reg != 0 || gpio_pin_get_dt(&config->irq_gpio)) {
		k_work_submit(work);
	}
}

static void fusb307_alert_callback(const struct device *dev, struct gpio_callback *cb,
				   uint32_t pins)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(pins);

	struct mfd_fusb307_data *data = CONTAINER_OF(cb, struct mfd_fusb307_data, gpio_cb);

	k_work_submit(&data->alert_worker);
}

static int mfd_fusb307_init(const struct device *dev)
{
	const struct mfd_fusb307_config *config = dev->config;
	struct mfd_fusb307_data *data = dev->data;
	uint16_t reg;
	int ret;

	/* Check I2C is ready */
	if (!device_is_ready(config->i2c.bus)) {
		LOG_ERR("%s device not ready", config->i2c.bus->name);
		return -ENODEV;
	}

	/* Reset FUSB307 */
	ret = i2c_reg_write_byte_dt(&config->i2c, FUSB307_REG_RESET, FUSB307_REG_RESET_SW_RST);
	if (ret < 0) {
		return ret;
	}

	k_msleep(1);

	/* Mask all alert, INT will stop pulling down */
	reg = 0;
	if (i2c_burst_write_dt(&config->i2c, TCPC_REG_ALERT_MASK, (uint8_t *)&reg, sizeof(reg)) ||
	    i2c_reg_write_byte_dt(&config->i2c, TCPC_REG_POWER_STATUS_MASK, (uint8_t)reg) ||
	    i2c_reg_write_byte_dt(&config->i2c, TCPC_REG_FAULT_STATUS_MASK, (uint8_t)reg) ||
	    i2c_reg_write_byte_dt(&config->i2c, FUSB307_REG_ALERT_VD_MASK, (uint8_t)reg)) {
		LOG_ERR("failed to mask all alert");
		return -EIO;
	}

	/* Clear all alert */
	reg = 0xFFFF;
	if (i2c_burst_write_dt(&config->i2c, TCPC_REG_ALERT, (uint8_t *)&reg, sizeof(reg)) ||
	    i2c_reg_write_byte_dt(&config->i2c, TCPC_REG_FAULT_STATUS, (uint8_t)reg) ||
	    i2c_reg_write_byte_dt(&config->i2c, FUSB307_REG_ALERT_VD, (uint8_t)reg)) {
		LOG_ERR("failed to clear all alert");
		return -EIO;
	}

	if (!gpio_is_ready_dt(&config->irq_gpio)) {
		LOG_ERR("%s device not ready", config->irq_gpio.port->name);
		return -ENODEV;
	}

	/* Set the interrupt pin for handling the alert */
	k_work_init(&data->alert_worker, fusb307_alert_worker);

	gpio_pin_configure_dt(&config->irq_gpio, GPIO_INPUT);

	gpio_pin_interrupt_configure_dt(&config->irq_gpio, GPIO_INT_EDGE_TO_ACTIVE);

	gpio_init_callback(&data->gpio_cb, fusb307_alert_callback, BIT(config->irq_gpio.pin));

	ret = gpio_add_callback(config->irq_gpio.port, &data->gpio_cb);
	if (ret) {
		LOG_ERR("failed to add GPIO callback");
		return  ret;
	}

	LOG_INF("initialized");

	return 0;
}

#define MFD_FUSB307_INIT(inst)                                                                    \
	static const struct mfd_fusb307_config mfd_fusb307_config_##inst = {                      \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                                                \
		.irq_gpio = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),                               \
	};                                                                                        \
	static struct mfd_fusb307_data mfd_fusb307_data_##inst = {                                \
		.dev = DEVICE_DT_INST_GET(inst),                                                  \
	};                                                                                        \
                                                                                                  \
	DEVICE_DT_INST_DEFINE(inst,&mfd_fusb307_init, NULL, &mfd_fusb307_data_##inst,             \
			      &mfd_fusb307_config_##inst, POST_KERNEL,                            \
			      CONFIG_MFD_FUSB307_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(MFD_FUSB307_INIT);
