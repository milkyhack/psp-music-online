#ifndef CONFIG_H
#define CONFIG_H

#define MAX_HOST 64
#define MAX_API_KEY 64
#define DEFAULT_PORT 8084

typedef struct {
    char host[MAX_HOST];
    int port;
    char api_key[MAX_API_KEY];
} ServerConfig;

int config_load(ServerConfig *cfg);
int config_save(const ServerConfig *cfg);

/* Resolved at runtime via paths_join — not hardcoded GAME/PSPMUSIC */
void config_path(char *out, int out_sz);
void cache_mp3_path(char *out, int out_sz);

#endif
