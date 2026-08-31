#if defined NET_INCLUDED
#endinput
#endif
#define NET_INCLUDED

// Internet functions, built on the REST in Pawn (ripext) and SMbz2 (bzip2) extensions.
// The request and callback functions live in rv_net_callbacks.inc, which is included
// after the state in rv_priv.inc that they read.

// Zone 0 is the main map, 1 - 30 are stages and 31 - 40 are bonuses.
#define NET_ZONE_MAP 0
#define NET_ZONE_FIRST_STAGE 1
#define NET_ZONE_FIRST_BONUS 31
#define NET_ZONES_MAX 40

#define NET_ANGLE_TYPE_FW 0
#define NET_ANGLE_TYPE_SW 1
#define NET_ANGLE_TYPE_HSW 2
#define NET_ANGLE_TYPE_BW 3
#define NET_ANGLE_TYPES_MAX 4

// Server connection settings, read by Net_LoadConfig from data/replay_viewer/:
// api.txt line 1 = base URL (e.g. "https://api.example.com"), optional line 2 = game
// ("css" by default, or "css100t"). auth.txt line 1 = the access token.
char net_api_base[192];
char net_api_game[32];
char net_auth_token[192];
bool net_config_ok;

// Why the config is not ok, shown in chat when someone actually uses the menu
// (map start only logs — see Net_LoadConfig).
char net_config_error[192];

// Index 0 will return stage 1.
stock int Net_GetStageZone(int idx)
{
    return NET_ZONE_FIRST_STAGE + idx;
}

// Index 0 will return bonus 1.
stock int Net_GetBonusZone(int idx)
{
    return NET_ZONE_FIRST_BONUS + idx;
}

stock void Net_CategorizeZoneId(int zone_id, char[] dest, int dest_size)
{
    if (zone_id == NET_ZONE_MAP)
    {
        Format(dest, dest_size, "Map");
        return;
    }

    if (zone_id >= NET_ZONE_FIRST_BONUS)
    {
        Format(dest, dest_size, "Bonus %d", (zone_id - NET_ZONE_FIRST_BONUS) + 1);
        return;
    }

    if (zone_id >= NET_ZONE_FIRST_STAGE)
    {
        Format(dest, dest_size, "Stage %d", (zone_id - NET_ZONE_FIRST_STAGE) + 1);
        return;
    }
}

// Remember why the config failed. Logged once here; shown in chat later, when
// someone actually uses the menu (Net_PrintConfigError).
static void Net_ConfigFailed(const char[] format, any ...)
{
    VFormat(net_config_error, sizeof(net_config_error), format, 2);
    LogError("%s", net_config_error);
}

// Read the server settings. Called on map start (quiet in chat) and again on
// demand from the menu, so fixed files work without reloading the plugin.
public bool Net_LoadConfig()
{
    net_config_ok = false;

    char path[PLATFORM_MAX_PATH];
    BuildPath(Path_SM, path, sizeof(path), "data/replay_viewer/api.txt");

    File file = OpenFile(path, "rt");

    if (file == null)
    {
        Net_ConfigFailed("Missing api.txt (addons/sourcemod/data/replay_viewer/api.txt); line 1 must be the API base URL");
        return false;
    }

    char line[192];

    if (!file.ReadLine(line, sizeof(line)))
    {
        delete file;
        Net_ConfigFailed("api.txt is empty; line 1 must be the API base URL");
        return false;
    }

    TrimString(line);

    int length = strlen(line);

    if (length > 0 && line[length - 1] == '/')
    {
        line[length - 1] = 0; // The URL builder adds its own slashes.
    }

    if (strncmp(line, "http://", 7) != 0 && strncmp(line, "https://", 8) != 0)
    {
        delete file;
        Net_ConfigFailed("api.txt line 1 must be a base URL starting with http:// or https://");
        return false;
    }

    strcopy(net_api_base, sizeof(net_api_base), line);

    strcopy(net_api_game, sizeof(net_api_game), "css");

    if (file.ReadLine(line, sizeof(line)))
    {
        TrimString(line);

        if (line[0] != 0)
        {
            strcopy(net_api_game, sizeof(net_api_game), line);
        }
    }

    delete file;

    BuildPath(Path_SM, path, sizeof(path), "data/replay_viewer/auth.txt");

    file = OpenFile(path, "rt");

    if (file == null)
    {
        Net_ConfigFailed("Missing auth.txt (addons/sourcemod/data/replay_viewer/auth.txt); sign in at %s/v1/replay-viewer/auth/login to get a token", net_api_base);
        return false;
    }

    if (!file.ReadLine(line, sizeof(line)))
    {
        line[0] = 0;
    }

    delete file;

    TrimString(line);

    if (line[0] == 0)
    {
        Net_ConfigFailed("auth.txt is empty; sign in at %s/v1/replay-viewer/auth/login and paste the token as line 1", net_api_base);
        return false;
    }

    strcopy(net_auth_token, sizeof(net_auth_token), line);

    net_config_ok = true;

    return true;
}

// True when this is a real, connected client we can print chat to. The prefetch
// at map start passes 0, which stays silent.
stock bool Net_UserValid(int user_id)
{
    return user_id > 0 && user_id <= MaxClients && IsClientInGame(user_id);
}

// Show the stored config problem to whoever is using the menu.
stock void Net_PrintConfigError(int user_id)
{
    if (Net_UserValid(user_id))
    {
        PrintToChat(user_id, "%s", net_config_error);
    }
}

// A request failed. A rejected token gets the sign-in link (the base URL is where
// tokens are minted); anything else gets the short reason.
stock void Net_PrintRequestFailure(int user_id, HTTPStatus status, const char[] what)
{
    if (!Net_UserValid(user_id))
    {
        return;
    }

    if (status == HTTPStatus_Unauthorized || status == HTTPStatus_Forbidden)
    {
        PrintToChat(user_id, "Your access token was rejected or expired.");
        PrintToChat(user_id, "Sign in at %s/v1/replay-viewer/auth/login and paste the new token into data/replay_viewer/auth.txt", net_api_base);
        return;
    }

    PrintToChat(user_id, "Could not download %s (HTTP %d)", what, status);
}

// Create a request for a path under /v1/<game>/, with auth attached.
// The path is given without a leading slash.
stock HTTPRequest Net_NewRequest(const char[] format, any ...)
{
    char tail[512];
    VFormat(tail, sizeof(tail), format, 2);

    char url[768];
    FormatEx(url, sizeof(url), "%s/v1/%s/%s", net_api_base, net_api_game, tail);

    HTTPRequest request = new HTTPRequest(url);
    request.SetHeader("Authorization", "Bearer %s", net_auth_token);
    request.SetHeader("Accept", "application/json");

    return request;
}

// A proxy answering for the API (timeout page etc.) sends HTML, and reading
// response.Data from a non-JSON body throws and aborts the whole callback.
stock bool Net_HasJsonBody(HTTPResponse response)
{
    char content_type[64];

    if (!response.GetHeader("Content-Type", content_type, sizeof(content_type)))
    {
        return false;
    }

    return StrContains(content_type, "json") != -1;
}
