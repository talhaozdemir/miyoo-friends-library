# Sabitlenmiş bağımlılıklar

| Bileşen | Sürüm | Kaynak / doğrulama |
| --- | --- | --- |
| Onion hedefi | 4.3.0 | [ARM build ayarları](https://github.com/OnionUI/Onion/blob/v4.3.0/src/common/config.mk) |
| Toolchain image | GCC 8.3.0, linux/amd64 | `aemiii91/miyoomini-toolchain@sha256:e5123590ad75d27f0f4c91196e3119a255cad45f3ae15243e29a8e0a2ec50132` |
| rclone Linux ARMv7 | 1.76.0 | [Release](https://downloads.rclone.org/v1.76.0/), ZIP SHA256 `090e1e2ce0f2a27c6c5ad5b2a2f7bbf7570933f3523e8bf83cb25ee50115f885` |
| Mozilla CA PEM (curl dönüşümü) | 2026-11-05 | [Kaynak](https://curl.se/ca/cacert-2026-11-05.pem), SHA256 `427c064216761a1e72a8b30f0d954d08cb6e4435a82091fb865afd20c8497285` |
| SDL | Cihazdaki SDL 1.2 | `/mnt/SDCARD/miyoo/lib`; dağıtım SDL binary'si içermez |
| cJSON | 1.7.19 | MIT; `vendor/cjson/` |

`scripts/fetch-deps.ps1` sürüm sabit dosyaları indirir ve hash'i doğrular.
CA güncellemesi bir bakım işlemidir: tarih, URL ve hash birlikte değiştirilmelidir.
TLS doğrulaması kapatılmaz. Saat hatası varsa Onion ağ/saat ayarlarından düzeltilmelidir.

rclone 1.76.0 için yayımlanmış asgari Linux kernel sürümü 3.2'dir.
ARMv7 binary seçimi ve ELF incelemesi fiziksel cihaz uyumluluğu testi yerine geçmez.
[rclone OS gereksinimleri](https://rclone.org/downloads/).
