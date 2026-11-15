# 0.3.2 — QR ve bilgisayar kurulumu

15 Kasım 2026. Eski telefon SSH tüneli/OAuth listener yolu, public telefon
profil parser'ı ve PHONE_SETUP rehberi kaldırıldı. Menüde HTTPS QR ve Help
korunur. Windows/macOS bilgisayar yardımcıları doğrudan Google/rclone kullanır.

HTTPS'nin kullandığı stage/commit/cleanup işlemleri ayrı `account.c` modülüne
taşındı. Mevcut hesap dosyaları yalnız doğrulanmış yeni hesabın açık cihaz
onayından sonra değiştirilir. Çakışan kullanıcı değişikliği, başarısız atomik
yazma, iptal, süre aşımı, offline ve symlink korumaları test edildi. Eski staging
dizinleri için güvenli toparlanma korunur; yeni uygulamada eski OAuth listener
başlatma veya poll sembolleri bulunmaz.

Güncellemede ZIP içindeki App klasörü SD kart köküne birleştirilir. İlk açılış
yalnız uygulamadaki eski `docs/PHONE_SETUP.html` ve `config/phone-setup.json`
dosyalarını kaldırır; symlink hedeflerini izlemez. Kişisel hesap `.config/`
altındadır ve korunur. ROM/save/BIOS/Onion ayarları ve SSH servis ayarları değişmez.

## Tek paket

`scripts/package.ps1` tek `dist/Miyoo-Friends-Library-Arkadas-Kurulum.zip`
oluşturur. Paket hem ilk kurulum hem güncelleme içindir; yeni binary, QR public
servis tanımı, Windows/macOS yardımcıları ve lisansları içerir. Kişisel Google
tokenı veya Web client secret içermez. Paket önce geçici dosyaya yazılır, ARM
formatı/bağımlılık hash'leri/ZIP bütünlüğü/konfigürasyonlar doğrulanır; ardından
son paketin yerini alır. Başarısız paketleme son geçerli ZIP'i korur.

Yalnız dist içindeki eski Miyoo Friends Library ZIP ve eşleşen checksum'ları
silinir. İlgisiz ZIP'ler/dependency indirmeleri ve kaynak/test kayıtları korunur.
Eski update script'i de aynı tam pakete yönlenir. İsteğe bağlı source arşivi
`build/source/` altındadır; public kaynak GitHub'da kalır. Bu sürümde 28 eski ZIP
ve 28 checksum temizlendi. GitHub tracked source'da ZIP veya release asset yoktu.

## Doğrulama

- Host: core153, backend263, SDL/rotation, download integration, controller117,
  Favorites84, ortak account60 ve HTTPS133; launcher migration testi geçti.
- Windows PowerShell 5.1 setup: fake OAuth, token log bastırma, UTF-8, başarısızlıkta
  mevcut hesap koruma ve staging cleanup geçti. Gerçek Mac login testi yapılmadı.
- Pinned Windows rclone 1.76: sahte loopback OAuth ile readonly/offline/consent,
  config kalıcılığı ve bilgisayar listener cleanup kontrolü geçti; upstream kapalı.
- Paket integration: yalnız son app ZIP'i, eski checksum temizliği, ilgisiz ZIP'i
  koruma, başarısız yeni paketlemede önceki geçerli ZIP'i koruma geçti.
- ARMv7 hard-float binary üretildi; en yüksek gereksinim GLIBC_2.17, derleyici
  uyarısı yok. ZIP'in checksum'ı ve gerçek ARM build ile byte eşleşmesi geçti.

Kanıtlar: `build/no-ssh-tests.log`, `no-ssh-arm-build.log`, `no-ssh-package.log`,
`no-ssh-windows-setup.log`, `no-ssh-desktop-oauth.log`, `no-ssh-release-tests.log`.
Silme envanteri `build/latest-package-cleanup.json` içindedir.

Kullanıcı önceki QR girişinin çalıştığını bildirdi. 0.3.2 binary'sinin fiziksel
Miyoo kabulü henüz yapılmadı; bu rapor başarılı donanım testi iddiası değildir.
