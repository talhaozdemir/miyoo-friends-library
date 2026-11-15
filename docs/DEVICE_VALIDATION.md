# Gerçek cihaz kabul listesi

0.3.0 HTTPS telefon girişi: aşağıdaki maddeler fiziksel cihazda **henüz denenmedi**.

- [ ] Hiç settings/token olmadan hazır HTTPS profili ile QR oluşur.
- [ ] iPhone Safari ve Android Chrome, ek uygulama/SSH olmadan Google girişini tamamlar.
- [ ] Telefon ve Miyoo farklı internet ağlarında da eşleşir; altı haneli kod aynıdır.
- [ ] A onayı öncesi eski hesap değişmez; kod eşleşince A ile atomik kaydedilir.
- [ ] Yanlış/izinsiz hesap, B iptal, beş dakika aşımı ve ağ kesintisi eski hesabı korur.
- [ ] Yeniden açılış, en az iki saat sonra yenileme ve gerçek Drive indirmesi çalışır.
- [ ] Servis geçici 429/503 verdiğinde Google izni iptal edilmiş gibi davranılmaz.
- [ ] Web client secret SD kartta yoktur; kişisel token ve QR loglarda yoktur.
- [ ] Dört sayfalık çevrimdışı rehber, rehber QR'si ve önceki ekrana dönüş okunabilir.
- [ ] Bilgisayar kurulumu ve Advanced SSH seçenekleri çalışır.
- [ ] Uygulamadan çıkınca curl/rclone alt süreci veya LAN login server kalmaz.

Durum: **Kullanıcı 0.1.1'de çizimin çalıştığını ama ters olduğunu doğruladı. 0.1.2 yön ve ikon güncellemesinin cihaz testi bekleniyor.**
Otomatik mock testlerinin geçmesi aşağıdaki kutuları işaretlemez.

Kayıt: cihaz/Onion sürümü, firmware, SD kart, rclone sürümü, tarih, kişi,
başlangıç bataryası ve her testin sonucu. Token, e-posta ve özel oyun listelerini
test kayıtlarına koymayın. Yalnız paylaşım hakkı bulunan küçük test dosyaları kullanın.

- [ ] OnionOS 4.3 stable / Miyoo Mini Plus sürümünü kaydet.
- [ ] ZIP'i SD köküne çıkar; Apps → Friends Library adı ve ikonu görünür.
- [ ] A/B, D-pad, X yenileme, L/R sayfalama, SELECT hesap ayırma çalışır.
- [ ] 640×480 okunabilirlik, uzun isimler, Türkçe karakter gösterimini kontrol et.
- [ ] Wi-Fi kapalı açılış açıklayıcıdır; önbellek varsa listeler görünür.
- [ ] İlk kişisel OAuth config'i tanınır; hiçbir token loga yazılmaz.
- [ ] Token sona erince sessiz yenilenir; rclone.conf güvenle güncellenir.
- [ ] 0.1.4: ilk girişten en az 2 saat sonra uygulamayı tekrar aç; bilgisayara
      ihtiyaç olmadan listeleme ve indirmeyi dene, cihaz tarih/saatini kaydet.
- [ ] 0.1.4: 401 sonrası tek otomatik yenileme görünümü, iptal ve yeniden açılış
      çalışır; gerçek invalid_grant hatası ayrı ve tokensız gösterilir.
- [ ] İptal edilmiş token yeniden bilgisayarda yetkilendirme mesajı verir.
- [ ] My Drive'a kopyalamadan davetli kök ve dinamik arkadaşlar listelenir.
- [ ] Boş/izinsiz/erişilemeyen klasör ve bilinmeyen konsol anlaşılırdır.
- [ ] Favorites klasörü yokken normal oyunlar All içinde listelenir.
- [ ] Favorites üstte, All altta; All kök ve Favorites oyunlarını birlikte gösterir.
- [ ] Boş Favorites, ayrı seçim/scroll, X yenileme ve offline cache denenir.
- [ ] Küçük NES ve NDS dosyası doğru Roms/FC ve Roms/NDS konumuna iner.
- [ ] Eksiksiz tek dosyalı PS CHD/PBP, Roms/PS konumuna akışla iner.
- [ ] Mevcut aynı isimli ROM onaysız değişmez; farklı içerik ayrımı görünür.
- [ ] Manuel Games → SELECT → Refresh all roms sonrası oyun görünür.
- [ ] Gerekli emülatör/BIOS kullanıcı tarafından kuruluysa oyun Onion'dan açılır.
- [ ] İndirme sırasında B iptal eder; mevcut ROM ve kayıtlar korunur.
- [ ] Wi-Fi kesilmesi / süre aşımı / alan tükenmesi açıklayıcı hata verir.
- [ ] İndirme sırasında güvenli kesinti sonrası relaunch yarım dosyayı kurulu saymaz.
- [ ] Büyük transferde RAM/CPU ve boş alan ölç; OOM veya UI takılması yok.
- [ ] 15 dakika idle ve aktif transfer batarya değişimini, cihaz ısısını kaydet.
- [ ] Kapatıp yeniden açınca config, seçilen kütüphane ve kurulu durum korunur.
- [ ] Uygulamadan çıkınca friends-library/rclone alt süreci kalmaz.
- [ ] SELECT disconnect çevrimiçi iptali dener; çevrimdışı yerel credential silinir.
- [ ] Güncelleme kullanıcı config, ROM, BIOS ve Saves dosyalarına dokunmaz.
- [ ] Kaldırma sonrası indirilen ROM'lar Onion'da kalır.

0.2.0 telefon akışı (bu maddeler henüz gerçek cihazda doğrulanmadı):

- [ ] Yeni SD uygulama kurulumunda hiçbir settings/token yokken arkadaş paketinin
      public profiliyle telefon girişi başlar; Computer setup alternatifi görünür.
- [ ] Android ve iPhone'da authenticated SSH LOCAL 53682→127.0.0.1:53682 tüneli
      normal tarayıcıdaki Google girişi boyunca açık kalır.
- [ ] Login QR gerçek Miyoo ekranından okunur; rehber QR'si doğru sayfayı açar.
- [ ] Sekiz rehber sayfası, IP adresi ve geri dönüşü çevrimdışı okunabilir.
- [ ] Gerçek davetli hesap klasör metadata kontrolünü geçer ve sonraki listeleme/
      indirme çalışır. Davetsiz hesap, B iptali ve süre aşımı eski hesabı korur.
- [ ] İlk girişten 2+ saat ve yeniden başlatma sonra otomatik token yenilemesi
      çalışır; telefon tüneli veya bilgisayar gerekmez.
- [ ] OAuth dinleyicisi yalnız 127.0.0.1:53682'de ve giriş süresince vardır;
      app kapanınca rclone/port kalmaz. SSH'yi kullanıcı kapatır.
- [ ] Giriş sırasında güç kesilmesi sonrası yalnız uygulamanın bilinen staging
      dosyaları temizlenir; mevcut config ve ROM'lar korunur.
- [ ] Eski Windows .cmd ve Mac .command kurulumları çalışmayı sürdürür.

Başarısızlıkta sürüm, adım ve hassas veri içermeyen hata kodu kaydedilir; aynı
paket için donanım kabulü verilmez. TLS hatasında önce cihaz saatini doğrulayın.
