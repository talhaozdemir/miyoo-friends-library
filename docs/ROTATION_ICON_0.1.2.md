# 0.1.2 ekran yönü ve ikon

14 Kasım 2026. Kullanıcı 0.1.1'in artık çizdiğini ama görüntünün ters olduğunu
bildirdi ve yeni ikon istedi. ARM hedefinde son sunum 180 derece döndürülür.
Mantıksal çizim, seçimler, geri gezinme ve fiziksel tuş eşlemeleri değişmez.
Host önizlemesi normal yöndedir; `FL_DISPLAY_ROTATION=180` test override'ıdır.
`FL_DISPLAY_ROTATION=0` yalnız tanı için dönüşü kapatabilir.

Dönüş yazılım yüzeyinde, her yüzeyin pitch değeri gözetilerek yapılır; SDL son
renk formatı dönüşümünü ve sunumu üstlenir. Ek yüzey yaklaşık 1,2 MiB RAM kullanır;
mevcut olay bazlı çizim ve 300 ms ilerleme sınırı korunur.

UI regresyon testi ekrana sunulan 307.200 pikseli 180 derece referansıyla birebir
karşılaştırdı ve tuşları yeniden doğruladı. Beş host test grubu geçti:
core153, backend126, controller53, UI/rotation ve download. ARM hard-float build
ve glibc ABI kontrolü geçti. Loglar `build/rotation-final-host.log` ve
`build/rotation-arm.log`. Bunlar kullanıcı cihazındaki sonucun yerine geçmez.

Yeni ikon built-in image_gen ile üretildi; kaynak PNG ve tam istem `assets/`
altındadır. Kurulumda mevcut 96 × 96 ikon boyutu ve PNG alpha korunur.
SDL_image veya ek çalışma zamanı bağımlılığı eklenmez.

Küçük güncelleme yalnız `App/FriendsLibrary/friends-library` ve
`App/FriendsLibrary/icon.png` içerir. Google ayarları, tokenlar ve ROM dosyaları
pakette bulunmaz. Sıradaki kabul: kullanıcı cihazında doğru yön ve menü ikonunu doğrula.
