// I used chatgpt for cleaning all mine shit code :)
// https://github.com/discord/discord-rpc used by msciotti

#include <ida.hpp>
#include <idp.hpp>
#include <loader.hpp>
#include <kernwin.hpp>
#include <diskio.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cctype>
#include "discord/include/discord_rpc.h"

#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#define Q_DIR_SEPARATOR '\\'
#else
#include <unistd.h>
#define Q_DIR_SEPARATOR '/'
#endif

#ifndef QMAXPATH
#define QMAXPATH 1024
#endif

static void my_trim(char* s)
{
    char* p = s;
    while (*p && isspace((unsigned char)*p))
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);
    char* end = s + strlen(s) - 1;
    while (end >= s && isspace((unsigned char)*end))
    {
        *end = '\0';
        end--;
    }
}

struct plugin_ctx_t : public plugmod_t
{
    bool rpc_initialized;
    plugin_ctx_t() : rpc_initialized(false) {}
    virtual bool idaapi run(size_t) override;
    virtual ~plugin_ctx_t();
};

bool idaapi plugin_ctx_t::run(size_t)
{
    char root_filename[QMAXPATH];
    if (get_root_filename(root_filename, QMAXPATH) < 0)
        qstrncpy(root_filename, "Unknown", QMAXPATH);

    msg("[JustAnother DiscordRPC] Initializing Discord RPC...\n");

    const char* user_dir = get_user_idadir();
    char config_dir[QMAXPATH];
    qsnprintf(config_dir, QMAXPATH, "%s%cJustAnother", user_dir, Q_DIR_SEPARATOR);

    qmkdir(config_dir, 0777);
    char config_path[QMAXPATH];
    qsnprintf(config_path, QMAXPATH, "%s%cconfig.hk", config_dir, Q_DIR_SEPARATOR);

    const char* default_state = "Eat. Sleep. Reverse engineer. Repeat";
    const char* default_details = "";
    const long  default_startTs = 0;
    const char* default_largeImageKey = "ida edu";
    const char* default_largeImageText = "IDA EDU";
    const char* default_clientId = "1338990536236990576";

    char cfg_state[256] = { 0 };
    char cfg_details[QMAXPATH] = { 0 };
    long cfg_startTimestamp = 0;
    char cfg_largeImageKey[64] = { 0 };
    char cfg_largeImageText[64] = { 0 };
    char cfg_clientId[64] = { 0 };

    FILE* f = qfopen(config_path, "r");
    if (!f)
    {
        f = qfopen(config_path, "w");
        if (f)
        {
            qfprintf(f, "state=%s\n", default_state);
            qfprintf(f, "Details=%s\n", default_details);
            qfprintf(f, "Timestamp=%ld\n", default_startTs);
            qfprintf(f, "Name=%s\n", default_largeImageKey);
            qfprintf(f, "LargeImageText=%s\n", default_largeImageText);
            qfprintf(f, "DiscordClientId=%s\n", default_clientId);
            qfclose(f);
            msg("[JustAnother DiscordRPC] Config file created at: %s\n", config_path);
        }

        f = qfopen(config_path, "r");
        if (!f)
        {
            warning("[JustAnother DiscordRPC] Unable to open config file :( -> %s\n", config_path);
        }
    }

    if (f)
    {
        char line[512];
        while (qfgets(line, sizeof(line), f))
        {
            char* ptr = line;
            while (*ptr && isspace((unsigned char)*ptr))
                ptr++;

            if (*ptr == '#' || *ptr == ';' || *ptr == '\0' || *ptr == '\n')
                continue;
            char* equal_sign = strchr(ptr, '=');
            if (!equal_sign)
                continue;
            *equal_sign = '\0';
            char* key = ptr;
            char* value = equal_sign + 1;
            my_trim(key);
            my_trim(value);
            if (strcmp(key, "state") == 0)
            {
                qstrncpy(cfg_state, value, sizeof(cfg_state));
            }
            else if (strcmp(key, "details") == 0)
            {
                qstrncpy(cfg_details, value, sizeof(cfg_details));
            }
            else if (strcmp(key, "startTimestamp") == 0)
            {
                cfg_startTimestamp = atol(value);
            }
            else if (strcmp(key, "largeImageKey") == 0)
            {
                qstrncpy(cfg_largeImageKey, value, sizeof(cfg_largeImageKey));
            }
            else if (strcmp(key, "largeImageText") == 0)
            {
                qstrncpy(cfg_largeImageText, value, sizeof(cfg_largeImageText));
            }
            else if (strcmp(key, "discordClientId") == 0)
            {
                qstrncpy(cfg_clientId, value, sizeof(cfg_clientId));
            }
        }
        qfclose(f);
    }

    // Если какие-либо параметры не заданы в конфиге, используем значения по умолчанию
    if (strlen(cfg_state) == 0)
        qstrncpy(cfg_state, default_state, sizeof(cfg_state));

    char final_details[QMAXPATH];
    if (strlen(cfg_details) == 0)
        qstrncpy(final_details, root_filename, sizeof(final_details));
    else
        qstrncpy(final_details, cfg_details, sizeof(final_details));

    time_t final_startTimestamp = (cfg_startTimestamp == 0) ? time(nullptr) : cfg_startTimestamp;

    if (strlen(cfg_largeImageKey) == 0)
        qstrncpy(cfg_largeImageKey, default_largeImageKey, sizeof(cfg_largeImageKey));
    if (strlen(cfg_largeImageText) == 0)
        qstrncpy(cfg_largeImageText, default_largeImageText, sizeof(cfg_largeImageText));
    if (strlen(cfg_clientId) == 0)
        qstrncpy(cfg_clientId, default_clientId, sizeof(cfg_clientId));

    DiscordEventHandlers handlers;
    memset(&handlers, 0, sizeof(handlers));
    Discord_Initialize(cfg_clientId, &handlers, 1, nullptr);

    DiscordRichPresence presence;
    memset(&presence, 0, sizeof(presence));
    presence.state = cfg_state;
    presence.details = final_details;
    presence.startTimestamp = final_startTimestamp;
    presence.largeImageKey = cfg_largeImageKey;
    presence.largeImageText = cfg_largeImageText;

    Discord_UpdatePresence(&presence);
    rpc_initialized = true;

    msg("[JustAnother DiscordRPC] Plugin is now running. It will remain active until IDA is closed.\n");
    return true;
}

