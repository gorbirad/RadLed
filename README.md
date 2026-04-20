# 🏃 RadLed – LED Track Controller

> Kontroler taśmy LED WS2812B dla toru biegowego, sterowany przez WiFi z aplikacji mobilnej (Android).  
> Symuluje biegaczy poruszających się po torze – każdy z własnym tempem, kolorem i długością ogona.

---

## 📋 Spis treści

- [Opis projektu](#opis-projektu)
- [Sprzęt](#sprzęt)
- [Schemat podłączenia](#schemat-podłączenia)
- [Instalacja firmware](#instalacja-firmware)
- [Konfiguracja WiFi](#konfiguracja-wifi)
- [API – Endpointy HTTP](#api--endpointy-http)
- [Aplikacja mobilna (Flutter)](#aplikacja-mobilna-flutter)
- [Struktura projektu](#struktura-projektu)

---

## 📖 Opis projektu

RadLed pozwala na symulację biegaczy na świetlnym torze. Każdy „runner" to animowany segment taśmy LED poruszający się z prędkością odpowiadającą zadanemu tempu biegu (min/km). System obsługuje do **10 runnerów jednocześnie**, każdy z unikalnym kolorem i płynnym efektem zanikania ogona.

Sterowanie odbywa się przez **przeglądarkę lub aplikację mobilną** za pomocą prostego API HTTP.

---

## 🔧 Sprzęt

| Komponent | Specyfikacja |
|---|---|
| Mikrokontroler | ESP8266 – Wemos D1 Mini |
| Taśma LED | WS2812B – **120 diod** (GRB) |
| Gęstość taśmy | 60 LED/m → 2 metry toru |
| Pin danych | **D2 (GPIO4)** |
| Zasilanie LED | 5V (osobne, min. 3A) |
| Platforma | PlatformIO + Arduino Framework |
| Biblioteka LED | FastLED 3.7.x |

---

## 🔌 Schemat podłączenia

```
ESP8266 D1 Mini          WS2812B Strip
┌──────────────┐         ┌─────────────┐
│   D2 (GPIO4) │────────▶│  DATA IN    │
│   GND        │────────▶│  GND        │
└──────────────┘         └─────────────┘
                   5V PSU ──▶ VCC (taśmy)
                   5V PSU ──▶ GND (wspólna masa z ESP)
```

> ⚠️ **Uwaga:** Nie zasilaj 120 diod bezpośrednio z pinu 3V3 ESP8266. Używaj zewnętrznego zasilacza 5V/3A+.

---

## 🚀 Instalacja firmware

### Wymagania
- [PlatformIO](https://platformio.org/) (VS Code extension lub CLI)
- Python 3.x

### Kroki

1. **Sklonuj repozytorium:**
   ```bash
   git clone https://github.com/gorbirad/RadLed.git
   cd RadLed
   ```

2. **Utwórz plik z danymi WiFi** (plik jest wykluczony z gita – nie trafi do GitHub):
   ```bash
   # Utwórz plik: include/secrets.h
   ```
   ```cpp
   // include/secrets.h
   #define WIFI_SSID "TwojaNetworkNazwa"
   #define WIFI_PASS "TwojeHaslo"
   ```

3. **Podłącz D1 Mini przez USB i wgraj firmware:**
   ```bash
   pio run --target upload
   ```

4. **Otwórz monitor szeregowy** (115200 baud) aby zobaczyć adres IP:
   ```bash
   pio device monitor
   ```

---

## 📡 Konfiguracja WiFi

Firmware automatycznie uruchamia **tryb AP+STA** – działa jednocześnie jako punkt dostępu i klient sieci domowej.

| Tryb | Opis |
|---|---|
| **AP (zawsze aktywny)** | SSID: `RadLed-Track` / Hasło: `radled1234` / IP: `192.168.4.1` |
| **STA (jeśli dostępna sieć)** | Łączy się z siecią z `secrets.h`, otrzymuje IP z DHCP |

Jeśli brak sieci domowej – urządzenie działa tylko jako Access Point.

---

## 🌐 API – Endpointy HTTP

Wszystkie endpointy działają na porcie **80**. Można je wywołać z przeglądarki lub aplikacji.

### Animacja

| Endpoint | Metoda | Opis |
|---|---|---|
| `/start` | GET | Uruchamia animację |
| `/stopAll` | GET | Zatrzymuje animację (zachowuje runnerów) |
| `/reverse` | GET | Odwraca kierunek ruchu |
| `/clear` | GET | Usuwa wszystkich runnerów, resetuje taśmę |
| `/brightness?value=120` | GET | Ustawia jasność (0–255) |

### Runnerzy

| Endpoint | Parametry | Opis |
|---|---|---|
| `/addRunner` | `tempo`, `len`, `r`, `g`, `b` | Dodaje runnera |
| `/stopRunner` | `id` | Zatrzymuje konkretnego runnera |
| `/listRunners` | – | Zwraca listę aktywnych runnerów (JSON) |

#### Przykład – dodanie runnera (tempo 5 min/km, kolor czerwony):
```
GET http://192.168.4.1/addRunner?tempo=5&len=10&r=255&g=0&b=0
```

### Status i diagnostyka

| Endpoint | Opis | Format |
|---|---|---|
| `/status` | Stan animacji, jasność, czas | JSON |
| `/wifiInfo` | Tryb WiFi, adresy IP | JSON |

#### Przykład odpowiedzi `/status`:
```json
{
  "animationRunning": true,
  "reverseDirection": false,
  "elapsed": 12.34,
  "trackLength": 120,
  "brightness": 120,
  "wifiMode": "STA+AP"
}
```

#### Przykład odpowiedzi `/listRunners`:
```json
[
  {
    "id": 0,
    "uuid": "A1B2C3D4",
    "name": "Runner 0",
    "tempo": 5.00,
    "speed": 3.33,
    "pos": 42.10,
    "length": 10,
    "timeToLap": 23.45,
    "hex": "#FF0000",
    "mode": "runner",
    "color": { "r": 255, "g": 0, "b": 0 }
  }
]
```

> 💡 Endpointy są **case-insensitive** (działają zarówno `/stopAll` jak i `/stopall`).

---

## 📱 Aplikacja mobilna (Flutter)

Folder `mobile_app/` zawiera aplikację na Android napisaną w **Flutter**.

### Funkcje aplikacji

- 🔗 Pole adresu IP kontrolera (zapamiętywane między sesjami)
- ▶️ Przyciski: Start / Stop All / Reverse / Clear
- 🔆 Suwak jasności (0–255)
- ➕ Formularz dodawania runnera: tempo, długość ogona, picker RGB z podglądem koloru
- 📋 Lista aktywnych runnerów z przyciskiem Stop dla każdego
- 🔄 Auto-odświeżanie co 2 sekundy

### Uruchomienie aplikacji

1. Zainstaluj [Flutter SDK](https://flutter.dev/docs/get-started/install)
2. Wejdź do folderu aplikacji:
   ```bash
   cd mobile_app
   flutter pub get
   ```
3. Podłącz telefon z Android (USB Debugging włączone) i uruchom:
   ```bash
   flutter run
   ```

### Wymagania systemowe

| Element | Wersja |
|---|---|
| Flutter | 3.x stable |
| Android API | 34 (Android 14) |
| Pakiety | `http: ^1.2.2`, `shared_preferences: ^2.3.2` |

> ⚠️ Aplikacja komunikuje się przez HTTP (nie HTTPS) – na Androidzie wymagane jest `android:usesCleartextTraffic="true"` (już ustawione w manifeście).

---

## 📁 Struktura projektu

```
RadLed/
├── src/
│   └── main.cpp              # Główny firmware ESP8266
├── include/
│   ├── README
│   └── secrets.h             # ⛔ NIE w repozytorium (WiFi credentials)
├── mobile_app/
│   ├── lib/
│   │   └── main.dart         # Aplikacja Flutter (Android)
│   ├── android/
│   │   └── app/src/main/
│   │       └── AndroidManifest.xml
│   └── pubspec.yaml
├── platformio.ini            # Konfiguracja PlatformIO (D1 Mini)
├── .gitignore
└── README.md
```

---

## 🔒 Bezpieczeństwo

Plik `include/secrets.h` z danymi WiFi jest wykluczony z repozytorium przez `.gitignore`.  
Nigdy nie commituj danych dostępowych do gita.

---

## 📜 Licencja

Projekt prywatny / do użytku własnego.

