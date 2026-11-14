"""Reproducible Turkish installation PDF. Requires ReportLab and embedded fonts."""
from pathlib import Path
from reportlab.pdfgen import canvas
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, PageBreak, Table, TableStyle
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "output/pdf/Miyoo-Friends-Library-Kurulum-Rehberi.pdf"
OUT.parent.mkdir(parents=True, exist_ok=True)
FONTS = Path("C:/Windows/Fonts")
for name, file in [("Segoe", "segoeui.ttf"), ("SegoeBold", "segoeuib.ttf"), ("Code", "consola.ttf")]:
    pdfmetrics.registerFont(TTFont(name, str(FONTS / file)))
pdfmetrics.registerFontFamily("Segoe", normal="Segoe", bold="SegoeBold")
NAVY, TEAL, GRAY = colors.HexColor("#152A3B"), colors.HexColor("#087F79"), colors.HexColor("#526474")
styles = {
    "title": ParagraphStyle("title", fontName="SegoeBold", fontSize=27, leading=33, textColor=NAVY, spaceAfter=16),
    "sub": ParagraphStyle("sub", fontName="SegoeBold", fontSize=15, leading=20, textColor=TEAL, spaceBefore=15, spaceAfter=8),
    "body": ParagraphStyle("body", fontName="Segoe", fontSize=11, leading=16, textColor=NAVY, spaceAfter=10),
    "small": ParagraphStyle("small", fontName="Segoe", fontSize=9, leading=13, textColor=GRAY, spaceAfter=8),
    "code": ParagraphStyle("code", fontName="Code", fontSize=10, leading=15, textColor=NAVY, spaceAfter=10),
}
story = []
def p(text, style="body"):
    story.append(Paragraph(text, styles[style]))
def heading(num, title, subtitle):
    if story:
        story.append(PageBreak())
    p(f"{num:02d} / KURULUM REHBERİ", "small")
    p(title, "title")
    p(subtitle)
def step(n, title, text):
    p(f"{n}. {title}", "sub")
    p(text)
def note(title, text):
    box = Table([[Paragraph(f"<b>{title}</b><br/>{text}", styles["body"])]], colWidths=[475])
    box.setStyle(TableStyle([("BACKGROUND", (0,0), (-1,-1), colors.HexColor("#EAF5F3")),
                            ("BOX", (0,0), (-1,-1), .7, colors.HexColor("#BEDDD6")),
                            ("LEFTPADDING", (0,0), (-1,-1), 13), ("RIGHTPADDING", (0,0), (-1,-1), 13),
                            ("TOPPADDING", (0,0), (-1,-1), 11), ("BOTTOMPADDING", (0,0), (-1,-1), 5)]))
    story.extend([box, Spacer(1, 12)])

heading(1, "Miyoo Friends Library", "Arkadaşların paylaştığı özel Google Drive oyun kütüphanesini Miyoo Mini Plus üzerinden gez ve oyunları indir.")
note("İlk kurulum bilgisayarda, kullanım Miyoo'da", "Bu ZIP ve rehber yeterli. Kendi Google hesabınla bir kez bağlanırsın. Uygulama ve ortak klasör tanımı hazırdır; OAuth istemcisi oluşturman gerekmez.")
p("Başlamadan önce", "sub")
p("Miyoo Mini Plus + OnionOS 4.3, microSD kart okuyucu, internet ve Windows 10/11 (64 bit) veya macOS 12 ve üzeri gerekir. Mac'te Intel ve Apple Silicon desteklenir.")
p("Google hesabın <b>ortak Drive klasörüne davetli</b> olmalı. Uygulama artık Production durumunda; test kullanıcıları listesine eklenmen gerekmez. Klasör iznin eksikse paketi gönderen kişiye haber ver.")
step(1, "ZIP'i bilgisayara çıkar", "Miyoo-Friends-Library-Arkadas-Kurulum.zip dosyasını tamamen aç. İçinden App ve FirstSetup klasörleri çıkacak. Kurulumu ZIP'in içinden çalıştırma.")
step(2, "App klasörünü SD kartın köküne kopyala", "Kartı tak. ZIP'teki <b>App</b> klasörünü, karttaki mevcut App klasörüyle birleştir. Sonuç aşağıdaki gibi olmalı. Kartı biçimlendirme; mevcut App klasörünü silme. FirstSetup bilgisayarda kalabilir.")
p("SD kart/ &nbsp; (örnek: E: veya /Volumes/MIYOO)<br/>&nbsp;&nbsp;App/<br/>&nbsp;&nbsp;&nbsp;&nbsp;FriendsLibrary/<br/>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;config.json<br/>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;launch.sh", "code")
p("Şimdi Windows için sayfa 2'ye, Mac için sayfa 3'e geç. Google girişi iki sistemde de sayfa 4'te anlatılıyor.", "small")

