# PS3 Ping Pong — Tasarım Dokümanı

Tarih: 2026-08-03
Yazar: Utku Halis

## Amaç

PS3 için basit bir ping pong homebrew oyunu. Menüsü ve oyun ekranı olacak.
Geliştirme tamamen Docker içinde yapılacak, host makineye (Apple Silicon Mac)
hiçbir SDK/derleyici kurulmayacak. Test önce RPCS3 emülatöründe (kullanıcı
kendi makinesine kuruyor), sonra CFW'li gerçek PS3 cihazında yapılacak.

## Kapsam Dışı (YAGNI)

- Ses/müzik
- Online oyun
- Kayıt/skor tablosu (kalıcı veri)
- 3D grafik, shader, texture
- Çoklu dil (sadece Türkçe)

## Geliştirme Ortamı

### Docker

- Temel imaj: `zeldin/ps3dev-docker` — önceden derlenmiş **ps3toolchain**
  (ppu-gcc 7.2.0) + **PSL1GHT** + paketleme araçları (fself, make_self_npdrm,
  pkg, sfo). **arm64 varyantı** mevcut, Apple Silicon'da native çalışır.
- Kaynak kodu bind-mount edilir; `build/` çıktıları host'ta görünür.

**Karar notu:** Önce toolchain'i kaynaktan derlemek denendi
(`ps3toolchain/toolchain.sh`). Docker VM'in disk alanı (15.6 GB, çoğu dolu)
GCC derlemesine yetmediği için libstdc++ aşamasında başarısız oldu. Hazır imaj
hem bu sorunu ortadan kaldırdı hem de amd64+Rosetta yerine native arm64 sağladı.

### Build akışı

Tek komutluk sarmalayıcı script (`./build.sh`) container içinde `make`
çalıştırır ve şu çıktıları üretir:

| Çıktı | Kullanım |
|---|---|
| `EBOOT.BIN` | PS3 çalıştırılabiliri |
| `pingpong.self` | RPCS3'te "Boot SELF" ile doğrudan test |
| `pingpong.pkg` | Gerçek PS3'e USB'den kurulum (fake NPDRM) |

### Test

1. **RPCS3 (Mac, kullanıcı kurulumu):** `.self` dosyası emülatöre sürüklenir.
2. **Gerçek PS3 (CFW):** `.pkg` USB ile kurulur, XMB'den çalıştırılır.

Docker içinde emülatör çalıştırılmaz — Apple Silicon'da GPU/Vulkan erişimi
olmadığı ve üstüne x86 emülasyonu bindiği için kullanılamaz hızda olur.

## Render Yaklaşımı

**Saf framebuffer + libgcm.** Çift tamponlu (double buffered) framebuffer
açılır, tüm çizim CPU tarafında dikdörtgen doldurma ile yapılır. Shader,
3D pipeline veya ek kütüphane (tiny3d vb.) yok. Ping pong için yeterli ve
hem RPCS3'te hem gerçek donanımda çalışma olasılığı en yüksek seçenek.

Çözünürlük: PS3'ün açtığı gerçek video moduna uyulur (`videoGetState` /
`videoGetResolution`). Oyun mantığı sanal **1280×720** koordinat sisteminde
yazılır, çizim sırasında gerçek framebuffer boyutuna oranlanır.

## Kod Mimarisi

Dil: C. Her dosya tek sorumluluk taşır.

```
Ping-Pong/
├── Dockerfile
├── build.sh              # host tarafı tek komut sarmalayıcı
├── Makefile              # PSL1GHT ppu_rules
├── source/
│   ├── main.c            # uygulama durum makinesi, ana döngü
│   ├── video.c / video.h # gcm init, framebuffer, fill_rect, flip
│   ├── font.c  / font.h  # 8x8 bitmap font + Türkçe glifler, draw_text
│   ├── input.c / input.h # pad okuma, kenar algılama (edge detection)
│   ├── menu.c  / menu.h  # menü durumu ve çizimi
│   ├── draw.c  / draw.h  # oyun sahası ve sonuç ekranı çizimi
│   └── game.c  / game.h  # fizik, skor, bot AI (saf mantık)
├── data/                 # ICON0.PNG vb. paket varlıkları
├── docs/
└── build/                # gitignore
```

### Modül sözleşmeleri

**video** — `video_init()`, `video_clear(color)`, `video_fill_rect(x,y,w,h,color)`,
`video_flip()`. Koordinatlar sanal 1280×720; modül içeride ölçekler.
Bağımlılık: PSL1GHT rsx/gcm.

**font** — `font_draw_text(x, y, utf8_str, color)`, `font_text_width(str)`.
İçinde 8×8 bitmap tablosu: ASCII 32–126 + Türkçe glifler
(ş Ş ı İ ç Ç ğ Ğ ü Ü ö Ö). UTF-8 çok baytlı dizileri çözer, bilinmeyen
kod noktası için '?' çizer. Bağımlılık: sadece `video`.

**input** — `input_init()`, `input_update()`, `input_held(pad, btn)`,
`input_pressed(pad, btn)` (sadece o karede basıldıysa true). 2 kola kadar
destek. Bağımlılık: PSL1GHT io/pad.

**menu** — `menu_update(void) -> MenuAction`, `menu_draw(void)`.
Kendi iç durumunu (hangi ekran, hangi satır seçili) tutar; dışarıya sadece
"oyuna gir (bot/2 kişi)" veya "çık" aksiyonunu bildirir.

