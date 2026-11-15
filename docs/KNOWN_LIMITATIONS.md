# Bilinen sınırlar — V1

- 0.3.2 eski SSH OAuth akışını kaldırır; giriş yalnız HTTPS QR veya bilgisayar
  kurulumu ile yapılır. QR ortak hesap kaydetme/iptal/toparlanma korumaları korunur.
  Yeni sürümün fiziksel cihaz testi henüz yapılmadı.

- HTTPS girişin kaynak/host testleri ve kullanıcıdan başarılı QR giriş bildirimi
  vardır. 0.3.2 temizliğinin fiziksel cihaz kabulü ayrıca gerekir. Genel pakette servis
  profili yoktur. Dağıtıcının Google Web client/Worker secrets ayarları ve hazır
  `https-setup.json` olmadan bu seçenek açıklayıcı hata verir.
- HTTPS ile bağlanan hesapların otomatik yenilemesi ücretsiz servisin açık
  kalmasına bağlıdır. Worker/D1 Free kota veya servis kesintileri erişimi
  durdurabilir; ücretli yükseltme yapılmaz. Bilgisayar kurulumu direct Google'dır.
- Servis auth/refresh sırasında Google tokenlarını işler. Bekleyen tokenlar
  AES-GCM şifrelidir; claim beş dakikada kapanır, ACK/cancel siler, saatlik cron
  temizler. Veritabanı yedekleri daha uzun saklanabilir. Yönetici/key ele geçirilirse
  şifreleme koruması yeterli olmaz. Kalıcı grant kayıtları token özeti içerir.

- **Cihaz kabulü açık:** ARM binary derlenmiştir; Miyoo Mini Plus fiziksel
  cihazında açılış, Wi-Fi, kaynak kullanımı, batarya ve gerçek Google OAuth
  bağlantısı denenmemiştir. Host SDL dummy driver sonucu cihaz testi değildir.
- Yalnız Onion 4.3 stable / Miyoo Mini Plus hedeflenir. Emülatör/BIOS kurma,
  oyun başlatma ve ROM listesi cache'ini otomatik değiştirme yoktur.
- Bir klasör için en fazla **5.000** kabul edilen girdi ve yakalanan listeleme
  yanıtı için **8 MiB** sınırı vardır. Daha büyük klasörler açıklayıcı hatayla
  durur; sessizce eksik bir liste başarılı diye gösterilmez. Küçük gruplar için
  kütüphaneyi konsol alt klasörlerine ayırın; bütün Drive başlangıçta taranmaz.
- Cache credential dosyası kimliğine de bağlıdır. OAuth access token yenilenip
  config dosyası değişince cache namespace'i değişebilir; eski cache'e dönmek
  yerine yeni bir ağ listelemesi gerekebilir. Bu, offline cache sürekliliği için
  bilinen bir sınırdır. Başka hesaba eski cache gösterilmez.
- Konsol yanındaki oyun sayısı yalnız o konsol listesi daha önce yüklenmişse
  bilinir. Sayı için bütün konsollar arka planda ayrı ayrı taranmaz.
- Tek aktif indirme vardır; süreçler arası kaldığı yerden devam yoktur. İptal
  edilen ve yarım transfer güvenli temizlenir, tekrar indirme baştan başlar.
- 0.1.3 konsol köküyle isteğe bağlı Favorites içeriğini birleştirir; toplam
  kapsam 5000 girdidir. Yalnız bu tek alt klasör seviyesi desteklenir. All bir
  uygulama görünümüdür; All adlı Drive klasörü açılması gerekmez.
- Varsayılan mevcut dosyaya dokunmayan commit, Linux `renameat2(RENAME_NOREPLACE)`
  desteğini ister; uygun sistemde atomik hardlink alternatifi vardır. VFAT
  hardlink desteklemez. Linux 4.9 VFAT kaynakları bu rename bayrağını kabul eder;
  cihazın gerçekten ilgili kernel/filesystem davranışını sağlaması ayrıca
  test edilir. Destek yoksa mevcut dosyayı riske atmak yerine kurulum hata verir.
- PS yalnız eksiksiz tek dosyalı CHD/PBP ile sınırlıdır. BIN/CUE, M3U, shortcut,
  multifile ve çok diskli setler desteklenmez. CHD/PBP içeriğinin eksiksizliği
  adından kanıtlanamaz; kütüphaneyi paylaşanlar V1 kurallarına uymalıdır.
- İzin verilen ZIP/7z dosyaları açılmaz veya içeriği incelenmez. Oyun uyumluluğu,
  BIOS gereksinimleri ve içeriğin yasal paylaşım yetkisi uygulama tarafından
  otomatik kanıtlanmaz.
- UI özgün basit bitmap font kullanır; Latin/Türkçe harfler okunabilir eşdeğere
  dönüştürülür. Diğer yazılarda `?` görülebilir. Gerçek dosya adı UTF-8 olarak
  korunur; dosya adının ekrandaki görünümü dosya kimliği olarak kullanılmaz.
- Google scope'u hesabın erişebildiği Drive'a salt okunur yetkidir; token
  yalnız seçili klasöre hapsedilmiş değildir. FAT32 fiziksel erişime karşı
  credential gizliliği sağlamaz. Bilgisayarda yeniden yetkilendirme bazen gerekir.
- Uygulama küçük bir native executable'dır; rclone genel amaçlı Go binary'si
  paket boyutunun çoğunu oluşturur. Cihaz RAM/CPU/batarya bütçesi gerçek cihazda
  ölçülmeden performans veya pil ömrü garantisi verilemez.

Kernel kaynağı: [Linux v4.9 VFAT rename](https://github.com/torvalds/linux/blob/v4.9/fs/fat/namei_vfat.c#L928).
