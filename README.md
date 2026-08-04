# PS3 Homebrew Oyunlari

PlayStation 3 icin PSL1GHT ile yazilmis homebrew oyunlar.
Gelistirme ortami tamamen Docker icindedir — host makineye SDK, derleyici veya
kutuphane kurulmaz. Apple Silicon uzerinde native (arm64) calisir.

## Projeler

### [Ping Pong](Ping-Pong/) — calisir durumda

Menulu, tam oynanabilir bir ping pong oyunu.

- Turkce menu (Baslat / Hakkinda / Cikis), bota karsi ve 2 kisilik mod
- 11 sayilik mac, duraklatma, kazanan ekrani
- Orta zorlukta bot yapay zekasi (topu takip eder ama yenilebilir)
- Analog cubukla orantili raket hizi, yon tuslariyla tam guc
- Koda gomulu 8x8 bitmap font; Turkce glifler (g G i I s S) elle cizildi
- Arkaplan gorseli destegi (derleme sirasinda ham piksele cevrilip gomulur)
- 29 birim testi

### [Basic Water](Basic-Water/) — yarim kaldi

3D deniz + gokyuzu sahnesi, serbest ucan kamera. Bir flight simulator'un ilk adimi
olarak tasarlandi.

Calisan kisimlar: Docker ortami (Cg shader derleme dahil), matris ve kamera
matematigi (30 birim testi), shader derleme zinciri, `.self`/`.pkg` uretimi.

**Bilinen sorun:** PS3'te sahne ekrana cizilmiyor. Program aciliyor, 60 FPS'te
donuyor ve gokyuzu rengi geliyor, ancak deniz geometrisi gorunmuyor. Vertex
verisi shader'a ulasmiyor gibi gorunuyor. PSL1GHT'in kendi 3D ornegi ayni
ortamda sorunsuz calistigi icin hata bu projenin cizim kodunda.

## Derleme

Her iki projede de:

```sh
./build.sh          # .self + .pkg uretir
./build.sh test     # birim testleri
./build.sh clean    # ciktilari temizler
```

Gereken tek sey Docker. Ilk calistirmada toolchain imaji indirilir
(`zeldin/ps3dev-docker`, ~400 MB, tek seferlik).

Ciktilar:

| Dosya | Kullanim |
|---|---|
| `*.fake.self` | RPCS3 emulatoru — File > Boot SELF/ELF |
| `*.pkg` | Gercek PS3 (CFW) — USB'den kurulum |

## Gorsel dogrulama

Her iki proje de cizim katmanini host tarafinda calistirip PNG uretebiliyor;
boylece ekran yerlesimi ve kamera matematigi emulatore gitmeden dogrulanabiliyor.
Detaylar proje README'lerinde.

## Notlar

- Font tablolari: [dhepper/font8x8](https://github.com/dhepper/font8x8) (public domain)
- Toolchain: [PSL1GHT](https://github.com/ps3dev/PSL1GHT) + [ps3toolchain](https://github.com/ps3dev/ps3toolchain)
