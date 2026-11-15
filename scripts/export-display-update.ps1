$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zipPath = Join-Path $projectRoot 'dist/Miyoo-Friends-Library-v0.3.0-app-update.zip'
$stream = [IO.File]::Open($zipPath,[IO.FileMode]::Create)
$archive = [IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create)
try {
    [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,
        (Join-Path $projectRoot 'build/friends-library-arm'),
        'App/FriendsLibrary/friends-library',[IO.Compression.CompressionLevel]::Optimal) | Out-Null
    [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,
        (Join-Path $projectRoot 'package/App/FriendsLibrary/icon.png'),
        'App/FriendsLibrary/icon.png',[IO.Compression.CompressionLevel]::Optimal) | Out-Null
    foreach($pair in @(@('docs/PHONE_SETUP.html','docs/PHONE_SETUP.html'),@('vendor/qrcodegen/LICENSE','licenses/qrcodegen-MIT.txt'),@('THIRD_PARTY_NOTICES.md','licenses/THIRD_PARTY_NOTICES.md'))){
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,(Join-Path $projectRoot $pair[0]),('App/FriendsLibrary/'+$pair[1]),[IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $archive.Dispose(); $stream.Dispose() }
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zipPath).Hash.ToLowerInvariant()
Set-Content -LiteralPath "$zipPath.sha256" -Encoding utf8 -Value "$hash  $([IO.Path]::GetFileName($zipPath))"
Write-Output "Created application update: $zipPath"
