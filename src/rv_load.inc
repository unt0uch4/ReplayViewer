#if defined LOAD_INCLUDED
#endinput
#endif
#define LOAD_INCLUDED

#define RV_MAGIC_V2 0x02 // Identifier or version.
#define RV_MAGIC_V3 0x03 // Self-describing header with larger frames; we use the v2 prefix of each frame.

// v3+ headers declare their own frame size in cells; RVFrame is the 10-cell v2 prefix of it.
#define RV_FRAME_CELLS_V2 10
#define RV_MAX_FRAME_CELLS 64

bool RV_LoadFileStream(char[] path, RVStream stream)
{
    File file = OpenFile(path, "rb");

    if (file == null)
    {
        PrintToChatAll("Could not load stream \"%s\"", path);
        return false;
    }

    // PrintToChatAll("Loaded stream \"%s\"", path);

    RVHeader header;
    ReadFile(file, header, sizeof(header), 4);

    if (header.magic != RV_MAGIC_V2 && header.magic != RV_MAGIC_V3)
    {
        PrintToChatAll("Could not load stream \"%s\", wrong version or wrong file type (%d), expected %d or %d", path, header.magic, RV_MAGIC_V2, RV_MAGIC_V3);
        delete file;
        return false;
    }

    int frame_cells = RV_FRAME_CELLS_V2;

    if (header.magic == RV_MAGIC_V3)
    {
        // v3 headers carry the frame size and an extension block (client settings snapshot) to skip.
        int ext_cells;
        ReadFileCell(file, frame_cells, 4);
        ReadFileCell(file, ext_cells, 4);

        if (frame_cells < RV_FRAME_CELLS_V2 || frame_cells > RV_MAX_FRAME_CELLS)
        {
            PrintToChatAll("Could not load stream \"%s\", unsupported frame size (%d cells)", path, frame_cells);
            delete file;
            return false;
        }

        if (ext_cells > 0)
        {
            FileSeek(file, ext_cells * 4, SEEK_CUR);
        }
    }

    stream.bookmarks = CreateArray(sizeof(RVBookmark));
    ResizeArray(stream.bookmarks, header.num_bookmarks);

    for (int i = 0; i < header.num_bookmarks; i++)
    {
        RVBookmark bk;
        ReadFile(file, bk, sizeof(bk), 4);

        SetArrayArray(stream.bookmarks, i, bk);
    }

    stream.frames = CreateArray(sizeof(RVFrame));
    ResizeArray(stream.frames, header.num_frames);

    // The header frame count can slightly exceed what is stored, so stop on a short read.
    // Only the first RV_FRAME_CELLS_V2 cells of each frame are used; the rest is newer telemetry.

    int frame_scratch[RV_MAX_FRAME_CELLS];
    int frames_read = 0;

    for (int i = 0; i < header.num_frames; i++)
    {
        if (ReadFile(file, frame_scratch, frame_cells, 4) != frame_cells)
        {
            break;
        }

        SetArrayArray(stream.frames, i, frame_scratch, sizeof(RVFrame));
        frames_read++;
    }

    if (frames_read != header.num_frames)
    {
        ResizeArray(stream.frames, frames_read);
    }

    stream.num_frames = frames_read;
    stream.num_bookmarks = header.num_bookmarks;
    stream.start_offset = 0;
    stream.team = header.team;

    // Find the start offset so we can synchronize the start of all replays.

    for (int i = 0; i < stream.num_bookmarks; i++)
    {
        RVBookmark bk;
        GetArrayArray(stream.bookmarks, i, bk);

        // First event of this type will be the true start.
        if (bk.type == RV_BK_START)
        {
            stream.start_offset = bk.frame;
            break;
        }
    }

    // Find the end offset so we can set the real duration.

    for (int i = stream.num_bookmarks - 1; i >= 0; i--)
    {
        RVBookmark bk;
        GetArrayArray(stream.bookmarks, i, bk);

        // Last event of this type will be the true end.
        if (bk.type == RV_BK_STOP)
        {
            stream.end_offset = stream.num_frames - bk.frame;
            break;
        }
    }

    stream.duration = stream.num_frames - stream.start_offset - stream.end_offset;

    delete file;

    return true;
}

// !!!!!!!!!!!!!!!!!!!!!!!!!
// Code unused for now, offline playback would need more menu work to work good.
// !!!!!!!!!!!!!!!!!!!!!!!!!

