# Miyoo Friends Library

0.2.0: Bilgisayardan kurulum ve otomatik OAuth yenilemesi korunur. İsteğe bağlı
**SELECT → Connect with phone** ile ilk Google bağlantısı veya yeniden giriş
kart çıkarılmadan yapılabilir. **Phone setup guide** cihaz içinde sekiz sayfa
çevrimdışı açıklama ve telefon rehberi QR'si sunar.

Telefon ve Miyoo aynı güvenilir Wi-Fi'da olmalı; telefonda SSH **local port
forwarding** uygulaması gerekir. QR tek başına tünel kurmaz. Hazır arkadaş
ZIP'inde ilk telefon girişi için ortak public Desktop uygulama/klasör tanımı
`App/FriendsLibrary/config/phone-setup.json` içine eklenir; kullanıcı tokenı
bulunmaz. Genel kaynak/OnionOS paketinde bu özel dağıtım tanımı bulunmaz;
mevcut bilgisayar bağlantısının public client bilgileri yeniden kullanılabilir.

İlk dosya kopyalamasından sonra telefonla Google yetkilendirmesi bilgisayar
istemez. Uygulama SSH servisini açmaz veya ayarlarını değiştirmez. Yeni hesap
ayrı çalışma dizininde yetkilendirilir, Google Files.get ile salt okunur klasör
erişimi doğrulanır, sonra atomik kaydedilir. İptal ve hata önceki hesabı korur.
[Ayrıntılı telefon rehberi](docs/PHONE_SETUP.html).

Bu özellik mock/loopback OAuth ve QR ekranı testlerinden geçti; Android/iOS +
OnionOS üzerinde gerçek uçtan uca doğrulama henüz yapılmadı. Pinned rclone
1.76.0 Drive yetkilendirmesi kendi OAuth state kontrolünü kullanır; Drive
backend'inde PKCE etkin değildir. Dinleyici loopback'te tutulur ve yalnız SSH
tüneli üzerinden kullanılır; bir LAN HTTP giriş servisi açılmaz.

OnionOS 4.3 stable kullanan **Miyoo Mini Plus** için özel Google Drive
kütüphanesi gezgini. Arkadaş → konsol → oyun seçerek dosyayı doğru `Roms/`
dizinine indirir. Oyun başlatma, emülasyon, GameSwitcher, kayıtlar ve favoriler
Onion'ın sorumluluğundadır. Yalnız yasal olarak paylaşmaya yetkili olduğunuz
oyunları kullanın; pakette ROM veya BIOS yoktur.

Sürüm **0.1.4 (otomatik erişim yenileme toparlanması)**. Genel HTTP 401 reddinde
mevcut refresh izni korunup yalnız kısa ömürlü erişim tokenı süresi geçmiş
işaretlenir ve işlem bir kez tekrar denenir. Gerçek `invalid_grant`, istemci
ayarı ve SD yazma hataları ayrılır. [Davranış ve kanıt](docs/AUTH_RECOVERY_0.1.4.md).
İsteğe bağlı Favorites, 180 derece cihaz sunumu ve küçük ikon korunur.
Bu sürümün gerçek cihazda 1-2 saat sonrası yeniden açılış testi bekleniyor.
Bu sürümün hedefi Onion 4.3 / Mini Plus'tır; diğer cihazlar ve Onion
sürümleri desteklenmiş sayılmaz. [Cihaz kabul listesi](docs/DEVICE_VALIDATION.md).

## Kurulum

