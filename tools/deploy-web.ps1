# Publishes the browser build at https://nakamaai.app/arcade/ : copies build/web/neko-arcade.html and the
# link-preview image into public/arcade of a clean nakama-backend worktree on main, commits only those files
# and pushes to main (Railway deploys on push). The main checkout of nakama-backend is never touched.
param([string]$Message = "feat(arcade): Neko Arcade playtest page at /arcade")
$ErrorActionPreference = 'Stop'
$src = 'C:\Users\tyler\M15-Backup\neko-arcade\build\web'
$wt = 'C:\Users\tyler\AppData\Local\Temp\nb-arcade'
if (-not (Test-Path "$wt\.git")) {
    git -C C:\Users\tyler\nakama-backend fetch origin main -q
    git -C C:\Users\tyler\nakama-backend worktree add -q -b arcade/playtest $wt origin/main
}
git -C $wt fetch origin main -q
git -C $wt merge --ff-only -q origin/main
New-Item -ItemType Directory -Force "$wt\public\arcade" | Out-Null
Copy-Item "$src\neko-arcade.html" "$wt\public\arcade\index.html" -Force
Copy-Item "$src\preview.png" "$wt\public\arcade\preview.png" -Force
git -C $wt add public/arcade/index.html public/arcade/preview.png
$staged = git -C $wt diff --cached --name-only
if (-not $staged) { "nothing changed"; return }
$body = "$Message`n`nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
git -C $wt commit -q -m $body
git -C $wt push origin HEAD:main
git -C $wt log --oneline -1
