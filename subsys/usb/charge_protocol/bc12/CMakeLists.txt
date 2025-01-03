# Copyright (c) 2024 Jianxiong Gu
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

# Portable Device
zephyr_library_sources_ifdef(
	CONFIG_USB_BC12_PD_ONLY
	bc12_pd.c
)

# Not support Charging Port(include CDP, DCP, SDP) for now
