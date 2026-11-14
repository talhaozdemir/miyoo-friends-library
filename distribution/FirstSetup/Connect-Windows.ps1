param([string]$SdRoot,[string]$RclonePath,[switch]$ReplaceAccount)
$ErrorActionPreference = 'Stop'
$workConfig = $null
try {
    $info = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'setup-info.json') -Raw | ConvertFrom-Json
    if ($info.client_type -ne 'desktop_public' -or $info.root_folder_id -notmatch '^[A-Za-z0-9_-]{1,159}$' -or $info.client_id -notmatch '\.apps\.googleusercontent\.com$' -or !$info.client_secret) { throw 'Package setup information is invalid. Ask the sender for a fresh ZIP.' }
    if (!$RclonePath) { $RclonePath = Join-Path $PSScriptRoot 'tools\windows\rclone.exe' }
    if (!(Test-Path -LiteralPath $RclonePath)) { throw 'Extract the entire ZIP before starting setup.' }
    if (!$SdRoot) { $SdRoot = Read-Host 'SD card drive letter (example E:)' }
    if ($SdRoot -match '^[A-Za-z]:?$') { $SdRoot = $SdRoot.TrimEnd(':') + ':\' }
    $sd = (Resolve-Path -LiteralPath $SdRoot).Path
    if (!(Test-Path -LiteralPath (Join-Path $sd 'App\FriendsLibrary\config.json'))) { throw 'Select the SD card ROOT after copying App to it. No account files were changed.' }
    $dataDir = Join-Path $sd '.config\friends-library'
    $configParent = Join-Path $sd '.config'
    if ((Test-Path -LiteralPath $configParent) -and ((Get-Item -LiteralPath $configParent).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Linked configuration directory is not supported.' }
    if ((Test-Path -LiteralPath $dataDir) -and ((Get-Item -LiteralPath $dataDir).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Linked configuration directory is not supported.' }
    New-Item -ItemType Directory -Force -Path $dataDir | Out-Null
    $credential = Join-Path $dataDir 'rclone.conf'
    if ((Test-Path -LiteralPath $credential) -and !$ReplaceAccount) {
        $answer = Read-Host 'Account already exists on this card. Reconnect? Type YES, or Enter to keep it'
        if ($answer -cne 'YES') { Write-Host 'Existing account preserved. No changes made.'; exit 0 }
    }
    $workConfig = Join-Path $dataDir ('.setup-' + [guid]::NewGuid().ToString('N') + '.conf')
    Write-Host 'Opening Google sign-in. Choose YOUR invited Google account.'
    Write-Host 'Wait for Success in the browser, then return here. Do not close this window.'
    $argsList = @('config','create','friends','drive','client_id',[string]$info.client_id,'client_secret',[string]$info.client_secret,
        'scope','drive.readonly','root_folder_id',[string]$info.root_folder_id,'config_is_local','true','config_change_team_drive','false',
        '--config',$workConfig,'--no-obscure','--no-output','--log-level','ERROR')
    # Never expose native configuration/token output or put it in a log file.
    & $RclonePath @argsList 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Google connection failed. Check your account, app access and internet connection; see the PDF.' }
    if (!(Test-Path -LiteralPath $workConfig)) { throw 'Google setup did not finish. Retry the connection.' }
    $account = Get-Content -LiteralPath $workConfig -Raw
    if ($account -notmatch '(?m)^scope\s*=\s*drive\.readonly\s*$' -or $account -notmatch '"refresh_token"\s*:\s*"[^"\s]+"') { throw 'Google did not provide an offline authorization. Reconnect and approve access.' }
    $settings = [ordered]@{root_folder_id=[string]$info.root_folder_id;remote='friends';cache_ttl_seconds=900;timeout_seconds=30}
    $settingsPath = Join-Path $dataDir 'settings.json'
    $settingsTemp = Join-Path $dataDir ('.settings-' + [guid]::NewGuid().ToString('N') + '.json')
    [IO.File]::WriteAllText($settingsTemp, ($settings | ConvertTo-Json) + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $workConfig -Destination $credential -Force
    $workConfig = $null
    Move-Item -LiteralPath $settingsTemp -Destination $settingsPath -Force
    Write-Host ''
    Write-Host 'READY. Eject the SD card safely, put it in the Miyoo and open Apps > Friends Library.'
} catch {
    # Only deliberate script errors are shown; native OAuth output stays private.
    if ($_.Exception.Message -match '^(Package|Extract|Select|Linked|Google)') { Write-Host $_.Exception.Message }
    else { Write-Host 'Setup could not finish. Check the SD drive, free space and write permission; see the PDF.' }
    exit 1
} finally {
    if ($workConfig -and (Test-Path -LiteralPath $workConfig)) { Remove-Item -LiteralPath $workConfig -Force }
}
