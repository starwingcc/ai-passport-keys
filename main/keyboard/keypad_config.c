#include "keypad_config.h"
#include <string.h>

static bool number(const cJSON *object, const char *name, unsigned max, unsigned *out) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(object, name);
    if (!cJSON_IsNumber(value) || value->valuedouble < 0 || value->valuedouble > max ||
        value->valuedouble != (double)value->valueint) return false;
    *out = value->valueint;
    return true;
}
static bool fields(const cJSON *obj, const char *const *names, unsigned count) {
    if (!cJSON_IsObject(obj) || (unsigned)cJSON_GetArraySize(obj) != count) return false;
    for (const cJSON *p = obj->child; p; p = p->next) {
        bool found = false;
        for (unsigned i = 0; i < count; ++i) if (p->string && !strcmp(p->string, names[i])) found = true;
        if (!found) return false;
        for (const cJSON *q = p->next; q; q = q->next)
            if (q->string && !strcmp(p->string, q->string)) return false;
    }
    return true;
}
bool kp_config_parse(const cJSON *json, kp_config_t *out) {
    const char *root_fields[] = {"version", "bindings"};
    const char *action_fields[] = {"type", "modifiers", "keys", "consumer"};
    kp_config_t c = {0}; unsigned n;
    if (!out || !fields(json, root_fields, 2) || !number(json, "version", KP_VERSION, &n)) return false;
    c.version = n;
    const cJSON *bindings = cJSON_GetObjectItemCaseSensitive(json, "bindings");
    if (!cJSON_IsArray(bindings) || cJSON_GetArraySize(bindings) != KP_BINDINGS) return false;
    for (unsigned i = 0; i < KP_BINDINGS; ++i) {
        const cJSON *a = cJSON_GetArrayItem(bindings, i);
        if (!fields(a, action_fields, 4)) return false;
        if (!number(a, "type", KP_MEDIA, &n)) return false;
        c.bindings[i].type = n;
        if (!number(a, "modifiers", 255, &n)) return false;
        c.bindings[i].modifiers = n;
        if (!number(a, "consumer", 0xffff, &n)) return false;
        c.bindings[i].consumer = n;
        const cJSON *keys = cJSON_GetObjectItemCaseSensitive(a, "keys");
        if (!cJSON_IsArray(keys) || cJSON_GetArraySize(keys) > 6) return false;
        for (int j = 0; j < cJSON_GetArraySize(keys); ++j) {
            const cJSON *k = cJSON_GetArrayItem(keys, j);
            if (!cJSON_IsNumber(k) || k->valuedouble <= 0 || k->valuedouble > 255 ||
                k->valuedouble != (double)k->valueint) return false;
            c.bindings[i].keys[j] = k->valueint;
        }
    }
    if (!kp_config_valid(&c)) return false;
    *out = c;
    return true;
}
cJSON *kp_config_json(const kp_config_t *c) {
    cJSON *root = cJSON_CreateObject();
    cJSON *list = cJSON_AddArrayToObject(root, "bindings");
    if (!root || !list || !cJSON_AddNumberToObject(root, "version", c->version)) goto fail;
    for (unsigned i = 0; i < KP_BINDINGS; ++i) {
        const kp_action_t *a = &c->bindings[i];
        cJSON *entry = cJSON_CreateObject();
        if (!entry) goto fail;
        if (!cJSON_AddItemToArray(list, entry)) { cJSON_Delete(entry); goto fail; }
        if (!cJSON_AddNumberToObject(entry, "type", a->type) ||
            !cJSON_AddNumberToObject(entry, "modifiers", a->modifiers) ||
            !cJSON_AddNumberToObject(entry, "consumer", a->consumer)) goto fail;
        cJSON *keys = cJSON_AddArrayToObject(entry, "keys");
        if (!keys) goto fail;
        for (unsigned j = 0; j < 6; ++j) if (a->keys[j]) {
            cJSON *k = cJSON_CreateNumber(a->keys[j]);
            if (!k) goto fail;
            if (!cJSON_AddItemToArray(keys, k)) { cJSON_Delete(k); goto fail; }
        }
    }
    return root;
fail:
    cJSON_Delete(root);
    return NULL;
}
