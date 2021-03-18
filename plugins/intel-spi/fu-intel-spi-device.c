/*
 * Copyright (C) 2021 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2+
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <gio/gunixinputstream.h>

#include "fu-intel-spi-common.h"
#include "fu-intel-spi-device.h"

#include "fu-ifd-common.h"
#include "fu-ifd-device.h"
#include "fu-ifd-firmware.h"

struct _FuIntelSpiDevice {
	FuDevice		 parent_instance;
	FuIntelSpiKind		 kind;
	guint32			 phys_spibar;
	gpointer		 spibar;
	guint16			 hsfs;
	guint16			 frap;
	guint32			 freg[4];
	guint32			 flvalsig;
	guint32			 descriptor_map0;
	guint32			 descriptor_map1;
	guint32			 descriptor_map2;
	guint32			 components_rcd;
	guint32			 illegal_jedec;
	guint32			 flpb;
	guint32			 flash_master[4];
};

#define FU_INTEL_SPI_PHYS_SPIBAR_SIZE		0x10000	/* bytes */
#define FU_INTEL_SPI_READ_TIMEOUT		10	/* ms */

G_DEFINE_TYPE (FuIntelSpiDevice, fu_intel_spi_device, FU_TYPE_DEVICE)

static void
fu_intel_spi_device_to_string (FuDevice *device, guint idt, GString *str)
{
	FuIntelSpiDevice *self = FU_INTEL_SPI_DEVICE (device);
	fu_common_string_append_kv (str, idt, "Kind",
				    fu_intel_spi_kind_to_string (self->kind));
	fu_common_string_append_kx (str, idt, "SPIBAR", self->phys_spibar);
	fu_common_string_append_kx (str, idt, "HSFS", self->hsfs);
	fu_common_string_append_kx (str, idt, "FRAP", self->frap);
	for (guint i = 0; i < 4; i++) {
		g_autofree gchar *title = g_strdup_printf ("FREG%u", i);
		fu_common_string_append_kx (str, idt, title, self->freg[i]);
	}
	for (guint i = 0; i < 4; i++) {
		g_autofree gchar *title = g_strdup_printf ("FLMSTR%u", i);
		fu_common_string_append_kx (str, idt, title, self->flash_master[i]);
	}
	fu_common_string_append_kx (str, idt, "FLVALSIG", self->flvalsig);
	fu_common_string_append_kx (str, idt, "FLMAP0", self->descriptor_map0);
	fu_common_string_append_kx (str, idt, "FLMAP1", self->descriptor_map1);
	fu_common_string_append_kx (str, idt, "FLMAP2", self->descriptor_map2);
	fu_common_string_append_kx (str, idt, "FLCOMP", self->components_rcd);
	fu_common_string_append_kx (str, idt, "FLILL", self->illegal_jedec);
	fu_common_string_append_kx (str, idt, "FLPB", self->flpb);
}

static gboolean
fu_intel_spi_device_open (FuDevice *device, GError **error)
{
	FuIntelSpiDevice *self = FU_INTEL_SPI_DEVICE (device);
	int fd;
	g_autoptr(GInputStream) istr = NULL;

	/* this will fail if the kernel is locked down */
	fd = open ("/dev/mem", O_SYNC | O_RDWR);
	if (fd == -1) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "failed to open /dev/mem: %s",
			     strerror (errno));
		return FALSE;
	}
	istr = g_unix_input_stream_new (fd, TRUE);
	if (istr == NULL) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_FAILED,
				     "failed to create input stream");
		return FALSE;
	}
	self->spibar = mmap (NULL, FU_INTEL_SPI_PHYS_SPIBAR_SIZE,
			     PROT_READ | PROT_WRITE,
			     MAP_SHARED, fd,
			     self->phys_spibar);
	if (self->spibar == MAP_FAILED) {
		g_set_error (error,
			     G_IO_ERROR,
			     G_IO_ERROR_FAILED,
			     "failed to open mmap SPIBAR: %s",
			     strerror (errno));
		return FALSE;
	}

	/* success */
	return TRUE;
}

static gboolean
fu_intel_spi_device_close (FuDevice *device, GError **error)
{
	FuIntelSpiDevice *self = FU_INTEL_SPI_DEVICE (device);

	/* close */
	if (self->spibar != NULL) {
		if (munmap (self->spibar, FU_INTEL_SPI_PHYS_SPIBAR_SIZE) == -1) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_FAILED,
				     "failed to unmap spibar: %s",
				     strerror (errno));
			return FALSE;
		}
		self->spibar = NULL;
	}

	/* success */
	return TRUE;
}

static guint32
fu_intel_spi_device_read_reg (FuIntelSpiDevice *self, guint8 section, guint16 offset)
{
	guint32 control = 0;
	control |= (((guint32) section) << 12) & FDOC_FDSS;
	control |= (((guint32) offset) << 2) & FDOC_FDSI;
	fu_mmio_write32_le (self->spibar, PCH100_REG_FDOC, control);
	return fu_mmio_read32_le (self->spibar, PCH100_REG_FDOD);
}

