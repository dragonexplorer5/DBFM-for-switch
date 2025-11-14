Auto Folders tool for DBFM
=================================

This repository includes a standalone Lua utility `tools/auto_folders.lua` that helps auto-generate and organize folders for DBFM-managed content based on local activity (playtime logs, browser history, and file metadata).

What it does
- Creates folders like `Mods/[Game]`, `Screenshots/[Game]`, and `Unused NSPs/` and moves matching files into them.
- Sorts downloads into `Downloads/<source>/<YYYY-MM-DD>/` when a source can be inferred from browser history.
- Offers prompts for actions like backing up saves or cleaning unused NSPs (interactive; destructive actions require explicit confirmation).

Design notes
- The script is a host-side utility (runs on Windows/Linux/macOS) and is intentionally standalone so you can preview (dry-run) before applying changes.
- It is written in plain Lua and uses `luafilesystem` (`lfs`) if available for directory walking. It attempts to use `cjson` or `dkjson` for JSON parsing but falls back to simple heuristics if not present.

Dependencies
- Lua 5.1/5.2/5.3/5.4
- Optional but recommended:
  - LuaFileSystem (lfs) for robust directory traversal
  - lua-cjson or dkjson for JSON parsing

Installing dependencies (Windows PowerShell example)
Ensure Lua and luarocks are installed. For example, using luarocks you can install dependencies like:
  luarocks install luafilesystem
  luarocks install lua-cjson
or
  luarocks install dkjson

Usage examples
PowerShell examples (run these in a terminal)
  lua tools\auto_folders.lua --root "E:\" --history "E:\romfs\saved_urls.json" --dry

Apply changes (interactive prompts will appear):
  lua tools\auto_folders.lua --root "E:\" --history "E:\romfs\saved_urls.json"

Safety
- The tool defaults to dry-run mode. Always run with `--dry` first to inspect proposed moves.
- Destructive commands (delete) are intentionally left as manual steps and require explicit confirmation.

Integration ideas
- Later we can embed this script into DBFM natively by either:
  - Adding an in-app plugin runner that embeds Lua (requires linking a Lua interpreter in the build).
  - Converting logic to C and calling from an in-app feature (safer for console build constraints).

Contact
- If you want the script adapted into an in-app DBFM plugin, tell me and I can guide adding a minimal Lua runtime or porting the logic to C.
