# Menü ikonu boyut düzeltmesi

14 Kasım 2026. Kullanıcı 96 × 96 ikonun menüde tam görünmediğini bildirdi.
Kaynak çizim değiştirilmeden şeffaf sınırı kırpılıp en fazla 52 × 52 alana
ölçeklendi ve 64 × 64 şeffaf canvas ortasına yerleştirildi. Son görünür alpha
sınırı `(11, 6, 55, 58)`; kenarlarda en az 6 piksel boşluk var.

`scripts/prepare-icon.py` (host Python/Pillow) bu işlemi tekrarlar ve yalnız
`App/FriendsLibrary/icon.png` içeren `v0.1.3-icon-small.zip` paketini oluşturur.
Çalışma zamanına bağımlılık eklenmez. C kodu, ARM binary, Google config ve ROM'lar
değişmez. Tam paket ve kaynak paketleri `0.1.3-iconfix` etiketiyle yenilenir;
uygulamanın fonksiyonel sürümü 0.1.3 olarak kalır.

PNG boyutu, alpha, boş kenarlar ve ZIP içeriği kontrol edildi. Menüdeki son
görünümü kullanıcı cihazında tekrar doğrulanacak. Kurulum: küçük ZIP'i SD köküne
açıp mevcut icon.png dosyasını değiştir; Miyoo menüsünü yeniden aç/yeniden başlat.
