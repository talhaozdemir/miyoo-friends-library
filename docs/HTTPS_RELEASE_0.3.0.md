# 0.3.0 HTTPS telefon girişi doğrulaması

15 Kasım 2026. İsteğe bağlı QR → Safari/Chrome → Google → Miyoo A onayı.
Mevcut Desktop/direct Google kurulumu ve Advanced SSH alternatifi korunur.

## Otomatik ve yerel olarak doğrulanan

- Host suite: core 153, backend 263, controller 176, Favorites 84, telefon 190,
  yeni HTTPS 133 kontrol; SDL render/180 derece ve download integration geçti.
- HTTPS transport mock'ları: ilk kurulum, explicit A olmadan kaydetmeme,
  origin/root/client/token_url doğrulaması, eski hesap koruma, iptal, timeout,
  permission/network, 429, concurrent config change ve kayıp ACK.
- Worker: yerel SQLite D1 adapter + sahte Google ile 11 test, typecheck ve
  Wrangler dry-run bundle geçti. Gerçek Google hesabı kullanılmadı.
- Gerçek Windows rclone 1.76.0, boş cihaz client_secret ve özel token_url ile
  sahte OAuth yenileme/restart/invalid_grant testini geçti. Upstream Google
  bağlantıları test proxy'si tarafından engellendi.
- Native HTTPS ASan/UBSan kontrolleri `handle_segv=0:detect_leaks=0` ile geçti.
  İlk leak-sanitizer denemesi tekrarlayan DEADLYSIGNAL çıktısıyla durduruldu;
  leak detector başarılı sayılmıyor. Sınırlı tanılama boş bir ASan programını
  ve aynı testin yukarıdaki seçeneklerle çalışmasını doğruladı.
- QR, bağımsız ZXing okuyucuyla ekran çıktısından çözüldü. Dört cihaz içi rehber
  sayfası render edildi. Sekiz sayfalık Türkçe PDF güncellendi ve yeni sayfalar
  ile ilk sayfa görsel olarak kontrol edildi.
- ARMv7 hard-float executable derlendi; gerekli glibc sürümü en çok 2.17.
  Binary yaklaşık 162 KiB. Bu dosya boyutu RAM/CPU ölçümü değildir.
- ZIP layout, binary/dependency hash'leri, LF launcher, credential-path exclusion
  ve public profil whitelist'leri kontrol edildi. Web client secret/token yok.

## Canlı servis üzerinde doğrulanan

Workers Free ve D1 Free kullanılarak servis kuruldu. Ücretli plan/billing açılmadı.
Yeni D1 içinde yalnız uygulamaya ait üç tablo var. Web OAuth client ayrı;
Desktop client korunur. Secret alanlarını kullanıcı girdi. Invocation URL
günlükleri kapalı; saatlik süre temizliği ayarlı.

Kendi oluşturulup iptal edilen bir eşleştirmede `/health`, D1 create/status,
HTTPS telefon sayfası + Secure/HttpOnly cookie, yanlış cihaz proof'unun reddi,
Google onayından önce claim reddi ve cancellation geçti. Bu kontrol Google'a
giriş yapmadı, gerçek account tokenı almadı ve Google client secret'ını
doğrulamadı. Kullanıcının SD kartı bu işlemde değiştirilmedi.

## Henüz doğrulanmayan

Gerçek Google authorization-code exchange, iPhone Safari/Android Chrome ve
Miyoo Mini Plus uçtan uca giriş, gerçek cihazda iki saat sonrası yenileme,
ROM indirme, batarya, idle CPU ve RAM ölçümü bekleniyor.
[Cihaz kabul listesi](DEVICE_VALIDATION.md) bu ayrımı korur.
