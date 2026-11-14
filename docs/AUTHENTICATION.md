# İlk Google yetkilendirmesi

Bu işlem bir defa bilgisayarda yapılır. Normal kullanımda bilgisayar, telefon veya
sunucu gerekmez. Her arkadaş kendi Google hesabını yetkilendirir. Tokenları
sohbete, issue'ya, e-postaya veya Git'e göndermeyin.

## Google OAuth istemcisi

1. [rclone'ın resmi kendi istemcini oluşturma yönergesini](https://rclone.org/drive/#making-your-own-client-id)
   izleyerek Google Cloud Console'da bir proje oluşturun, Drive API'yi etkinleştirin.
2. OAuth consent ekranını yapılandırın; kişisel Google hesapları için External
   seçeneği gerekebilir. Kullanacağınız hesapları izinli test kullanıcısı yapın.
3. OAuth client türü **Desktop app** olsun. Client ID ve client secret bilgisini
   yalnız kendi yerel rclone kurulumunuzda kullanın. Paket ortak/master token içermez.
4. Yalnız `https://www.googleapis.com/auth/drive.readonly` kapsamını isteyin.
   Bu kapsam hesabın erişebildiği Drive içeriğini okuyabilir. Bir klasöre özel
   OAuth scope değildir. `root_folder_id` yalnız uygulamanın gezinme kökünü belirler.

**Süre sınırı:** External / Testing durumunda Drive refresh tokenı genellikle
7 gün geçerlidir. Düzenli kullanım için Google'ın Production/yayın ve gerekirse
doğrulama koşullarını kendi OAuth projenizde değerlendirin. Production otomatik
olarak Google doğrulaması anlamına gelmez; hesap/Workspace yöneticisi politikaları
da erişimi sınırlayabilir. Süresiz token garantisi yoktur. Google hesabından izin
iptali veya güvenlik olayı sonrası bilgisayarda yeniden yetkilendirme gerekir.

Ortak rclone istemcisine dayanmayın; kendi Desktop istemcinizi kullanın.
[Google token expiration açıklaması](https://developers.google.com/identity/protocols/oauth2#expiration).

## SD karta hesap kurma (Windows)

Önce release ZIP'ini SD kart köküne çıkarın. Kaynak checkout'unda PowerShell 7:

```powershell
./scripts/fetch-deps.ps1 -WithWindowsRclone
./scripts/setup-account.ps1 -SdRoot E:\ -FolderId 'PAYLASILAN_KLASOR_ID'
```

`E:\` yerine gerçekten SD kartınızı yazın. Script interaktif `rclone config` açar:

- Yeni remote adı: **friends**.
- Backend: **Google Drive**.
- `client_id` ve `client_secret`: kendi Desktop istemciniz.
- Scope: **drive.readonly** (salt okunur).
- `root_folder_id`: paylaşılan kökün ID'si; gelişmiş ayarlarda sorulabilir.
- Service account dosyası: boş; Workspace Shared Drive ayarı: normal davetli
  klasörünüz için hayır. “Shared with me” özel filtrelemesini açmayın.
- Bilgisayarda browser authentication yapın; klasöre davetli hesabı seçin.
- Config encryption açmayın; V1 başsız açılışta şifre soramaz.

Menüde kaydedip çıkın. Script remote tipini, scope değerini ve refresh tokenının
varlığını tokenı yazdırmadan kontrol eder. Ayarlar
`/.config/friends-library/settings.json`, kişisel OAuth bilgileri
`/.config/friends-library/rclone.conf` konumundadır. Uygulama güncellemesi bunları
içermez. Başka hesaba ait hazır config kopyalamayın. Yalnız bu uygulamanın
`friends` remote'unu içeren ayrı bir config kullanın; başka remoteları eklemeyin.

macOS/Linux'ta aynı sürüm rclone ile `rclone config --config /SD/.config/friends-library/rclone.conf`
komutunu kullanın, sonra `config.example.json` dosyasını aynı dizine `settings.json`
olarak kopyalayıp klasör ID'sini değiştirin.

**Önemli:** Eski geniş yetkili tokenın config'inde `scope` yazısını değiştirmek
token yetkisini daraltmaz. Geniş izinli eski yetkiyi Google hesabından iptal edin ve
salt okunur scope ile yeni OAuth oturumu oluşturun.

## Paylaşılan klasör ID'si

Drive'da açılan `https://drive.google.com/drive/folders/ABC123` adresinde ID
`ABC123` bölümüdür; `?usp=...` kısmını almayın. Klasörün My Drive kökünde bulunması
veya oraya kopyalanması gerekmez; davetli hesabın ID ile erişimi yeterlidir.
Normal davetli klasörü Google Workspace **Shared Drive** ile karıştırmayın.
Shortcut'lar V1'de atlanır; gerçek arkadaş ve konsol alt klasörlerini kullanın.

## Hesabı ayırma ve güvenlik

Uygulamada SELECT ile hesap ayırma ekranını açın ve cihazdaki açık onayı izleyin.
Yerel credential silinir; uygulama çevrimiçi Google iptalini de dener. İnternet
yoksa yerel silme yapılabilir; Google hesabı → Güvenlik → Üçüncü taraf bağlantıları
sayfasından OAuth yetkisini ayrıca iptal edin. ROM dosyaları korunur.

FAT32'de `chmod 0600` fiziksel SD kart erişimine karşı güvenli gizlilik sağlamaz.
Kartı okuyabilen kişi refresh tokenını da okuyabilir. Karttaki anahtarla otomatik
şifreleme bu riski çözmez. Kartı paylaşmadan önce hesabı ayırın; kayıp kartta
Google'dan yetkiyi iptal edin. Uygulama tokenları loglamaz; rclone debug/dump
çıktısı üretip paylaşmayın. TLS denetimini kapatmayın. Token yenilemesi config'e
yazdığı için SD kartın yazılabilir olması gerekir.
