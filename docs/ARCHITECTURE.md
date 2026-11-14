# Teknik karar kaydı — 2026-11-14

Hedef OnionOS **v4.3.0 / Miyoo Mini Plus**; ARMv7 Cortex-A7 hard-float.
Native uygulama C + SDL 1.2 seçildi. Python, SDL2, ncurses, dialog, jq veya
desktop Linux paketi var sayılmıyor. Font uygulama içinde özgün bitmap
glyph tablosu; font, SDL_ttf veya SDL_image runtime gereksinimi eklenmedi.

Drive için sürüm/hash sabit rclone alt süreci seçildi. Doğrudan API, OAuth
yenileme ve TLS/indirme kodunu yeniden yazmayı gerektiriyor; dış sunucu ise
normal kullanım bağımsızlığına ve bakım sadeliğine ters ek bağımlılık getiriyor.
rclone browser OAuth'u bir defa bilgisayarda yapar; cihazda refresh token ile
çalışır. Servis, mount veya açık RC HTTP listener kullanılmaz. Alt süreçler
argüman dizisiyle başlatılır; ağdaki dosya adları shell komutuna çevrilmez.

Listeleme ve indirme UI thread'ini bekletmeyen sahipli alt süreçlerdir.
Bir indirme, sınırlı tamponlar, stream/hash doğrulaması, hedefle aynı dosya
sisteminde geçici dosya ve son aşamada güvenli kurulum kullanılır. Cache klasör,
hesap ve kök kimliğiyle ayrılır; cihazın tüm Drive'ı başlangıçta taranmaz.
Onion'ın ROM cache/DB, save, BIOS ve emülatör ayarları değiştirilmez.

RomManager kaynak/dağıtımı incelendi; README'deki build kaynaklarına karşılık
incelenen tree'de yeniden üretilebilir tam UI kaynağı bulunmadığından kod veya
binary bağımlılığı alınmadı. MIT lisansı tek başına yeniden kullanım gerekçesi
değildir. Cloud-saves projesi ARMv7 rclone kullanımına tarihî emsal sağlar;
güncel binary'nin gerçek cihazda kabulünü kanıtlamaz.

Kanıt kaynakları:

- [Onion v4.3 toolchain](https://github.com/OnionUI/Onion/blob/v4.3.0/Makefile)
- [ARM ayarları](https://github.com/OnionUI/Onion/blob/v4.3.0/src/common/config.mk)
- [Tuş haritası](https://github.com/OnionUI/Onion/blob/v4.3.0/src/common/system/keymap_sw.h)
- [ROM refresh](https://onionui.github.io/docs/faq)
- [rclone Drive](https://rclone.org/drive/)
- [RomManager](https://github.com/Plus-Cloud/RomManager)
- [Cloud-saves emsali](https://github.com/hotcereal/cloud-saves-miyoo-mini-plus)

Bu karar kaynak incelemesi ve host testleriyle desteklenir; gerçek cihaz, batarya,
Wi-Fi, token yenileme ve özel paylaşılan klasör kabulü ayrıca yapılmalıdır.
