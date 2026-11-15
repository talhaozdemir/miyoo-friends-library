# İlk Google yetkilendirmesi

0.3.1'de ilk açılışta **A: Connect**, sonra **SELECT → Account → Connect with phone** HTTPS QR ile
Safari/Chrome'dan ilk giriş ve yeniden bağlantı sunar. Ek telefon uygulaması
gerekmez. Kod karşılaştırma ve Miyoo'da A onayı gerekir. [Telefon rehberi](HTTPS_SETUP.html).
Dağıtıcının ayrı Google **Web** OAuth istemcisi ve çalışan ücretsiz Worker'ı
kurmuş olması gerekir; [servis kurulumu](../auth-service/README.md). Yalnız public
client ID, servis origin ve root ID `config/https-setup.json` içinde dağıtılır.
Web client secret yalnız Worker secrets'tadır. Kalıcı Google refresh tokenı SD'de
tutulur; yenileme HTTPS servisi üzerinden yapılır. Bu servisin erişilebilirliği
gereklidir; kısa süreli şifreli token oturumu ve kalıcı hash/grant kaydı sunucudadır.
Drive listeleme/ROM transferi cihazdan doğrudan Google'a gider.

Bilgisayardan aşağıdaki doğrudan Google kurulumu aynen kullanılabilir. Eski telefon
tünelinin teknik referansı normal menü dışında tutulur: [ileri SSH rehberi](PHONE_SETUP.html).

Bu işlem bilgisayarda veya telefon yöntemiyle yapılır. Normal kullanımda bilgisayar
ve telefon gerekmez; HTTPS yönteminde token yenileme servisi çalışmalıdır.
Her arkadaş kendi Google hesabını yetkilendirir. Tokenları
sohbete, issue'ya, e-postaya veya Git'e göndermeyin.

## Hazır arkadaş paketi

Arkadaşlara doğrudan verilen ZIP'in `FirstSetup` yardımcıları ortak Desktop
uygulama tanımını kullanır; yeni OAuth istemcisi oluşturmak gerekmez. Her kişi
yine kendi davetli Google hesabıyla giriş yapar. Bu kişisel profil/token içeren
bir paket değildir; dağıtım tanımı kaynak reposuna eklenmez.

Ortak MiyooFriendsLibrary projesi 14 Kasım 2026'da **In production** durumuna
geçirildi. Tanıtım, gizlilik ve koşul sayfaları
<https://talhaozdemir.github.io/miyoo-friends-library/> adresindedir.
Testing sırasında bağlananlar `YES` ile bir kez yeniden giriş yapmalıdır.
Yeni Production yetkilendirmelerinde Testing'e özgü 7 günlük süre uygulanmaz.
Google doğrulaması tamamlanmadı: doğrulanmamış uygulama uyarısı, 100 kullanıcı
sınırı ve diğer iptal/sona erme koşulları devam eder.

Aşağıdaki bölüm kendi OAuth projesini kurmak isteyenler içindir.

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
