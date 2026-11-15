$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$distDir = Join-Path $projectRoot 'dist'
New-Item -ItemType Directory -Force -Path $distDir | Out-Null
$zipPath = Join-Path $distDir 'Miyoo-Friends-Library-v0.3.0-source.zip'
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
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,(Join-Path $projectRoot $relative),$relative,[IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $archive.Dispose(); $stream.Dispose() }
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zipPath).Hash.ToLowerInvariant()
Set-Content -LiteralPath "$zipPath.sha256" -Encoding utf8 -Value "$hash  $([IO.Path]::GetFileName($zipPath))"
Write-Output "Created source archive: $zipPath"
