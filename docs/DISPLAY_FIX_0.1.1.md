# 0.1.1 cihaz görüntü düzeltmesi

14 Kasım 2026. Kullanıcı, 0.1.0'ın ilk Miyoo açılışında yazılar yerine bozuk
çizgiler gösterdiğini fotoğrafla bildirdi. Bu açılış donanım kabulünü geçmedi.
Bilgisayardaki OAuth tamamlanmış olsa da cihazda Drive erişimi doğrulanmadı.

Önceki sürüm 640 × 480 / 16 bit SDL video yüzeyine doğrudan çok sayıda FillRect
ile çiziyordu. Onion v4.3.0 cihaz görüntü başlatması 32 bit video modu kullanıyor.
Bu fark olası uyumsuzluk nedeni; fiziksel cihazdaki kesin kök neden henüz izole değil.

0.1.1, video modunu 32 bit açar ve yazıları ayrı 32 bit yazılım canvas'ına çizer.
Sonuç tek SDL_BlitSurface ile video yüzeyine aktarılıp SDL_Flip ile sunulur.
Canvas program kapanırken serbest bırakılır; yaklaşık 1,2 MiB ek RAM gerektirir.
Google yapılandırması ve indirilen oyunlar değiştirilmez.

UI testinde 32 bit video modu denetlenir; beş host test grubu ve ARM ABI denetimi
yeniden çalıştırıldı ve geçti: core153, backend126, controller53, UI ve indirme.
Loglar `build/display-fix-final-host.log` ve `build/display-fix-arm.log`.
ARM binary SHA256: `cce2a11e19b68b0e0e1ee46d735004f904ab8de32c082fcf895cb404e578ae96`.
Host sonucu gerçek Miyoo görüntüsünün düzeldiğini kanıtlamaz.
Yeni paketin cihazda yeniden denenmesi gerekir.

Tanı için uygulama her açılışta `/.config/friends-library/display-info.txt`
dosyasına video driver'ı, çözünürlük, renk derinliği ve pitch yazar. Bu dosya token,
hesap veya kütüphane içeriği taşımaz. Görüntü sorunu sürerse bu kayıt kullanılabilir.

Kaynak: [Onion v4.3.0 görüntü başlatması](https://github.com/OnionUI/Onion/blob/v4.3.0/src/clock/gfx.c#L470).
Eski test/build kanıtı [0.1.0 doğrulama raporunda](VERIFICATION.md) korunmuştur.
