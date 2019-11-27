/*
 * Copyright (C) 2010-2017 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <gio/gio.h>

#include <limits.h>
#include <stdlib.h>

#include "fu-test.h"
#include "fu-common.h"

static GMainLoop *_test_loop = NULL;
static guint _test_loop_timeout_id = 0;

static gboolean
fu_test_hang_check_cb (gpointer user_data)
{
	g_main_loop_quit (_test_loop);
	_test_loop_timeout_id = 0;
	return G_SOURCE_REMOVE;
}

/**
 * fu_test_loop_run_with_timeout:
 * @timeout_ms: The timeout in milliseconds
 *
 * Quits the test loop after a timeout
 *
 * Since: 0.9.1
 **/
void
fu_test_loop_run_with_timeout (guint timeout_ms)
{
	g_assert (_test_loop_timeout_id == 0);
	g_assert (_test_loop == NULL);
	_test_loop = g_main_loop_new (NULL, FALSE);
	_test_loop_timeout_id = g_timeout_add (timeout_ms, fu_test_hang_check_cb, NULL);
	g_main_loop_run (_test_loop);
}

/**
 * fu_test_loop_quit:
 *
 * Quits the test loop
 *
 * Since: 0.9.1
 **/
void
fu_test_loop_quit (void)
{
	if (_test_loop_timeout_id > 0) {
		g_source_remove (_test_loop_timeout_id);
		_test_loop_timeout_id = 0;
	}
	if (_test_loop != NULL) {
		g_main_loop_quit (_test_loop);
		g_main_loop_unref (_test_loop);
		_test_loop = NULL;
	}
}

/**
 * fu_test_get_filename:
 * @testdatadirs: semicolon delimitted list of directories
 * @filename: the filename to look for
 *
 * Returns the first path that matches filename in testdatadirs
 *
 * Returns: (transfer full): full path to file or NULL
 *
 * Since: 0.9.1
 **/
gchar *
fu_test_get_filename (const gchar *testdatadirs, const gchar *filename)
{
	g_auto(GStrv) split = g_strsplit (testdatadirs, ":", -1);
	for (guint i = 0; split[i] != NULL; i++) {
		g_autofree gchar *tmp = NULL;
		g_autofree gchar *path = NULL;
		path = g_build_filename (split[i], filename, NULL);
		tmp = fu_common_realpath (path, NULL);
		if (tmp != NULL)
			return g_steal_pointer (&tmp);
	}
	return NULL;
}

/**
 * fu_test_compare_lines:
 * @txt1: First line to compare
 * @txt2: second line to compare
 * @error: A #GError or #NULL
 *
 * Compare two lines.
 *
 * Returns: #TRUE if identical, #FALSE if not (diff is set in error)
 *
 * Since: 1.0.4
 **/
gboolean
fu_test_compare_lines (const gchar *txt1, const gchar *txt2, GError **error)
{
	g_autofree gchar *output = NULL;

	/* exactly the same */
	if (g_strcmp0 (txt1, txt2) == 0)
		return TRUE;

	/* matches a pattern */
	if (fu_common_fnmatch (txt2, txt1))
		return TRUE;

	/* save temp files and diff them */
	if (!g_file_set_contents ("/tmp/a", txt1, -1, error))
		return FALSE;
	if (!g_file_set_contents ("/tmp/b", txt2, -1, error))
		return FALSE;
	if (!g_spawn_command_line_sync ("diff -urNp /tmp/b /tmp/a",
					&output, NULL, NULL, error))
		return FALSE;

	/* just output the diff */
	g_set_error_literal (error, 1, 0, output);
	return FALSE;
}