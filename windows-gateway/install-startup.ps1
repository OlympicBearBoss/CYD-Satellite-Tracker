$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Python = (Get-Command pythonw.exe -ErrorAction SilentlyContinue).Source
if (-not $Python) { $Python = (Get-Command python.exe -ErrorAction SilentlyContinue).Source }
if (-not $Python) { throw "Python 3 is not installed or not in PATH. Install Python 3.11+ and retry." }
$Gateway = Join-Path $Root "gateway.py"
$Action = New-ScheduledTaskAction -Execute $Python -Argument ('"' + $Gateway + '"')
$Trigger = New-ScheduledTaskTrigger -AtLogOn -User $env:USERNAME
$Settings = New-ScheduledTaskSettingsSet -StartWhenAvailable -RestartCount 999 -RestartInterval (New-TimeSpan -Minutes 1) -ExecutionTimeLimit ([TimeSpan]::Zero)
Register-ScheduledTask -TaskName "CYD Satellite Tracker Gateway" -Action $Action -Trigger $Trigger -Settings $Settings -Description "Relays local CYD telemetry to the cloud" -Force | Out-Null
Write-Host "Installed. The gateway starts at the next Windows sign-in."
Write-Host "Run Start-ScheduledTask -TaskName 'CYD Satellite Tracker Gateway' to start now."