static gboolean
fu_intel_spi_device_probe (FuDevice *device, GError **error)
{
	FuIntelSpiDevice *self = FU_INTEL_SPI_DEVICE (device);

	/* verify this was set in the quirk file */
	if (self->kind == FU_INTEL_SPI_KIND_UNKNOWN) {
		g_set_error_literal (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_SUPPORTED,
				     "IntelSpiKind not set");
		return FALSE;
	}

	/* success */
	return TRUE;
}

static gboolean
fu_intel_spi_device_setup (FuDevice *device, GError **error)
{
	FuIntelSpiDevice *self = FU_INTEL_SPI_DEVICE (device);
	guint64 total_size = 0;
	guint8 comp1_density;
	guint8 comp2_density;

	/* dump everything */
	if (g_getenv ("FWUPD_INTEL_SPI_VERBOSE") != NULL) {
		for (guint i = 0; i < 0xff; i += 4) {
			guint32 tmp = fu_mmio_read32 (self->spibar, i);
			g_print ("SPIBAR[0x%02x] = 0x%x\n", i, tmp);
		}
	}

	/* read from descriptor */
	self->hsfs = fu_mmio_read16 (self->spibar, ICH9_REG_HSFS);
	self->frap = fu_mmio_read16 (self->spibar, ICH9_REG_FRAP);
	for (guint i = FU_IFD_REGION_DESC; i < 4; i++)
		self->freg[i] = fu_mmio_read32 (self->spibar, ICH9_REG_FREG0 + i * 4);
	self->flvalsig = fu_intel_spi_device_read_reg (self, 0, 0);
	self->descriptor_map0 = fu_intel_spi_device_read_reg (self, 0, 1);
	self->descriptor_map1 = fu_intel_spi_device_read_reg (self, 0, 2);
	self->descriptor_map2 = fu_intel_spi_device_read_reg (self, 0, 3);
	self->components_rcd = fu_intel_spi_device_read_reg (self, 1, 0);
	self->illegal_jedec = fu_intel_spi_device_read_reg (self, 1, 1);
	self->flpb = fu_intel_spi_device_read_reg (self, 1, 2);

	for (guint i = 0; i < 4; i++)
		self->flash_master[i] = fu_intel_spi_device_read_reg (self, 3, i);

	/* set size */
	comp1_density = (self->components_rcd & 0x0f) >> 0;
	if (comp1_density != 0xf)
		total_size += 1 << (19 + comp1_density);
	comp2_density = (self->components_rcd & 0xf0) >> 4;
	if (comp2_density != 0xf)
		total_size += 1 << (19 + comp2_density);
	fu_device_set_firmware_size (device, total_size);

	/* add children */
	for (guint i = FU_IFD_REGION_BIOS; i < 4; i++) {
		g_autoptr(FuDevice) child = NULL;
		if (self->freg[i] == 0x0)
			continue;
		child = fu_ifd_device_new (i, self->freg[i]);
		for (guint j = 1; j < 4; j++) {
			FuIfdAccess access;
			access = fu_ifd_region_to_access (i, self->flash_master[j-1], TRUE);
			fu_ifd_device_set_access (FU_IFD_DEVICE (child), j, access);
		}
		fu_device_add_child (device, child);
	}

	return TRUE;
}

static gboolean
fu_intel_spi_device_wait (FuIntelSpiDevice *self, guint timeout_ms, GError **error)
{
	g_usleep (1);
	for (guint i = 0; i < timeout_ms * 100; i++) {
		guint16 hsfs = fu_mmio_read16 (self->spibar, ICH9_REG_HSFS);
		if (hsfs & HSFS_FDONE)
			return TRUE;
		if (hsfs & HSFS_FCERR) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_FAILED,
				     "HSFS transaction error");
			return FALSE;
		}
		g_usleep (10);
	}
	g_set_error (error,
		     G_IO_ERROR,
		     G_IO_ERROR_TIMED_OUT,
		     "HSFS timed out");
	return FALSE;
}

static void
fu_intel_spi_device_set_addr (FuIntelSpiDevice *self, guint32 addr)
{
	guint32 addr_old = fu_mmio_read32 (self->spibar, ICH9_REG_FADDR) & ~PCH100_FADDR_FLA;
	fu_mmio_write32 (self->spibar, ICH9_REG_FADDR, (addr & PCH100_FADDR_FLA) | addr_old);
}

