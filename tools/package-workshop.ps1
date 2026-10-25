param([string]$GameDirectory = 'D:\SteamLibrary\steamapps\common\Euro Truck Simulator 2', [switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'package-dev.ps1') -GameDirectory $GameDirectory -SkipBuild:$SkipBuild
if ($LASTEXITCODE -ne 0) { throw 'Package build failed.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$output = Join-Path $projectRoot 'dist/workshop'
$content = Join-Path $output 'content'
$package = Join-Path $content 'ets2_16119'
if (Test-Path -LiteralPath $content) {
    # Reject stale/extra files rather than risking shipping unexpected contents.
    throw 'dist/workshop/content already exists. Archive the previous output before rebuilding.'
}
[System.IO.Directory]::CreateDirectory($package) | Out-Null
[System.IO.Compression.ZipFile]::ExtractToDirectory((Join-Path $projectRoot 'dist/experimental/ETS2CargoFilter.scs'), $package)
$manifestPath = Join-Path $package 'manifest.sii'
$manifest = [System.IO.File]::ReadAllText($manifestPath)
$manifest = [regex]::Replace($manifest, '(?m)^\s*(display_name|compatible_versions\[\]):[^\r\n]*\r?\n', '')
$utf8 = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText($manifestPath, $manifest, $utf8)
$versions = @'
SiiNunit
{
package_version_info : .universal
{
 package_name: "unsupported"
}
package_version_info : .ets2_16119
{
 package_name: "ets2_16119"
 compatible_versions[]: "1.61.1.9"
}
}
'@
[System.IO.File]::WriteAllText((Join-Path $content 'versions.sii'), $versions, $utf8)
$fallback = Join-Path $content 'unsupported'
[System.IO.Directory]::CreateDirectory($fallback) | Out-Null
Copy-Item -LiteralPath $manifestPath -Destination $fallback
Copy-Item -LiteralPath (Join-Path $package 'mod_icon.jpg') -Destination $fallback
Copy-Item -LiteralPath (Join-Path $projectRoot 'workshop/unsupported.txt') -Destination (Join-Path $fallback 'mod_description.txt')
Copy-Item -LiteralPath (Join-Path $projectRoot 'workshop/preview.jpg') -Destination $output
Copy-Item -LiteralPath (Join-Path $projectRoot 'workshop/description.txt') -Destination $output

$staging = Join-Path $projectRoot 'build/release-r4'
if (Test-Path -LiteralPath $staging) { throw 'build/release-r4 already exists; archive it before rebuilding.' }
[System.IO.Directory]::CreateDirectory($staging) | Out-Null
foreach ($name in @('ETS2CargoFilter.dll', 'ETS2CargoFilter.scs', 'sha256.json')) {
    Copy-Item -LiteralPath (Join-Path $projectRoot "dist/experimental/$name") -Destination $staging
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'workshop/INSTALL.txt') -Destination $staging
Copy-Item -LiteralPath (Join-Path $projectRoot 'vendor/scs-sdk-1.16/sdk_license.txt') -Destination $staging
Copy-Item -LiteralPath (Join-Path $projectRoot 'tools/THIRD_PARTY_NOTICES.md') -Destination $staging
$zipPath = Join-Path $projectRoot 'dist/ETS2CargoFilter-1.61.1.9-r4.zip'
[System.IO.Compression.ZipFile]::CreateFromDirectory($staging, $zipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false)
$hash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
[System.IO.File]::WriteAllText((Join-Path $projectRoot 'dist/SHA256SUMS-r4.txt'), "$hash  ETS2CargoFilter-1.61.1.9-r4.zip`n", $utf8)
Write-Output "Workshop data: $content"
Write-Output "Release: $zipPath"