plugin_ctx_t::~plugin_ctx_t()
{
    if (rpc_initialized)
    {
        msg("[JustAnother DiscordRPC] Shutting down Discord RPC...\n");
        Discord_Shutdown();
    }
}

static plugmod_t* idaapi init()
{
    return new plugin_ctx_t;
}

plugin_t PLUGIN =
{
    IDP_INTERFACE_VERSION,          // Версия интерфейса плагина (должна быть IDP_INTERFACE_VERSION)
    PLUGIN_MULTI,                   // Флаги плагина (PLUGIN_MULTI означает поддержку нескольких IDB одновременно)
    init,                           // Функция инициализации плагина
    nullptr,                        // Функция завершения работы плагина (не используется)
    nullptr,                        // Функция запуска плагина (run() вызывается через plugmod_t)
    "JustAnother DiscordRPC -> made for blast.hk forum by Amnesia :skull:",  // Длинный комментарий о плагине
    "JustAnother DiscordRPC -> plugin for basic Discord RPC integration.\nTo use, press Alt-F9.\nIf you want to change something, open the config folder in your plugins directory", // Подробная справка о плагине
    "JustAnother DiscordRPC",       // Предпочтительное короткое имя плагина
    "Alt-F9"                        // Предпочтительный горячий ключ для запуска плагина
};
