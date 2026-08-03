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

// Read the server settings. Called on map start so a replaced token or URL is
// picked up without reloading the plugin.
public bool Net_LoadConfig()
{
    net_config_ok = false;

    char path[PLATFORM_MAX_PATH];
    BuildPath(Path_SM, path, sizeof(path), "data/replay_viewer/api.txt");

    File file = OpenFile(path, "rt");

    if (file == null)
    {
        LogError("Could not open api config file (%s)", path);
        return false;
    }

    char line[192];

    if (!file.ReadLine(line, sizeof(line)))
    {
        delete file;
        LogError("api.txt is empty; line 1 must be the API base URL");
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
        LogError("api.txt line 1 must be a base URL starting with http:// or https://");
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
        LogError("Could not open auth file (%s)", path);
        return false;
    }

    if (!file.ReadLine(line, sizeof(line)))
    {
        delete file;
        LogError("auth.txt is empty; line 1 must be the access token");
        return false;
    }

    delete file;

    TrimString(line);

    if (line[0] == 0)
    {
        LogError("auth.txt is empty; line 1 must be the access token");
        return false;
    }

    strcopy(net_auth_token, sizeof(net_auth_token), line);

    net_config_ok = true;

    return true;
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
