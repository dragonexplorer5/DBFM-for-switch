-- auto_folders.lua
-- Standalone Lua utility for DBFM: auto-generate folders based on activity
-- Usage (PowerShell): lua tools\auto_folders.lua --root "E:\switch" --playtime "E:\switch\playtime.log" --history "romfs/saved_urls.json" --dry

local json = nil
local lfs = nil
-- try to load lua-cjson or dkjson
local function load_json()
  if json then return json end
  local ok, cj = pcall(require, 'cjson')
  if ok then json = cj; return json end
  ok, cj = pcall(require, 'dkjson')
  if ok then json = cj; return json end
  -- fallback: simple parser not available
  return nil
end

local function try_load_lfs()
  if lfs then return lfs end
  local ok, mod = pcall(require, 'lfs')
  if ok then lfs = mod; return lfs end
  return nil
end

-- small helpers
local function exists(path)
  local f = io.open(path, 'rb')
  if f then f:close(); return true end
  return false
end

local function mkdir(path)
  local ok, _ = pcall(function()
    if package.config:sub(1,1) == '\\' then
      os.execute('mkdir "'..path..'" >NUL 2>&1')
    else
      os.execute('mkdir -p "'..path..'"')
    end
  end)
  return ok
end

local function move(src, dst, dry)
  if dry then
    print(string.format('[DRY] -> move: %s -> %s', src, dst))
    return true
  end
  -- ensure target dir exists
  local ok = mkdir(dst:match('^(.*)[/\\]') or '.')
  local res, err = os.rename(src, dst)
  if res then return true end
  -- try copy+remove
  local in_f = io.open(src, 'rb')
  if not in_f then print('Failed to open '..src..' for copy: '..tostring(err)); return false end
  local out_f = io.open(dst, 'wb')
  if not out_f then in_f:close(); print('Failed to create '..dst); return false end
  out_f:write(in_f:read('*a'))
  out_f:close()
  in_f:close()
  os.remove(src)
  return true
end

local function read_file(path)
  local f = io.open(path, 'rb')
  if not f then return nil end
  local data = f:read('*a')
  f:close()
  return data
end

-- heuristics: extract game name from filename
local function guess_game_from_name(name)
  -- naive: look for parentheses or brackets or words separated by '.' or '_'
  -- try common patterns: Title (XYZ), Title [XYZ], Title.XXX
  local s = name:gsub('_', ' '):gsub('%.', ' ')
  -- strip extension
  s = s:gsub('%.[^%.]+$', '')
  -- try parenthesis
  local p = s:match('^(.-)%s*%(')
  if p and #p > 2 then return p end
  p = s:match('^(.-)%s*%[')
  if p and #p > 2 then return p end
  -- else take first 3 words
  local words = {}
  for w in s:gmatch('%S+') do table.insert(words, w) end
  local name_guess = table.concat({words[1] or '', words[2] or '', words[3] or ''}, ' ')
  return (name_guess:gsub('%s+$',''))
end

local function parse_saved_urls(path)
  if not exists(path) then return {} end
  local data = read_file(path)
  local j = load_json()
  if j then
    local ok, parsed = pcall(j.decode, data)
    if ok and type(parsed) == 'table' then
      return parsed
    end
  end
  -- naive fallback: try to find URLs and file names
  local urls = {}
  for url in data:gmatch('https?://[%w%p]+') do table.insert(urls, url) end
  return { urls = urls }
end