// Instead of managing a huge list with many pages of hard to read names,
// let the file management be done in the file explorer and just add everything in there.
public void RV_AddAllFileReplaysForMap()
{
    char dir_path[PLATFORM_MAX_PATH];
    BuildPath(Path_SM, dir_path, sizeof(dir_path), "%s/replays/%s", RV_ROOT_PATH, rv_cur_map);

    if (!DirExists(dir_path))
    {
        PrintToChatAll("No replays exist for map %s", rv_cur_map);
        return;
    }

    DirectoryListing dir_lister = OpenDirectory(dir_path);

    if (dir_lister == null)
    {
        PrintToChatAll("Could not open directory for map %s", rv_cur_map);
        return;
    }

    FileType file_type;
    char file_name[PLATFORM_MAX_PATH];
    int index = 0;

    char full_path[PLATFORM_MAX_PATH];

    while (ReadDirEntry(dir_lister, file_name, sizeof(file_name), file_type))
    {
        if (file_type != FileType_File)
        {
            continue;
        }

        if (!RV_CanSpawnMoreBots())
        {
            break; // We are full.
        }

        FormatEx(full_path, sizeof(full_path), "%s/%s", dir_path, file_name);

        RVStream stream;

        if (!RV_LoadFileStream(full_path, stream))
        {
            continue;
        }

        // For files, we do not have any way to get the player name.
        // Use the file name and let the user make good names.
        RV_SpawnBotWithStream(stream, file_name);

        index++;
    }

    delete dir_lister;

    if (index == 0)
    {
        PrintToChatAll("No replays available for %s", rv_cur_map);
        return;
    }
}

public void RV_FreeStream(RVStream stream)
{
    if (stream.frames != null)
    {
        delete stream.frames;
        stream.frames = null;
    }

    if (stream.bookmarks != null)
    {
        delete stream.bookmarks;
        stream.bookmarks = null;
    }
}

public void RV_DupeStream(RVStream source, RVStream dest)
{
    dest = source;
    dest.bookmarks = RV_CloneArrayHandle(source.bookmarks);
    dest.frames = RV_CloneArrayHandle(source.frames);
}

// Called on map start.
public void RV_StartLoad()
{
    rv_replay_listings = CreateTrie();
}

// Called on map end.
public void RV_StopLoad()
{
    delete rv_replay_listings;
}

// Store data from the replay listing.
public void RV_ProcessDownloadedListing(JSONArray items, int zone_id, int angle_type)
{
    int num_replays = items.Length;

    for (int i = 0; i < num_replays; i++)
    {
        JSONObject item = view_as<JSONObject>(items.Get(i));

        RVReplayListing listing;
        item.GetString("file", listing.id, sizeof(listing.id));
        item.GetString("player_name", listing.name, sizeof(listing.name));
        item.GetString("time_str", listing.time, sizeof(listing.time));
        item.GetString("date_str", listing.date, sizeof(listing.date));
        listing.angle_type = angle_type;
        listing.zone_id = zone_id;

        delete item;

        SetTrieArray(rv_replay_listings, listing.id, listing, sizeof(listing));
    }
}

// Get full path to a replay file in the current map.
public void RV_GetReplayFilePath(char[] stream_id, char[] dest, int dest_size)
{
    BuildPath(Path_SM, dest, dest_size, "%s/replays/%s/%s", RV_ROOT_PATH, rv_cur_map, stream_id);
}

// Load a downloaded replay and spawn a bot with that stream.
public bool RV_LoadStreamSpawnBot(char[] stream_id)
{
    char load_path[PLATFORM_MAX_PATH];
    RV_GetReplayFilePath(stream_id, load_path, sizeof(load_path));

    if (!FileExists(load_path))
    {
        return false;
    }

    RVStream stream;

    if (!RV_LoadFileStream(load_path, stream))
    {
        return false;
    }

    // This function is only to be used when downloading replays, in which case the listings must exist.
    // The offline file loadings above do not care about the listings.

    RVReplayListing listing;

    if (!GetTrieArray(rv_replay_listings, stream_id, listing, sizeof(listing)))
    {
        return false;
    }

    char name[256];
    Format(name, sizeof(name), "[%s] %s (%s)", listing.time, listing.name, listing.date);

    bool res = RV_SpawnBotWithStream(stream, name);
    return res;
}

// Downloaded replays are saved to file by the network layer (rv_net_callbacks.inc:
// ripext downloads the compressed file, SMbz2 inflates it to the cache path), then
// loaded from disk here via RV_LoadStreamSpawnBot like any offline file.
