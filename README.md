# ReplayViewer

This allows you to view KSF replays played on Counter-Strike: Source in a local server. Many replays can be played at once and remain synchronized with each other, allowing you to spectate individually.

Compatible with [SVR](https://github.com/crashfort/SourceDemoRender/), which can be used to create historical record videos or record comparison videos where you need to have more simultaneous players. The replay viewer will also produce smoother and more accurate playback without teleport lag, compared to traditional interpolated server side demo playback.

## User instructions

Steps 1 to 5 only have to be done once, or when respective programs have to update.

1. Download [SourceMod](https://www.sourcemod.net/downloads.php) and [Metamod](https://www.metamodsource.net/).
2. Extract **SourceMod** and **Metamod** archives into `cstrike/`.
3. Download [REST in Pawn (ripext)](https://github.com/ErikMinekus/sm-ripext/releases) and extract it into `cstrike/`.
4. Extract **ReplayViewer** archive from the [releases page](https://github.com/crashfort/ReplayViewer/releases) into `cstrike/`.
5. Set up server access in `cstrike/addons/sourcemod/data/replay_viewer/` (see below).
6. Start the game with `-insecure` or use `svr_launcher.exe` from [SVR](https://github.com/crashfort/SourceDemoRender/).
7. Start the map you want using the console command `map <name>`.
8. Use the chat command `/replay_viewer` to open the interface.
9. Load all the replays you want to play using the interface.
10. Control playback using the interface.
11. In spectator mode, you can now use `startmovie` from [SVR](https://github.com/crashfort/SourceDemoRender/).

## Server access

The viewer downloads replays from the replay API. Two text files in
`cstrike/addons/sourcemod/data/replay_viewer/` configure it:

- `api.txt` — first line: the API base URL (for example `https://api.example.com`).
  Optional second line: the game, `css` (default) or `css100t`.
- `auth.txt` — your access token as the only line. Get a token by signing in through Steam at
  `<api base URL>/v1/replay-viewer/auth/login` (your Steam account must be on the allowlist —
  ask the API operator). Tokens expire after a day; sign in again to get a new one. The token is
  tied to your Steam account, so don't share it.

## Building the SourcePawn code

1. Compile `replay_viewer.sp` with `cstrike\addons\sourcemod\scripting\spcomp.exe -i include`
   (the `src/include/` directory carries the ripext include files).

All networking goes through the [REST in Pawn (ripext)](https://github.com/ErikMinekus/sm-ripext)
extension, which ships prebuilt Windows binaries — there is no C++ to build anymore.
