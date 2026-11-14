# Miyoo Friends Library 0.1.0 doğrulaması

Tarihsel rapor: 0.1.0 kullanıcı cihaz denemesinde ekran bozuk çıktı. 0.1.1 için
[görüntü düzeltme kaydını](DISPLAY_FIX_0.1.1.md) okuyun; aşağıdaki host/ABI sonuçları
0.1.0 kaydıdır ve başarılı donanım testi anlamına gelmez.

14 Kasım 2026. Sonuç: kaynak uygulandı, host testleri geçti ve hedef ARM binary
üretildi. Fiziksel Miyoo Mini Plus ve gerçek Google hesabı ile ürün kabulü açıktır.

## Otomatik test sonucu

Digest'e sabit Onion toolchain container'ında host GCC 8.3 ve SDL 1.2 ile bütün
test hedefleri yeniden derlendi. Son birleşik koşu uyarısız ve başarılıdır.

| Grup | Doğrulanan sonuç |
| --- | --- |
| `test-core` | 153 kontrol geçti: eşleme, hiyerarşi, dosya adları, traversal, tipler, boyutlar, alan, checksum ve yerel dosyalar. |
| `test-backend` | 126 assertion geçti: config, readonly config politikası, mock Drive listeleri, önbellek, hata sınıfları, iptal, hesap ayırma ve alt süreç güvenliği. |
| `test-ui` | Fiziksel tuş SDL olayları, metin sınırları ve liste/ayrıntı/ilerleme/hata çizimleri geçti. |
| `test-download` | Başarılı indirme, mevcut dosya koruması, açık değiştirme, checksum/boyut hatası, auth/ağ/izin/alan hatası, iptal, worker/owner ölümü, yarış ve recovery geçti. |
| `test-app` | 53 controller assertion geçti: dinamik adlar, gezinme, scroll koruma, desteklenmeyen girdiler, dosya çakışması, onaylar, offline ve cache durumları. |

Google davranışı kontrollü rclone alt süreç taklitleri ve JSON yanıtlarıyla
test edildi. Bu testler gerçek Google OAuth endpoint'ine bağlanmaz; rclone'ın
gerçek token yenilemesini veya Google'ın hesap politikasını kanıtlamaz.

İlk geliştirme koşularında controller testinin eşleme dosyasını yüklemesi iki kez
başarısız oldu; bu sırada kaynak ve testler birlikte düzenleniyordu. Son sabit
kaynakların tam test koşusu geçti. Bu belirsizlik için controller testi aynı
binary ile 20 ardışık kez ayrıca çalıştırıldı; 20/20 koşu geçti. İlk hataların
kök nedeni bağımsız olarak kesinleştirilmedi; geçmiş hatalar başarılı koşu diye
sunulmamıştır.

Ana log: `build/final-host-validation.log`.

## İstenen test kapsamıyla eşleşme

| İstek | Kanıt |
| --- | --- |
| Klasör hiyerarşisi ve arkadaş keşfi | core, backend ve controller |
| Konsol normalizasyonu, ROM hedefi ve bilinmeyen konsol | core ve controller |
| Dosya adı temizleme ve traversal engeli | core; symlink ve izin verilen hedef kontrolleri dahil |
| Çift isim ve mevcut ROM tespiti | core, controller ve download |
| Boyut ve yetersiz alan | core ve download |
| Kesilen indirme ve iptal | download; worker/owner kaybı ve mevcut dosyanın korunması dahil |
| Kimlik doğrulama ve ağ hataları | backend ve download |
| Boş dizin ve cache yenileme | core ve backend |
| Simüle edilmiş Drive yanıtları | backend fixture'ları; gerçek hesap gerektirmez |

## Build ve ABI

