# Uçak, Uçuş Fiziği, Kamera Modları ve Ses — Tasarım Dokümanı

Tarih: 2026-08-06
Yazar: Utku Halis

## Amaç

Serbest süzülen kamerayı gerçek bir uçağa dönüştürmek: görünen bir uçak
gövdesi, hareket eden kumanda yüzeyleri, basit ama inandırıcı uçuş fiziği,
kokpit/dış kamera görüşleri ve uçuşa tepki veren ses.

## Kapsam Dışı (YAGNI)

- Gerçekçi aerodinamik (girdap, yer etkisi, buz, motor arızası)
- İniş takımı, kapılar, iç kokpit paneli
- Çoklu uçak tipi
- Kaza/hasar modeli

## Neden Hazır Model Değil

Kullanıcı önce hazır bir 3D model istedi. Değerlendirme sonucu kodla üretmeye
karar verildi:

- PSL1GHT'ta model yükleyici yok; OBJ/FBX okuyup PS3'e aktaran zinciri
  sıfırdan yazmak gerekirdi.
- Asıl istek **flap ve spoiler'ın çalışması**. Hazır model tek parça gelir;
  hareketli yüzeyleri ayırmak için modeli yeniden bölmek gerekir. Yani hazır
  model bu iş için avantaj değil, ek yük.
- Kodla üretilen model düşük poli olur ama her yüzey en baştan ayrı parçadır.

## Uçak Geometrisi

`aircraft.c` uçağı parçalardan kurar. Her parça yerel koordinatta tanımlanır;
hareketli yüzeyler kendi menteşe ekseni etrafında döndürülür.

| Parça | Hareket |
|---|---|
| Gövde, kanatlar, dikey/yatay stabilizatör, iki motor | sabit |
| Flap (sol/sağ, kanat arkası iç) | menteşeden aşağı, 0–40° |
| Aileron (sol/sağ, kanat arkası dış) | ters yönlerde, ±20° |
| Spoiler (sol/sağ, kanat üstü) | yukarı, 0–55° |
| Elevator (kuyruk arkası) | ±25° |
| Rudder (dikey stabilizatör arkası) | ±25° |

Her karede tüm vertex'ler uçağın konum/yönelimine ve yüzey açılarına göre
CPU'da dönüştürülüp tek tampona yazılır, tek çizim çağrısıyla gönderilir.
Vertex sayısı birkaç yüz olduğu için bu maliyetsizdir ve parça başına ayrı
çizim çağrısından çok daha ucuzdur.

Çizim mevcut `solid` shader'ı ile yapılır (pistlerde kullanılan). Yüzeylere
farklı parlaklık verilerek hacim hissi oluşturulur.

## Uçuş Fiziği

`flight.c` — donanımdan bağımsız, birim testli.

### Durum

Konum, hız vektörü, yönelim (yaw/pitch/roll), motor gücü (0–1), yakıt,
yüzey açıları.

### Kuvvetler

| Kuvvet | Model |
|---|---|
| İtki | motor gücü × azami itki, burun yönünde |
| Taşıma | ½·ρ·V²·S·CL, CL hücum açısı ve flap kademesiyle artar |
| Sürükleme | ½·ρ·V²·S·CD, CD taşımayla, flap ve spoiler ile artar |
| Ağırlık | (boş ağırlık + yakıt) × g, aşağı |

### Stall

Hücum açısı kritik değeri (16°) aşarsa CL çöker ve burun düşer. Flap açıkken
kritik açı bir miktar artar.

### Yüzeylerin etkisi

- **Flap:** CL ve CD artar → daha düşük hızda uçulabilir, iniş kolaylaşır
- **Spoiler:** CL düşer, CD artar → hızlı alçalma ve frenleme
- **Aileron/elevator/rudder:** yatış, burun ve sapma momentleri

### Yakıt

Motor gücüyle orantılı tüketilir. Ağırlık azaldıkça performans artar.
Yakıt biterse itki kesilir (uçak süzülür).

## Kamera Modları

`camera_mode.c` üç mod arasında geçiş yapar (tuşla):

1. **Dış:** uçağın arkasında ve biraz üstünde, yumuşak takip (gecikmeli)
2. **Kokpit:** burun konumunda, uçağın yönelimine tam bağlı
3. **Serbest:** mevcut serbest uçuş kamerası (sahneyi incelemek için)

Mevcut `Camera` yapısı korunur; uçuş modlarında konum ve açılar uçaktan
türetilir. Böylece sahne çizimi hiç değişmez.

## Ses

`audio.c` — PSL1GHT ses kütüphanesi üzerinden **gerçek zamanlı üretim**.
Ses dosyası kullanılmaz; bu hem dosya boyutunu sıfırlar hem sesin uçuşa
sürekli tepki vermesini sağlar.

| Ses | Üretim | Bağlı olduğu değer |
|---|---|---|
| Motor | temel frekans + harmonikler + gürültü | gaz kolu, hız |
| Rüzgar | filtrelenmiş gürültü | hava hızı |
| Deniz | alçak frekanslı uğultu | deniz seviyesine yakınlık |

Üç kaynak karıştırılıp tek akışa yazılır.

## Test Stratejisi

1. **Fizik birim testleri (host):**
   - Taşıma hızın karesiyle artıyor mu
   - Kritik açıda stall geliyor mu, flap açıkken kritik açı artıyor mu
   - Flap iniş hızını düşürüyor mu (aynı taşıma daha düşük hızda)
   - Spoiler alçalma hızını artırıyor mu
   - Yakıt tükenince ağırlık boş ağırlığa eşitleniyor mu
   - Uçak hiçbir girdi kombinasyonuyla deniz seviyesinin altına inmiyor mu
2. **Yüzey açıları:** kumanda girdisi → yüzey açısı dönüşümü sınırlar içinde mi
3. **RPCS3 duman testi:** uçak görünüyor mu, yüzeyler hareket ediyor mu,
   kamera modları çalışıyor mu, ses geliyor mu

## Riskler

| Risk | Azaltma |
|---|---|
| PS3 ses kütüphanesi bu projede ilk kez kullanılıyor | Ses ayrı adım olarak ele alınır; çalışmazsa uçak ve fizik yine de tamamlanmış olur |
| Fizik oynanabilirlik açısından zor/kolay olabilir | Katsayılar tek yerde toplanır, ayarlanması kolay olur |
| Uçak modeli düşük poli göründüğü için beğenilmeyebilir | Gövde hatları ve parlaklık farklarıyla hacim hissi güçlendirilir; sonradan Blender modeline geçiş yolu açık kalır |
