#ifndef JUTIL_H
#define JUTIL_H

#define MAX_LIST_ITEMS 64
#define MAX_NAME 96
#define MAX_INFO 220

typedef struct {
    int id;
    char name[MAX_NAME];
    char artist[MAX_NAME];
    char album[MAX_NAME];
    char genre[32];
    int duration;
    int extra;         /* track count in lists / rating for tracks */
    int year;
    int user_rating;   /* 0-5 stars */
    int net_score;     /* 0-100 from network */
    char format[8];    /* flac / mp3 */
    int sample_rate;
    int bit_depth;
    int channels;
    int bytes;
    int lossless;
    int bitrate;
    int track_num;
    int play_count;
} ListItem;

int jutil_parse_named_list(const char *json, ListItem *items, int max_items);
int jutil_parse_tracks(const char *json, ListItem *items, int max_items);
int jutil_parse_genre_list(const char *json, ListItem *items, int max_items);

int jutil_extract_string(const char *json, const char *key, char *out, int out_sz);
int jutil_extract_int(const char *json, const char *key, int *out);

#endif
