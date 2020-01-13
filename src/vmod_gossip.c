#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <syslog.h>
#include <ctype.h>
#include <errno.h>

#include "vcl.h"
#include "vrt.h"
#include "cache/cache.h"
#include "vcc_if.h"

#ifndef v_unused_
#  define v_unused_ __attribute__((__unused__))
#endif
#include "vtree.h"

#define LOG(ctx, level, fmt, ...) \
    do { \
        syslog(level, "[GOSSIP][%s:%d] " fmt, __func__, __LINE__, __VA_ARGS__); \
        unsigned slt; \
        if (level <= LOG_ERR) { \
            slt = SLT_VCL_Error; \
        } else { \
            slt = SLT_VCL_Log; \
        } \
        if (ctx != NULL && (ctx)->vsl != NULL) { \
            VSLb((ctx)->vsl, slt, "[GOSSIP][%s:%d] " fmt, __func__, __LINE__, __VA_ARGS__); \
        } else { \
            VSL(slt, 0, "[GOSSIP][%s:%d] " fmt, __func__, __LINE__, __VA_ARGS__); \
        } \
    } while (0)

typedef struct object {
    unsigned magic;
    #define OBJECT_MAGIC 0x83a592da
    struct objcore *oc;
    const char *info;
    VRBT_ENTRY(object) tree;
} object_t;

typedef VRBT_HEAD(objects, object) objects_t;

static int
objectcmp(const object_t *i1, const object_t *i2)
{
    if ((uintptr_t) i1->oc < (uintptr_t) i2->oc) {
        return -1;
    }
    if ((uintptr_t) i1->oc > (uintptr_t) i2->oc) {
        return 1;
    }
    return 0;
}

VRBT_PROTOTYPE_STATIC(objects, object, tree, objectcmp);
VRBT_GENERATE_STATIC(objects, object, tree, objectcmp);

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned inits = 0;
static uintptr_t callback_handle;

static const char header[] = "X-Gossip-Info:";

typedef struct vmod_state {
    unsigned magic;
    #define VMOD_STATE_MAGIC 0x78aaea42
    double tst;
    objects_t objects;
} vmod_state_t;

static vmod_state_t *vmod_state = NULL;

/******************************************************************************
 * UTILITIES.
 *****************************************************************************/

static object_t *
new_object(struct objcore *oc, const char *info)
{
    object_t *result;
    ALLOC_OBJ(result, OBJECT_MAGIC);
    AN(result);

    result->oc = oc;

    if (info != NULL) {
        result->info = strdup(info);
        AN(result->info);
    } else {
        result->info = NULL;
    }

    return result;
}

static void
free_object(object_t *object)
{
    object->oc = NULL;

    if (object->info != NULL) {
        free((void *) object->info);
        object->info = NULL;
    }

    FREE_OBJ(object);
}

static object_t *
find_object(objects_t *objects, struct objcore *oc)
{
    object_t object;
    object.oc = oc;
    return VRBT_FIND(objects, objects, &object);
}

static void
discard_objects(objects_t *objects)
{
    object_t *object, *tmp;
    VRBT_FOREACH_SAFE(object, objects, objects, tmp) {
        CHECK_OBJ_NOTNULL(object, OBJECT_MAGIC);
        VRBT_REMOVE(objects, objects, object);
        free_object(object);
    }
}

static double
ctx2now(VRT_CTX)
{
    if (ctx->bo || ctx->method == VCL_MET_INIT) {
        return ctx->now;
    } else {
        CHECK_OBJ(ctx->req, REQ_MAGIC);
        return ctx->method == VCL_MET_DELIVER ? ctx->now : ctx->req->t_req;
    }
}

static vmod_state_t *
new_vmod_state(double tst)
{
    vmod_state_t *result;
    ALLOC_OBJ(result, VMOD_STATE_MAGIC);
    AN(result);

    result->tst = tst;

    VRBT_INIT(&result->objects);

    return result;
}

static void*
free_vmod_state_thread(void *obj)
{
    vmod_state_t *state;
    CAST_OBJ_NOTNULL(state, obj, VMOD_STATE_MAGIC);

    state->tst = 0.0;
    discard_objects(&state->objects);
    FREE_OBJ(state);

    return NULL;
}

static void
free_vmod_state(vmod_state_t *state, unsigned async)
{
    if (async) {
        pthread_t thread;
        AZ(pthread_create(&thread, NULL, &free_vmod_state_thread, state));
    } else {
        free_vmod_state_thread(state);
    }
}

/******************************************************************************
 * CALLBACKS.
 *****************************************************************************/

