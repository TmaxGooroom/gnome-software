/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 * vi:set noexpandtab tabstop=8 shiftwidth=8:
 *
 * Copyright (C) 2016 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <config.h>

#include <glib/gi18n.h>
#include <gnome-software.h>
#include <json-glib/json-glib.h>
#include <sys/stat.h>
#include <time.h>

void
gs_plugin_initialize (GsPlugin *plugin)
{
	gs_plugin_add_rule (plugin, GS_PLUGIN_RULE_RUN_AFTER, "appstream");
	gs_plugin_add_rule (plugin, GS_PLUGIN_RULE_RUN_AFTER, "packagekit-refine");
	gs_plugin_add_rule (plugin, GS_PLUGIN_RULE_RUN_AFTER, "rpm-ostree");
	gs_plugin_add_rule (plugin, GS_PLUGIN_RULE_RUN_BEFORE, "icons");
}

static gboolean
gs_plugin_generic_updates_merge_os_update (GsApp *app)
{
	/* this is only for grouping system-installed packages */
	if (gs_app_get_bundle_kind (app) != AS_BUNDLE_KIND_PACKAGE ||
	    gs_app_get_scope (app) != AS_APP_SCOPE_SYSTEM)
		return FALSE;

	if (gs_app_get_kind (app) == AS_APP_KIND_GENERIC)
		return TRUE;
	if (gs_app_get_kind (app) == AS_APP_KIND_SOURCE)
		return TRUE;

	return FALSE;
}

static const gchar* get_release_note_uri(const gchar *target)
{
  const gchar *filename;
  const gchar *uri = NULL;
  g_autofree gchar *data = NULL;
  GMatchInfo *match_info;
  GRegex *regex;

  filename = "/etc/apt/sources.list";
  if (!g_file_get_contents (filename, &data, NULL, NULL)) {
    g_print ("Fail to get sources.list\n");
    return NULL;
  }

  regex = g_regex_new ("^deb\\s+(\\w+://[^/\\s]+)", 0, 0, NULL);
  g_regex_match (regex, data, 0, &match_info);

  if (g_match_info_matches (match_info)) {
    gchar *res;
    res = g_match_info_fetch (match_info, 1);

    uri = g_strconcat (res, "/release-notes/", target, NULL);

    g_free (res);
  }

  g_match_info_free (match_info);
  g_regex_unref (regex);

  return uri;
}

static GsApp *
gs_plugin_generic_updates_get_os_update (GsPlugin *plugin)
{
	GsApp *app;
	const gchar *id = "org.gnome.Software.OsUpdate";
	g_autoptr(AsIcon) ic = NULL;
  g_autoptr(GsOsRelease) os = NULL;
  const gchar *os_ver = NULL;
  const gchar *os_summary = NULL;
  g_autoptr(SoupMessage) msg = NULL;
  g_autoptr(JsonParser) parser = NULL;
  JsonNode *root = NULL;
  JsonObject *obj = NULL;
  const gchar *os_latest = NULL;
  guint status_code;
  g_autofree gchar *uri = NULL;

  /* get last modification time */
  struct stat buf;
  char time[50] = {0,};
  stat ("/etc/os-release", &buf);
  strftime(time, 50, "%Y-%m-%d", localtime(&buf.st_mtime));

  /* get latest os version */
  uri = get_release_note_uri ("latest");
  msg = soup_message_new (SOUP_METHOD_GET, uri);
  status_code = soup_session_send_message (gs_plugin_get_soup_session (plugin), msg);
  if (status_code == SOUP_STATUS_OK) {
    parser = json_parser_new ();
    json_parser_load_from_data (parser, msg->response_body->data, -1, NULL);
    root = json_parser_get_root (parser);
    obj = json_node_get_object (root);
    os_latest = json_object_get_string_member (obj, "latestVersion");
  }

  /* get current os version */
  os = gs_os_release_new (NULL);
  os_ver = gs_os_release_get_version_id (os);
  os_summary =
    g_strconcat(_("Update version"), ":   ", "TmaxOS ", os_latest, "\n",
                _("Current version"), ":   ", "TmaxOS ", os_ver, "\n",
                _("Last installed"), ":   ", time,
                NULL);

	/* create new */
	app = gs_app_new (id);
	gs_app_add_quirk (app, GS_APP_QUIRK_IS_PROXY);
	gs_app_set_management_plugin (app, "");
	gs_app_set_kind (app, AS_APP_KIND_OS_UPDATE);
	gs_app_set_state (app, AS_APP_STATE_UPDATABLE_LIVE);
	gs_app_set_name (app,
			 GS_APP_QUALITY_NORMAL,
			 /* TRANSLATORS: this is a group of updates that are not
			  * packages and are not shown in the main list */
			 _("TmaxOS Updates"));
	gs_app_set_summary (app,
			    GS_APP_QUALITY_NORMAL,
			    /* TRANSLATORS: this is a longer description of the
			     * "OS Updates" string */
          os_summary);
	gs_app_set_description (app,
				GS_APP_QUALITY_NORMAL,
				gs_app_get_summary (app));
	ic = as_icon_new ();
	as_icon_set_kind (ic, AS_ICON_KIND_STOCK);
	as_icon_set_name (ic, "emblem-tmaxos");
	gs_app_add_icon (app, ic);

  g_free(os_summary);

	return app;
}

gboolean
gs_plugin_refine (GsPlugin *plugin,
		  GsAppList *list,
		  GsPluginRefineFlags flags,
		  GCancellable *cancellable,
		  GError **error)
{
	g_autoptr(GsApp) app = NULL;
	g_autoptr(GsAppList) os_updates = gs_app_list_new ();

	/* not from get_updates() */
	if ((flags & GS_PLUGIN_REFINE_FLAGS_REQUIRE_UPDATE_DETAILS) == 0)
		return TRUE;

	/* do we have any packages left that are not apps? */
	for (guint i = 0; i < gs_app_list_length (list); i++) {
		GsApp *app_tmp = gs_app_list_index (list, i);
		if (gs_app_has_quirk (app_tmp, GS_APP_QUIRK_IS_WILDCARD))
			continue;
		if (gs_plugin_generic_updates_merge_os_update (app_tmp))
			gs_app_list_add (os_updates, app_tmp);
	}
	if (gs_app_list_length (os_updates) == 0)
		return TRUE;

	/* create new meta object */
	app = gs_plugin_generic_updates_get_os_update (plugin);
	for (guint i = 0; i < gs_app_list_length (os_updates); i++) {
		GsApp *app_tmp = gs_app_list_index (os_updates, i);
		gs_app_add_related (app, app_tmp);
		gs_app_list_remove (list, app_tmp);
	}
	gs_app_list_add (list, app);
	return TRUE;
}
