# ROM konumları ve V1 dosya sınırı

Merkezi izin listesi `package/App/FriendsLibrary/config/systems.json` dosyasıdır.
Konsol takma adları büyük/küçük harfe duyarsız eşlenir; hedef dizin adı sabittir.
Bilinmeyen konsol indirmeye açılmaz. Kaynak klasörü kişiyi, konsolu ve dosyayı
dinamik belirler; hiçbir arkadaş adı yapılandırmada sabitlenmez.

| Kaynak alias | Hedef | V1 uzantıları |
| --- | --- | --- |
| FC / NES / Famicom | `Roms/FC/` | nes, fds, unf, unif, zip, 7z |
| SFC / SNES / Super Famicom | `Roms/SFC/` | sfc, smc, fig, zip, 7z |
| GB / Game Boy | `Roms/GB/` | gb, zip, 7z |
| GBC / Game Boy Color | `Roms/GBC/` | gbc, gb, zip, 7z |
| GBA / Game Boy Advance | `Roms/GBA/` | gba, zip, 7z |
| MD / Genesis / Mega Drive | `Roms/MD/` | md, gen, smd, bin, zip, 7z |
| PS / PS1 / PSX | `Roms/PS/` | chd, pbp |
| NDS / DS / Nintendo DS | `Roms/NDS/` | nds |

İzin listesi, Onion'ın desteklediği bütün formatları kapsamayı hedeflemez.
PS yalnız eksiksiz tek dosyalı oyunlar içindir; BIN/CUE, CCD/TOC, M3U ve çok diskli
setler bu sürümde kurulmaz. CHD uzantısı tek başına oyunun tek diskli olduğunu
kanıtlamaz: paylaşım yapan kişi çok diskli setleri bu kütüphaneye koymamalıdır.
ZIP/7z içerikleri açılmaz veya oyun eksiksizliği için incelenmez; yalnız tek
desteklenen ROM içeren arşivleri paylaşın. Multifile oyunları arşivle gizlemeyin.

Emülatör paketleri Onion Package Manager'dan kullanıcı tarafından kurulur. Bu
uygulama paketleri veya BIOS'u kurmaz; gerekli BIOS kullanıcı tarafından sağlanır.
Örneğin FDS ve PlayStation için uygun BIOS gerekebilir. DOS, arcade ROM setleri,
bağımlılık paketleri veya otomatik klasör tahmini eklenmemiştir.

Kaynak doğrulaması Onion **v4.3.0** emülatör `config.json` dosyaları üzerinden
yapıldı: [emülatör paketleri](https://github.com/OnionUI/Onion/tree/v4.3.0/static/packages/Emu).
PS emülatörünün config dizini `Emu/PSX` olsa da `rompath` değeri `Roms/PS`'dir.
SFC'deki `swc` v4.3.0 config izin listesinde bulunmadığından V1'e alınmadı.
NDS [config](https://github.com/OnionUI/Onion/blob/v4.3.0/static/packages/Emu/Nintendo%20-%20DS%20(Drastic)/Emu/NDS/config.json).