GBytes *
fu_intel_spi_device_dump (FuIntelSpiDevice *self,
			  FuDevice *device,
			  guint32 offset,
			  guint32 length,
			  GError **error)
{
	guint8 block_len = 0x40;
	g_autoptr(GByteArray) buf = g_byte_array_sized_new (length);

	/* set FDONE, FCERR, AEL */
	fu_device_set_status (device, FWUPD_STATUS_DEVICE_READ);
	fu_mmio_write16 (self->spibar, ICH9_REG_HSFS,
			 fu_mmio_read16 (self->spibar, ICH9_REG_HSFS));
	for (guint32 addr = offset; addr < offset + length; addr += block_len) {
		guint16 hsfc;
		guint32 buftmp32 = 0;

		/* set up read */
		fu_intel_spi_device_set_addr (self, addr);
		hsfc = fu_mmio_read16 (self->spibar, ICH9_REG_HSFC);
		hsfc &= ~PCH100_HSFC_FCYCLE;
		hsfc &= ~HSFC_FDBC;

		/* set byte count */
		hsfc |= ((block_len - 1) << 8) & HSFC_FDBC;
		hsfc |= HSFC_FGO;
		fu_mmio_write16 (self->spibar, ICH9_REG_HSFC, hsfc);
		if (!fu_intel_spi_device_wait (self, FU_INTEL_SPI_READ_TIMEOUT, error)) {
			g_prefix_error (error, "failed @0x%x: ", addr);
			return NULL;
		}

		/* copy out data */
		for (guint i = 0; i < block_len; i++) {
			if (i % 4 == 0)
				buftmp32 = fu_mmio_read32 (self->spibar, ICH9_REG_FDATA0 + i);
			fu_byte_array_append_uint8 (buf, buftmp32 >> ((i % 4) * 8));
		}

		/* progress */
		fu_device_set_progress_full (device, addr - offset + block_len, length);
	}

	/* success */
	return g_byte_array_free_to_bytes (g_steal_pointer (&buf));
}

static GBytes *
fu_intel_spi_device_dump_firmware (FuDevice *device, GError **error)
{
	FuIntelSpiDevice *self = FU_INTEL_SPI_DEVICE (device);
	guint64 total_size = fu_device_get_firmware_size_max (device);
	return fu_intel_spi_device_dump (self, device,
						       0x0, total_size,
						       error);
}

static FuFirmware *
fu_intel_spi_device_read_firmware (FuDevice *device, GError **error)
{
	g_autoptr(FuFirmware) firmware = fu_ifd_firmware_new ();
	g_autoptr(GBytes) blob = NULL;

	blob = fu_intel_spi_device_dump_firmware (device, error);
	if (blob == NULL)
		return NULL;
	if (!fu_firmware_parse (firmware, blob, FWUPD_INSTALL_FLAG_NONE, error))
		return NULL;
	return g_steal_pointer (&firmware);
}

static gboolean
fu_intel_spi_device_set_quirk_kv (FuDevice *device,
				  const gchar *key,
				  const gchar *value,
				  GError **error)
{
	FuIntelSpiDevice *self = FU_INTEL_SPI_DEVICE (device);
	if (g_strcmp0 (key, "IntelSpiBar") == 0) {
		guint64 tmp = fu_common_strtoull (value);
		self->phys_spibar = tmp;
		return TRUE;
	}
	if (g_strcmp0 (key, "IntelSpiKind") == 0) {
		self->kind = fu_intel_spi_kind_from_string (value);
		if (self->kind == FU_INTEL_SPI_KIND_UNKNOWN) {
			g_set_error (error,
				     G_IO_ERROR,
				     G_IO_ERROR_NOT_SUPPORTED,
				     "%s not supported",
				     value);
			return FALSE;
		}
		return TRUE;
	}
	g_set_error_literal (error,
			     G_IO_ERROR,
			     G_IO_ERROR_NOT_SUPPORTED,
			     "no supported");
	return FALSE;
}

static void
fu_intel_spi_device_init (FuIntelSpiDevice *self)
{
	fu_device_add_flag (FU_DEVICE (self), FWUPD_DEVICE_FLAG_INTERNAL);
	fu_device_add_flag (FU_DEVICE (self), FWUPD_DEVICE_FLAG_CAN_VERIFY_IMAGE);
	fu_device_add_icon (FU_DEVICE (self), "computer");
	fu_device_set_physical_id (FU_DEVICE (self), "intel_spi");
}

static void
fu_intel_spi_device_class_init (FuIntelSpiDeviceClass *klass)
{
	FuDeviceClass *klass_device = FU_DEVICE_CLASS (klass);
	klass_device->to_string = fu_intel_spi_device_to_string;
	klass_device->probe = fu_intel_spi_device_probe;
	klass_device->setup = fu_intel_spi_device_setup;
	klass_device->dump_firmware = fu_intel_spi_device_dump_firmware;
	klass_device->read_firmware = fu_intel_spi_device_read_firmware;
	klass_device->open = fu_intel_spi_device_open;
	klass_device->close = fu_intel_spi_device_close;
	klass_device->set_quirk_kv = fu_intel_spi_device_set_quirk_kv;
}
