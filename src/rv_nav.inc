#if defined NAV_INCLUDED
#endinput
#endif
#define NAV_INCLUDED

// Navigation mesh functions.

public void RV_StartNav()
{
    char dummy_nav_path[PLATFORM_MAX_PATH];
    BuildPath(Path_SM, dummy_nav_path, sizeof(dummy_nav_path), "%s/dummy.nav", RV_ROOT_PATH);

    // Create a dummy navigation file so it doesn't take ages to load new maps.
    RV_CreateDummyNavForMap(dummy_nav_path);
}

// Copy the dummy navigation file to maps/<map>.nav, unless a nav already exists.
// This used to be a native in the retired net extension; now a plain file copy.
public void RV_CreateDummyNavForMap(char[] dummy_nav_path)
{
    char dest_path[PLATFORM_MAX_PATH];
    FormatEx(dest_path, sizeof(dest_path), "maps/%s.nav", rv_cur_map);

    if (FileExists(dest_path))
    {
        return; // Don't overwrite if something exists already.
    }

    File source = OpenFile(dummy_nav_path, "rb");

    if (source == null)
    {
        return;
    }

    File dest = OpenFile(dest_path, "wb");

    if (dest == null)
    {
        delete source;
        return;
    }

    int chunk[512];
    int num;

    while ((num = source.Read(chunk, sizeof(chunk), 1)) > 0)
    {
        dest.Write(chunk, num, 1);
    }

    delete source;
    delete dest;
}
