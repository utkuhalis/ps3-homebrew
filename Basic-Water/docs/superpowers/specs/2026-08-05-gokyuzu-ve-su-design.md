# Gökyüzü ve Su — Tasarım Dokümanı

Tarih: 2026-08-05
Yazar: Utku Halis

## Amaç

Basic Water sahnesini gerçek bir deniz manzarasına dönüştürmek:

- **Gökyüzü:** mavi gradyan, güneş, rüzgarla süzülen beyaz bulutlar
- **Su:** dalgalanan yüzey, güneş parlaması, gökyüzü yansıması

Hava sabittir: gün döngüsü, gün batımı, hava değişimi bu adımın kapsamında değil.

## Ön Koşul: RSX Çizim Hatası

Sahne şu anda ekrana çizilmiyor. Program açılıyor, 60 FPS'te dönüyor, gökyüzü
rengi (clear color) geliyor, ancak hiçbir geometri görünmüyor. Ölçümler vertex
konum verisinin shader'a ulaşmadığını gösteriyor.

Önemli veri noktası: PSL1GHT'ın kendi `rsxtest` örneği **aynı ortamda, aynı
toolchain ve aynı `cgcomp` kurulumuyla derlenip kusursuz çalışıyor.** Yani
toolchain, shader derleyici, emülatör ve RSX emülasyonu sağlam; hata bu
projenin çizim kodunda.

### Çözüm yöntemi: çalışan koddan başlayıp ikiye bölerek daraltma

Önceki denemelerde hata tahmin edilmeye çalışıldı (matris düzeni, transpoze
yönü, kırpma ayarları) ve her seferinde başka bir olasılık ıskalandı. Bu kez
yöntem değişiyor:

1. Çalışan `rsxtest` örneği taban alınır.
2. Her adımda **tek bir şey** bizim koda çevrilir:
   mesh → shader → çizim çağrıları → başlatma kodu.
3. Her adımdan sonra ekran kontrol edilir.
4. Ekranın bozulduğu ilk adım, hatanın tam yeridir.

Bu, Ping Pong'daki pad hatasında işe yarayan yaklaşımın aynısıdır: tahmin
etmek yerine ölçmek.

## Aşama 1: Gökyüzü

### Geometri

Ekranı kaplayan tek bir dörtgen. Derinlik yazımı kapalı, sahnede en önce
çizilir; böylece arkasındaki her şey üzerine gelir.

### Bakış yönü

Vertex shader dörtgenin köşeleri için dünya uzayındaki bakış yönünü hesaplar,
fragment shader'a interpolasyonla geçer. Her piksel kendi ışın yönünü bilir.

### Renk hesabı (texture yok, tamamı prosedürel)

| Bileşen | Yöntem |
|---|---|
| Gradyan | Işın yönünün y bileşenine göre: ufukta açık, tepede derin mavi |
| Güneş | Işın yönü ile güneş yönü arasındaki nokta çarpımı; dar açıda parlak disk, geniş açıda sönen hale |
| Bulutlar | Işının bulut düzlemiyle (y = 900 birim) kesişim noktasında katmanlı gürültü (fBm). Koordinat zamanla kaydırılır → rüzgarla süzülme |

Bulut yoğunluğu eşiklenip yumuşatılır, böylece kümeler oluşur ve gökyüzü
tamamen kapanmaz.

## Aşama 2: Su

### Geometri

Mevcut ızgara düzlemi (kamera merkezli, ızgara aralığının katlarına yuvarlanan
kaydırma). Dalga detayının görünmesi için bölme sayısı 32'den 64'e çıkarılır.

### Dalga (vertex shader)

Farklı yön, frekans ve genlikte üç-dört sinüs dalgası toplanır. Yükseklik
ofsetinin yanında yüzey normali de analitik türevden hesaplanır — normal
doğru olmazsa parlama ve yansıma yanlış görünür.

### Renk (fragment shader)

| Bileşen | Yöntem |
|---|---|
| Yansıma | Bakış ışını normal etrafında yansıtılır, **gökyüzü fonksiyonu** ile renklendirilir |
| Fresnel | Dik bakışta suyun derin rengi, sığ açıda yansıma baskın (Schlick yaklaşımı) |
| Parlama | Güneş yönüne göre Blinn-Phong; dalgalar üzerinde kıpırdayan ışık yolu |
| Derin renk | Koyu mavi-yeşil taban rengi |
| Mesafe | Uzakta gökyüzü rengine karışır, ufuk doğal görünür |

**Tutarlılık notu:** Gökyüzü rengi hem gökyüzü shader'ında hem su
yansımasında aynı formülle hesaplanır. Cg'de `#include` olmadığı için fonksiyon
iki shader dosyasında tekrarlanır; biri değişirse diğeri de değişmelidir.

## Zaman

Ana döngü kare sayacından türeyen bir `time` değeri (saniye) her iki shader'a
uniform olarak geçer. Dalga hareketi ve bulut süzülmesi buna bağlıdır.

## Modül Yapısı

```
shaders/
├── sky.vcg / sky.fcg      # gokyuzu
└── water.vcg / water.fcg  # su (gokyuzu fonksiyonunu tekrarlar)
source/
├── scene.c                # iki gecis: once gokyuzu, sonra su
├── waves.c / waves.h      # dalga matematigi (saf C, testli, onizlemede kullanilir)
└── skycolor.c / .h        # gokyuzu renk fonksiyonu (saf C, onizlemede kullanilir)
```

`waves` ve `skycolor` donanımdan bağımsızdır. Shader'lardaki formüllerin C
karşılığıdır ve önizleme aracı tarafından kullanılır.

## Test Stratejisi

1. **Birim testleri (host):** dalga yüksekliğinin sınırlı kalması, dalga
   fonksiyonunun sürekliliği, normalin birim uzunlukta olması, gökyüzü renginin
   geçerli aralıkta kalması.
2. **Görsel önizleme (host):** `skycolor` ve `waves` kullanılarak sahne CPU'da
   ışın atmayla render edilip PNG'ye dökülür. "Güneş doğru yerde mi, bulutlar
   nasıl duruyor, dalga çok mu sert" soruları PS3'e gitmeden yanıtlanır.
   Bu, RSX'e güvenmeden ilerlemenin yolu.
3. **Derleme doğrulaması:** shader'lar `cgcomp` ile derleniyor mu.
4. **RPCS3 duman testi:** sahne görünüyor mu, kare hızı ne.
5. **Gerçek PS3 testi:** aynı kontroller + performans.

## Riskler

| Risk | Azaltma |
|---|---|
| RSX hatası çözülemezse | Sahne CPU tarafında ışın atmayla çizilir (Ping Pong'da kanıtlanmış framebuffer yolu); çözünürlük ve kare hızı düşer |
| Fragment shader komut sınırını aşarsa | Önce bulut gürültüsü sadeleştirilir (oktav sayısı azaltılır), sonra yansıma basitleştirilir |
| Kare hızı düşükse | Su ızgarası seyreltilir, bulutlar daha ucuz hesaplanır |
| Gökyüzü formülü iki dosyada tekrarlandığı için ayrışabilir | Her iki dosyanın başına uyarı notu; değişiklik ikisinde birden yapılır |
