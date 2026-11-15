$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$distDir = Join-Path $projectRoot 'dist'
New-Item -ItemType Directory -Force -Path $distDir | Out-Null
$zipPath = Join-Path $distDir 'Miyoo-Friends-Library-v0.3.1-source.zip'
$relativePaths = @('Makefile','README.md','LICENSE','THIRD_PARTY_NOTICES.md','config.example.json','.gitignore','.gitattributes')
foreach ($directory in @('src','include','vendor','tests','scripts','docs','assets','distribution','sites')) {
    $relativePaths += Get-ChildItem -LiteralPath (Join-Path $projectRoot $directory) -File -Recurse | ForEach-Object {
        [IO.Path]::GetRelativePath($projectRoot, $_.FullName).Replace('\','/')
    }
}
$relativePaths += @('package/App/FriendsLibrary/config.json','package/App/FriendsLibrary/launch.sh','package/App/FriendsLibrary/icon.png','package/App/FriendsLibrary/config/systems.json')
$relativePaths += @('auth-service/package.json','auth-service/package-lock.json','auth-service/wrangler.jsonc','auth-service/tsconfig.json','auth-service/.gitignore','auth-service/.dev.vars.example','auth-service/README.md')
foreach ($directory in @('auth-service/src','auth-service/test','auth-service/migrations')) {
    $relativePaths += Get-ChildItem -LiteralPath (Join-Path $projectRoot $directory) -File -Recurse | ForEach-Object {
        [IO.Path]::GetRelativePath($projectRoot, $_.FullName).Replace('\','/')
    }
}
$relativePaths += Get-ChildItem -LiteralPath "$projectRoot/package/App/FriendsLibrary/licenses" -File | ForEach-Object {
    [IO.Path]::GetRelativePath($projectRoot, $_.FullName).Replace('\','/')
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$stream = [IO.File]::Open($zipPath,[IO.FileMode]::Create)
$archive = [IO.Compression.ZipArchive]::new($stream,[IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($relative in ($relativePaths | Sort-Object -Unique)) {
        if ($relative -match '(?i)(\.conf$|(^|/)settings\.json$|\.part$)') { throw "Forbidden source path: $relative" }
        $entry = [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,(Join-Path $projectRoot $relative),$relative,[IO.Compression.CompressionLevel]::Optimal)
        $entry.ExternalAttributes = $(if ($relative -match '\.(sh|command)$') { [int]0x81ED -shl 16 } else { [int]0x81A4 -shl 16 })
    }
} finally { $archive.Dispose(); $stream.Dispose() }
# .NET on Windows marks ZIP entries as DOS even with Unix mode bits. Set the
# central directory's creator OS to Unix so Linux unzip honors executable scripts.
# This source archive is deliberately small (no ZIP64 or archive comment).
$bytes = [IO.File]::ReadAllBytes($zipPath)
$end = $bytes.Length - 22
if ($end -lt 0 -or [BitConverter]::ToUInt32($bytes,$end) -ne 0x06054b50) { throw 'Unexpected source ZIP trailer.' }
$count = [BitConverter]::ToUInt16($bytes,$end+10)
$offset = [int][BitConverter]::ToUInt32($bytes,$end+16)
for ($i=0; $i -lt $count; $i++) {
    if ($offset -lt 0 -or $offset+46 -gt $end -or [BitConverter]::ToUInt32($bytes,$offset) -ne 0x02014b50) { throw 'Invalid source ZIP directory.' }
    $bytes[$offset+5] = 3
    $offset += 46 + [BitConverter]::ToUInt16($bytes,$offset+28) + [BitConverter]::ToUInt16($bytes,$offset+30) + [BitConverter]::ToUInt16($bytes,$offset+32)
}
if ($offset -ne $end) { throw 'Unexpected source ZIP directory size.' }
[IO.File]::WriteAllBytes($zipPath,$bytes)
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zipPath).Hash.ToLowerInvariant()
Set-Content -LiteralPath "$zipPath.sha256" -Encoding utf8 -Value "$hash  $([IO.Path]::GetFileName($zipPath))"
Write-Output "Created source archive: $zipPath"
