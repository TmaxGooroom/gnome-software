#include "tmaxos-release.h"
#include <json-glib/json-glib.h>
#include <libsoup/soup.h>

#define TMAXOS_URI "http://220.90.208.56/release-notes/"

struct _TmaxosRelease
{
	GObject parent_instance;

  SoupSession *soup;
  JsonParser *parser;
};

G_DEFINE_TYPE (TmaxosRelease, tmaxos_release, G_TYPE_OBJECT)

const gchar* tmaxos_release_get_content (TmaxosRelease *self, const gchar *name)
{
  const gchar *uri = g_strconcat (TMAXOS_URI, name, NULL);
  SoupMessage *msg = NULL;
  JsonNode *root;
  JsonObject *obj;
  const gchar *ret;
  guint status_code;

  msg = soup_message_new (SOUP_METHOD_GET, uri);
  status_code = soup_session_send_message (self->soup, msg);
  if (status_code != SOUP_STATUS_OK) {
    g_object_unref (msg);
    g_free (uri);
    return NULL;
  }

  json_parser_load_from_data (self->parser, msg->response_body->data, -1, NULL);
  root = json_parser_get_root (self->parser);
  obj = json_node_get_object (root);

  ret = json_object_get_string_member (obj, "releaseNoteContents");

  g_object_unref (msg);
  g_free (uri);

  return g_strdup (ret);
}

GList* tmaxos_release_get_list (TmaxosRelease *self)
{
  const gchar *uri = g_strconcat (TMAXOS_URI, "list", NULL);
  SoupMessage *msg = NULL;
  JsonNode *root;
  JsonObject *obj;
  JsonArray *arr;
  guint list_size;
  GList *list = NULL;
  guint status_code;

  msg = soup_message_new (SOUP_METHOD_GET, uri);
  status_code = soup_session_send_message (self->soup, msg);
  if (status_code != SOUP_STATUS_OK) {
    g_object_unref (msg);
    g_free (uri);
    return NULL;
  }

  json_parser_load_from_data (self->parser, msg->response_body->data, -1, NULL);
  root = json_parser_get_root (self->parser);
  obj = json_node_get_object (root);
  arr = json_object_get_array_member (obj, "notelist");
  list_size = json_array_get_length (arr);

  for (guint i = 0; i < list_size; i++)
  {
    list = g_list_append (list, g_strdup (json_array_get_string_element (arr, i)));
  }

  g_object_unref (msg);
  g_free (uri);

  return list;
}

static void
tmaxos_release_init (TmaxosRelease *self)
{
  self->soup = soup_session_new();
  self->parser = json_parser_new();
}

static void
tmaxos_release_finalize (GObject *object)
{
	TmaxosRelease *tos = TMAXOS_RELEASE (object);

  g_object_unref(tos->soup);
  g_object_unref(tos->parser);

	G_OBJECT_CLASS (tmaxos_release_parent_class)->finalize (object);
}

static void
tmaxos_release_class_init (TmaxosReleaseClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = tmaxos_release_finalize;
}

TmaxosRelease *
tmaxos_release_new (void)
{
	TmaxosRelease *tos;
	tos = g_object_new (TMAXOS_TYPE_RELEASE, NULL);
	return TMAXOS_RELEASE (tos);
}