heading(2, "Windows'ta hesabını bağla", "Kaynak kod, PowerShell 7 veya ayrıca rclone kurman gerekmez. Yönetici olarak çalıştırma.")
step(1, "Kartın sürücü harfine bak", "Dosya Gezgini > Bu bilgisayar bölümünden Miyoo kartının harfini öğren. Örneğin <b>E:</b>. Her bilgisayarda aynı harf olmayabilir.")
step(2, "Windows başlatıcısını aç", "Bilgisayara çıkardığın pakette <b>FirstSetup</b> klasörünü aç. <b>Connect-Windows.cmd</b> dosyasına çift tıkla. .ps1 dosyasına çift tıklamak yerine .cmd başlatıcısını kullan.")
step(3, "SD kartı belirt", "SD kart sorulduğunda kendi kartının harfini yaz ve Enter'a bas. Örneğin:")
p("E:", "code")
p("Yanlış disk seçilirse App/FriendsLibrary/config.json bulunmadığı için kurulum durur. Diskte App/App/FriendsLibrary oluştuysa kopyalama konumunu düzelt.")
step(4, "Google girişini tamamla", "Tarayıcı açılınca sayfa 4'teki adımları uygula. Giriş sürerken terminali kapatma. Tarayıcıda Success yazdıktan sonra terminale dön.")
step(5, "READY mesajını bekle", "Terminalde <b>READY</b> göründüğünde hesap ayarları karttadır. Windows'tan <b>Güvenle kaldır</b> yap; kartı Miyoo'ya tak ve sayfa 5'ten devam et.")
note("Daha önce hesap bağlandıysa", "Reconnect sorusunda Enter mevcut hesabı korur. Yeniden giriş yapmak veya hesabı değiştirmek için <b>YES</b> yaz. Bağlantı başarısız olursa mevcut hesap dosyası korunur.")
p("Güvenlik uyarısı çıkarsa paketin beklediğin kişiden geldiğini kontrol et. Kurulum genel PowerShell güvenlik ayarlarını değiştirmez; .cmd yalnız kendi başlattığı süreç için betik çalıştırma seçeneği kullanır.", "small")

heading(3, "Mac'te hesabını bağla", "Python, Homebrew veya yönetici parolası gerekmez. İlk çalıştırmada Mac işlemcine uygun resmi rclone indirilir ve SHA-256 doğrulanır.")
step(1, "Terminal'i aç", "Command + Space tuşlarına bas, Terminal yaz ve Enter'a bas. Paketin tamamen çıkarılmış ve App klasörünün SD karta kopyalanmış olduğundan emin ol.")
step(2, "Başlatıcıyı Terminal'e sürükle", "Terminal'e <b>bash</b> yazıp bir boşluk bırak. Finder'dan FirstSetup içindeki <b>Connect-Mac.command</b> dosyasını Terminal penceresine sürükle. Sonra Enter'a bas.")
p("bash /dosyanin/yolu/FirstSetup/Connect-Mac.command", "code")
p("Yukarıdaki yolu elle yazma; sürükleme işlemi doğru yolu ve boşlukları hazırlar. İlk indirme internet hızına göre biraz sürebilir.")
step(3, "SD kartın kökünü seç", "Açılan klasör seçicide SD kartı seç. Seçtiğin yer App klasörünü içermeli; App veya FriendsLibrary klasörünün içine girme.")
step(4, "Google girişini tamamla", "Tarayıcı açılınca sayfa 4'ü uygula. Terminal açık kalsın. Tarayıcıda Success yazması tek başına son adım değildir; terminalde de <b>READY</b> görmelisin.")
step(5, "Kartı güvenle çıkar", "Finder'da kartın yanındaki çıkar simgesine bas. Kartı Miyoo'ya takıp sayfa 5'ten devam et.")
note("Mevcut hesabı koruma", "Reconnect sorusunda Enter mevcut hesabı korur. Tekrar bağlanmak için <b>YES</b> yaz. macOS 12'den eski sürümde yardımcı desteklenmez.")
p("Doğrulama durumu: Mac yardımcısının bash sözdizimi kontrol edildi; gerçek bir Mac üzerinde Google giriş akışı henüz denenmedi. Sorun olursa hata mesajını paketi gönderen kişiye ilet; hesap dosyanı gönderme.", "small")

