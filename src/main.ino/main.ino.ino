// ===== Blynk & Project Info =====
#define BLYNK_TEMPLATE_ID   "TMPL6kHzRcBsJ"
#define BLYNK_TEMPLATE_NAME "Plant watering system update"
#define BLYNK_AUTH_TOKEN    "XqpK3Lcq-mxZrkPl-j_oUth0361h6NVv"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include <DHT.h>

// ===== Pins & Modules =====
#define SDA_PIN     21
#define SCL_PIN     22
#define LCD_ADDR    0x27
#define SOIL_PIN    33
#define DHT_PIN     25
#define DHTTYPE     DHT11
#define SOUND_PIN   32      // cảm biến âm thanh (digital OUT)

// ===== Relay control =====
#define RELAY_PIN         27
#define RELAY_ACTIVE_LOW  0   // module của bạn active HIGH -> đặt 0

// ===== Safety & control tunables =====
const int HYSTERESIS_PCT = 3;                  // ±3% quanh ngưỡng
const unsigned long MIN_ON_MS   = 5000;        // bật tối thiểu 5s
const unsigned long MIN_OFF_MS  = 5000;        // tắt tối thiểu 5s
const unsigned long MAX_ON_MS   = 10UL * 60UL * 1000UL;  // bật tối đa 10 phút
const unsigned long COOLDOWN_MS = 30UL * 1000UL;         // nghỉ 30s sau khi tắt

// ===== Clap logic (đơn giản) =====
bool     emergencyStop = false;
uint32_t lastEdgeMs    = 0;    // mốc cạnh lên gần nhất (debounce)
uint8_t  clapCount     = 0;    // số clap trong chuỗi hiện tại (chỉ dùng cho double khi không EMG)
const uint16_t CLAP2_WINDOW_MS = 1000; // 2 cái trong ≤ 1s để vào EMG
const uint16_t MIN_EDGE_GAP_MS = 80;   // chống dội (2 cạnh tối thiểu cách nhau 80ms)
uint32_t clapSeqStartMs = 0;           // thời điểm clap đầu tiên của chuỗi hiện tại

// ===== Soil filtering (không hiệu chuẩn) =====
const uint8_t SOIL_SAMPLES = 8;  // số mẫu trung bình

// ===== States =====
bool  pumpOnState   = false;
bool  manualPumpCmd = false;   // lệnh từ V1
bool  autoMode      = false;   // sync từ V6
int   soilThreshold = 40;      // sync từ V5
unsigned long lastSwitchMs = 0;        // mốc đổi trạng thái bơm gần nhất
unsigned long pumpOnStartMs = 0;       // mốc bật bơm để kiểm tra MAX_ON

// ===== WiFi =====
char ssid[] = "IphoneforIoT";
char pass[] = "12345679";

// ===== Objects =====
LiquidCrystal_PCF8574 lcd(LCD_ADDR);
BlynkTimer timer;
DHT dht(DHT_PIN, DHTTYPE);

// ===== Relay helpers =====
inline void writeRelay(bool on) {
  int level = on ? (RELAY_ACTIVE_LOW ? LOW : HIGH)
                 : (RELAY_ACTIVE_LOW ? HIGH : LOW);
  digitalWrite(RELAY_PIN, level);
}
void pumpOn()  {
  if (!pumpOnState) {
    pumpOnState   = true;
    writeRelay(true);
    pumpOnStartMs = millis();
  }
}
void pumpOff() {
  if (pumpOnState) {
    pumpOnState = false;
    writeRelay(false);
  }
}

// ===== Soil read/map =====
int readSoilRawAvg() {
  uint32_t acc = 0;
  for (uint8_t i = 0; i < SOIL_SAMPLES; i++) {
    acc += analogRead(SOIL_PIN);
    delay(2);
  }
  return acc / SOIL_SAMPLES;
}
// Mặc định: raw cao ~ khô, raw thấp ~ ướt
int soilRawToPercent(int raw) {
  long pct = map(raw, 0, 4095, 100, 0); // 0->100% (ướt), 4095->0% (khô)
  if (pct < 0) pct = 0; if (pct > 100) pct = 100;
  return (int)pct;
}

// ===== BLYNK HANDLERS =====

