# PS3 Ping Pong

PlayStation 3 için basit bir ping pong homebrew oyunu (PSL1GHT).
Tüm derleme zinciri Docker içindedir — host makineye SDK/derleyici kurulmaz.

## Derleme

```sh
./build.sh          # .self + .pkg üretir
./build.sh clean    # çıktıları temizler
./build.sh test     # oyun mantığı birim testleri
```

İlk çalıştırmada hazır toolchain imajı indirilir (`zeldin/ps3dev-docker`,
~400 MB — ppu-gcc 7.2.0 + PSL1GHT). Apple Silicon'da arm64 varyantı kullanılır,
yani Rosetta'sız native hızda çalışır. Sonraki derlemeler saniyeler sürer.

### Çıktılar

| Dosya | Nerede kullanılır |
|---|---|
| `pingpong.fake.self` | RPCS3 — emülatöre sürükle (File → Boot SELF) |
| `pingpong.self` | CEX imzalı self |
| `pingpong.pkg` | Gerçek PS3 (CFW) — USB'den kur, XMB'den çalıştır |

## Ekran önizleme (PS3 gerekmeden)

Çizim katmanı host'ta çalıştırılıp PNG'ye dökülebilir:

```sh
docker run --rm -v "$PWD":/project -w /project alpine:3.20 sh -c '
  apk add --no-cache build-base imagemagick >/dev/null &&
  gcc -O1 -o build-test/preview tests/preview.c source/font.c source/menu.c \
      source/draw.c source/game.c -lm && ./build-test/preview &&
  cd build-test && for f in *.ppm; do magick "$f" "${f%.ppm}.png"; done'
```

Görüntüler `build-test/` altına çıkar.

## Kontroller

**Menü** — Yön tuşları / sol analog: seçim, **X**: onayla, **O**: geri

**Oyun** — Oyuncu 1: 1. kol, Oyuncu 2: 2. kol (yukarı/aşağı)
**START**: duraklat, duraklatılmışken **O**: menüye dön

## Oyun

İlk 11 sayıyı alan kazanır. Bot modunda sağ raketi orta zorlukta bir yapay
zekâ kullanır: topu takip eder ama hızı sınırlıdır ve küçük bir reaksiyon
gecikmesi vardır.

## Kod yapısı

| Dosya | Sorumluluk |
|---|---|
| `source/game.c` | Fizik, skor, bot AI — donanımdan bağımsız, birim testli |
| `source/video.c` | libgcm çift tamponlu framebuffer, dikdörtgen doldurma |
| `source/font.c` | 8x8 bitmap font, UTF-8, Türkçe glifler |
| `source/input.c` | Pad okuma, kenar algılama |
| `source/menu.c` | Menü durumu ve çizimi |
| `source/draw.c` | Oyun sahası ve sonuç ekranı çizimi |
| `source/main.c` | Durum makinesi ve ana döngü |

Font tabloları `source/font_data.h` içinde (dhepper/font8x8, public domain);
Türkçeye özgü 6 glif (ğ Ğ ı İ ş Ş) `font.c` içinde elle çizilmiştir.
