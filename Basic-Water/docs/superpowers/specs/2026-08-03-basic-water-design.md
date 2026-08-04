# Basic Water — Tasarım Dokümanı

Tarih: 2026-08-03
Yazar: Utku Halis

## Amaç

PS3 üzerinde 3D bir sahne: üstte gökyüzü, altta sonsuza uzanan deniz.
Serbest kamera ile uçarak gezilir; kamera su yüzeyinin altına inemez.

Bu proje bir **flight simulator'ın ilk adımıdır**. Bu yüzden öncelik "çalışan
bir görüntü" değil, **üstüne inşa edilebilir bir 3D temel** kurmaktır.

## Kapsam Dışı (YAGNI)

- Uçak modeli, uçuş fiziği, kumanda yüzeyleri
- Arazi, ada, bina, herhangi bir kara parçası
- Ses, HUD, menü
- Texture, gölge, yansıma, dalga simülasyonu
- Gökyüzü kubbesi, bulut, gün döngüsü

Bunların hepsi sonraki adımların konusu. Bu adım yalnızca: RSX 3D pipeline'ı
ayağa kaldır, bir düzlem çiz, kamerayı uçur, su seviyesinde durdur.

## Geliştirme Ortamı

Ping Pong projesiyle **birebir aynı**: `zeldin/ps3dev-docker` (arm64, native),
`build.sh` sarmalayıcı, aynı `.self` / `.pkg` çıktıları.

Ek disk gereksinimi yok — imaj zaten kurulu ve 3D için gereken her şeyi içerir:
`librsx.a`, `libgcm_sys.a`, `cgcomp` (Cg shader derleyicisi).

Yeni olan tek şey: PSL1GHT'ın shader derleme kuralları kullanılacak
(`ppu_rules` içinde `%.vpo: %.vcg` ve `%.fpo: %.fcg` hazır tanımlı).
Derlenen shader'lar `.o` olarak binary gömülür ve `#include "*_vpo.h"` ile
koddan erişilir.

## Render Yaklaşımı

**Ham RSX + kendi Cg shader'larımız.** tiny3d gibi bir ara katman kullanılmaz;
GPU'ya doğrudan üçgen gönderilir, kendi vertex/fragment programlarımız çalışır.
Gerekçe: flight sim'in ileride ihtiyaç duyacağı her şey (sis, gökyüzü, arazi,
shader'lı su) bu temelin üstüne kurulacak. Ara katman ilerde sökülmek zorunda
kalınacak bir bağımlılık olurdu.

Referans alınan PSL1GHT örneği: `samples/graphics/rsxtest`.

### Kritik teknik detaylar

- Matrisler RSX'e **transpoze** gönderilir (`transpose(proj)`, `transpose(view)`).
- Derinlik tamponu `setRenderTarget` ile her karede bağlanır.
- Fragment programının ucode'u RSX belleğine kopyalanır (`rsxMemalign` + offset).
- Vertex öznitelikleri `rsxBindVertexArrayAttrib` ile bağlanır.

## Kod Mimarisi

```
Basic-Water/
├── Dockerfile            # Ping Pong ile aynı
├── build.sh
├── Makefile              # + SHADERS kuralı
├── shaders/
│   ├── water.vcg         # vertex programı
│   └── water.fcg         # fragment programı
├── source/
│   ├── main.c            # ana döngü
│   ├── mat4.c / .h       # 4x4 matris matematiği (saf, testli)
│   ├── camera.c / .h     # kamera durumu + su çarpışması (saf, testli)
│   ├── rsx3d.c / .h      # RSX init, draw env, shader yükleme, çizim
│   ├── scene.c / .h      # deniz düzlemi geometrisi ve çizimi
│   └── input.c / .h      # Ping Pong'dan uyarlandı, analog eksenler dahil
├── tests/
└── docs/
```

### Modül sözleşmeleri

**mat4** — `mat4_perspective(fovy, aspect, near, far)`,
`mat4_look_at(eye, target, up)`, `mat4_mul(a, b)`, `mat4_transpose(m)`.
Saf C, donanım bağımsız, birim testli. Matrisler `float m[16]` (row-major).

**camera** — `camera_init(Camera*)`,
`camera_update(Camera*, forward, strafe, yaw_in, pitch_in, up_down)`.
Kamera durumu: `pos[3]`, `yaw`, `pitch`. Girdi -1..+1 aralığında normalize
gelir; kamera modülü donanımı ve tuş eşlemesini bilmez.
Kısıtlar: `pitch` ±85°'de kırpılır, `pos[1] >= WATER_LEVEL + MIN_ALTITUDE`.
Saf C, birim testli.