**game** — `game_init(Game*, GameMode, seed)`, `game_update(Game*, p1dir, p2dir)
-> GameStatus`. Fizik, skor ve bot AI burada. Menüden ve çizimden habersizdir;
donanıma hiç dokunmaz, bu sayede host tarafında birim testi yazılabilir.

**draw** — `draw_game(const Game*)`, `draw_result(const Game*, p1_won)`.
Oyun durumunu ekrana çizer. `game` modülünü saf tutmak için ayrıldı.
Bağımlılık: `video`, `font`.

**main** — durum makinesi: `MENU → GAME → RESULT → MENU`, çıkışta
`sysProcessExit`.

## Menü Tasarımı

```
        PING PONG

      > Başlat
        Hakkında
        Çıkış
```

- **Başlat** → alt menü: `Bota Karşı` / `2 Kişi` / `Geri`
- **Hakkında** → "Utku Halis PS3 deneme oyunu" metni, O ile geri
- **Çıkış** → XMB'ye döner

Kontroller: D-pad yukarı/aşağı ile seçim, **X** onay, **O** geri.
Seçili satır farklı renkle ve `>` işaretiyle gösterilir.

## Oyun Tasarımı

### Alan ve nesneler (sanal 1280×720)

- Saha: tüm ekran, ortada kesikli orta çizgi
- Raketler: 20×120 px, kenarlardan 60 px içeride
- Top: 20×20 px kare

### Kontroller

| | Yukarı/Aşağı | Diğer |
|---|---|---|
| Oyuncu 1 (kol 1) | D-pad veya sol analog | — |
| Oyuncu 2 (kol 2) | D-pad veya sol analog | bot modunda AI |

START = duraklat/devam, O = menüye dön.

### Fizik

- Top hız vektörü (vx, vy); başlangıç hızı sabit, yön rastgele
- Üst/alt duvarda vy işaret değiştirir
- Raket çarpışması AABB; **çarpma noktasının rakete göre konumu** çıkış
  açısını belirler (merkez = düz, uç = keskin açı)
- Her raket vuruşunda hız %2 artar, üst sınırla kısıtlanır
- Top sol/sağ kenardan çıkarsa karşı tarafa 1 sayı, top merkeze döner,
  sayıyı yiyen tarafa doğru servis

### Bot AI (orta zorluk)

Botun raketi topun y konumunu takip eder ama:
- maksimum hareket hızı oyuncununkinden biraz düşük
- küçük bir reaksiyon gecikmesi (top yön değiştirdikten sonra tepki verir)
- hedefe küçük bir sapma eklenir

Sonuç: rekabetçi ama yenilebilir.

### Maç sonu

İlk **11** sayıya ulaşan kazanır → "Kazanan: Oyuncu 1" (veya "Bot")
ekranı → **X** ile menüye dönüş.

## Hata Yönetimi

- `video_init` veya pad init başarısız olursa ekrana çizim yapılamaz; hata
  kodu `printf` ile log'a yazılıp (RPCS3 log / `ps3load` konsolu) uygulama
  hemen temiz şekilde sonlanır — PS3'te donmuş ekran bırakılmaz.
- 2. kol bağlı değilken "2 Kişi" seçilirse: oyun yine başlar, 2. raket
  sabit kalır (girdi gelmez). Ekranda uyarı gösterilmez — basit tutulur.
- Ana döngü her karede pad durumunu yeniden okur; kol çıkarılıp takılırsa
  kendiliğinden düzelir.

## Test Stratejisi

PS3 donanımına bağlı kod (gcm, pad) birim testi ile kapsanamaz. Bu yüzden:

1. **Host tarafı birim testleri:** `game.c` içindeki fizik ve skor mantığı
   donanımdan bağımsız yazılır (saf fonksiyonlar, `float`/`int` üzerinde
   çalışır). Bu dosya Docker içinde normal `gcc` ile derlenip küçük bir
   test koşucusuyla doğrulanır: sekme, skor, servis yönü, bot sınırları.
2. **Görsel önizleme (host):** `video`/`input` yerine bellek tamponuna yazan
   stub'lar konularak `menu`/`draw` katmanı host'ta çalıştırılır ve ekranlar
   PPM/PNG olarak dökülür. Türkçe glifler ve yerleşim PS3 olmadan doğrulanır.
3. **Derleme doğrulaması:** `EBOOT.BIN`/`.self`/`.pkg` üretiliyor mu.
4. **RPCS3 duman testi:** menüde gezinme, oyuna girme, sayı alma,
   maç bitirme, menüye dönüş, çıkış.
5. **Gerçek PS3 testi (Salı):** aynı duman testi + iki kol ile 2 kişilik mod.

## Riskler

| Risk | Azaltma |
|---|---|
| ~~Toolchain kaynaktan derlenemezse~~ | **Gerçekleşti** — disk yetersizliği. Hazır arm64 imaja geçildi, sorun ortadan kalktı |
| Hazır imaj Docker Hub'dan kalkarsa | Imaj yerel olarak mevcut; `docker save` ile yedeklenebilir |
| RPCS3 `.self` açmazsa | `.pkg` kurulup emülatörde oyun listesinden çalıştırılır |
| Türkçe glifler bozuk çıkarsa | Font tablosu elle çizilir ve RPCS3'te görsel kontrol edilir |
