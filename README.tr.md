# KOU Racing - FSAE Telemetri Sistemi

[🇬🇧 English](README.md) | [🇹🇷 Türkçe](README.tr.md)

Formula Student Electric yarış aracı için geliştirilmiş gerçek zamanlı telemetri ve veri toplama sistemi. Sistem mimarisi üç ana alt bileşenden oluşur: araç içi dijital gösterge paneli (STM32), Ethernet tabanlı telemetri ağ geçidi (Raspberry Pi) ve pit alanı izleme uygulaması (Android).

---

## Mimari Genel Bakış

Uçtan uca veri akışı birbirine bağlı üç katman üzerinden yürütülür:

```mermaid
flowchart LR
    subgraph Arac["Araç Kokpiti (STM32F746G-DISCO)"]
        Sensors["Sensör Verileri\n(RPM, Sıcaklık, Voltaj, Güç)"] --> LVGL["Gösterge Paneli\n(480x272 LTDC / LVGL 9)"]
        Sensors --> UDP["LwIP UDP İstemcisi\n(10 Hz JSON Akışı)"]
    end

    subgraph Gateway["Araç İçi Ağ Geçidi (Raspberry Pi 5)"]
        UDPRecv["UDP Alıcı\n(Port 5000)"] --> WS["WebSocket Sunucusu\n(Port 3005)"]
        WS --> Tunnel["Cloudflare Tunnel\n(Uzak LTE Bağlantısı)"]
    end

    subgraph Pit["Pit Alanı İzleme (Android)"]
        Tunnel --> Client["WebSocket İstemcisi\n(OkHttp Akışı)"]
        Client --> UI["Telemetri Paneli\n(Gerçek Zamanlı Kartlar & Tanılama)"]
    end

    UDP -->|"Ethernet 100Mbps\n192.168.1.10 -> 192.168.1.2"| UDPRecv
    WS -->|"Yerel Wi-Fi\nws://192.168.1.2:3005"| Client
```

---

## Alt Sistemler

### 1. Araç Kokpit Yazılımı (`Telemetry/`)
* **Hedef Donanım:** STM32F746G-DISCO (ARM Cortex-M7 @ 216 MHz, 1 MB Flash, 320 KB SRAM, 64 Mbit harici SDRAM).
* **Ekran ve Grafik:**
  * 4.3 inç 480x272 LCD ekran, LTDC birimi üzerinden `0xC0000000` SDRAM taban adresindeki 16-bit RGB565 bellek tamponuyla sürülür.
  * Grafik arayüzü, LVGL 9 kütüphanesi ve doğrudan bellek tamponu (direct framebuffer flush) yöntemiyle işlenir.
  * FMC SDRAM arayüzü 100 MHz bellek saat frekansı ve CAS latency 3 değerine göre yapılandırılmıştır.
* **İşletim Sistemi ve Ağ:**
  * FreeRTOS çekirdeği arayüz çizimi ve telemetri paket iletim görevlerini yönetir.
  * LwIP TCP/IP yığını, RMII Ethernet PHY üzerinde statik IP (`192.168.1.10`) ile çalışır.
  * Telemetri verileri 10 Hz frekansla UDP port 5000 üzerinden yayınlanır.
* **Sürücü Arayüzü:**
  * Çift ekran mimarisi: Birincil kokpit ekranı (RPM LED göstergeleri, çekiş kontrol bildirimleri, hız, sıcaklıklar, batarya durumu) ve ikincil tanılama ekranı. Ekranlar arası geçiş, kart üzerindeki kullanıcı butonu (PI11) ile gerçekleştirilir.

### 2. Pit Alanı Android Uygulaması (`TelemetryAndroid/`)
* **Platform:** Yerel Android (Kotlin, Min SDK 24, Target SDK 34).
* **Ağ Katmanı:** OkHttp WebSocket istemcisi iki çalışma modunu destekler:
  * **Yerel Ağ Modu:** Pit alanındaki yerel Wi-Fi ağı üzerinden doğrudan bağlantı (`ws://<GATEWAY_IP>:3005`).
  * **Tünel Modu:** Hücresel LTE ağı üzerinden Cloudflare Zero Trust tünel uç noktasına uzaktan bağlantı (`wss://...`).
* **Kimlik Doğrulama:** Veri iletimi başlamadan önce soket üzerinden takım belirteci (token) el sıkışması gerçekleştirilir.
* **Veri Ayrıştırıcı:** Ağ geçidinden iletilen 28 baytlık little-endian IEEE-754 ikili paketleri doğrudan yapılandırılmış telemetri durum modellerine dönüştürür.

### 3. Ağ Geçidi ve Yardımcı Araçlar
* **`rpi_gateway_auth.js`:** Raspberry Pi üzerinde çalışan, takım kimlik bilgilerini doğrulayan ve doğrulanmamış bağlantıları zaman aşımıyla sonlandıran Node.js WebSocket ara yazılımı.
* **`udp_test_sender.py`:** Fiziksel donanım olmaksızın gömülü yazılım ve mobil uygulamayı doğrulamak üzere UDP üzerinden yapay Formula Student telemetri akışı üreten Python test aracı.