// V1: Manual ON/OFF (gỡ EMG bằng ON hoặc OFF)
BLYNK_WRITE(V1) {
  int cmd = param.asInt();   // 0/1
  manualPumpCmd = cmd;

  if (emergencyStop) {
    emergencyStop = false;
    lastSwitchMs = millis();

    if (cmd == 1) {
      if (!autoMode) pumpOn();      // chỉ bật ngay khi Manual
      Blynk.virtualWrite(V1, 1);
      Serial.println("[EMG] Cleared by V1->ON; Pump ON (Manual)");
    } else {
      pumpOff();
      Blynk.virtualWrite(V1, 0);
      Serial.println("[EMG] Cleared by V1->OFF; Pump OFF");
    }
    return;
  }

  // Không còn EMG: Manual điều khiển trực tiếp
  if (!autoMode) {
    if (cmd) pumpOn(); else pumpOff();
    lastSwitchMs = millis();
  }
}

// V6: 1=Auto, 0=Manual
BLYNK_WRITE(V6) {
  autoMode = param.asInt();

  // Đổi mode thì gỡ khóa EMG (nếu đang khóa)
  if (emergencyStop) {
    emergencyStop = false;
    Serial.println("[EMG] Cleared by mode change V6");
  }

  if (autoMode) {
    // Cho phép AUTO đánh giá ngay (không phải chờ cooldown lần đầu)
    unsigned long unlock = (COOLDOWN_MS > MIN_OFF_MS) ? COOLDOWN_MS : MIN_OFF_MS;
    lastSwitchMs = millis() - unlock;
  } else {
    // về Manual -> thực thi theo lệnh tay đang có
    if (manualPumpCmd) pumpOn(); else pumpOff();
    lastSwitchMs = millis();
  }
}

// V5: Threshold 0..100
BLYNK_WRITE(V5) {
  soilThreshold = constrain(param.asInt(), 0, 100);
}

// ===== SENSORS LOOP =====
void readSensors() {
  // Soil %
  int raw  = readSoilRawAvg();
  int soil = soilRawToPercent(raw);
  Blynk.virtualWrite(V0, soil);

  // DHT
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h) && !isnan(t)) {
    Blynk.virtualWrite(V2, t);
    Blynk.virtualWrite(V3, h);
  }

  // ---- Điều khiển bơm ----
  unsigned long now = millis();

  // Giới hạn an toàn khi đang ON
  if (pumpOnState) {
    if (now - pumpOnStartMs >= MAX_ON_MS) {
      pumpOff();
      emergencyStop = true;             // EMG mềm (bảo vệ)
      lastSwitchMs = now;
      Serial.println("[SAFE] MAX_ON cut -> EMG soft");
      Blynk.virtualWrite(V1, 0);
    }
  }

  if (emergencyStop) {
    if (pumpOnState) { pumpOff(); lastSwitchMs = now; }
  } else if (autoMode) {
    bool okMinOn  = (now - lastSwitchMs >= MIN_ON_MS);
    bool okMinOff = (now - lastSwitchMs >= MIN_OFF_MS);
    bool okCool   = (now - lastSwitchMs >= COOLDOWN_MS);

    if (pumpOnState) {
      bool conditionToOff =
        (soil >= (soilThreshold + HYSTERESIS_PCT)) &&
        okMinOn;
      if (conditionToOff) { pumpOff(); lastSwitchMs = now; }
    } else {
      bool conditionToOn =
        (soil <= (soilThreshold - HYSTERESIS_PCT)) &&
        okMinOff && okCool;
      if (conditionToOn) { pumpOn(); lastSwitchMs = now; }
    }
  } else {
    // Manual: đã xử lý trong V1
  }

  // ---- LCD ----
  // Dòng 1: T/H
  lcd.setCursor(0, 0);
  lcd.print("T:");
  if (isnan(t)) lcd.print("--"); else lcd.print(t, 1);
  lcd.print("C H:");
  if (isnan(h)) lcd.print("--"); else lcd.print(h, 0);
  lcd.print("%  ");

  // Dòng 2: Soil + Mode + Pump (+ EMG)
  lcd.setCursor(0, 1);
  lcd.print("Soil:");
  lcd.print(soil);
  lcd.print("% ");
  lcd.print(autoMode ? "[A]" : "[M]");
  lcd.print(" ");
  if (emergencyStop)      lcd.print("[EMG]");
  else if (pumpOnState)   lcd.print("[ON ]");
  else                    lcd.print("[OFF]");
  lcd.print(" "); // đệm

  // ---- Serial log ----
  Serial.printf("RAW:%d Soil:%d%% | Thr:%d%% | Mode:%s | Pump:%s | EMG:%d | T:%.1fC | H:%.0f%%\n",
                raw, soil, soilThreshold,
                autoMode ? "AUTO" : "MAN",
                pumpOnState ? "ON" : "OFF",
                emergencyStop, t, h);
}