local function scan_and_organize(opts)
  local root = opts.root or '.'
  local dry = opts.dry
  local playtime = opts.playtime
  local history = opts.history

  print('Auto-folder tool starting. Root='..root)

  -- Build simple map of sources from history
  local hist = {}
  if history and exists(history) then
    local parsed = parse_saved_urls(history)
    -- crude: map host -> count
    if parsed then
      for k,v in pairs(parsed) do
        if type(v) == 'table' then
          -- handle arrays of entries
          for _,e in ipairs(v) do
            if type(e) == 'string' then
              local host = e:match('https?://([^/]+)')
              if host then hist[host] = (hist[host] or 0) + 1 end
            elseif type(e) == 'table' and e.url then
              local host = e.url:match('https?://([^/]+)')
              if host then hist[host] = (hist[host] or 0) + 1 end
            end
          end
        elseif type(v) == 'string' then
          local host = v:match('https?://([^/]+)')
          if host then hist[host] = (hist[host] or 0) + 1 end
        end
      end
    end
  end

  -- Walk the data directory (romfs or root) and collect files of interest
  local files = {}
  local lfsm = try_load_lfs()
  if lfsm then
    for file in lfsm.dir(root) do
      if file ~= '.' and file ~= '..' then
        local full = root..'/'..file
        local attr = lfsm.attributes(full)
        if attr and attr.mode == 'file' then
          table.insert(files, full)
        end
      end
    end
  else
    -- fallback: iterate using io.popen if available
    local p = io.popen('cmd /c dir "'..root..'" /b')
    if p then
      for line in p:lines() do table.insert(files, root..'/'..line) end
      p:close()
    end
  end

  -- classify and move
  for _, f in ipairs(files) do
    local name = f:match('[^/\\]+$')
    local lower = name:lower()
    if lower:match('%.nsp$') or lower:match('%.xci$') then
      -- NSP/XCI -> Unused NSPs if older than X days or not in playtime
      local game = guess_game_from_name(name)
      local dest = root..'/Unused NSPs/'..game..'/'..name
      print('Classify NSP: '..name..' -> '..dest)
      move(f, dest, dry)
    elseif lower:match('screenshot') or lower:match('%.jpg$') or lower:match('%.png$') then
      local game = guess_game_from_name(name)
      local dest = root..'/Screenshots/'..game..'/'..name
      print('Classify Screenshot: '..name..' -> '..dest)
      move(f, dest, dry)
    elseif lower:match('mod') or lower:match('%.zip$') or lower:match('%.7z$') then
      local game = guess_game_from_name(name)
      local dest = root..'/Mods/'..game..'/'..name
      print('Classify Mod: '..name..' -> '..dest)
      move(f, dest, dry)
    else
      -- downloads: try detect source from history by matching name
      local found_src = nil
      for host,_ in pairs(hist) do
        if name:lower():find(host:lower():gsub('%W','')) then found_src = host; break end
      end
      if found_src then
        local date = os.date('%Y-%m-%d')
        local dest = root..'/Downloads/'..found_src..'/'..date..'/'..name
        print('Classify Download: '..name..' -> '..dest)
        move(f, dest, dry)
      end
    end
  end

  -- prompts
  if not dry then
    io.write('\nWould you like to back up saves now? (y/N): ')
    local ans = io.read()
    if ans and ans:lower():match('^y') then
      print('Backing up saves... (not implemented automatically)')
      -- Placeholder: user can add actual logic here to copy save folders
    end

    io.write('Scan for unused NSPs and remove? (type CLEAN to proceed): ')
    ans = io.read()
    if ans and ans == 'CLEAN' then
      print('Cleaning Unused NSPs... (dry-run required to actually delete)')
      -- caution: deleting files is destructive; require more confirmation
    end
  else
    print('\nDry-run complete. No files were moved. Re-run without --dry to apply changes.')
  end
end

-- Simple CLI
local function parse_args()
  local opts = { root='.', dry=false }
  for i=1,#arg do
    local a = arg[i]
    if a == '--root' and arg[i+1] then opts.root = arg[i+1]; i = i+1 end
    if a == '--playtime' and arg[i+1] then opts.playtime = arg[i+1]; i = i+1 end
    if a == '--history' and arg[i+1] then opts.history = arg[i+1]; i = i+1 end
    if a == '--dry' or a == '-n' then opts.dry = true end
  end
  return opts
end

local ok, err = pcall(function()
  local opts = parse_args()
  scan_and_organize(opts)
end)
if not ok then print('Error: '..tostring(err)) end