heading(4, "Google girişini tamamla", "Windows ve Mac için ortak adımlar. Her arkadaş kendi hesabıyla bağlanır; paketi gönderen kişinin hesabı kullanılmaz.")
step(1, "Doğru hesabı seç", "Ortak Drive klasörüne davet edildiğin Google hesabını seç. Tarayıcı başka hesapta açıksa hesap seçimini değiştir. Production durumunda ayrıca test kullanıcısı kaydı gerekmez.")
step(2, "Uygulama adını kontrol et", "Beklenen uygulama <b>MiyooFriendsLibrary</b>. Google'ın gösterdiği izinleri oku ve erişimi onayla. Bu kurulum Drive dosyalarını listelemek ve indirmek için <b>salt okunur</b> yetki ister.")
p("Google doğrulaması henüz tamamlanmadığı için doğrulanmamış uygulama uyarısı görülebilir. Yalnız tanıdığın kişinin bu paketini kurduğundan ve doğru uygulamada olduğundan eminsen devam et. Başka bir uygulama veya yazma/silme izni görürsen işlemi durdur.")
step(3, "Success ekranından terminale dön", "Tarayıcıda <b>Success! All done. Please go back to rclone.</b> yazdığında izin verilmiştir. Terminale dön ve kurulumun READY mesajını bekle. Sonrasında tarayıcı sekmesini kapatabilirsin.")
note("403 / access_denied", "Seçilen hesabı, uygulama projesinin yayın durumunu ve Google hesabındaki kurumsal güvenlik kısıtlarını kontrol et. Klasör erişimi için ayrıca Drive daveti gerekir. Hata sürerse paketi gönderen kişiye bildir.")
p("Production ve yeniden giriş", "sub")
p("Uygulama 14 Kasım 2026'da <b>In production</b> durumuna geçirildi. Bundan sonra alınan yetkilendirmelerde test moduna özgü <b>7 günlük sınır uygulanmaz</b>. Test modunda daha önce bağlandıysan kartı bilgisayara takıp aynı başlatıcıdan <b>YES</b> ile bir kez yeniden giriş yap.")
p("Oturum yine de izin iptali veya uzun süre kullanılmama gibi nedenlerle sona erebilir. Google doğrulaması tamamlanmadığından uyarı ve 100 kullanıcı sınırı devam eder. Bu paket sınırsız oturum süresi vaat etmez.", "small")
p("Bu yardımcıda n/24/friends seçmek veya client_id/client_secret elle girmek gerekmez. Bu sorular görünüyorsa paket başlatıcısı yerine rclone config komutunu elle açmış olabilirsin.", "small")

heading(5, "Miyoo'da kullan", "OnionOS oyunları çalıştırmaya devam eder. Friends Library yalnız ortak kütüphaneyi listeler ve seçtiğin dosyayı indirir.")
step(1, "Wi-Fi'yi aç ve uygulamayı başlat", "Miyoo'da Wi-Fi bağlantısını ve tarih/saatin doğruluğunu kontrol et. <b>Apps > Friends Library</b> yolunu aç. İlk liste yüklemesi bağlantıya göre birkaç saniye sürebilir.")
step(2, "Arkadaş ve konsol seç", "D-pad ile gez, <b>A</b> ile aç, <b>B</b> ile geri dön. Listeler Drive'dan bulunur; isimler önceden sabitlenmez. Ekrandaki Refresh kısayoluyla listeyi yenileyebilirsin.")
p("Favorites en üstte görünür. <b>All</b>, konsol altındaki normal oyunlarla Favorites oyunlarını birlikte gösteren ekran seçeneğidir; Drive'da All klasörü oluşturulmaz. Favorites eklemek isteğe bağlıdır.")
p("Ortak klasör/<br/>&nbsp;&nbsp;ARKADAS/<br/>&nbsp;&nbsp;&nbsp;&nbsp;NES/<br/>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Ornek.nes<br/>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Favorites/<br/>&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Favori.nes", "code")
step(3, "İndirme bilgilerini kontrol et", "Bir oyun seç. Boyut, boş alan, hedef klasör ve mevcut dosya durumunu kontrol edip <b>A</b> ile onayla. Aktarımda <b>B</b> iptal eder. İndirme bitene kadar kartı çıkarma ve cihazı kapatma.")
step(4, "OnionOS oyun listesini yenile", "Uygulamadan çık. OnionOS ana menüsünde <b>SELECT > Refresh all roms</b> seçeneğini kullan. Ardından oyunu normal Games bölümünden aç. İlgili konsolun emülatörü Onion Package Manager'da kurulu olmalı.")
note("Dosyalar ve mevcut oyunlar", "NES/FC dosyaları Roms/FC'ye; SNES/SFC dosyaları Roms/SFC'ye; DS/NDS dosyaları Roms/NDS'ye gider. PS için tam CHD/PBP kullan. CUE/BIN gibi çok dosyalı oyunların yalnız bir parçasını indirme. Yalnız paylaşmaya hakkınız olan dosyaları ekleyin.")

