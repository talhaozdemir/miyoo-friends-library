param([switch]$WithWindowsRclone)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$downloadDir = Join-Path $projectRoot 'downloads'
$appDir = Join-Path $projectRoot 'package/App/FriendsLibrary'
New-Item -ItemType Directory -Force -Path $downloadDir, "$appDir/bin", "$appDir/certs", "$appDir/licenses" | Out-Null

function Get-VerifiedFile([string]$Url, [string]$Path, [string]$Sha256) {
    if (!(Test-Path -LiteralPath $Path) -or ((Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash -ne $Sha256)) {
        Invoke-WebRequest -Uri $Url -OutFile $Path -MaximumRetryCount 3 -RetryIntervalSec 2
    }
    if ((Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash -ne $Sha256) {
        throw "SHA256 mismatch: $Path"
    }
}

$rcloneVersion = '1.76.0'
$armName = "rclone-v$rcloneVersion-linux-arm-v7"
$armZip = Join-Path $downloadDir "$armName.zip"
Get-VerifiedFile "https://downloads.rclone.org/v$rcloneVersion/$armName.zip" $armZip '090e1e2ce0f2a27c6c5ad5b2a2f7bbf7570933f3523e8bf83cb25ee50115f885'
Expand-Archive -LiteralPath $armZip -DestinationPath $downloadDir -Force
Copy-Item -LiteralPath "$downloadDir/$armName/rclone" -Destination "$appDir/bin/rclone" -Force
Get-VerifiedFile 'https://curl.se/ca/cacert-2026-11-05.pem' "$appDir/certs/cacert.pem" '427c064216761a1e72a8b30f0d954d08cb6e4435a82091fb865afd20c8497285'
Get-VerifiedFile 'https://www.mozilla.org/media/MPL/2.0/index.txt' "$appDir/licenses/Mozilla-MPL-2.0.txt" '3f3d9e0024b1921b067d6f7f88deb4a60cbe7a78e76c64e3f1d7fc3b779b9d04'
Get-VerifiedFile 'https://raw.githubusercontent.com/golang/go/go1.26.0/LICENSE' "$appDir/licenses/Go-BSD-3-Clause.txt" '911f8f5782931320f5b8d1160a76365b83aea6447ee6c04fa6d5591467db9dad'

if ($WithWindowsRclone) {
    $windowsName = "rclone-v$rcloneVersion-windows-amd64"
    $windowsZip = Join-Path $downloadDir "$windowsName.zip"
    Get-VerifiedFile "https://downloads.rclone.org/v$rcloneVersion/$windowsName.zip" $windowsZip '8b4fd2737ade4e70c057ef4a49deb7cffe94b0fb9d7587c189d149823f21a1dc'
    Expand-Archive -LiteralPath $windowsZip -DestinationPath $downloadDir -Force
    Write-Output "Windows setup executable: $downloadDir/$windowsName/rclone.exe"
}
Write-Output 'Verified rclone ARMv7 and CA certificate bundle.'
