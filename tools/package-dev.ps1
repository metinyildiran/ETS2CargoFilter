param([string]$GameDirectory = 'D:\SteamLibrary\steamapps\common\Euro Truck Simulator 2', [switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'build-native.ps1') }
& node (Join-Path $PSScriptRoot 'build-ui.mjs') --game-dir $GameDirectory
if ($LASTEXITCODE -ne 0) { throw 'UI build failed.' }
$output = Join-Path $projectRoot 'dist\experimental'
[System.IO.Directory]::CreateDirectory($output) | Out-Null
$archivePath = Join-Path $output 'ETS2CargoFilter.scs'
Add-Type -AssemblyName System.IO.Compression
$stream = [System.IO.File]::Open($archivePath, [System.IO.FileMode]::Create)
try {
    $archive = [System.IO.Compression.ZipArchive]::new($stream, [System.IO.Compression.ZipArchiveMode]::Create, $true)
    try {
        $localeIds = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'localization/locales.json') | ConvertFrom-Json
        $packageFiles = @('manifest.sii', 'mod_icon.jpg', 'mod_description.txt', 'ui/cargo_market.sii')
        foreach ($localeId in $localeIds) {
            if ($localeId -notmatch '^[a-z]{2}_[a-z]{2}$') { throw 'Invalid locale identifier.' }
            $packageFiles += "locale/$localeId/local_module.ets2cargo_filter.sii"
            $packageFiles += "mod_description.$localeId.txt"
        }
        foreach ($relative in $packageFiles) {
            $entry = $archive.CreateEntry($relative, [System.IO.Compression.CompressionLevel]::Optimal)
            $entryStream = $entry.Open()
            try {
                $bytes = [System.IO.File]::ReadAllBytes((Join-Path $projectRoot "build\mod\$relative"))
                $entryStream.Write($bytes, 0, $bytes.Length)
            } finally { $entryStream.Dispose() }
        }
    } finally { $archive.Dispose() }
} finally { $stream.Dispose() }
Copy-Item -LiteralPath (Join-Path $projectRoot 'build\native\Release\ETS2CargoFilter.dll') -Destination $output -Force
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\TEST-BUILD.md') -Destination (Join-Path $output 'TEST-BUILD.md') -Force
$hashes = @{}
foreach ($name in @('ETS2CargoFilter.scs', 'ETS2CargoFilter.dll')) {
    $hashes[$name] = (Get-FileHash -LiteralPath (Join-Path $output $name) -Algorithm SHA256).Hash.ToLowerInvariant()
}
$hashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'sha256.json') -Encoding utf8
Write-Output "Experimental package: $output"
