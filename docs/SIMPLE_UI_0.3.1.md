# 0.3.1 — sade arayüz

Kullanıcının 15 Kasım 2026 tarihli "çok fazla yazı / SSH ayarı" geri bildirimi
üzerine normal arayüz sadeleştirildi.

- Hesap menüsü: Connect with phone, Help, Disconnect account.
- İlk açılış A: Connect; hesap menüsündeki Connect tek seçimle QR açar.
- SSH ve ayrı kurulum ekranları normal uygulamadan kaldırıldı. Bilgisayardan
  kurulum yardımcıları, mevcut Desktop hesapları ve otomatik yenileme korunur.
- QR ekranı dört kısa satır, onay aşaması üç satır gösterir. Kod karşılaştırma
  ve Miyoo'da A onayı zorunludur; iptal/hata eski hesabı korur.
- Tekrarlanan yönlendirmeler normal listelerden kaldırıldı. İndirme detaylarında
  konsol/arkadaş, boyut/boş alan, hedef ve mevcut dosya durumu korunur. Overwrite
  onayı, indirme ilerlemesi, hata ve çevrimdışı durumlar korunur.
- Kullanım, telefon, bilgisayar ve online rehber yalnız Help içindedir.

Host suite: core153, backend263, SDL/rotation/download, controller117,
Favorites84, phone190, HTTPS133 geçti. QR sırasında Help açma/geri dönme/iptal,
menü sınırları, ilk kurulum ve eski gezinme/scroll testleri geçti. Ekranlar
SDL dummy ile render edilip görsel olarak incelendi; QR bağımsız okuyucuyla
kontrol edilir. ARMv7 hard-float derlemesi ve ZIP güvenlik/layout kontrolleri
geçti. Gerçek 0.3.1 cihaz görünümü ve giriş kabul testi henüz yapılmadı.

Sunucuda ayrıca Origin/no-referrer düzeltmesi ve okunabilir giriş hata türleri
vardır. Google izin/istemci/klasör hataları ayrı gösterilir. 13 Worker mock testi
ve typecheck geçti. Kullanıcının gerçek Google denemesi eski genel
authorization_failed ile sonuçlandı; eski kayıttan kesin alt neden çıkmaz.
Yeni QR denemesinde güvenli kategoriyle doğrulama gerekir; Google doğrulaması
tamamlandı veya yeni telefon girişi başarılı oldu diye raporlanamaz.