[Kaynak kod](https://github.com/talhaozdemir/miyoo-friends-library) ve
[tanıtım / gizlilik / koşul sayfaları](https://talhaozdemir.github.io/miyoo-friends-library/)
Public olarak yayınlandı. Ortak OAuth uygulaması 14 Kasım 2026'da **In production**
durumuna geçirildi. Google doğrulaması tamamlanmadı; giriş uyarısı ve 100 kullanıcı
sınırı devam eder. Testing sırasında bağlananlar yardımcının `YES` seçeneğiyle
bir kez yeniden giriş yapmalıdır. Yeni Production yetkilendirmelerinde Testing'e
özgü 7 günlük süre uygulanmaz; diğer iptal/sona erme koşulları devam eder.

Arkadaşlara hazır dağıtım için `dist/Miyoo-Friends-Library-Arkadas-Kurulum.zip`
ve `output/pdf/Miyoo-Friends-Library-Kurulum-Rehberi.pdf` kullanılır. ZIP bilgisayara
çıkarılır; yalnız `App` klasörü SD köküne birleştirilir. Windows'ta
`FirstSetup/Connect-Windows.cmd`, macOS 12+ üzerinde Terminal'den
`bash ` ardından sürüklenen `FirstSetup/Connect-Mac.command` çalıştırılır.
Ortak Desktop OAuth uygulaması ve klasör tanımı hazırdır; her arkadaş kendi
Google hesabıyla yetkilendirir. Pakette kişisel erişim/yenileme tokenı yoktur.
Hesap ortak Drive klasörüne davetli olmalıdır. Testing'e geri alınırsa test
kullanıcısı kaydı ve 7 günlük yenileme tokenı süresi yeniden önem kazanır.
Windows yardımcısı sahte yanıtlarla test edildi; gerçek Mac OAuth testi bekleniyor.

Genel paket ve kendi OAuth istemcisini kullanmak isteyenler için:

1. `dist/Miyoo-Friends-Library-v0.1.4-OnionOS-4.3.zip` ve `.sha256` dosyasını alın.
2. ZIP'i microSD kartın **köküne** çıkarın. Sonuç `App/FriendsLibrary/` olmalı.
3. Bilgisayarda [kendi Google hesabınızı yetkilendirin](docs/AUTHENTICATION.md).
4. Kartı güvenle çıkarıp Miyoo'ya takın. Wi-Fi ve cihaz saati doğru olsun.
5. **Apps → Friends Library** açın. İlk kullanımda liste ağdan yüklenir.

Firmware flashlama, Python, SDL2, telefon, harici sunucu veya sürekli arka plan
servisi gerekmez. SDL 1.2 Onion runtime'dan, rclone ve CA dosyası uygulama
paketinden kullanılır. SSH normal kullanım gereksinimi değildir.

## Kütüphane ve yapılandırma

```text
Paylaşılan kök/
  TALHA/FC/Tiny Toon Adventures.nes
  TALHA/FC/Favorites/Favorite Homebrew.nes
  FRIEND_2/GBA/Example.gba
  FRIEND_3/PS/Example.chd
```

`Favorites` tamamen isteğe bağlıdır; konsol altında yoksa mevcut oyunlar çalışır.
Konsolu açınca üstte `Favorites`, altında `All` görünür. `All` bir Drive klasörü
değildir: konsolun doğrudan içindeki oyunlarla `Favorites` oyunlarını birlikte
gösterir. Favoriye almak için dosyayı Drive'da `Favorites` içine taşıyın; kopyalamak
gerekmez. `X` iki listeyi yeniler. Boş Favorites görünümünde açıklama gösterilir.
Uygulama Drive'da klasör oluşturmaz veya dosya taşımaz. [Kapsam ve testler](docs/FAVORITES_0.1.3.md).

İsimler örnektir. Arkadaşlar ve alt klasörler dinamik keşfedilir. Her kullanıcı
kendi hesabıyla kök klasöre davet edilmiş olmalı; klasörü herkese açmayın.
Drive URL'sindeki klasör ID'sini `/.config/friends-library/settings.json`
içindeki `root_folder_id` alanına yazın. [Örnek](config.example.json):

```json
{"root_folder_id":"YOUR_FOLDER_ID","remote":"friends","cache_ttl_seconds":900,"timeout_seconds":30}
```

Kişisel credential aynı dizindeki `rclone.conf` dosyasında, paket dosyalarından
ayrı tutulur. Bu dosyayı paylaşmayın. Her hesap kendi Desktop OAuth istemcisini
ve yeni `drive.readonly` yetkisini kullanır. `root_folder_id` tokenın OAuth
yetkisini tek klasöre daraltmaz. Ayrıntılar [ilk kurulum](docs/AUTHENTICATION.md).

## Tuşlar ve indirme

| Tuş | İşlev |
| --- | --- |
| D-pad | Listede gezin |
| A | Aç / ayrıntı / ekrandaki açık onayı uygula |
| B | Geri / iptal / ana ekrandan çık |
| X | Geçerli klasörü elle yenile |
| L1/L2, R1/R2 | Önceki / sonraki sayfa |
| SELECT | Hesabı ayırma ekranı |

Oyunu seçince konsol, paylaşan arkadaş, boyut, boş alan, hedef ve mevcut dosya
durumu gösterilir. İndirme tek dosya halinde akışla yapılır; yüzde, byte, hız
ve iptal durumu görünür. Dosya boyutu ve sağlanan checksum doğrulanmadan ROM
hedefine kurulmaz. Aynı isimde mevcut ROM ayrı bir açık onay gerektirir.

İndirme bitince Onion'da **Games → SELECT → Refresh all roms** kullanın.
Uygulama Onion ROM cache/DB dosyalarını silmez. İlgili emülatörü Onion Package
Manager'dan kurun; gerekiyorsa BIOS'u kendiniz sağlayın.

`FC`, `SFC`, `GB`, `GBC`, `GBA`, `MD`, `PS`, `NDS` desteklenir.
[Merkezi eşleme ve format sınırları](docs/ROM_MAPPING.md).
PS V1 desteği eksiksiz tek dosyalık CHD/PBP içindir. BIN/CUE, M3U, diğer
multifile ve çok diskli setleri paylaşmayın. ZIP/7z otomatik açılmaz.

## Çevrimdışı ve sorun giderme

- **Önbellek:** klasör başına kaydedilir; X manuel yeniler. Yenileme başarısızsa
  mevcut önbellek ve yaşı gösterilebilir. Çevrimdışı indirme tamamlanamaz;
  Onion'daki önceden indirilen oyunlar etkilenmez.
- **Authentication required/expired:** kişisel `friends` remote'u, salt okunur
  scope ve refresh tokenını kontrol edin; gerekiyorsa bilgisayarda yeni OAuth yapın.
  Testing OAuth projelerinde 7 günlük token sınırını okuyun.
- **Folder unavailable/permission denied:** ID'nin gerçek köke ait olduğunu,
  doğru Google hesabının davetli olduğunu kontrol edin. Shortcut'lar atlanır.
- **TLS/saat hatası:** cihaz saatini Onion üzerinden düzeltin; TLS doğrulamasını
  kapatmayın. Uygulamadaki CA bundle bakımda güncellenmelidir.
- **Wi-Fi/DNS/timeout/kota:** bağlantıyı düzeltip X ile yeniden deneyin. Sınırlı
  yeniden deneme vardır; uygulama arka planda sonsuz istek yapmaz.
- **Alan yetersiz/bozuk indirme:** SD kartı ve boş alanı kontrol edin. V1 iptal
  edilen/yarım indirmeyi tekrar başlatır; süreçler arası resume vaat edilmez.
- **Oyun görünmüyor:** desteklenen format/hedef, etkin emülatör ve resmi ROM
  yenilemesini kontrol edin. Uygulama oyunların içeriksel oynanabilirliğini kanıtlamaz.

Hata raporuna token, rclone.conf, OAuth secret veya özel kütüphane dökümü eklemeyin.

## Güvenlik, güncelleme ve kaldırma

OAuth erişimi salt okunur; uygulama Drive'a ROM yüklemez veya paylaşım izni açmaz.
Tokenlar loglanmaz. FAT32 üzerindeki dosya izinleri kartı fiziksel okuyabilen
kişiye karşı güçlü koruma değildir. Hesabı SELECT ile ayırın; çevrimdışı iptal
başarısızsa Google hesabından OAuth iznini ayrıca kaldırın.

Güncellemek için yeni ZIP'i aynı SD köküne çıkarın. Dağıtım yalnız
`App/FriendsLibrary/` içerir; `/.config/friends-library/` kullanıcı verisi korunur.
Kaldırmak için hesabı ayırıp yalnız `App/FriendsLibrary/` klasörünü silin. İsterseniz
uygulamaya ait `/.config/friends-library/` verisini ayrıca kaldırın. `Roms/`,
`Saves/`, `BIOS/` ve Onion ayarlarını silmeyin; indirilen oyunlar kalır.

## Kaynaktan build ve test

PowerShell 7 ve çalışan Docker Desktop (Linux containers):

```powershell
./scripts/fetch-deps.ps1
./scripts/build.ps1 -TestsOnly
./scripts/build.ps1
./scripts/package.ps1
```

Toolchain digest'i scriptte sabittir; [bağımlılıklar](docs/DEPENDENCIES.md).
Host Linux üzerinde GCC, GNU make ve SDL 1.2 development dosyalarıyla `make test`
çalışır. Testler gerçek Google hesabı veya internet erişimi istemez. UI testleri
SDL dummy driver ile BMP çıktıları üretir; bunlar fiziksel ekran testi değildir.

Kaynak `src/`, public arayüzler `include/`, testler `tests/`; eşleme
`package/App/FriendsLibrary/config/systems.json` içindedir. Dağıtım scripti
credential yollarını ve iki ARM ELF dosyasını kontrol eder.

Bilinen sınırlar: donanım/OAuth kabulü açık; V1 bütün kütüphane yerine seçilen
dosyayı indirir; shortcut/multifile/multidisc set, upload, sync ve içerik arşiv
incelemesi yoktur. UI Latin/Türkçe karakterleri basit fonta dönüştürür; diğer
yazılarda `?` görülebilir, gerçek dosya adları değiştirilmez.
[Ayrıntılı sınırlar](docs/KNOWN_LIMITATIONS.md).
[Gerçek otomatik test ve build sonuçları](docs/VERIFICATION.md).

MIT lisansı; [üçüncü taraf bildirimleri](THIRD_PARTY_NOTICES.md).