heading(6, "Sorun giderme ve güvenlik", "Bir sorun bildirirken ekrandaki hata metnini paylaş. rclone.conf dosyasını, erişim tokenını veya yenileme tokenını kimseye gönderme.")
rows = [("Sorun", "Ne yapmalı?"),
        ("Hesap / oturum hatası", "Kartı bilgisayara tak; aynı FirstSetup başlatıcısını YES ile yeniden çalıştır. Eski Testing oturumu için bir kez yeniden giriş yap."),
        ("Klasör görünmüyor / erişim reddedildi", "Aynı Google hesabına Drive klasörü daveti verildiğini kontrol et. Google girişinin başarılı olması tek başına klasör izni vermez."),
        ("İndirme / ağ hatası", "Wi-Fi ve tarih/saati kontrol et. Sonra tekrar dene. Çevrimdışıyken önbellekteki listeler görünebilir; indirme için bağlantı gerekir."),
        ("Oyun Games'te yok", "SELECT > Refresh all roms çalıştır. Doğru konsolun Onion emülatörünün kurulu ve dosya biçiminin destekli olduğunu kontrol et."),
        ("Dosya zaten var / alan yetersiz", "Mevcut oyunu koru veya açıkça değiştirmeyi onayla. Yer açman gerekiyorsa kendi dosyalarını kontrol et; saves/BIOS klasörlerini silme.")]

table = Table([[Paragraph(a, styles["small"]), Paragraph(b, styles["small"])] for a,b in rows], colWidths=[142,333], hAlign="LEFT")
table.setStyle(TableStyle([("BACKGROUND",(0,0),(-1,0),colors.HexColor("#DDEEEB")), ("VALIGN",(0,0),(-1,-1),"TOP"), ("LINEBELOW",(0,0),(-1,-1),.4,colors.HexColor("#D7E0E6")), ("LEFTPADDING",(0,0),(-1,-1),9), ("RIGHTPADDING",(0,0),(-1,-1),9), ("TOPPADDING",(0,0),(-1,-1),8), ("BOTTOMPADDING",(0,0),(-1,-1),3)]))
story.extend([table, Spacer(1,10)])
p("Hesap, güncelleme ve kaldırma", "sub")
p("ZIP'te kişisel hesap tokenı yoktur. Her hesap kartta .config/friends-library altında saklanır. FAT32 dosya izinleri hesabı fiziksel erişime karşı koruyamaz. Salt okunur OAuth izni erişebildiğin Drive dosyalarını kapsar; uygulama yalnız tanımlı ortak klasörü gösterir.", "small")
p("Hesabı ayırmak için uygulamanın hesap bağlantısını kesme seçeneğini kullan; Google hesabındaki üçüncü taraf erişimlerinden uygulamanın iznini de iptal edebilirsin. Güncellemede App/FriendsLibrary dosyalarını birleştir; kişisel .config klasörünü başkasına gönderme. Kaldırmak için yalnız App/FriendsLibrary'yi sil. İndirilen ROM'lar yerinde kalır.", "small")
p("Doğrulama: Windows 5.1 yardımcısı sahte yanıtlarla test edildi; Mac yardımcısı sözdizimi kontrolünden geçti. Gerçek Mac girişi ve uçtan uca cihaz indirme kabul testleri henüz tamamlanmadı.", "small")
p('Resmi kaynaklar: <link href="https://developers.google.com/identity/protocols/oauth2#expiration" color="#087F79">Google OAuth oturum süreleri</link> · <link href="https://rclone.org/drive/" color="#087F79">rclone Google Drive</link> · <link href="https://onionui.github.io/docs/" color="#087F79">OnionOS belgeleri</link>', "small")

def decorate(c, doc):
    w,h = A4
    c.setFillColor(TEAL); c.rect(0,h-9,w,9,fill=1,stroke=0)
    c.setFont("SegoeBold",9); c.setFillColor(NAVY)
    c.drawString(60,h-39,"MIYOO FRIENDS LIBRARY")
    c.setFont("Segoe",8); c.setFillColor(GRAY)
    c.drawRightString(w-60,h-39,"v0.1.3 | Windows + macOS")
    c.setStrokeColor(colors.HexColor("#D7E0E6")); c.line(60,48,w-60,48)
    c.drawString(60,32,"Arkadaş kurulumu | 14 Kasım 2026")
    c.drawRightString(w-60,32,f"{doc.page} / 6")

doc = SimpleDocTemplate(str(OUT), pagesize=A4, rightMargin=60, leftMargin=60,
                        topMargin=65, bottomMargin=62, title="Miyoo Friends Library - Kurulum Rehberi",
                        author="Miyoo Friends Library", pageCompression=1)
doc.build(story, onFirstPage=decorate, onLaterPages=decorate)
print(f"Guide ready: {OUT.name}")
