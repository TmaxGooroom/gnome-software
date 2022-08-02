#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define TMAXOS_TYPE_RELEASE (tmaxos_release_get_type ())

G_DECLARE_FINAL_TYPE (TmaxosRelease, tmaxos_release, TMAXOS, RELEASE, GObject)

TmaxosRelease *tmaxos_release_new (void);
GList* tmaxos_release_get_list (TmaxosRelease *self);
const gchar* tmaxos_release_get_content (TmaxosRelease *self, const gchar *name);

G_END_DECLS
