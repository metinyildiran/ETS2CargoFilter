param([Parameter(Mandatory=$true)][int]$TargetProcessId, [switch]$GeneratorPreflight)
$ErrorActionPreference = 'Stop'
$gameProcess = Get-Process -Id $TargetProcessId
$gameModule = $gameProcess.MainModule
$expectedPath = 'D:\SteamLibrary\steamapps\common\Euro Truck Simulator 2\bin\win_x64\eurotrucks2.exe'
if ($gameModule.FileName -ne $expectedPath) { throw 'The selected process is not the configured ETS2 executable.' }
$digest = (Get-FileHash -LiteralPath $expectedPath -Algorithm SHA256).Hash
if ($digest -ne 'B70C2357CC56EB5834EE47FEA3C60B2F2D4591383FF2AC479CFD3CD254301B8C') { throw 'Unrecognized build.' }
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class CargoReadOnlyProbe {
  [DllImport("kernel32.dll", SetLastError=true)] public static extern IntPtr OpenProcess(uint rights, bool inherit, int pid);
  [DllImport("kernel32.dll", SetLastError=true)] public static extern bool ReadProcessMemory(IntPtr process, IntPtr address, byte[] buffer, UIntPtr size, out UIntPtr count);
  [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
  public static byte[] Read(IntPtr process, long address, int size) {
    if(address < 65536 || size < 0 || size > 65536) throw new ArgumentOutOfRangeException();
    var bytes = new byte[size]; UIntPtr got;
    if(!ReadProcessMemory(process, new IntPtr(address), bytes, new UIntPtr((uint)size), out got) || got.ToUInt64() != (ulong)size) throw new InvalidOperationException("Unreadable game data");
    return bytes;
  }
}
'@
$gameHandle = [CargoReadOnlyProbe]::OpenProcess(0x1010, $false, $TargetProcessId)
if ($gameHandle -eq [IntPtr]::Zero) { throw 'Cannot open game for read-only inspection.' }
try {
  function Read-U64([long]$Address) { [BitConverter]::ToInt64([CargoReadOnlyProbe]::Read($gameHandle, $Address, 8), 0) }
  function Read-I32([long]$Address) { [BitConverter]::ToInt32([CargoReadOnlyProbe]::Read($gameHandle, $Address, 4), 0) }
  function Hex([long]$Value) { '0x' + $Value.ToString('x') }
  $imageBase = $gameModule.BaseAddress.ToInt64()
  $economy = Read-U64 ($imageBase + 0x3046780)
  $player = Read-U64 ($economy + 0x18)
  $vehiclesData = Read-U64 ($player + 0x78)
  $vehiclesCount = Read-U64 ($player + 0x80)
  if ($vehiclesCount -lt 1 -or $vehiclesCount -gt 32) { throw 'No stable player vehicle snapshot.' }
  $vehicle = Read-U64 $vehiclesData
  $activeTrailer = Read-U64 ($vehicle + 0x48)
  if ($GeneratorPreflight) {
    $truck = Read-U64 ($vehicle + 0x10)
    $accessoryArray = Read-U64 ($truck + 0x38)
    $accessoryData = Read-U64 ($accessoryArray + 8)
    $accessoryCount = Read-U64 ($accessoryArray + 0x10)
    if ($accessoryCount -lt 0 -or $accessoryCount -gt 4096) { throw 'Unexpected accessory collection.' }
    $coldAccessories = 0
    for ($i = 0; $i -lt $accessoryCount; $i++) {
      $accessory = Read-U64 ($accessoryData + $i * 8)
      $cache = Read-U64 ($accessory + 0x38)
      if ($cache -eq 0 -or (Read-I32 ($cache + 8)) -ge 0) { $coldAccessories++ }
    }
    $companyData = Read-U64 ($economy + 0x28)
    $companyCount = Read-U64 ($economy + 0x30)
    if ($companyCount -lt 0 -or $companyCount -gt 20000) { throw 'Unexpected company collection.' }
    $eligible = 0
    $shortSeeds = 0
    $wrongArrayType = 0
    $coldCities = 0
    for ($i = 0; $i -lt $companyCount; $i++) {
      $company = Read-U64 ($companyData + $i * 8)
      $discovered = [CargoReadOnlyProbe]::Read($gameHandle, $company + 0xf8, 1)[0]
      $city = Read-U64 ($company + 0x100)
      if ($city -eq 0 -or (Read-I32 ($city + 8)) -ge 0) { $coldCities++ }
      if ($discovered -eq 0 -or (Read-I32 ($company + 0x70)) -eq 2) { continue }
      $eligible++
      if ((Read-U64 ($company + 0x50)) -lt 10) { $shortSeeds++ }
      if ((Read-U64 ($company + 0xa0)) -ne ($imageBase + 0x21d88d8)) { $wrongArrayType++ }
    }
    $scheduler = Read-U64 ($imageBase + 0x36ad978)
    $defaultScheduler = (Read-U64 ($scheduler + 8)) -eq ($imageBase + 0x167e00) -and (Read-U64 ($scheduler + 0x28)) -eq ($imageBase + 0x167e40)
    [pscustomobject]@{source='ReadProcessMemory only; no process writes or execution';timestamp=(Get-Date -Format o);accessoryCount=$accessoryCount;coldAccessories=$coldAccessories;companyCount=$companyCount;eligibleCompanies=$eligible;companiesWithShortSeeds=$shortSeeds;wrongArrayTypes=$wrongArrayType;coldCities=$coldCities;defaultScheduler=$defaultScheduler} | ConvertTo-Json
    return
  }
  $trailerData = Read-U64 ($player + 0x1b0)
  $trailerCount = Read-U64 ($player + 0x1b8)
  if ($trailerCount -lt 0 -or $trailerCount -gt 4096) { throw 'Invalid trailer count.' }
  $trailers = @()
  for ($i = 0; $i -lt $trailerCount; $i++) {
    $trailer = Read-U64 ($trailerData + $i * 8)
    if ($trailer -eq 0) { continue }
    $definition = Read-U64 ($trailer + 0x1a0)
    $plateData = Read-U64 ($trailer + 0x78)
    $plateLength = [BitConverter]::ToInt32([CargoReadOnlyProbe]::Read($gameHandle, $trailer + 0x80, 4),0)
    $plate = ''
    if ($plateLength -gt 0 -and $plateLength -lt 256 -and $plateData -gt 65536) {
      $plate = [Text.Encoding]::UTF8.GetString([CargoReadOnlyProbe]::Read($gameHandle,$plateData,$plateLength))
    }
    $trailers += [pscustomobject]@{index=$i;address=(Hex $trailer);definition=(Hex $definition);active=($trailer -eq $activeTrailer);plate=$plate}
  }
  if ($player -ne (Read-U64 ($economy+0x18)) -or $trailerCount -ne (Read-U64 ($player+0x1b8)) -or $trailerData -ne (Read-U64 ($player+0x1b0))) { throw 'Snapshot changed; do not use it.' }
  [pscustomobject]@{source='ReadProcessMemory only; no process writes or execution';timestamp=(Get-Date -Format o);pid=$TargetProcessId;image=(Hex $imageBase);economy=(Hex $economy);player=(Hex $player);vehicle=(Hex $vehicle);activeTrailer=(Hex $activeTrailer);trailerCount=$trailerCount;trailers=$trailers} | ConvertTo-Json -Depth 5
} finally { [void][CargoReadOnlyProbe]::CloseHandle($gameHandle) }