// ===== POLL ÂM THANH (đơn giản) =====
// Double (≤ CLAP2_WINDOW_MS) = EMG OFF; Single (khi đang EMG) = Clear EMG + ON
void pollSound() {
  static int lastVal = LOW;
  int v = digitalRead(SOUND_PIN);
  uint32_t now = millis();

  // Phát hiện cạnh lên (rising edge) + debounce tối thiểu
  if (v == HIGH && lastVal == LOW) {
    if (now - lastEdgeMs >= MIN_EDGE_GAP_MS) {
      lastEdgeMs = now;

      if (!emergencyStop) {
        // ---- BÌNH THƯỜNG: 2 clap -> EMG
        if (clapCount == 0) {
          clapCount = 1;
          clapSeqStartMs = now;
        } else {
          if (now - clapSeqStartMs <= CLAP2_WINDOW_MS) {
            // DOUBLE -> EMG
            emergencyStop   = true;
            pumpOff();
            lastSwitchMs    = now;
            Blynk.virtualWrite(V1, 0);
            Serial.println("[EMG] Triggered by DOUBLE clap");
            clapCount = 0; // kết thúc chuỗi
          } else {
            // quá cửa sổ -> mở chuỗi mới
            clapCount = 1;
            clapSeqStartMs = now;
          }
        }
      } else {
        // ---- ĐANG EMG: 1 clap bất kỳ -> gỡ EMG & bật lại
        emergencyStop   = false;
        manualPumpCmd   = true;   // mong muốn bật tay
        pumpOn();
        lastSwitchMs    = now;    // tránh bị tắt ngay bởi cooldown/min off
        Blynk.virtualWrite(V1, 1);
        Serial.println("[EMG] Cleared by SINGLE clap -> Pump ON");
        clapCount = 0;
      }
    }
  }

  // Hết cửa sổ double nhưng chưa đủ 2 clap -> reset
  if (!emergencyStop && clapCount > 0 && (now - clapSeqStartMs > CLAP2_WINDOW_MS)) {
    clapCount = 0;
  }

  lastVal = v;

  // (Tuỳ chọn) xem xung: Blynk.virtualWrite(V4, v);
}

// ===== SETUP/LOOP =====
void setup() {
  Serial.begin(115200);

  // Pre-drive relay để không kích lúc boot
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? HIGH : LOW);
  pinMode(RELAY_PIN, OUTPUT);
  pumpOff();

  // I2C & LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.begin(16, 2);
  lcd.setBacklight(255);
  lcd.clear();
  lcd.print("Connecting WiFi...");

  // Sensors & IO
  dht.begin();
  pinMode(SOUND_PIN, INPUT);

  delay(100);
  // Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass, "blynk.cloud", 8080);
  Blynk.syncVirtual(V1, V5, V6);   // đồng bộ điều khiển

  // Nếu sau sync đang AUTO -> cho phép đánh giá ngay, không phải chờ cooldown lần đầu
  if (autoMode) {
    unsigned long unlock = (COOLDOWN_MS > MIN_OFF_MS) ? COOLDOWN_MS : MIN_OFF_MS;
    lastSwitchMs = millis() - unlock;
  }

  lcd.clear();
  lcd.print("WiFi OK!");
  lcd.setCursor(0, 1);
  lcd.print("Sensors ready");

  // Timers
  timer.setInterval(2000, readSensors);
  timer.setInterval(10,   pollSound);
}

void loop() {
  Blynk.run();
  timer.run();
}
