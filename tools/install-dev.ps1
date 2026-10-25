param(
    [Parameter(Mandatory=$true)][string]$GameDirectory,
    [string]$UserDirectory = (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Euro Truck Simulator 2'),
    [Parameter(Mandatory=$true)][string]$SteamProfileDirectory
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path -LiteralPath $SteamProfileDirectory -PathType Container)) {
    throw 'Specify the existing Steam cloud profile directory to back up; no files installed.'
}
if (Get-Process -Name eurotrucks2 -ErrorAction SilentlyContinue) { throw 'Save and close ETS2 before installing the test build.' }
$executable = Join-Path $GameDirectory 'bin\win_x64\eurotrucks2.exe'
if ((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'b70c2357cc56eb5834ee47fea3c60b2f2d4591383ff2ac479cfd3cd254301b8c') {
    throw 'The installed game does not match the supported executable.'
}
$package = Join-Path $projectRoot 'dist\experimental'
$hashes = Get-Content -Raw -LiteralPath (Join-Path $package 'sha256.json') | ConvertFrom-Json
foreach ($name in @('ETS2CargoFilter.dll', 'ETS2CargoFilter.scs')) {
    if ((Get-FileHash -LiteralPath (Join-Path $package $name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $hashes.$name) {
        throw "Package verification failed: $name"
    }
}
$backupRoot = Join-Path $projectRoot ('build\backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
[System.IO.Directory]::CreateDirectory($backupRoot) | Out-Null
# Copy only this game's profile/configuration data, before placing any new files.
foreach ($relative in @('profiles', 'steam_profiles', 'config.cfg', 'global_controls.sii')) {
    $source = Join-Path $UserDirectory $relative
    if (Test-Path -LiteralPath $source) { Copy-Item -LiteralPath $source -Destination $backupRoot -Recurse }
}
if (Test-Path -LiteralPath $SteamProfileDirectory) {
    Copy-Item -LiteralPath $SteamProfileDirectory -Destination (Join-Path $backupRoot 'steam-cloud-profile') -Recurse
} else { throw 'Steam profile source not found; no files installed.' }
$pluginDestination = Join-Path $GameDirectory 'bin\win_x64\plugins\ETS2CargoFilter.dll'
$modDestination = Join-Path $UserDirectory 'mod\ETS2CargoFilter.scs'
$targets = @(
    @{ source = (Join-Path $package 'ETS2CargoFilter.dll'); destination = $pluginDestination },
    @{ source = (Join-Path $package 'ETS2CargoFilter.scs'); destination = $modDestination }
)
foreach ($item in $targets) {
    [System.IO.Directory]::CreateDirectory((Split-Path -Parent $item.destination)) | Out-Null
    if (Test-Path -LiteralPath $item.destination) {
        Copy-Item -LiteralPath $item.destination -Destination (Join-Path $backupRoot ([System.IO.Path]::GetFileName($item.destination)))
    }
    Copy-Item -LiteralPath $item.source -Destination $item.destination -Force
    if ((Get-FileHash -LiteralPath $item.source).Hash -ne (Get-FileHash -LiteralPath $item.destination).Hash) {
        throw "Installed copy verification failed: $($item.destination)"
    }
}
@{ backup = $backupRoot; plugin = $pluginDestination; mod = $modDestination; installed = (Get-Date).ToString('o') } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $backupRoot 'install.json') -Encoding utf8
Write-Output "Installed experimental package. Profile backup: $backupRoot"
