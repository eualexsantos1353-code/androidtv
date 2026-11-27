#include "core/later.h"
#include "core/store.h"
#include "core/json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define KEY "watchLater"
#define MAXL 60
static cJSON *list(void) { cJSON *l = store_node(KEY); return cJSON_IsArray(l) ? l : NULL; }
int later_has(const char *id) { const cJSON *l = list(), *e; if (!id || !*id) return 0; cJSON_ArrayForEach(e, l) if (!strcmp(js_str(e, "id", ""), id)) return 1; return 0; }
int later_toggle(const Item *it) {
  const cJSON *old = list(); cJSON *nl = cJSON_CreateArray(); int had = 0, n = 0; const cJSON *e;
  cJSON_ArrayForEach(e, old) if (!strcmp(js_str(e, "id", ""), it->id)) had = 1;
  if (!had) { cJSON *o = cJSON_CreateObject(); cJSON_AddStringToObject(o, "id", it->id); cJSON_AddStringToObject(o, "type", it->type); cJSON_AddStringToObject(o, "name", it->name);
    cJSON_AddStringToObject(o, "poster", it->poster); cJSON_AddStringToObject(o, "background", it->background); cJSON_AddStringToObject(o, "logo", it->logo); cJSON_AddItemToArray(nl, o); n++; }
  cJSON_ArrayForEach(e, old) { if (!strcmp(js_str(e, "id", ""), it->id) || n >= MAXL) continue; cJSON_AddItemToArray(nl, cJSON_Duplicate(e, 1)); n++; }
  store_set_node(KEY, nl); return !had;
}
int later_items(ItemList *out) {
  memset(out, 0, sizeof *out); const cJSON *l = list(), *e; int m = cJSON_GetArraySize(l); if (m <= 0) return 0;
  out->items = calloc((size_t)m, sizeof(Item)); if (!out->items) return 0;
  cJSON_ArrayForEach(e, l) { Item *it = &out->items[out->n++]; memset(it, 0, sizeof *it);
    snprintf(it->id, sizeof it->id, "%s", js_str(e, "id", "")); snprintf(it->type, sizeof it->type, "%s", js_str(e, "type", "movie")); snprintf(it->name, sizeof it->name, "%s", js_str(e, "name", ""));
    snprintf(it->poster, sizeof it->poster, "%s", js_str(e, "poster", "")); snprintf(it->background, sizeof it->background, "%s", js_str(e, "background", "")); snprintf(it->logo, sizeof it->logo, "%s", js_str(e, "logo", "")); }
  return out->n;
}
