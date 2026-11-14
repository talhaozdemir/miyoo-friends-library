$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$base = Join-Path $repo ('build\friends setup test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path "$base\FirstSetup", "$base\sd\App\FriendsLibrary" | Out-Null
Copy-Item -LiteralPath "$repo\distribution\FirstSetup\Connect-Windows.ps1" -Destination "$base\FirstSetup\Connect-Windows.ps1"
[IO.File]::WriteAllText("$base\FirstSetup\setup-info.json", '{"schema_version":1,"client_type":"desktop_public","root_folder_id":"FAKE_FOLDER","client_id":"fake.apps.googleusercontent.com","client_secret":"FAKE_DESKTOP_METADATA"}')
[IO.File]::WriteAllText("$base\sd\App\FriendsLibrary\config.json", '{}')
$code = @'
using System;
using System.IO;
public class MockRclone {
  public static int Main(string[] args) {
    string target=null; bool noOutput=false;
    for(int i=0;i<args.Length;i++){if(args[i]=="--config")target=args[i+1];if(args[i]=="--no-output")noOutput=true;}
    if(target==null||!noOutput)return 7;
    Console.WriteLine("NEVER_LOG_FAKE_TOKEN");
    string mode=Environment.GetEnvironmentVariable("FL_SETUP_TEST_MODE");
    if(mode=="fail"){Console.Error.WriteLine("NEVER_LOG_FAKE_TOKEN");return 1;}
    string token=mode=="missing"?"{}":"{\"refresh_token\":\"FAKE_REFRESH_TEST_ONLY\"}";
    File.WriteAllText(target,"[friends]\ntype=drive\nscope=drive.readonly\nclient_id=fake.apps.googleusercontent.com\ntoken="+token+"\n");
    return 0;
  }
}
'@
$mock = "$base\mock-rclone.exe"
Add-Type -TypeDefinition $code -OutputAssembly $mock -OutputType ConsoleApplication
$ps = "$env:WINDIR\System32\WindowsPowerShell\v1.0\powershell.exe"
$script = "$base\FirstSetup\Connect-Windows.ps1"
$env:FL_SETUP_TEST_MODE = 'ok'
$output = & $ps -NoProfile -ExecutionPolicy Bypass -File $script -SdRoot "$base\sd" -RclonePath $mock 2>&1
if ($LASTEXITCODE -ne 0 -or ($output -join "`n") -match 'NEVER_LOG_FAKE_TOKEN') { throw 'Successful setup/token redaction failed' }
$settingsPath = "$base\sd\.config\friends-library\settings.json"
$bytes = [IO.File]::ReadAllBytes($settingsPath)
if ($bytes[0] -eq 239) { throw 'Unexpected UTF8 BOM' }
$settings = [IO.File]::ReadAllText($settingsPath) | ConvertFrom-Json
if ($settings.root_folder_id -ne 'FAKE_FOLDER' -or $settings.remote -ne 'friends') { throw 'Wrong settings' }
$credential = "$base\sd\.config\friends-library\rclone.conf"
$saved = [IO.File]::ReadAllText($credential)
foreach ($mode in @('fail','missing')) {
    $env:FL_SETUP_TEST_MODE = $mode
    $output = & $ps -NoProfile -ExecutionPolicy Bypass -File $script -SdRoot "$base\sd" -RclonePath $mock -ReplaceAccount 2>&1
    if ($LASTEXITCODE -eq 0 -or ($output -join "`n") -match 'NEVER_LOG_FAKE_TOKEN') { throw 'Failed setup was not safely reported' }
    if ([IO.File]::ReadAllText($credential) -ne $saved) { throw 'Existing account was modified on failure' }
}
if (Get-ChildItem -LiteralPath "$base\sd\.config\friends-library" -Filter '.setup-*') { throw 'Staging credential was not cleaned' }
Write-Output 'Friends setup: Windows PowerShell 5.1 passed; success, UTF8, secret-output suppression, failure preservation and cleanup verified with fake credentials.'