**rsx3d** — `rsx3d_init()`, `rsx3d_begin_frame(clear_color)`,
`rsx3d_end_frame()`, `rsx3d_set_matrices(proj, view)`,
`rsx3d_draw_mesh(vertices, count, indices, index_count)`.
RSX/gcm ayrıntılarını tamamen kapsar; üst katman gcm bilmez.

**scene** — `scene_init()` deniz düzlemini oluşturur, `scene_draw(cam)` çizer.

**input** — Ping Pong'daki modülün analog eksenleri de dışarı veren hali:
`input_axis_left_x/y()`, `input_axis_right_x/y()` → -1.0..+1.0 float.
Ölü bölge (dead zone) burada uygulanır. RPCS3 klavye modunda analog verisi
gelmediğinde (tüm eksenler 0) analog yok sayılır — Ping Pong'da bulunan hata.

**main** — init → döngü (girdi → kamera → çizim → flip) → temiz çıkış.

## Sahne

- **Gökyüzü:** ayrı geometri yok. Ekran gökyüzü mavisiyle temizlenir; deniz
  ufka kadar uzandığı için ufuk çizgisi kendiliğinden oluşur.
- **Deniz:** `y = 0` düzleminde büyük bir dörtgen (4000×4000 birim).
  Kamera merkezli konumlandırılır, yani hiçbir zaman kenarına ulaşılamaz.
- **Izgara:** fragment shader'da prosedürel. Dünya koordinatının belirli bir
  aralıkta tekrarına göre çizgi çizilir; texture yok. Uçarken çizgilerin
  altından kayması hız hissi verir.
- **Mesafe solması (fog):** kameradan uzaklaştıkça su rengi gökyüzü rengine
  karışır. Hem ufku doğal gösterir hem uzaktaki ızgaranın titremesini gizler.

## Kamera ve Çarpışma

| Girdi | Etki |
|---|---|
| Sol analog Y | İleri / geri (bakış yönünde) |
| Sol analog X | Yanlara kayma |
| Sağ analog X | Yaw (sağa/sola bakış) |
| Sağ analog Y | Pitch (yukarı/aşağı bakış) |
| L1 / R1 | Alçal / yüksel |
| START | Çıkış |

- `pitch` ±85° ile sınırlı (tepetaklak olmayı önler)
- **Çarpışma:** `pos.y` asla `WATER_LEVEL + 2.0` altına inemez. Suya doğru
  inince yüzeyin hemen üstünde durur, altına geçilemez.
- Başlangıç: `(0, 20, 0)`, ufka bakar

## Hata Yönetimi

- RSX init veya shader yükleme başarısız olursa: hata `printf` ile log'a
  yazılır, uygulama temiz şekilde sonlanır (PS3'te donmuş ekran bırakmamak için).
- Kol bağlı değilse kamera sabit kalır; uygulama çalışmaya devam eder.

## Test Stratejisi

1. **Birim testleri (host):** `mat4` ve `camera` donanım bağımsız olduğu için
   Docker içinde normal `gcc` ile derlenip test edilir:
   - perspektif matrisinin bilinen değerleri
   - `look_at` sonucunun ortonormalliği
   - matris çarpımının birim matrisle değişmezliği
   - pitch'in ±85°'de kırpılması
   - kameranın su seviyesinin altına **hiçbir girdi kombinasyonuyla** inememesi
   - ileri hareketin bakış yönüyle tutarlılığı
2. **Görsel doğrulama (host):** `mat4` + `camera` kullanılarak deniz ızgarası
   CPU'da tel-kafes olarak projekte edilip PNG'ye dökülür. Kamera matematiği
   RPCS3'e gitmeden görsel olarak doğrulanır (Ping Pong'daki ekran önizlemesinin
   3D karşılığı).
3. **Derleme doğrulaması:** shader'lar derleniyor mu, `.self`/`.pkg` çıkıyor mu.
4. **RPCS3 duman testi:** ufuk görünüyor mu, kamera dönüyor mu, su altına
   inilemiyor mu.
5. **Gerçek PS3 testi (Salı):** aynı kontroller + performans (kare hızı).

## Riskler

| Risk | Azaltma |
|---|---|
| Cg shader derleme zinciri ilk kez kullanılıyor | PSL1GHT'ın hazır `%.vcg → %.vpo` kuralları var; rsxtest örneği referans alındı. Önce en basit shader ile "ekranda tek üçgen" doğrulanacak |
| RPCS3'ün RSX emülasyonu gerçek donanımdan farklı davranabilir | Salı günü gerçek PS3 testi planlı; shader'lar mümkün olduğunca basit tutuluyor |
| Matris transpoze / el kuralı hataları (ekranda hiçbir şey görünmemesi) | Kamera matematiği host'ta tel-kafes önizlemeyle doğrulanıyor; hata GPU'ya gitmeden yakalanıyor |
| Uzak ızgara çizgilerinde titreme (aliasing) | Mesafe fog'u ile gizlenir |
