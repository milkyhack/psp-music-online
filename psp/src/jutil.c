#include "jutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *find_key(const char *p, const char *key) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    return strstr(p, pattern);
}

static int parse_int_after_key(const char *obj, const char *key, int *out) {
    const char *p = find_key(obj, key);
    if (!p) {
        return -1;
    }
    p = strchr(p + strlen(key) + 2, ':');
    if (!p) {
        return -1;
    }
    p++;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    *out = atoi(p);
    return 0;
}

static int parse_float_rating_after_key(const char *obj, const char *key, int *out_stars) {
    const char *p = find_key(obj, key);
    float f;
    if (!p) {
        return -1;
    }
    p = strchr(p + strlen(key) + 2, ':');
    if (!p) {
        return -1;
    }
    p++;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    f = (float)atof(p);
    if (f <= 0.0f) {
        *out_stars = 0;
    } else if (f > 5.0f) {
        *out_stars = 5;
    } else {
        *out_stars = (int)(f + 0.5f);
    }
    return 0;
}

static int parse_string_after_key(const char *obj, const char *key, char *out, int out_sz) {
    const char *p = find_key(obj, key);
    if (!p) {
        return -1;
    }
    p = strchr(p + strlen(key) + 2, ':');
    if (!p) {
        return -1;
    }
    p++;
    while (*p && isspace((unsigned char)*p)) {
        p++;
    }
    if (*p != '"') {
        return -1;
    }
    p++;
    int i = 0;
    while (*p && *p != '"' && i < out_sz - 1) {
        if (*p == '\\' && p[1]) {
            p++;
        }
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 0;
}

static int parse_objects(const char *json, ListItem *items, int max_items,
                         int (*fill)(const char *obj, ListItem *item)) {
    const char *p = json;
    int count = 0;
    while (count < max_items) {
        const char *start = strchr(p, '{');
        if (!start) {
            break;
        }
        const char *end = strchr(start, '}');
        if (!end) {
            break;
        }
        int len = (int)(end - start + 1);
        char *obj = (char *)malloc(len + 1);
        if (!obj) {
            break;
        }
        memcpy(obj, start, len);
        obj[len] = '\0';
        if (fill(obj, &items[count]) == 0) {
            count++;
        }
        free(obj);
        p = end + 1;
    }
    return count;
}

static int fill_named(const char *obj, ListItem *item) {
    memset(item, 0, sizeof(*item));
    if (parse_int_after_key(obj, "id", &item->id) < 0) {
        return -1;
    }
    if (parse_string_after_key(obj, "name", item->name, MAX_NAME) < 0) {
        return -1;
    }
    parse_string_after_key(obj, "artist", item->artist, MAX_NAME);
    parse_string_after_key(obj, "genre", item->genre, (int)sizeof(item->genre));
    parse_int_after_key(obj, "tracks", &item->extra);
    parse_int_after_key(obj, "year", &item->year);
    parse_float_rating_after_key(obj, "user_rating", &item->user_rating);
    parse_int_after_key(obj, "external_score", &item->net_score);
    parse_int_after_key(obj, "play_count", &item->play_count);
    return 0;
}

static int fill_track(const char *obj, ListItem *item) {
    int lossless = 0;
    memset(item, 0, sizeof(*item));
    if (parse_int_after_key(obj, "id", &item->id) < 0) {
        return -1;
    }
    if (parse_string_after_key(obj, "title", item->name, MAX_NAME) < 0) {
        return -1;
    }
    parse_string_after_key(obj, "artist", item->artist, MAX_NAME);
    parse_string_after_key(obj, "album", item->album, MAX_NAME);
    parse_string_after_key(obj, "genre", item->genre, (int)sizeof(item->genre));
    parse_string_after_key(obj, "format", item->format, (int)sizeof(item->format));
    {
        float dur = 0.0f;
        const char *p = find_key(obj, "duration");
        if (p) {
            p = strchr(p, ':');
            if (p) {
                dur = (float)atof(p + 1);
            }
        }
        item->duration = (dur > 0.0f) ? (int)(dur + 0.5f) : 0;
    }
    parse_int_after_key(obj, "year", &item->year);
    parse_int_after_key(obj, "rating", &item->extra);
    parse_int_after_key(obj, "play_count", &item->play_count);
    parse_int_after_key(obj, "sample_rate", &item->sample_rate);
    parse_int_after_key(obj, "bit_depth", &item->bit_depth);
    parse_int_after_key(obj, "channels", &item->channels);
    parse_int_after_key(obj, "bytes", &item->bytes);
    parse_int_after_key(obj, "bitrate", &item->bitrate);
    parse_int_after_key(obj, "track_num", &item->track_num);
    if (parse_int_after_key(obj, "lossless", &lossless) == 0) {
        item->lossless = lossless ? 1 : 0;
    } else if (item->format[0]) {
        item->lossless = (strcmp(item->format, "flac") == 0) ? 1 : 0;
    }
    return 0;
}

static int fill_genre(const char *obj, ListItem *item) {
    memset(item, 0, sizeof(*item));
    /* Server sends "genre"; accept "name" as fallback. */
    if (parse_string_after_key(obj, "genre", item->name, MAX_NAME) < 0) {
        if (parse_string_after_key(obj, "name", item->name, MAX_NAME) < 0) {
            return -1;
        }
    }
    if (parse_int_after_key(obj, "track_count", &item->extra) < 0) {
        parse_int_after_key(obj, "tracks", &item->extra);
    }
    item->id = 0;
    return 0;
}

int jutil_parse_named_list(const char *json, ListItem *items, int max_items) {
    return parse_objects(json, items, max_items, fill_named);
}

int jutil_parse_tracks(const char *json, ListItem *items, int max_items) {
    return parse_objects(json, items, max_items, fill_track);
}

int jutil_parse_genre_list(const char *json, ListItem *items, int max_items) {
    return parse_objects(json, items, max_items, fill_genre);
}

int jutil_extract_string(const char *json, const char *key, char *out, int out_sz) {
    if (!json || !key || !out || out_sz <= 0) {
        return -1;
    }
    return parse_string_after_key(json, key, out, out_sz);
}

int jutil_extract_int(const char *json, const char *key, int *out) {
    if (!json || !key || !out) {
        return -1;
    }
    return parse_int_after_key(json, key, out);
}
