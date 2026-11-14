param([switch]$TestsOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$image = 'aemiii91/miyoomini-toolchain@sha256:e5123590ad75d27f0f4c91196e3119a255cad45f3ae15243e29a8e0a2ec50132'
docker info --format '{{.OSType}}' | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Start Docker Desktop (Linux containers) and retry.' }
if ($TestsOnly) {
    docker run --rm --mount "type=bind,source=$projectRoot,target=/workspace" -w /workspace $image /bin/bash -lc 'make test CC=/usr/bin/gcc'
} else {
    docker run --rm --mount "type=bind,source=$projectRoot,target=/workspace" -w /workspace $image /bin/bash -lc 'set -e; source /root/.bashrc; export PATH="/opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/bin:$PATH"; make -B APP=build/friends-library-arm CC="${CROSS_COMPILE}gcc" CFLAGS="-Os -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve"; "${CROSS_COMPILE}readelf" -h -l -d -V build/friends-library-arm; if "${CROSS_COMPILE}readelf" -V build/friends-library-arm | grep -Eq "GLIBC_2\.(1[89]|[2-9][0-9])"; then echo "Unexpected glibc requirement newer than 2.17" >&2; exit 2; fi'
}
if ($LASTEXITCODE -ne 0) { throw 'Build/test command failed.' }
