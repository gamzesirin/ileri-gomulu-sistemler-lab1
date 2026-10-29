# Laboratuvar 1 – GPIO ile LED Sürme (ESP32-S3 ve Raspberry Pi 4)

İleri Gömülü Sistemler dersi (Dr. Murat Sürücü), Laboratuvar 1 çalışması.

- **Hazırlayan:** Gamze Şirin
- **Öğrenci numarası:** 2510140022
- **Donanım:** ESP32-S3-N16R8 geliştirme kartı, Raspberry Pi 4 Model B, 2 LED, 2 direnç
- **Yazılım:** ESP-IDF v6.1 (macOS), Raspberry Pi OS Lite 64-bit (trixie), libgpiod 2.2.1, Docker
- **Tarih:** 29.10.2026

Rapor: [`lab1/Lab1_Rapor.pdf`](lab1/Lab1_Rapor.pdf)

Lojik analizör kullanılmadığı için ölçümler kartların kendi saatleriyle alındı. Kayıtlar `lab1/olcum/` klasöründedir.

## Klasörler

| Klasör / dosya | İçerik |
|---|---|
| `lab1/lab1_esp32_register/` | Hocanın verdiği ESP32 projesi: GPIO4 LED'i yalnızca registerlarla |
| `lab1/lab1_pi_libgpiod/` | Pi programı `pi_gpio.c`, `Makefile`, Docker ile çapraz derleme (`Dockerfile`, `capraz_derle.sh`) |
| `lab1/grafik.py` | Şekil 1'i kayıtlardan çizer |
| `lab1/olcum/` | Monitör ve Pi çıktıları, `sekil1.png` |

## Bağlantılar

| Kart | Bağlantı |
|---|---|
| ESP32-S3 | GPIO4 → direnç → LED (+), LED (−) → GND |
| Raspberry Pi 4 | Pin 11 (GPIO17) → direnç → LED (+), LED (−) → pin 9 (GND) |

## Görevler ve sonuçlar

**Görev 1 – hello_world ve `idf.py size-components`.** Program 150 685 bayt. Bunun yalnızca 298 baytı uygulama kodu (`libmain.a`). %60'ı flash'ta, %40'ı iç RAM'de duruyor (`olcum/hello_size_components.txt`).

**Görev 2 – LED'i registerlarla sürmek.** `IO_MUX_GPIO4 = 0x1A02`, `FUNC4_OUT_SEL = 0x500`, `GPIO_ENABLE = 0x10`. LED yakıldıktan sonra `GPIO_IN` bit 4 = 1 (`olcum/esp32_register_kayit.txt`).

**Görev 3 – Pi'de libgpiod ve çapraz derleme.** `pi_gpio.c` Mac'te Docker içinde `aarch64-linux-gnu-gcc 14.2.0` ile derlendi (`ELF 64-bit ... ARM aarch64`). Mac'te çalışmadı (`exec format error`), Pi'de çalıştı (`olcum/pi_derleme.txt`).

**Görev 4 – En yüksek frekans ve periyodun düzenliliği** (`olcum/pi_olcum.txt`):

| | ESP32-S3, register | Pi, normal | Pi, SCHED_FIFO 90 + CPU 2 |
|---|---|---|---|
| Frekans | 7,993 MHz | 514,2 kHz | 539,9 kHz |
| Periyot | 125,1 ns | 1,92–2,06 µs | 1,85 µs |
| %99,9 / en uzun periyot | – | 6,1 µs / 121,5 µs | 1,9 µs / 31,3 µs |
| 5 µs'den uzun periyot | – | 1 546 / 1 000 000 | 473 / 1 000 000 |

## Derleme ve çalıştırma

ESP32:

```
cd lab1/lab1_esp32_register
idf.py set-target esp32s3
idf.py -p /dev/cu.usbmodemXXXX flash monitor
```

Raspberry Pi (Mac'te, Docker açıkken):

```
cd lab1/lab1_pi_libgpiod
./capraz_derle.sh
scp pi_gpio pi@rpi.local:~/
ssh pi@rpi.local
./pi_gpio yak                          # 1 Hz, 5 kez
./pi_gpio frekans                      # 5 x 1 s pencere
./pi_gpio dagilim                      # 1 000 000 periyot, histogram
sudo ./pi_gpio -r -c 2 frekans         # gerçek zamanlı öncelik, CPU 2
sudo ./pi_gpio -r -c 2 dagilim
```

Grafik: `cd lab1 && python3 grafik.py`

---

Beyan: Kodların hazırlanmasında yapay zekâ araçlarından destek alınmıştır.
