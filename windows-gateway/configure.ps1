$ErrorActionPreference = "Stop"
$AppDir = Join-Path $env:LOCALAPPDATA "CYDSatelliteTracker"
New-Item -ItemType Directory -Force -Path $AppDir | Out-Null
$ConfigPath = Join-Path $AppDir "config.json"
$DefaultUrl = "http://cyd-tracker.local"
if (Test-Path $ConfigPath) {
    try {
        $Existing = Get-Content $ConfigPath -Raw | ConvertFrom-Json
        if ($Existing.cyd_url) { $DefaultUrl = [string]$Existing.cyd_url }
    } catch {}
}
$CydUrl = Read-Host "Локальный адрес CYD [$DefaultUrl]"
if ([string]::IsNullOrWhiteSpace($CydUrl)) { $CydUrl = $DefaultUrl }
$CydUrl = $CydUrl.Trim().TrimEnd("/")
if (-not $CydUrl.StartsWith("http://")) {
    throw "Нужен локальный HTTP-адрес, например http://192.168.0.48"
}
$Code = Read-Host "Одноразовый 8-значный код привязки с сайта"
if ($Code -notmatch '^\d{8}$') { throw "Код должен содержать ровно 8 цифр." }
$Name = Read-Host "Имя устройства [CYD Satellite Tracker]"
if ([string]::IsNullOrWhiteSpace($Name)) { $Name = "CYD Satellite Tracker" }
$Config = @{
    cyd_url = $CydUrl
    cloud_url = "https://cyd-satellite-tracker.pages.dev"
    device_name = $Name
    pairing_code = $Code
}
$Config | ConvertTo-Json | Set-Content -Path $ConfigPath -Encoding UTF8
Write-Host "Конфигурация сохранена: $ConfigPath"
Write-Host "Запустите gateway.py. Код привязки будет использован один раз и удалён из конфигурации."
