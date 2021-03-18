/*
 * Copyright (C) 2021 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include <config.h>

#include "fu-ifd-common.h"

/**
 * fu_ifd_region_to_string:
 * @region: A #FuIfdRegion, e.g. %FU_IFD_REGION_BIOS
 *
 * Converts a #FuIfdRegion to a string.
 *
 * Return value: identifier string
 *
 * Since: 1.6.0
 **/
const gchar *
fu_ifd_region_to_string (FuIfdRegion region)
{
	if (region == FU_IFD_REGION_DESC)
		return "desc";
	if (region == FU_IFD_REGION_BIOS)
		return "bios";
	if (region == FU_IFD_REGION_ME)
		return "me";
	if (region == FU_IFD_REGION_GBE)
		return "gbe";
	if (region == FU_IFD_REGION_PLATFORM)
		return "platform";
	if (region == FU_IFD_REGION_DEVEXP)
		return "devexp";
	if (region == FU_IFD_REGION_BIOS2)
		return "bios2";
	if (region == FU_IFD_REGION_EC)
		return "ec";
	if (region == FU_IFD_REGION_IE)
		return "ie";
	if (region == FU_IFD_REGION_10GBE)
		return "10gbe";
	return NULL;
}

/**
 * fu_ifd_region_to_name:
 * @region: A #FuIfdRegion, e.g. %FU_IFD_REGION_BIOS
 *
 * Converts a #FuIfdRegion to a name the user might recognize.
 *
 * Return value: identifier string
 *
 * Since: 1.6.0
 **/
const gchar *
fu_ifd_region_to_name (FuIfdRegion region)
{
	if (region == FU_IFD_REGION_DESC)
		return "IFD descriptor region";
	if (region == FU_IFD_REGION_BIOS)
		return "BIOS";
	if (region == FU_IFD_REGION_ME)
		return "Intel Management Engine";
	if (region == FU_IFD_REGION_GBE)
		return "Gigabit Ethernet";
	if (region == FU_IFD_REGION_PLATFORM)
		return "Platform firmware";
	if (region == FU_IFD_REGION_DEVEXP)
		return "Device Firmware";
	if (region == FU_IFD_REGION_BIOS2)
		return "BIOS Backup";
	if (region == FU_IFD_REGION_EC)
		return "Embedded Controller";
	if (region == FU_IFD_REGION_IE)
		return "Innovation Engine";
	if (region == FU_IFD_REGION_10GBE)
		return "10 Gigabit Ethernet";
	return NULL;
}

/**
 * fu_ifd_access_to_string:
 * @access: A #FuIfdAccess, e.g. %FU_IFD_ACCESS_READ
 *
 * Converts a #FuIfdAccess to a string.
 *
 * Return value: identifier string
 *
 * Since: 1.6.0
 **/
const gchar *
fu_ifd_access_to_string (FuIfdAccess access)
{
	if (access == FU_IFD_ACCESS_READ)
		return "ro";
	if (access == FU_IFD_ACCESS_WRITE)
		return "wr";
	if (access == (FU_IFD_ACCESS_READ | FU_IFD_ACCESS_WRITE))
		return "rw";
	return NULL;
}
