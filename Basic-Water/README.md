# Basic Water

PS3 üzerinde 3D deniz + gökyüzü sahnesi, serbest uçan kamera ve su yüzeyi
çarpışması. Bir flight simulator projesinin ilk adımı.

Tüm derleme Docker içindedir — host makineye SDK/derleyici kurulmaz.

## Derleme

```sh
./build.sh          # .self + .pkg üretir
./build.sh test     # matematik ve kamera birim testleri
./build.sh clean    # çıktıları temizler
```

### Neden iki imaj kullanılıyor

| Aşama | İmaj | Sebep |
|---|---|---|
| Cg shader derleme | `ps3-cgcomp` (amd64) | `cgcomp`, NVIDIA Cg Toolkit'e ihtiyaç duyar ve Cg Toolkit yalnızca x86 için vardır |
| PS3 derlemesi | `zeldin/ps3dev-docker` (arm64) | Apple Silicon'da native hızda çalışır |

Shader'ların çıktısı (`.vpo`/`.fpo`) RSX mikrokodudur — mimariden bağımsızdır,
bu yüzden amd64'te üretilip arm64 derlemesine sorunsuz girer. `build.sh` bu iki
adımı zincirler, sen tek komut çalıştırırsın.

### Çıktılar

| Dosya | Nerede kullanılır |
|---|---|
| `basicwater.fake.self` | RPCS3 — File → Boot SELF/ELF |
| `basicwater.pkg` | Gerçek PS3 (CFW) — USB'den kur |

## Kontroller

| İşlem | Kol |
|---|---|
| İleri / geri | Sol analog ↑↓ (veya D-pad ↑↓) |
| Yanlara kayma | Sol analog ←→ |
| Bakış (yaw / pitch) | Sağ analog |
| Yüksel / alçal | R1 / L1 |
| Çıkış | START |

Kamera su yüzeyinin 2 birim üstünden aşağı inemez.

## Görsel önizleme (PS3 gerekmeden)

Kamera ve projeksiyon matematiği host'ta tel-kafes olarak çizilip PNG'ye
dökülebilir — "kamera doğru yere bakıyor mu" sorusu emülatöre gitmeden görülür:

```sh
docker run --rm -v "$PWD":/project -w /project alpine:3.20 sh -c '
  apk add --no-cache build-base imagemagick >/dev/null &&
  gcc -O1 -o build-test/preview tests/preview.c source/mat4.c source/camera.c -lm &&
  ./build-test/preview && cd build-test &&
  for f in *.ppm; do magick "$f" "${f%.ppm}.png"; done'
```

## Kod yapısı

| Dosya | Sorumluluk |
|---|---|
| `source/mat4.c` | 4×4 matris matematiği — donanımsız, birim testli |
| `source/camera.c` | Kamera durumu, hareket, su çarpışması — donanımsız, birim testli |
| `source/rsx3d.c` | RSX kurulumu, derinlik tamponu, çizim ortamı, flip |
| `source/scene.c` | Deniz geometrisi, shader yükleme, çizim |
| `source/input.c` | Pad okuma, analog eksenler, ölü bölge |
| `source/main.c` | Ana döngü |
| `shaders/water.vcg` | Vertex programı — MVP dönüşümü |
| `shaders/water.fcg` | Fragment programı — prosedürel ızgara + mesafe fog'u |

Deniz `y=0` düzleminde 8000×8000 birimlik bir ızgara. Kamera hareket ettikçe
düzlem ızgara aralığının katlarına yuvarlanarak kaydırılır; böylece sonsuz
deniz hissi verirken ızgara dünyada sabit görünür.

## Sonraki adımlar (bu projede yok)

Uçak modeli ve uçuş fiziği, dalgalanan su, gökyüzü kubbesi ve bulutlar,
arazi/ada, HUD.
