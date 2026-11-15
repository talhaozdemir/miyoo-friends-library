# 0.1.4 - erişim tokenı yenileme toparlanması

Kullanıcı Production sonrasında yeniden giriş yapıp listeye eriştiğini,
1-2 saat sonraki açılışta tekrar bilgisayar istendiğini bildirdi. 0.1.3 aynı
mesajı hem genel HTTP 401 hem `invalid_grant` için kullanıyordu. Cihazdaki ham
alt hata elde olmadığından gerçek nedenin hangisi olduğu henüz kanıtlanmadı.

## Davranış

- Listeleme veya indirme HTTP 401/UNAUTHENTICATED ile reddedilirse, yalnız bir
  kez mevcut OAuth tokenının `expiry` alanı geçmişe alınır. `refresh_token`
  değiştirilmez veya silinmez; rclone normal yenileme akışını yeniden dener.
- Bu, geride kalan cihaz saatinin sunucuda süresi dolmuş access tokenını hâlâ
  geçerli sanması gibi durumların otomatik toparlanmasına olanak verir. Sistem
  saati değiştirilmez; TLS veya sertifika denetimi kapatılmaz.
- rclone 1.76.0 boş `access_token` değerini legacy format sanabildiğinden alan
  boşaltılmaz. Mevcut nonempty değer korunur; yoksa nonsecret geçersiz bir
  placeholder ve geçmiş expiry kullanılır. Böylece yenileme izni korunur.
- Yapılandırma atomik değiştirilir. Okuma/yazma başarısızlığında eski dosya
  korunur. Diğer remote ve alanlar korunur. Kimlik değişikliği tespit edilirse
  yazma yapılmaz. Ham stderr veya tokenlar loga ya da ekrana verilmez.
- Sürekli 401 yalnız iki toplam işlem denemesiyle sonlanır; bilgisayar girişi
  otomatik önerilmez. `invalid_client` / `invalid_scope`, SD yazma ve ağ hataları
  ayrıdır. `invalid_grant` gerçekten alınırsa yeniden Google yetkilendirmesi
  gerekebilir; uygulama Google'ın iptal ettiği izni geri getiremez.
- Listeleme UI'si yenilemeyi bildirir ve B ile iptal edilebilir. İndirmedeki
  başarısız kısmi transfer temizlenerek bir kez yeniden başlatılır; hedef ROM
  ancak eski tamamlama/boyut/hash/overwrite kontrollerinden sonra kurulur.

## Otomatik doğrulama

`scripts/build.ps1 -TestsOnly`: altı test grubu geçti; core153, backend263,
controller55, Favorites84 ve UI/rotation/download entegrasyonu. Yeni vakalar:
401 toparlanması, iki-deneme sınırı, gerçek refresh reddi, client hatası,
token gizliliği, yenileme sırasında iptal, diğer config alanlarını koruma ve
RLIMIT_FSIZE ile atomik yazma başarısızlığı. Test hesapları tamamen sahtedir.

`python tests/test_rclone_refresh.py`: pinned **Windows rclone 1.76.0** ikilisi
ve loopback OAuth sunucusuyla geçmiş access tokenı yenilendi; yenileme tokenı
korundu, config'e kaydedildi, yeni süreçte tekrar kullanılabildi. invalid_grant
ve invalid_client gerçek rclone çıktısında ayrıldı. Yerel proxy tüm upstream
HTTPS CONNECT isteklerini reddeder; gerçek Google hesabı veya Drive bağlantısı
kullanılmaz. OAuth kütüphanesinin Basic/body auth denemesi en fazla iki POST
ile sınırlı gözlendi. Bu test cihazdaki ARM ikilisinin donanım kabulü değildir.

Kanıt logları yereldir: `build/auth-recovery-final-tests.log`,
`build/auth-recovery-real-rclone.log`. Güncel kurulum için küçük auth-recovery
güncelleme ZIP'i mevcut App klasörüyle birleştirilir; hesap ayarları pakette yok.
Mevcut yetkilendirmeyle denenmelidir. Cihazda en az 2 saat sonrası relaunch ve
gerçek indirme sonucu bekleniyor; kullanıcıdaki sorunun giderildiği iddia edilmez.

Kaynaklar: [rclone token okuma/yenileme](https://github.com/rclone/rclone/blob/v1.76.0/lib/oauthutil/oauthutil.go),
[Drive OAuth istemcisi](https://github.com/rclone/rclone/blob/v1.76.0/backend/drive/drive.go),
[Google token sona erme koşulları](https://developers.google.com/identity/protocols/oauth2#expiration).
