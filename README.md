# barony-mods

Repo for maintaining a collection of custom [Barony](https://store.steampowered.com/app/371970/Barony/) mods and publishing them to the Steam Workshop.

## Layout

```text
barony-mods/
├── mods/<mod-slug>/
│   ├── workshop.toml        # Workshop title, description, preview, publishedfileid
│   ├── preview.png          # Workshop preview image, copied into the upload package
│   └── content/             # Files as Barony should see them at the mod root
│       ├── data/            # JSON gameplay files
│       ├── maps/            # .lmp maps and map metadata
│       ├── models/          # .vox model replacements
│       ├── images/
│       ├── items/
│       ├── lang/
│       ├── music/
│       ├── sounds/
│       └── books/
├── dist/                    # Generated clean upload folders; ignored by git
├── .workshop/               # Generated SteamCMD VDF files; ignored by git
└── tools/barony_mods.py     # Local helper CLI
```

Barony loads a mod folder whose contents mirror the game install structure. This repo keeps repo metadata outside `content/`, then builds a clean upload folder under `dist/<mod-slug>`.

## Create a mod

```bash
python tools/barony_mods.py new goblin-chaos --title "Goblin Chaos"
```

Then place the actual Barony files under `mods/goblin-chaos/content/`.

Common targets:

- `content/data/` for JSON gameplay data such as monster curves and monster variants.
- `content/maps/` for `.lmp` maps and related map files.
- `content/models/` for `.vox` model replacements.
- `content/images/`, `content/items/`, `content/lang/`, `content/music/`, `content/sounds/`, and `content/books/` for matching game assets.

Update `mods/goblin-chaos/workshop.toml` before publishing:

```toml
slug = "goblin-chaos"
title = "Goblin Chaos"
description = """
Short Steam Workshop description.
"""
preview = "preview.png"
visibility = 2 # 0 public, 1 friends-only, 2 hidden
publishedfileid = "0" # Steam fills this after first upload; keep it for updates
changenote = "Initial upload"

[content]
folder = "content"
```

## Validate and build

```bash
python tools/barony_mods.py validate
python tools/barony_mods.py build goblin-chaos
```

The build output goes to `dist/goblin-chaos/`. It excludes repo-only placeholders such as `.gitkeep` and copies `preview.png` into the Barony-ready root.

## Test locally in Barony

If Barony is installed in the default Linux Steam path, this should work:

```bash
python tools/barony_mods.py install goblin-chaos
```

Otherwise pass the install directory:

```bash
python tools/barony_mods.py install goblin-chaos --barony-dir "/path/to/Steam/steamapps/common/Barony"
```

That copies the built package to:

```text
<Barony install>/mods/goblin-chaos
```

Then launch Barony and use the modded-game / Workshop UI to test the local mod.

## Publish

Recommended path for Barony-specific publishing:

1. Build and install the mod into `<Barony install>/mods/<mod-slug>`.
2. Launch Barony.
3. Open `Play Modded Game` → `My Workshop Items`.
4. Create or update the Workshop item from the installed local mod folder.
5. Check the Workshop page after upload; Steam items may start hidden/private until you accept the Workshop agreement and set visibility.

Optional SteamCMD path:

```bash
python tools/barony_mods.py vdf goblin-chaos
steamcmd +login <username> +workshop_build_item .workshop/goblin-chaos.vdf +quit
```

or:

```bash
python tools/barony_mods.py publish goblin-chaos --steam-user <username>
```

Do not commit Steam credentials. The helper only stores Workshop metadata and generated VDF files are gitignored. Valve documents SteamCMD Workshop uploads as a testing-oriented path because it requires Steam credentials in the command-line workflow.

After the first successful SteamCMD upload, copy the generated `publishedfileid` back into `mods/<mod-slug>/workshop.toml` so future uploads update the same item rather than creating a new one.

## Useful references

- [Barony Steam page / app id 371970](https://store.steampowered.com/app/371970/Barony/)
- [Barony Official Workshop Creation Tutorial](https://steamcommunity.com/sharedfiles/filedetails/?id=1359907800)
- [Barony Official JSON Modding Guide V3.3.4+](https://steamcommunity.com/sharedfiles/filedetails/?id=2113716893)
- [Barony Official Translations Mod Tutorial](https://steamcommunity.com/sharedfiles/filedetails/?id=3041735536)
- [Steamworks Workshop implementation: SteamCMD VDF fields](https://partner.steamgames.com/doc/features/workshop/implementation#SteamCmd)