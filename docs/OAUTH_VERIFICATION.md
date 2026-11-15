# Google OAuth doğrulama başvuru dosyası

15 Kasım 2026. Bu belge başvuru hazırlığıdır; Google onayı veya CASA denetimi
tamamlandığı anlamına gelmez. Kullanıcı QR girişinin çalıştığını bildirdi.

## Gerçek uygulama ve kapsam

- Proje: `miyoofriendslibrary`; uygulama: MiyooFriendsLibrary.
- Kaynak: https://github.com/talhaozdemir/miyoo-friends-library
- Ana sayfa: https://talhaozdemir.github.io/miyoo-friends-library/
- Gizlilik: https://talhaozdemir.github.io/miyoo-friends-library/privacy.html
- Koşullar: https://talhaozdemir.github.io/miyoo-friends-library/terms.html
- İstenen tek kapsam: `https://www.googleapis.com/auth/drive.readonly`.
- Mevcut Desktop OAuth istemcisi ve HTTPS QR Web istemcisi korunur.
- Drive özel kalır. Her katılımcı kendi hesabıyla, davet edildiği klasöre erişir.

Cloud Console başlangıçta Data Access listelerinde hiçbir kapsam göstermiyordu;
kodda istenen readonly izin başvuru listesine bildirilmelidir. Bu bir izin
genişletme veya mevcut kullanıcılara yeniden izin verme işlemi değildir.

## Marka sahipliği

Canlı Branding > Verify branding kontrolü şu eksikliği bildirdi:
"The website of your home page URL ... is not registered to you."
Google, sahiplik doğrulamasından sonra yeniden kontrol için 24 saat beklenmesini
istiyor. Search Console URL-prefix mülkü mevcut ana sayfa için açıldı. Google'ın
verdiği `google9a8124ee0004bb33.html` dosyası aynı Pages dizinine yayınlanmak üzere
eklendi. Dosya herkese açık sahiplik kanıtıdır; OAuth tokenı veya client secret
değildir. Doğrulama devam ettiği sürece dosyayı silmeyin.

Sahiplik, marka onayı, restricted-scope onayı ve varsa CASA birbirinden ayrı
adımlardır. Birinin tamamlanması diğerlerinin onaylandığı anlamına gelmez.

## İngilizce kapsam gerekçesi (form için taslak)

Miyoo Friends Library is a read-only browser and downloader for a private Google
Drive library shared by invitation among friends. Each person authorizes their
own account. On a Miyoo Mini Plus running OnionOS, rclone lists existing nested
folders and reads file IDs, names, sizes and hashes, then streams only files the
user selects into local ROM directories. The app never uploads, edits, deletes
or changes sharing. Browsing is rooted at the configured library folder; the
OAuth scope itself is account-wide. drive.metadata.readonly cannot download
content. drive.file does not provide the existing rclone flow with discovery
and download access to arbitrary pre-existing shared files without additional
per-file authorization. A Picker-based redesign has not been implemented.
Cloudflare handles optional OAuth exchange and renewal, not game downloads.

## Veri akışı ve saklama beyanı

- Cihaz: liste/metadata önbelleği, indirme kayıtları ve kişisel OAuth yapılandırması
  SD kartta; ROM dosyaları OnionOS dizinlerinde. FAT32 fiziksel erişime karşı
  güvenilir dosya izinleri sağlamaz.
- Desktop kurulumu: rclone doğrudan Google ile yetkilendirir ve yeniler.
- HTTPS QR: telefondan kod/state/cookie/PKCE doğrulaması; Worker Google token
  exchange ve sabit klasör metadata erişim kontrolünü yapar. Cihazda kod
  karşılaştırılıp A ile onaylanana kadar mevcut hesap değiştirilmez.
- Pending token paketi D1'de AES-GCM şifreli; claim 5 dakika sonra engellenir.
  ACK/cancel kaydı kaldırır; saatlik temizlik süresi dolmuş kayıtları temizler.
  Cloudflare yedekleri eski şifreli kayıtları tutabilir. Anahtara sahip operatöre
  karşı şifreleme koruması iddia edilmez.
