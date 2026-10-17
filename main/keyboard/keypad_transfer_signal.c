#include "keypad_transfer_signal.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
static kp_transfer_peer_t *peer(kp_transfer_signal_t *s, int fd) {
    for (unsigned i = 0; i < KP_TRANSFER_PEERS; ++i) if (s->peers[i].id && s->peers[i].fd == fd) return &s->peers[i];
    return NULL;
}
static bool send_json(kp_transfer_signal_t *s, int fd, cJSON *json) {
    if (!json) return false;
    char *text = cJSON_PrintUnformatted(json);
    bool ok = text && strlen(text) <= KP_TRANSFER_MESSAGE && s->send(s->user, fd, text, strlen(text));
    free(text); return ok;
}
static void publish(kp_transfer_signal_t *s) {
    cJSON *msg = cJSON_CreateObject(), *list = cJSON_AddArrayToObject(msg, "peers");
    if (!msg || !list) { cJSON_Delete(msg); return; }
    cJSON_AddStringToObject(msg, "type", "peers");
    for (unsigned i = 0; i < KP_TRANSFER_PEERS; ++i) {
        kp_transfer_peer_t *p = &s->peers[i];
        if (!p->id || !p->joined) continue;
        cJSON *entry = cJSON_CreateObject();
        cJSON_AddNumberToObject(entry, "id", p->id); cJSON_AddStringToObject(entry, "name", p->name);
        cJSON_AddBoolToObject(entry, "rtc", p->rtc); cJSON_AddItemToArray(list, entry);
    }
    for (unsigned i = 0; i < KP_TRANSFER_PEERS; ++i) if (s->peers[i].id && s->peers[i].joined) send_json(s, s->peers[i].fd, msg);
    cJSON_Delete(msg);
}
void kp_transfer_signal_init(kp_transfer_signal_t *s, kp_transfer_send_t send, void *user) {
    memset(s, 0, sizeof(*s)); s->next_id = 1; s->send = send; s->user = user;
}
static void clear_peer(kp_transfer_peer_t *p) {
    free(p->catalog);
    memset(p, 0, sizeof(*p));
}
void kp_transfer_signal_clear(kp_transfer_signal_t *s) {
    if (s) for (unsigned i = 0; i < KP_TRANSFER_PEERS; ++i) clear_peer(&s->peers[i]);
}
void kp_transfer_signal_discard(kp_transfer_signal_t *s, int fd) {
    kp_transfer_peer_t *p = peer(s, fd);
    if (p) clear_peer(p);
}
bool kp_transfer_signal_open(kp_transfer_signal_t *s, int fd, uint64_t now) {
    if (peer(s, fd) || !s->next_id || s->next_id > INT32_MAX) return false;
    for (unsigned i = 0; i < KP_TRANSFER_PEERS; ++i) if (!s->peers[i].id) {
        kp_transfer_peer_t *p = &s->peers[i]; *p = (kp_transfer_peer_t){.fd = fd, .id = s->next_id++, .last_ms = now, .rtc = true};
        char welcome[80]; int n = snprintf(welcome, sizeof(welcome), "{\"type\":\"welcome\",\"id\":%lu}", (unsigned long)p->id);
        if (!s->send(s->user, fd, welcome, n)) { memset(p, 0, sizeof(*p)); return false; }
        return true;
    }
    return false;
}
void kp_transfer_signal_close(kp_transfer_signal_t *s, int fd) {
    kp_transfer_peer_t *p = peer(s, fd); if (!p) return;
    clear_peer(p); publish(s);
}
static bool fields(const cJSON *object, const char *allowed) {
    if (!cJSON_IsObject(object)) return false;
    for (const cJSON *p = object->child; p; p = p->next) {
        if (!p->string || !*p->string) return false;
        char key[64]; int n = snprintf(key, sizeof(key), "|%s|", p->string);
        if (n < 0 || (size_t)n >= sizeof(key) || !strstr(allowed, key)) return false;
        for (const cJSON *q = p->next; q; q = q->next) if (q->string && !strcmp(p->string, q->string)) return false;
    }
    return true;
}
static bool string(const cJSON *p, size_t min, size_t max) { return cJSON_IsString(p) && strlen(p->valuestring) >= min && strlen(p->valuestring) <= max; }
static bool number(const cJSON *p, double max) { return cJSON_IsNumber(p) && isfinite(p->valuedouble) && p->valuedouble >= 0 && p->valuedouble <= max && floor(p->valuedouble) == p->valuedouble; }
static bool file_id(const cJSON *p) {
    if (!string(p, 1, 40)) return false;
    for (const char *c = p->valuestring; *c; ++c) if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '-')) return false;
    return true;
}
static cJSON *get(const cJSON *o, const char *key) { return cJSON_GetObjectItemCaseSensitive(o, key); }
static bool valid_files(const cJSON *list) {
    if (!cJSON_IsArray(list) || cJSON_GetArraySize(list) > 8) return false;
    for (const cJSON *f = list->child; f; f = f->next) {
        if (!fields(f, "|id||name||size||mime||to|") || !file_id(get(f, "id")) || !string(get(f, "name"), 1, 180) ||
            !string(get(f, "mime"), 0, 80) || !number(get(f, "size"), 8589934592.0) || !number(get(f, "to"), INT32_MAX)) return false;
        for (const cJSON *g = f->next; g; g = g->next) if (file_id(get(g,"id")) && !strcmp(get(f,"id")->valuestring, get(g,"id")->valuestring)) return false;
    }
    return true;
}
static bool catalog_send(kp_transfer_signal_t *s, const kp_transfer_peer_t *p, int fd) {
    cJSON *msg = cJSON_CreateObject();
    if (!msg) return false;
    cJSON_AddStringToObject(msg, "type", "files"); cJSON_AddNumberToObject(msg, "from", p->id);
    cJSON *files = p->catalog ? cJSON_Parse(p->catalog) : cJSON_CreateArray();
    if (!files || !cJSON_AddItemToObject(msg, "files", files)) {
        cJSON_Delete(files); cJSON_Delete(msg); return false;
    }
    bool ok = send_json(s, fd, msg); cJSON_Delete(msg); return ok;
}
// Bound parser stack and allocation count before cJSON sees untrusted frames.
// Valid signaling needs at most three structural levels and fewer than 100
// separators. Quoted SDP/candidate/filename characters do not count as JSON.
static bool bounded_json(const char *data, size_t size) {
    unsigned depth = 0, tokens = 0;
    bool quoted = false, escaped = false;
    for (size_t i = 0; i < size; ++i) {
        char c = data[i];
        if (quoted) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        } else if (c == '"') quoted = true;
        else if (c == '{' || c == '[') { if (++depth > 4 || ++tokens > 160) return false; }
        else if (c == '}' || c == ']') { if (!depth) return false; --depth; }
        else if ((c == ',' || c == ':') && ++tokens > 160) return false;
    }
    return !quoted && !depth;
}
bool kp_transfer_signal_message(kp_transfer_signal_t *s, int fd, const char *data, size_t size, uint64_t now) {
    kp_transfer_peer_t *p = peer(s, fd);
    if (!p || now - p->last_ms >= KP_TRANSFER_IDLE_MS || !size || size > KP_TRANSFER_MESSAGE || memchr(data, 0, size) || !bounded_json(data, size)) return false;
    const char *end; cJSON *msg = cJSON_ParseWithLengthOpts(data, size, &end, false);
    if (!msg || end != data + size || !string(get(msg,"type"), 1, 10)) { cJSON_Delete(msg); return false; }
    const char *type = get(msg,"type")->valuestring; bool ok = false;
    if (!strcmp(type,"hello") && fields(msg,"|type||name||rtc||generation||lease|") && string(get(msg,"name"),1,48) &&
        (!get(msg,"rtc") || cJSON_IsBool(get(msg,"rtc")))) {
        snprintf(p->name,sizeof(p->name),"%s",get(msg,"name")->valuestring);
        p->rtc = !get(msg,"rtc") || cJSON_IsTrue(get(msg,"rtc")); p->joined = true; publish(s);
        for (unsigned i=0;i<KP_TRANSFER_PEERS;++i) if (s->peers[i].id && s->peers[i].joined && &s->peers[i]!=p) catalog_send(s,&s->peers[i],fd);
        ok = true;
    } else if (p->joined && !strcmp(type,"ping") && fields(msg,"|type|")) {
        static const char pong[]="{\"type\":\"pong\"}"; ok=s->send(s->user,fd,pong,sizeof(pong)-1);
    } else if (p->joined && !strcmp(type,"files") && fields(msg,"|type||files|") && valid_files(get(msg,"files"))) {
        char *list=cJSON_PrintUnformatted(get(msg,"files"));
        if (list && strlen(list)<=KP_TRANSFER_CATALOG) {
            // Transfer ownership only after successful bounded serialization.
            // Failed updates preserve the previous catalog; [] needs no heap.
            free(p->catalog); p->catalog = NULL;
            if (strcmp(list,"[]")) { p->catalog=list; list=NULL; }
            ok=true;
            for(unsigned i=0;i<KP_TRANSFER_PEERS;++i) if(s->peers[i].id && s->peers[i].joined && &s->peers[i]!=p) catalog_send(s,p,s->peers[i].fd);
        }
        free(list);
    } else if (p->joined && !strcmp(type,"signal") && fields(msg,"|type||to||description||candidate||request|") && number(get(msg,"to"),INT32_MAX)) {
        cJSON *d=get(msg,"description"), *c=get(msg,"candidate"), *r=get(msg,"request");
        bool valid = ((d!=NULL)+(c!=NULL)+(r!=NULL))==1;
        if (d) valid = valid && fields(d,"|type||sdp|") && string(get(d,"sdp"),1,3400) && string(get(d,"type"),1,6) &&
            (!strcmp(get(d,"type")->valuestring,"offer") || !strcmp(get(d,"type")->valuestring,"answer"));
        if (c) valid = valid && fields(c,"|candidate||sdpMid||sdpMLineIndex||usernameFragment|") && string(get(c,"candidate"),1,1024) &&
            (cJSON_IsNull(get(c,"sdpMid")) || string(get(c,"sdpMid"),0,32)) && (cJSON_IsNull(get(c,"sdpMLineIndex")) || number(get(c,"sdpMLineIndex"),16)) &&
            (!get(c,"usernameFragment") || cJSON_IsNull(get(c,"usernameFragment")) || string(get(c,"usernameFragment"),0,256));
        if(r)valid=valid&&cJSON_IsTrue(r);
        kp_transfer_peer_t *target=NULL;
        for(unsigned i=0;i<KP_TRANSFER_PEERS;++i) if(s->peers[i].id==(uint32_t)get(msg,"to")->valuedouble && s->peers[i].joined)target=&s->peers[i];
        if(valid && get(msg,"to")->valuedouble>0 && target!=p) {
            uint32_t to=(uint32_t)get(msg,"to")->valuedouble;
            cJSON_DeleteItemFromObjectCaseSensitive(msg,"to");cJSON_AddNumberToObject(msg,"from",p->id);
            ok=target&&send_json(s,target->fd,msg);
            if(!ok) {
                // A peer may close between discovery and the last ICE message.
                // This is a routing race, not a malformed sender session.
                char error[100];int n=snprintf(error,sizeof(error),"{\"type\":\"error\",\"to\":%lu,\"code\":\"peer-offline\"}",(unsigned long)to);
                ok=s->send(s->user,fd,error,(size_t)n);
            }
        }
    }
    if(ok)p->last_ms=now;
    cJSON_Delete(msg); return ok;
}
unsigned kp_transfer_signal_count(const kp_transfer_signal_t *s) { unsigned n=0;for(unsigned i=0;i<KP_TRANSFER_PEERS;++i)n+=s->peers[i].id&&s->peers[i].joined;return n; }
unsigned kp_transfer_signal_online(const kp_transfer_signal_t *s,uint64_t now) {unsigned n=0;for(unsigned i=0;i<KP_TRANSFER_PEERS;++i)n+=s->peers[i].id&&s->peers[i].joined&&now-s->peers[i].last_ms<KP_TRANSFER_IDLE_MS;return n;}
int kp_transfer_signal_expired(const kp_transfer_signal_t *s,uint64_t now) {for(unsigned i=0;i<KP_TRANSFER_PEERS;++i)if(s->peers[i].id&&now-s->peers[i].last_ms>=KP_TRANSFER_IDLE_MS)return s->peers[i].fd;return -1;}
void kp_transfer_signal_end(kp_transfer_signal_t *s) {static const char end[]="{\"type\":\"end\"}";for(unsigned i=0;i<KP_TRANSFER_PEERS;++i)if(s->peers[i].id)s->send(s->user,s->peers[i].fd,end,sizeof(end)-1);}
