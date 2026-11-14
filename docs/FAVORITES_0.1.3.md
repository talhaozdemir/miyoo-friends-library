# 0.1.3 isteğe bağlı Favorites

14 Kasım 2026. Kullanıcı, konsol klasörüne doğrudan oyun koymaya devam etmek,
isterse yalnız seçilenleri Favorites alt klasörüne taşımak ve uygulamada
Favorites'ı en üstte görmek istedi. All yalnız arayüzde bir görünüm adıdır.

```text
Ortak kök/
  TALHA/
    NES/
      Normal.nes
      Favorites/
        Favorite.nes
```

NES açıldığında `Favorites (1)` ilk satırda, `All (2)` ikinci satırda görünür.
İlk seçim mevcut bütün oyunlara erişim veren All'dır. Her görünümün seçim ve
scroll konumu ayrı korunur. Favorites'tan indirilen oyun da normal `Roms/FC/`
hedefine gider; Drive klasör adı yerel ROM yoluna eklenmez.

Favorites yoksa veya boşsa normal oyunlar etkilenmez. Favorites görünümünde
boşluk açıklanır ve B ile geri dönülür. Konsol tamamen boşsa iki sayı da sıfırdır.
Önerilen İngilizce klasör adı `Favorites`; büyük/küçük harf farkı, `Favourites`
ve önceki Türkçe `Favoriler` adı da kabul edilir. Aynı konsolda birden fazla
eşleşen klasör varsa uygulama rastgele birini seçmez; anlaşılır hata verir.

Yalnız bir alt seviye desteklenir. Favorites içindeki başka alt klasörler ve
multifile oyunlar otomatik kurulmaz. All adında ek bir Drive klasörü gerekmez
ve böyle bir klasör otomatik olarak taranmaz. PS CHD/PBP sınırlaması korunur.

Konsol kökü ve varsa Favorites ID'si sırayla asenkron listelenir. Mevcut hesap/
kök/klasör bazlı cache kullanılır; klasör yoksa ikinci ağ isteği yapılmaz. Manuel
yenileme her iki listeyi yeniler. Ortak kütüphane yalnız her iki gerekli sonuç
geldikten sonra güncellenir; izinsiz/ulaşılamayan Favorites sessizce eksik bir
All listesine dönüşmez. Önceki tam görünüm hata/iptalde korunur. Cache sonuçları
yaşlıysa birleşik görünüm de yaşlı sayılır. Toplam konsol kapsamı 5000 girdidir.

Birleşimde aynı Drive ID'si yalnız bir kez gösterilir; favori kaydı önceliklidir.
Farklı ID'lerde aynı yerel hedef adı korunur ve indirme seçiminde çakışma
bildirilir. Bu kontrol Favorites filtresinin dışındaki oyunları da kapsar.
Dosya adından kimlik çıkarmak veya Drive'a yazmak yoktur.

Testler `tests/test_favorites.c` ve `tests/fixtures/favorites-rclone.sh`:
eksik/boş klasörler, ad normalizasyonu, ID birleşimi, aynı isimli farklı ID,
global hedef çakışması, sınır, ayrı görünüm/scroll, asenkron iki aşama, offline
cache, izin hatasında önceki tam görünümü koruma ve çocuk listeleme iptali.
Diğer beş test grubu da yeniden çalıştırıldı ve geçti. Son sonuçlar: core153,
backend126, controller55 ve Favorites84 kontrolü; UI/307.200 piksel dönüş ve
indirme entegrasyonu başarılı. ARM ABI denetimi başarılı. Loglar
`build/favorites-final-tests.log`, `build/favorites-arm.log`. Menü ve boş favori
önizlemeleri 640 × 480 olarak görsel kontrolden geçti. Gerçek Google hesabı ve Miyoo
ile bu yeni özelliğin kabulü henüz yapılmadı.
