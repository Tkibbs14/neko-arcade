# Installs Neko Arcade on the M15 card: the stick core as cubegm/cores/nekoarcade_libretro.so, a launcher
# file roms/nekoarcade/Neko Arcade.neko, a core_overrides.txt line mapping that folder to the core and an
# ext_filters.txt line so only the launcher shows. Both config files are backed up first
# (M15-Backup\2026-09-27\pre-nekoarcade) and every copied file is verified by SHA-256.
# Undo: copy the two backed-up files back and delete the core and the roms/nekoarcade folder.
$ErrorActionPreference = 'Stop'
$root = 'C:\Users\tyler\M15-Backup\neko-arcade'
$core = "$root\build\stick\nekoarcade_libretro.so"
if (-not (Test-Path 'E:\cubegm\icube.stock')) { throw 'E: is not the M15 card' }
if (-not (Test-Path $core)) { throw "build the stick core first: $core" }
$utf8 = New-Object System.Text.UTF8Encoding($false)

# Back up the two FrogUI config files we change.
$bk = 'C:\Users\tyler\M15-Backup\2026-09-27\pre-nekoarcade\frogui'
New-Item -ItemType Directory -Force $bk | Out-Null
foreach ($f in 'core_overrides.txt', 'ext_filters.txt') {
    if (-not (Test-Path "$bk\$f")) { Copy-Item "E:\frogui\$f" "$bk\$f" }
}

# Core and launcher, verified.
function Copy-Verified($src, $dst) {
    New-Item -ItemType Directory -Force (Split-Path $dst) | Out-Null
    Copy-Item -LiteralPath $src $dst -Force
    if ((Get-FileHash -LiteralPath $src).Hash -ne (Get-FileHash -LiteralPath $dst).Hash) { throw "verify failed: $dst" }
    "{0}  {1:N0} bytes  ok" -f $dst, (Get-Item -LiteralPath $dst).Length
}
Copy-Verified $core 'E:\cubegm\cores\nekoarcade_libretro.so'
$launcher = "$root\build\stick\Neko Arcade.neko"
[IO.File]::WriteAllText($launcher, "Neko Arcade - Nia's builds. Start this file to play.`n", $utf8)
Copy-Verified $launcher 'E:\roms\nekoarcade\Neko Arcade.neko'

# Map the folder to the core, and show only .neko files in it (LF line endings, no BOM).
function Set-Line($path, $pattern, $line) {
    $lines = @(Get-Content $path -Encoding UTF8 | Where-Object { $_ -and $_ -notmatch $pattern })
    $lines += $line
    [IO.File]::WriteAllText($path, (($lines -join "`n") + "`n"), $utf8)
}
Set-Line 'E:\frogui\core_overrides.txt' '^/mnt/sdcard/roms/nekoarcade\|' '/mnt/sdcard/roms/nekoarcade|/mnt/sdcard/cubegm/cores/nekoarcade_libretro.so'
Set-Line 'E:\frogui\ext_filters.txt' '^nekoarcade\|' 'nekoarcade|1|neko'
foreach ($f in 'core_overrides.txt', 'ext_filters.txt') {
    Copy-Item "E:\frogui\$f" "C:\Users\tyler\M15-Backup\m15-mod\test7\frogui\$f" -Force
}
"core_overrides.txt: " + (Select-String -Path E:\frogui\core_overrides.txt -Pattern 'nekoarcade').Line
"ext_filters.txt:    " + (Select-String -Path E:\frogui\ext_filters.txt -Pattern 'nekoarcade').Line
$v = Get-Volume -DriveLetter E; "card free: {0:N2} GB" -f ($v.SizeRemaining / 1GB)
