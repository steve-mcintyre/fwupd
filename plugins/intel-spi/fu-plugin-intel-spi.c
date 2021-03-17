/*
 * Copyright (C) 2021 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include "fu-plugin-vfuncs.h"

#include "fu-intel-spi-device.h"

#include "fu-ifd-firmware.h"
#include "fu-ifd-bios.h"

#include "fu-efi-firmware-file.h"
#include "fu-efi-firmware-filesystem.h"
#include "fu-efi-firmware-section.h"
#include "fu-efi-firmware-volume.h"

void
fu_plugin_init (FuPlugin *plugin)
{
	fu_plugin_set_build_hash (plugin, FU_BUILD_HASH);
	fu_plugin_add_firmware_gtype (plugin, NULL, FU_TYPE_EFI_FIRMWARE_FILE);
	fu_plugin_add_firmware_gtype (plugin, NULL, FU_TYPE_EFI_FIRMWARE_FILESYSTEM);
	fu_plugin_add_firmware_gtype (plugin, NULL, FU_TYPE_EFI_FIRMWARE_SECTION);
	fu_plugin_add_firmware_gtype (plugin, NULL, FU_TYPE_EFI_FIRMWARE_VOLUME);
	fu_plugin_add_firmware_gtype (plugin, NULL, FU_TYPE_IFD_BIOS);
	fu_plugin_add_firmware_gtype (plugin, NULL, FU_TYPE_IFD_FIRMWARE);
	fu_plugin_add_udev_subsystem (plugin, "pci");
	fu_plugin_add_possible_quirk_key (plugin, "IntelSpiBar");
	fu_plugin_add_possible_quirk_key (plugin, "IntelSpiKind");
	fu_plugin_set_device_gtype (plugin, FU_TYPE_INTEL_SPI_DEVICE);
}