- Hedef binary: `build/friends-library-arm`, 110.364 byte.
- SHA256: `18e9f2e517b3170fc5634095c5b1a9eb82f3ad14f99ad4a3ce4179846d3eb9e0`.
- ELF32 little-endian ARM EABI5, hard-float; loader `/lib/ld-linux-armhf.so.3`.
- Dinamik bağımlılıklar: SDL 1.2, pthread, libm ve libc. SDL'nin kendi firmware
  bağımlılıkları Onion tarafından sağlanır; uygulama bunları değiştirmez.
- Uygulamanın versioned glibc gereksinimleri: GLIBC 2.4 ve 2.17. Sysroot'un
  varsayılan GLIBC 2.28 `fcntl` sembol yönlendirmesi açık eski sembol bağlamasıyla
  giderildi; 64 bit dosya boyutu desteği korundu.
- Host `--probe` çıktısı 64 bit `off_t` bildirdi. ARM hedefinde derleme bayrağı
  `_FILE_OFFSET_BITS=64`; gerçek cihazda `--probe` ayrıca doğrulanacak.
- rclone, resmi 1.76.0 ARMv7 binary'sidir; ZIP ve binary hash'leri sabittir.

ABI logu: `build/final-arm-build.log`. Bu inceleme firmware veya cihaz çalıştırma
testi yerine geçmez. Gerçek cihazda loader, SDL ve kernel davranışı kabul kapısıdır.

## Arayüz ve kaynak kullanımı

640 × 480 host önizlemeleri `build/app-demo.png`, `build/ui-list.bmp`,
`build/ui-download.png`, `build/ui-error.png` dosyalarında kontrol edildi.
Gösterilen kütüphane ve boyutlar mock/demo veridir; gerçek cihaz görüntüsü değildir.

Arayüz yalnız değişimde çizilir; aktif ilerleme çizimi en fazla yaklaşık 300 ms'de
bir yapılır. İndirme ve checksum arayüzden ayrı süreçtedir. Tek indirme, 1 MiB
rclone transfer buffer'ı ve kapalı çoklu aktarım kullanılır. Bunlar tasarım
sınırlarıdır; gerçek cihaz RAM/CPU/batarya ölçümü henüz yoktur.

## Güvenlik denetimi

- İndirilen isimler shell kodu olarak çalıştırılmaz; `execv` argümanları kullanılır.
- Kimlik bilgileri App dağıtımından ayrı tutulur; ham rclone hata çıktısı kullanıcı
  mesajına veya uygulama loguna aktarılmaz.
- Shortcut'lar atlanır, hedefler doğrulanmış ROM eşlemesi altındadır.
- Yerel dosya ve dizin symlink'leri reddedilir. İndirme, uygulamaya ait geçici
  dosyada kalır; boyut/checksum kontrolünden sonra kurulur.
- Varsayılan kurulum mevcut dosyanın üstüne yazmayan atomik işlem kullanır.
- İptal ve owner/worker kaybında ilgili alt süreçler sonlandırılır; arayüz pipe
  okumalarında süresiz bloklanmaz. Kurtarma normal ROM dosyalarını silmez.
- TLS doğrulaması açıktır. CA dosyası sürüm ve checksum ile sabittir.

## Açık kabul kapıları

Gerçek Google OAuth kurulumu, token yenileme, davetli özel klasör erişimi, Wi-Fi,
Miyoo Apps açılışı, fiziksel tuşlar, NES/NDS/PS aktarımı, resmi ROM yenilemesi,
FAT32 güvenli kurulum ve kaynak tüketimi [cihaz listesinde](DEVICE_VALIDATION.md)
bekliyor. [Bilinen sınırlar](KNOWN_LIMITATIONS.md) V1 kapsamının parçasıdır.

Kurulum ZIP'i oluşturulurken paket yolu, credential dışlama, ARM ELF başlıkları,
rclone/CA checksum'ları ve launcher LF satır sonları `scripts/test-package.ps1`
ile ayrıca doğrulanır. ZIP sonucu ve hash'i dağıtımın `.sha256` dosyasında tutulur.
