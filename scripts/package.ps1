$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$appDir = Join-Path $projectRoot 'package/App/FriendsLibrary'
$binary = Join-Path $projectRoot 'build/friends-library-arm'
if (!(Test-Path -LiteralPath $binary)) { throw 'First build the ARM application with scripts/build.ps1.' }
foreach ($required in @('bin/rclone', 'certs/cacert.pem', 'launch.sh', 'config.json', 'config/systems.json', 'icon.png')) {
    if (!(Test-Path -LiteralPath "$appDir/$required")) { throw "Missing package file: $required" }
}
Copy-Item -LiteralPath $binary -Destination "$appDir/friends-library" -Force
Copy-Item -LiteralPath "$projectRoot/LICENSE" -Destination "$appDir/licenses/FriendsLibrary-MIT.txt" -Force
Copy-Item -LiteralPath "$projectRoot/THIRD_PARTY_NOTICES.md" -Destination "$appDir/licenses/THIRD_PARTY_NOTICES.md" -Force
Copy-Item -LiteralPath "$projectRoot/THIRD_PARTY_NOTICES.md" -Destination "$appDir/THIRD_PARTY_NOTICES.md" -Force
Copy-Item -LiteralPath "$projectRoot/README.md" -Destination "$appDir/README.md" -Force
New-Item -ItemType Directory -Force -Path "$appDir/docs" | Out-Null
Get-ChildItem -LiteralPath "$projectRoot/docs" -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination "$appDir/docs/$($_.Name)" -Force
}
if (Test-Path -LiteralPath "$projectRoot/vendor/cjson/LICENSE") {
    Copy-Item -LiteralPath "$projectRoot/vendor/cjson/LICENSE" -Destination "$appDir/licenses/cJSON-MIT.txt" -Force
}
Copy-Item -LiteralPath "$projectRoot/vendor/qrcodegen/LICENSE" -Destination "$appDir/licenses/qrcodegen-MIT.txt" -Force
if (Test-Path -LiteralPath "$projectRoot/config.example.json") {
    Copy-Item -LiteralPath "$projectRoot/config.example.json" -Destination "$appDir/config.example.json" -Force
}
foreach ($file in Get-ChildItem -LiteralPath $appDir -Recurse -File) {
    if ($file.Name -match '(?i)^(rclone\.conf|settings\.json|token.*|credentials.*)$' -or $file.Extension -eq '.conf') {
        throw "Sensitive configuration is forbidden in the package: $($file.FullName)"
    }
    if ($file.Extension -in @('.json','.sh','.md','.txt')) {
        $contents = Get-Content -LiteralPath $file.FullName -Raw
        if ($contents -match '"(access_token|refresh_token)"\s*:\s*"[^"<>]+"') {
            throw "Possible OAuth token in package: $($file.FullName)"
        }
    }
}
$distDir = Join-Path $projectRoot 'dist'
New-Item -ItemType Directory -Force -Path $distDir | Out-Null
$zip = Join-Path $distDir 'Miyoo-Friends-Library-v0.3.0-OnionOS-4.3.zip'
Compress-Archive -LiteralPath "$projectRoot/package/App" -DestinationPath $zip -Force
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash.ToLowerInvariant()
Set-Content -LiteralPath "$zip.sha256" -Encoding utf8 -Value "$hash  $([IO.Path]::GetFileName($zip))"
& "$PSScriptRoot/test-package.ps1" -ZipPath $zip
Write-Output "Created: $zip"
Write-Output "SHA256: $hash"