static void
insert_callback(struct worker *wrk, struct objcore *oc)
{
    CHECK_OBJ_NOTNULL(oc, OBJCORE_MAGIC);

    const char *info = NULL;
    HTTP_FOREACH_PACK(wrk, oc, info) {
        if (!strncasecmp(info, header, sizeof(header) - 1)) {
            info = strchr(info, ':');
            AN(info);
            info++;
            for (; isspace(*info); info++);
            break;
        }
    }

    object_t *object = new_object(oc, info);

    AZ(pthread_mutex_lock(&mutex));
    AZ(VRBT_INSERT(objects, &vmod_state->objects, object));
    AZ(pthread_mutex_unlock(&mutex));
}

static void
remove_callback(struct objcore *oc)
{
    CHECK_OBJ_NOTNULL(oc, OBJCORE_MAGIC);

    AZ(pthread_mutex_lock(&mutex));
    object_t *object = find_object(&vmod_state->objects, oc);
    if (object != NULL) {
        CHECK_OBJ_NOTNULL(object, OBJECT_MAGIC);
        VRBT_REMOVE(objects, &vmod_state->objects, object);
        free_object(object);
    }
    AZ(pthread_mutex_unlock(&mutex));
}

static void
callback(struct worker *wrk, struct objcore *oc, enum exp_event_e e, void *priv)
{
    CHECK_OBJ_NOTNULL(wrk, WORKER_MAGIC);
    CHECK_OBJ_NOTNULL(oc, OBJCORE_MAGIC);
    AZ(priv);

    switch (e) {
        case EXP_INSERT:
        case EXP_INJECT:
            insert_callback(wrk, oc);
            break;

        case EXP_REMOVE:
            remove_callback(oc);
            break;

        default:
            WRONG("Unexpected event");
    }
}

/******************************************************************************
 * gossip.dump();
 *****************************************************************************/

static void
dump(
    VRT_CTX, vmod_state_t *state, const char *filename, double now,
    unsigned safe_ocs)
{
    CHECK_OBJ_NOTNULL(state, VMOD_STATE_MAGIC);

    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        LOG(ctx, LOG_ERR, "Failed to open output file %s (%s)",
            filename, strerror(errno));
    } else {
        LOG(ctx, LOG_INFO, "Started dump to file %s", filename);

        struct vsb *vsb = VSB_new_auto();
        AN(vsb);

        AZ(VSB_printf(vsb, "{\"tst\":%.6f,\"now\":%.6f}\n", state->tst, now));

        object_t *object;
        VRBT_FOREACH(object, objects, &state->objects) {
            CHECK_OBJ_NOTNULL(object, OBJECT_MAGIC);

            if (safe_ocs) {
                if (object->oc->flags & OC_F_BUSY) {
                    continue;
                }
            }

            if (safe_ocs) {
                AZ(VSB_cat(vsb, "{\"info\":"));
            }
            if (object->info != NULL) {
                AZ(VSB_cat(vsb, object->info));
            } else {
                AZ(VSB_cat(vsb, "null"));
            }
            if (safe_ocs) {
                AZ(VSB_cat(vsb, ","));
                AZ(VSB_printf(vsb, "\"hits\":%ld,", object->oc->hits));
                AZ(VSB_printf(vsb, "\"ttl\":%.6f,",
                    (object->oc->exp.t_origin + object->oc->exp.ttl) - now));
                AZ(VSB_printf(vsb, "\"grace\":%.6f,", object->oc->exp.grace));
                AZ(VSB_printf(vsb, "\"keep\":%.6f", object->oc->exp.keep));
                AZ(VSB_cat(vsb, "}"));
            }
            AZ(VSB_cat(vsb, "\n"));
            AZ(VSB_finish(vsb));

            if (fwrite(VSB_data(vsb), 1, VSB_len(vsb), file) != VSB_len(vsb)) {
                LOG(ctx, LOG_ERR, "Error while writing file %s", filename);
            }

            VSB_clear(vsb);
        }

        VSB_destroy(&vsb);
        fclose(file);

        LOG(ctx, LOG_INFO, "Finished dump to file %s", filename);
    }
}

struct dump_thread_args {
    unsigned magic;
#define DUMP_THREAD_ARGS_MAGIC 0xd5ae987b
    double now;
    const char *file;
    vmod_state_t *state;
};

static void*
dump_thread(void *obj)
{
    struct dump_thread_args *args;
    CAST_OBJ_NOTNULL(args, obj, DUMP_THREAD_ARGS_MAGIC);
    CHECK_OBJ_NOTNULL(args->state, VMOD_STATE_MAGIC);

    dump(NULL, args->state, args->file, args->now, 0);

    free((void *) args->file);
    free_vmod_state(args->state, 0);
    FREE_OBJ(args);

    return NULL;
}

