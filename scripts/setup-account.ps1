param(
    [Parameter(Mandatory=$true)][string]$SdRoot,
    [Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]{1,159}$')][string]$FolderId,
    [string]$Rclone,
    [switch]$ReplaceSettings
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$Rclone) { $Rclone = Join-Path $projectRoot 'downloads/rclone-v1.76.0-windows-amd64/rclone.exe' }
if (!(Test-Path -LiteralPath $Rclone)) { throw 'Run scripts/fetch-deps.ps1 -WithWindowsRclone, or provide -Rclone.' }
$sd = (Resolve-Path -LiteralPath $SdRoot).Path
$dataDir = Join-Path $sd '.config/friends-library'
$settings = Join-Path $dataDir 'settings.json'
if ((Test-Path -LiteralPath $settings) -and !$ReplaceSettings) {
    throw 'Settings already exist. Use -ReplaceSettings only when you intend to change the library.'
}
New-Item -ItemType Directory -Force -Path $dataDir | Out-Null
Write-Host 'Read docs/AUTHENTICATION.md first. Create/edit ONLY the friends remote.'
Write-Host 'Use your own Desktop OAuth client and scope drive.readonly. Do not enable config encryption.'
Write-Host 'Choose a fresh authorization if an existing token had broader permissions.'
& $Rclone config --config (Join-Path $dataDir 'rclone.conf')
if ($LASTEXITCODE -ne 0) { throw 'rclone configuration failed. No settings were changed.' }
$text = Get-Content -LiteralPath (Join-Path $dataDir 'rclone.conf') -Raw
$remoteMatch = [regex]::Match($text, '(?ms)^\[friends\]\s*\r?\n(?<section>.*?)(?=^\[|\z)')
$section = $remoteMatch.Groups['section'].Value
if (!$remoteMatch.Success -or $section -notmatch '(?m)^scope\s*=\s*drive\.readonly\s*$' -or $section -notmatch '(?m)^type\s*=\s*drive\s*$' -or $section -notmatch '(?m)^client_id\s*=\s*\S+' -or $section -notmatch '"refresh_token"\s*:\s*"[^"\s]+"') {
    throw 'A friends Drive remote with drive.readonly, personal client_id and refresh_token is required. Credentials were kept for correction; never post them in chat.'
}
$data = [ordered]@{ root_folder_id=$FolderId; remote='friends'; cache_ttl_seconds=900; timeout_seconds=30 }
$settingsJson = $data | ConvertTo-Json
[IO.File]::WriteAllText($settings, $settingsJson + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
Write-Host 'Account configuration is on the named SD card. Eject it safely. No token was printed by this script.'
