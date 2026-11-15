param([Parameter(Mandatory=$true)][string]$ZipPath)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead((Resolve-Path -LiteralPath $ZipPath).Path)
try {
    $names = @($zip.Entries | ForEach-Object { $_.FullName.Replace('\','/') })
    foreach ($name in $names) {
        if ((!$name.StartsWith('App/FriendsLibrary/') -and !$name.StartsWith('FirstSetup/')) -or $name.Contains('../') -or $name.StartsWith('/')) {
            throw "Unexpected archive path: $name"
        }
        if ($name -match '(?i)(rclone\.conf|/settings\.json|\.part$|/Roms/|/BIOS/|/Saves/|/PHONE_SETUP\.html$|/phone-setup\.json$)') {
            throw "Forbidden package path: $name"
        }
    }
    foreach ($required in @('config.json','launch.sh','friends-library','config/systems.json','icon.png','bin/rclone','certs/cacert.pem')) {
        if ("App/FriendsLibrary/$required" -notin $names) { throw "Missing archive path: $required" }
    }
    foreach ($binaryPath in @('App/FriendsLibrary/friends-library','App/FriendsLibrary/bin/rclone')) {
        $entry = $zip.Entries | Where-Object { $_.FullName.Replace('\','/') -eq $binaryPath }
        $stream = $entry.Open()
        try {
            $header = New-Object byte[] 20
            if ($stream.Read($header,0,20) -ne 20 -or $header[0] -ne 127 -or $header[1] -ne 69 -or $header[2] -ne 76 -or $header[3] -ne 70 -or $header[4] -ne 1 -or $header[5] -ne 1 -or $header[18] -ne 40 -or $header[19] -ne 0) {
                throw "Not a 32-bit little-endian ARM ELF: $binaryPath"
            }
        } finally { $stream.Dispose() }
    }
    $expectedHashes = @{
        'App/FriendsLibrary/bin/rclone' = '23a2c35922b5e3bd74e57404ff82c317a360c7d83004ab9a5d1ba822d4c2d16f'
        'App/FriendsLibrary/certs/cacert.pem' = '427c064216761a1e72a8b30f0d954d08cb6e4435a82091fb865afd20c8497285'
    }
    foreach ($path in $expectedHashes.Keys) {
        $entry = $zip.Entries | Where-Object { $_.FullName.Replace('\','/') -eq $path }
        $stream = $entry.Open()
        $sha = [Security.Cryptography.SHA256]::Create()
        try {
            $actual = [Convert]::ToHexString($sha.ComputeHash($stream)).ToLowerInvariant()
            if ($actual -ne $expectedHashes[$path]) { throw "Package dependency hash mismatch: $path" }
        } finally { $sha.Dispose(); $stream.Dispose() }
    }
    $launch = $zip.Entries | Where-Object { $_.FullName.Replace('\','/') -eq 'App/FriendsLibrary/launch.sh' }
    $reader = [IO.StreamReader]::new($launch.Open())
    try {
        $text = $reader.ReadToEnd()
        if (!$text.StartsWith("#!/bin/sh`n") -or $text.Contains("`r")) { throw 'Launcher must use a POSIX shebang and LF line endings.' }
    } finally { $reader.Dispose() }
} finally { $zip.Dispose() }
Write-Output 'Package layout, credential-path exclusion, ARM ELF headers, dependency hashes and LF launcher passed.'
