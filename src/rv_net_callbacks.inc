#if defined NET_CALLBACKS_INCLUDED
#endinput
#endif
#define NET_CALLBACKS_INCLUDED

// Network requests and their callbacks (ripext + SMbz2).
// Kept separate from rv_net.inc because these read state declared in rv_priv.inc.

// Get information about the current map, such as the zones.
public void Net_DownloadMapInfo()
{
    if (!net_config_ok)
    {
        PrintToChatAll("Replay viewer is not configured, see data/replay_viewer/api.txt and auth.txt");
        return;
    }

    HTTPRequest request = Net_NewRequest("replay-viewer/maps/%s", rv_cur_map);
    request.Get(Net_OnMapInfo);
}

public void Net_OnMapInfo(HTTPResponse response, any value)
{
    if (response.Status != HTTPStatus_OK || !Net_HasJsonBody(response))
    {
        PrintToChatAll("Could not download map information");
        return;
    }

    JSONObject data = view_as<JSONObject>(response.Data);

    bool linear = data.GetBool("is_linear");
    int num_stages = data.GetInt("stage_count");
    int num_bonuses = data.GetInt("bonus_count");

    RV_CreateZoneListMenu(linear, num_stages, num_bonuses);
}

// Download a list of possible replays for a given zone in a leaderboard.
// See NET_ZONE_MAP, Net_GetStageZone and Net_GetBonusZone to get a zone id from a logical stage number.
// See NET_ANGLE_TYPE constants for angled leaderboards.
// The user id is used to open the menu for that client when the list arrives.
public void Net_DownloadReplayList(int user_id, int zone_id, int angle_type)
{
    if (!net_config_ok)
    {
        PrintToChatAll("Replay viewer is not configured, see data/replay_viewer/api.txt and auth.txt");
        return;
    }

    HTTPRequest request = Net_NewRequest("replay-viewer/maps/%s/zones/%d/replays?finish_type=%d", rv_cur_map, zone_id, angle_type);

    // User id, zone id and angle type packed into the one callback value cell.
    request.Get(Net_OnReplayList, user_id | (zone_id << 8) | (angle_type << 16));
}

public void Net_OnReplayList(HTTPResponse response, any value)
{
    int user_id = value & 0xFF;
    int zone_id = (value >> 8) & 0xFF;
    int angle_type = (value >> 16) & 0xFF;

    if (response.Status != HTTPStatus_OK || !Net_HasJsonBody(response))
    {
        PrintToChatAll("Could not download replay list");
        return;
    }

    JSONObject data = view_as<JSONObject>(response.Data);
    JSONArray items = view_as<JSONArray>(data.Get("items"));

    if (items == null)
    {
        PrintToChatAll("Could not download replay list");
        return;
    }

    RV_ProcessDownloadedListing(items, zone_id, angle_type);
    RV_CreateNewReplayListMenu(items, user_id, zone_id);

    delete items;
}

// Download a particular replay for the given zone and leaderboard.
// The replay id comes from the zone replay list.
// The server sends the file already decompressed, straight into the cache location,
// and a bot is spawned with the stream when everything worked.
public void Net_DownloadReplay(int user_id, char[] id)
{
    if (!net_config_ok)
    {
        PrintToChatAll("Replay viewer is not configured, see data/replay_viewer/api.txt and auth.txt");
        return;
    }

    // Maybe create new directory for this map, ripext will not create it for us.

    char dir_path[PLATFORM_MAX_PATH];
    BuildPath(Path_SM, dir_path, sizeof(dir_path), "%s/replays/%s", RV_ROOT_PATH, rv_cur_map);
    RV_MaybeCreateDir(dir_path);

    char save_path[PLATFORM_MAX_PATH];
    RV_GetReplayFilePath(id, save_path, sizeof(save_path));

    DataPack pack = new DataPack();
    pack.WriteString(id);

    HTTPRequest request = Net_NewRequest("replay-viewer/replays/%s", id);
    request.DownloadFile(save_path, Net_OnReplayDownload, pack);
}

public void Net_OnReplayDownload(HTTPStatus status, any value)
{
    DataPack pack = view_as<DataPack>(value);
    pack.Reset();

    char id[RV_REPLAY_ID_SIZE];
    pack.ReadString(id, sizeof(id));
    delete pack;

    char save_path[PLATFORM_MAX_PATH];
    RV_GetReplayFilePath(id, save_path, sizeof(save_path));

    if (status != HTTPStatus_OK)
    {
        DeleteFile(save_path); // ripext leaves a partial or error-body file behind.
        PrintToChatAll("Could not download replay");
        return;
    }

    PrintToServer("Downloaded %s", id);

    RV_LoadStreamSpawnBot(id);
}