- Kalıcı sunucu kaydı: refresh-token SHA-256 özeti ve grant/rate-limit durumu;
  kalıcı plaintext refresh-token deposu yoktur. Worker yetkilendirme ve yenileme
  sırasında gerçek tokenları işler. Web client secret yalnız Worker Secrets'tadır.
- ROM indirmeleri/listeleme cihazdan doğrudan Google'a gider; sunucu Google
  tokenları ve klasör doğrulama metadata'sını işlediğinden "sunucu hiç Google
  verisi görmez" veya "CASA kesin gerekmiyor" beyanı yapılamaz.
- Analitik/reklam/AI eğitimi yoktur; OAuth callback invocation logging kapalıdır.
- Uygulamada Disconnect yerel kimlik bilgilerini siler ve online revocation
  dener. Google hesabından uygulama erişimi kaldırılabilir. Sunucudaki retained
  grant kayıtlarının silinmesi için operatöre başvurulur; bu self-service olarak
  uygulanmış değildir.

## Gerçek demo videosu için çekim sırası

Google formu YouTube bağlantısı ve projedeki bütün OAuth istemcilerinin
gösterilmesini istiyor. Video henüz çekilmedi/yüklenmedi; örnek URL kullanmayın.
Yalnız paylaşmaya yasal olarak yetkili olduğunuz test dosyasını kullanın.

1. Uygulama adı, ana sayfa, gizlilik ve amaç; OnionOS Apps'ten gerçek cihaz açılışı.
2. Connect with phone, yeni QR, telefonda HTTPS sayfası ve kod karşılaştırması.
3. Google uyarısı/consent ve yalnız readonly Drive izni. Şifre, OTP, token,
   client secret, özel QR/pairing URL veya SD kimlik dosyasını videoya almayın.
   Gerekli adımları göstermeyi engellemeyen şekilde kişisel bilgileri maskeleyin.
4. Telefonda tamamlanma, cihazda A ile kaydetme; friend/console/Favorites gezinme.
5. Küçük izinli test ROM'unun seçilmesi, hedef/boyut/onay, indirme, tamamlanma,
   OnionOS ROM listesini yenileyip dosyanın görünmesi.
6. Projede tutulan Desktop istemcisi için mevcut bilgisayar kurulum akışını da
   gösterin; token/config içeriğini veya Web secret'ı açmayın.
7. Disconnect/revoke seçeneklerini açıklayın; çalışan kişisel hesabı yalnız demo
   uğruna iptal etmeyin. Gerçekte göstermediğiniz testi başarılı diye anlatmayın.
8. Liste dışı YouTube videosu olarak sahibinin hesabından paylaşın; Google'ın
   inceleyebileceği bağlantıyı forma girin.

## İnceleme ve bütçe sınırı

Google'ın restricted-scope kuralları, sunucu üzerinden bu verilere erişen
uygulamalarda bağımsız güvenlik değerlendirmesi gerektirebilir. Değerlendirme
gerekliliğini/tier'ını Google belirler; ücretli bir denetim veya yıllık taahhüt
başlatılmadı. Bu dosya bir güvenlik denetimi raporu değildir.

Kişisel olarak tanınan küçük arkadaş grubu Google'ın personal-use istisnasına
uyabilir; bu istisna doğrulanmamış uygulama uyarısını kaldırmaz. Kullanıcının
amacı uyarıyı kaldırmak olduğundan yalnız istisna beyan edip süreci tamamlandı
saymayın. Tüm onayları Google verir; başvuru sonucu garanti edilmez.

Resmî kaynaklar:

- https://developers.google.com/identity/protocols/oauth2/production-readiness/restricted-scope-verification
- https://support.google.com/cloud/answer/13807376
- https://support.google.com/cloud/answer/13464018
- https://developers.google.com/workspace/drive/api/guides/api-specific-auth