VCL_VOID
vmod_dump(VRT_CTX, VCL_STRING file, VCL_BOOL discard)
{
    if (file != NULL && *file) {
        struct dump_thread_args *args;
        double now = ctx2now(ctx);

        AZ(pthread_mutex_lock(&mutex));
        if (discard) {
            ALLOC_OBJ(args, DUMP_THREAD_ARGS_MAGIC);
            AN(args);
            args->now = now;
            args->file = strdup(file);
            AN(args->file);
            args->state = vmod_state;

            vmod_state = new_vmod_state(now);
        } else {
            dump(ctx, vmod_state, file, now, 1);
        }
        AZ(pthread_mutex_unlock(&mutex));

        if (discard) {
            pthread_t thread;
            AZ(pthread_create(&thread, NULL, &dump_thread, args));
        }
    }
}

/******************************************************************************
 * gossip.discard();
 *****************************************************************************/

VCL_VOID
vmod_discard(VRT_CTX)
{
    AZ(pthread_mutex_lock(&mutex));
    free_vmod_state(vmod_state, 1);
    vmod_state = new_vmod_state(ctx2now(ctx));
    AZ(pthread_mutex_unlock(&mutex));
}

/******************************************************************************
 * gossip.escape_json_string();
 *****************************************************************************/

static const char *json_hex_chars = "0123456789abcdef";