---

## Telemetri Paket Biçimi

### UDP Paketi (STM32 -> Ağ Geçidi)
STM32 mikrodenetleyicisi, telemetri verilerini UDP port 5000 üzerinden 10 Hz frekansla JSON formatında yayınlar:

```json
{
  "rpm": 5420,
  "speed": 82,
  "motor_temp": 68,
  "battery_voltage": 358.4,
  "throttle": 0.72,
  "brake": 0.00,
  "bat_temp": 42,
  "inv_temp": 48,
  "oil_temp": 78,
  "wtr_temp": 62,
  "tire_left": 58,
  "tire_right": 61,
  "power_watts": 185,
  "bat_percent": 74,
  "regen_percent": 12,
  "tcs_percent": 5,
  "mode": "ENDR",
  "ready": "true",
  "fault": "false",
  "traction_control": "false"
}
```

### İkili WebSocket Çerçevesi (Ağ Geçidi -> Android)
Mobil istemciye aktarılan yüksek frekanslı telemetri paketleri 28 baytlık little-endian ikili tampon olarak iletilir:

| Konum (Bayt) | Alan | Veri Tipi | Ölçek / Aralık |
|---|---|---|---|
| `0 - 3` | Paket Sıra No | uint32_t | Monotonik sayaç değeri |
| `4 - 7` | Motor Devri (RPM) | float (IEEE-754) | 0 - 10000 RPM |
| `8 - 11` | Araç Hızı | float (IEEE-754) | 0 - 150 km/h |
| `12 - 15` | Motor Sıcaklığı | float (IEEE-754) | °C |
| `16 - 19` | Batarya Voltajı | float (IEEE-754) | 300.0 - 400.0 V |
| `20 - 23` | Gaz Kelebeği Girdisi | float (IEEE-754) | 0.0 - 1.0 (veya %0 - %100) |
| `24 - 27` | Arıza Durumu | uint32_t | 0 = Normal, 1 = Arıza |

---

## Dizin Yapısı

```text
├── README.md                      # İngilizce sistem dokümantasyonu
├── README.tr.md                   # Türkçe sistem dokümantasyonu
├── LICENSE                        # MIT Lisansı
├── .gitignore                     # Derleme ve ortam dışlama kuralları
├── rpi_gateway_auth.js            # Ağ geçidi doğrulama ara yazılımı
├── udp_test_sender.py             # Telemetri simülasyon betiği
├── Telemetry_Final_Project.pdf    # Proje raporu ve şartname
├── Telemetry/                     # STM32CubeIDE projesi (C / FreeRTOS / LVGL)
│   ├── Core/                      # Uygulama kaynak kodları ve HAL sürücüleri
│   ├── Drivers/                   # CMSIS ve STM32F7xx HAL kütüphaneleri
│   ├── LWIP/                      # LwIP ağ yapılandırması ve hedef sürücüler
│   ├── Middlewares/               # FreeRTOS, LwIP ve LVGL kütüphanesi
│   └── Telemetry.ioc              # STM32CubeMX donanım yapılandırma dosyası
└── TelemetryAndroid/              # Android Studio projesi (Kotlin)
    └── app/src/main/
        ├── java/com/kouracing/    # Soket yöneticisi, veri modelleri, arayüz kodu
        └── res/                   # Ekran düzenleri, renkler, temalar, kaynaklar
```

---

## Kurulum ve Çalıştırma Kılavuzu

### Gömülü Yazılım (STM32)
1. **STM32CubeIDE** uygulamasını açın ve `Telemetry/` klasörünü mevcut proje olarak çalışma alanına aktarın.
2. GCC derleyici yapılandırmasında ARM Cortex-M7 donanımsal kayan nokta desteğinin (`-mfpu=fpv5-sp-d16 -mfloat-abi=hard`) etkin olduğunu doğrulayın.
3. STM32F746G-DISCO kartını ST-LINK USB portu (CN14) üzerinden geliştirme bilgisayarına bağlayın.
4. Hedefi derleyin (`Project -> Build Project`) ve ST-LINK hata ayıklama yapılandırmasıyla mikrodenetleyiciye yükleyin.

### Mobil Uygulama (Android)
1. **Android Studio** uygulamasında `TelemetryAndroid/` dizinini açın.
2. Gradle bağımlılık senkronizasyonunun (OkHttp, AndroidX, Material3) tamamlanmasını bekleyin.
3. Bir Android hedef cihaz bağlayın veya ağ erişimi etkin bir emülatör başlatın.
4. `:app` çalıştırma yapılandırmasını yürütün.

### Telemetri Simülatörü
Sistemi fiziksel araç veya sensörler olmadan test etmek için:
```bash
python udp_test_sender.py
```

---

## Lisans

Bu proje MIT Lisansı kapsamında sunulmaktadır. Ayrıntılar için [LICENSE](LICENSE) belgesine bakınız.