static int
escape_json_string(const char *value, char *out, unsigned max_out_size)
{
    unsigned size = 0;

    #define DUMP_CHAR(C) \
        if (size < max_out_size) { \
            *(out + size) = C;  \
            size++;  \
        } else {  \
            return -1;  \
        }

    #define DUMP_FOUR(V) \
        DUMP_CHAR(json_hex_chars[(V >> 12) & 0xf]); \
        DUMP_CHAR(json_hex_chars[(V >> 8) & 0xf]); \
        DUMP_CHAR(json_hex_chars[(V >> 4) & 0xf]); \
        DUMP_CHAR(json_hex_chars[V & 0xf]);

    for (int i = 0; value[i]; i++) {
        if (value[i] > 31 && value[i] != '\"' && value[i] != '\\') {
            DUMP_CHAR(value[i]);
        } else {
            DUMP_CHAR('\\');
            switch (value[i]) {
                case '\\':
                    DUMP_CHAR('\\');
                    break;
                case '"':
                    DUMP_CHAR('"');
                    break;
                case '\b':
                    DUMP_CHAR('b');
                    break;
                case '\f':
                    DUMP_CHAR('f');
                    break;
                case '\n':
                    DUMP_CHAR('n');
                    break;
                case '\r':
                    DUMP_CHAR('r');
                    break;
                case '\t':
                    DUMP_CHAR('t');
                    break;
                default:
                    // What follows assumes input bytes are UTF-8 encoded.
                    DUMP_CHAR('u');
                    unsigned valid_utf8_sequence = 0;
                    unsigned char c1 = (unsigned char) value[i];

                    // 0xxxxxxx (c1)
                    // Code points U+0000 - U+007F (7 bits for code point)
                    if (c1 >= 0x00 && c1 <= 0x7f) {
                        valid_utf8_sequence = 1;
                        uint8_t v = c1;
                        DUMP_FOUR(v);

                    // 110xxxxx (c1) 10xxxxxx (c2)
                    // Code points U+0080 - U+07FF (11 bits for code point)
                    } else if (c1 >= 0xc0 && c1 <= 0xdf &&
                               value[i + 1]) {
                        unsigned char c2 = (unsigned char) value[i + 1];
                        if (// Disallow overlong 2-byte sequences.
                            c1 >= 0xc2 &&
                            // Check continuation bytes.
                            c2 >= 0x80 && c2 <= 0xbf) {
                            valid_utf8_sequence = 1;
                            uint16_t v =
                                (c1 & 0x1f) << 6 |
                                (c2 & 0x3f);
                            DUMP_FOUR(v);
                            i += 1;
                        }

                    // 1110xxxx (c1) 10xxxxxx (c2) 10xxxxxx (c3)
                    // Code points U+0800 - U+FFFF (16 bits for code point)
                    } else if (c1 >= 0xe0 && c1 <= 0xef &&
                               value[i + 1] &&
                               value[i + 2]) {
                        unsigned char c2 = (unsigned char) value[i + 1];
                        unsigned char c3 = (unsigned char) value[i + 2];
                        if (// Disallow overlong 3-byte sequences.
                            (c1 != 0xe0 || c2 >= 0xa0) &&
                            // Disallow U+D800..U+DFFF code points (reserved
                            // for for UTF-16 surrogate pair encoding).
                            (c1 != 0xed || c2 <= 0x9f) &&
                            // Check continuation bytes.
                            c2 >= 0x80 && c2 <= 0xbf &&
                            c3 >= 0x80 && c3 <= 0xbf) {
                            valid_utf8_sequence = 1;
                            uint16_t v =
                                (c1 & 0x0f) << 12 |
                                (c2 & 0x3f) << 6 |
                                (c3 & 0x3f);
                            DUMP_FOUR(v);
                            i += 2;
                        }

                    // 11110xxx (c1) 10xxxxxx (c2) 10xxxxxx (c3) 10xxxxxx (c4)
                    // Code points U+10000 - U+10FFFF (21 bits for code point)
                    } else if (c1 >= 0xf0 && c1 <= 0xf7 &&
                               value[i + 1] &&
                               value[i + 2] &&
                               value[i + 3]) {
                        unsigned char c2 = (unsigned char) value[i + 1];
                        unsigned char c3 = (unsigned char) value[i + 2];
                        unsigned char c4 = (unsigned char) value[i + 3];
                        if (// Disallow overlong 4-byte sequences.
                            (c1 != 0xf0 || c2 >= 0x90) &&
                            // Disallow 0xf5..0xff prefixes.
                            c1 <= 0xf4 &&
                            // Check continuation bytes.
                            c2 >= 0x80 && c2 < 0xc0 &&
                            c3 >= 0x80 && c3 < 0xc0 &&
                            c4 >= 0x80 && c4 < 0xc0) {
                            uint32_t v =
                                (c1 & 0x07) << 18 |
                                (c2 & 0x3f) << 12 |
                                (c3 & 0x3f) <<  6 |
                                (c4 & 0x3f);
                            // Maximum possible value of a Unicode code point?
                            if (v <= 0x10ffff) {
                                valid_utf8_sequence = 1;
                                // Represent the code point as an UTF-16
                                // surrogate pair if 2 bytes are not enough.
                                if (v <= 0xffff) {
                                    DUMP_FOUR(v);
                                } else {
                                    v = v - 0x10000;
                                    uint16_t uc = ((v >> 10) & 0x3ff) | 0xd800;
                                    uint16_t lc = (v & 0x3ff) | 0xdc00;
                                    DUMP_FOUR(uc);
                                    DUMP_CHAR('\\');
                                    DUMP_CHAR('u');
                                    DUMP_FOUR(lc);
                                }
                                i += 3;
                            }
                        }
                    }

                    if (!valid_utf8_sequence) {
                        DUMP_FOUR(0xfffd);
                    }

                    break;
            }
        }
    }

    DUMP_CHAR('\0');
    return size;

    #undef DUMP_CHAR
    #undef DUMP_FOUR
}

VCL_STRING
vmod_escape_json_string(VRT_CTX, VCL_STRING value)
{
    char *result = NULL;

    if (value != NULL) {
        unsigned free_ws = WS_Reserve(ctx->ws, 0);
        result = ctx->ws->f;

        int used_ws = escape_json_string(value, result, free_ws);
        assert(used_ws > 0);

        WS_Release(ctx->ws, used_ws);
    }

    return result;
}

/******************************************************************************
 * VMOD EVENTS.
 *****************************************************************************/

int
event_function(VRT_CTX, struct vmod_priv *vcl_priv, enum vcl_event_e e)
{
    switch (e) {
        case VCL_EVENT_LOAD:
            if (inits == 0) {
                AZ(pthread_mutex_lock(&mutex));
                AZ(vmod_state);
                vmod_state = new_vmod_state(ctx2now(ctx));
                callback_handle = EXP_Register_Callback(
                    callback, NULL);
                AN(callback_handle);
                AZ(pthread_mutex_unlock(&mutex));
            }
            inits++;
            break;

        case VCL_EVENT_DISCARD:
            assert(inits > 0);
            inits--;
            AN(callback_handle);
            if (inits == 0) {
                AZ(pthread_mutex_lock(&mutex));
                EXP_Deregister_Callback(&callback_handle);
                AZ(callback_handle);
                free_vmod_state(vmod_state, 1);
                vmod_state = NULL;
                AZ(pthread_mutex_unlock(&mutex));
            }
            break;

        default:
            break;
    }

    return 0;
}
