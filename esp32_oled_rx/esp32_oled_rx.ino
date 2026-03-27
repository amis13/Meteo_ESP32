#include <Arduino.h>
#include <limits.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

namespace Config {
constexpr uint8_t I2C_SDA_PIN = 21;
constexpr uint8_t I2C_SCL_PIN = 22;
constexpr uint8_t OLED_ADDRESS = 0x3C;

constexpr uint8_t LED_YELLOW_PIN = 26;
constexpr uint8_t LED_BLUE_PIN = 27;
constexpr uint8_t LED_RED_PIN = 25;

constexpr float HOT_TEMPERATURE_C = 25.0f;
constexpr float COLD_TEMPERATURE_C = 18.0f;

// true para reaccion mas rapida en pruebas; false para una tendencia mas real.
constexpr bool FAST_DEMO_MODE = true;
constexpr unsigned long TREND_WINDOW_MS =
    FAST_DEMO_MODE ? 2UL * 60UL * 1000UL : 15UL * 60UL * 1000UL;
constexpr unsigned long TREND_REFERENCE_TOLERANCE_MS =
    FAST_DEMO_MODE ? 30UL * 1000UL : 5UL * 60UL * 1000UL;
constexpr float STORM_DROP_THRESHOLD_HPA =
    FAST_DEMO_MODE ? -0.6f : -2.0f;
constexpr float IMPROVEMENT_RISE_THRESHOLD_HPA =
    FAST_DEMO_MODE ? 0.6f : 2.0f;

constexpr unsigned long RED_BLINK_INTERVAL_MS = 350;
constexpr unsigned long STORM_ALERT_SCREEN_MS = 5000;
constexpr unsigned long LINK_TIMEOUT_MS = 180000;
constexpr size_t PRESSURE_HISTORY_SIZE = 180;
}  // namespace Config

constexpr uint8_t SCREEN_WIDTH = 128;
constexpr uint8_t SCREEN_HEIGHT = 64;
constexpr int8_t OLED_RESET_PIN = -1;

struct __attribute__((packed)) SensorPacket {
  uint32_t sequence;
  uint32_t uptimeMs;
  int16_t sensorStatus;
  float temperatureC;
  float humidityPct;
  float pressureHpa;
  float altitudeM;
};

struct PressureSample {
  unsigned long receivedAtMs;
  float pressureHpa;
  bool valid;
};

enum class ForecastTrend : uint8_t {
  Waiting,
  Learning,
  Stable,
  StormAlert,
  Improving,
  SensorError,
  LinkLost,
};

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET_PIN);

SensorPacket latestPacket = {};
bool hasPacket = false;
bool displayReady = false;
unsigned long lastPacketMs = 0;
unsigned long lastBlinkToggleMs = 0;
unsigned long stormAlertOverlayUntilMs = 0;
bool redLedState = false;
float lastStormDeltaHpa = 0.0f;
ForecastTrend previousForecastTrend = ForecastTrend::Waiting;

PressureSample pressureHistory[Config::PRESSURE_HISTORY_SIZE] = {};
size_t pressureWriteIndex = 0;

void pushPressureSample(float pressureHpa, unsigned long nowMs) {
  pressureHistory[pressureWriteIndex] = {nowMs, pressureHpa, true};
  pressureWriteIndex = (pressureWriteIndex + 1) % Config::PRESSURE_HISTORY_SIZE;
}

bool getTrendReference(unsigned long nowMs, float &referencePressure,
                       unsigned long &referenceAgeMs) {
  bool found = false;
  unsigned long closestAgeDelta = ULONG_MAX;

  for (const PressureSample &sample : pressureHistory) {
    if (!sample.valid || sample.receivedAtMs >= nowMs) {
      continue;
    }

    const unsigned long ageMs = nowMs - sample.receivedAtMs;
    if (ageMs < Config::TREND_WINDOW_MS) {
      continue;
    }
    if (ageMs >
        Config::TREND_WINDOW_MS + Config::TREND_REFERENCE_TOLERANCE_MS) {
      continue;
    }

    const unsigned long distanceToWindow = ageMs - Config::TREND_WINDOW_MS;
    if (!found || distanceToWindow < closestAgeDelta) {
      found = true;
      closestAgeDelta = distanceToWindow;
      referencePressure = sample.pressureHpa;
      referenceAgeMs = ageMs;
    }
  }

  return found;
}

ForecastTrend evaluateForecast(float &deltaHpa, unsigned long &referenceAgeMs) {
  deltaHpa = 0.0f;
  referenceAgeMs = 0;

  if (!hasPacket) {
    return ForecastTrend::Waiting;
  }

  const unsigned long nowMs = millis();
  if (nowMs - lastPacketMs > Config::LINK_TIMEOUT_MS) {
    return ForecastTrend::LinkLost;
  }

  if (latestPacket.sensorStatus != 1) {
    return ForecastTrend::SensorError;
  }

  float referencePressure = 0.0f;
  if (!getTrendReference(nowMs, referencePressure, referenceAgeMs)) {
    return ForecastTrend::Learning;
  }

  deltaHpa = latestPacket.pressureHpa - referencePressure;
  if (deltaHpa <= Config::STORM_DROP_THRESHOLD_HPA) {
    return ForecastTrend::StormAlert;
  }
  if (deltaHpa >= Config::IMPROVEMENT_RISE_THRESHOLD_HPA) {
    return ForecastTrend::Improving;
  }
  return ForecastTrend::Stable;
}

const __FlashStringHelper *temperatureLabel(float temperatureC) {
  if (temperatureC >= Config::HOT_TEMPERATURE_C) {
    return F("CALOR");
  }
  if (temperatureC <= Config::COLD_TEMPERATURE_C) {
    return F("FRIO");
  }
  return F("NORMAL");
}

const __FlashStringHelper *forecastLabel(ForecastTrend trend) {
  switch (trend) {
    case ForecastTrend::Waiting:
      return F("ESPERANDO");
    case ForecastTrend::Learning:
      return F("ANALIZANDO");
    case ForecastTrend::Stable:
      return F("ESTABLE");
    case ForecastTrend::StormAlert:
      return F("ALERTA TORMENTA");
    case ForecastTrend::Improving:
      return F("MEJORA");
    case ForecastTrend::SensorError:
      return F("ERROR BME280");
    case ForecastTrend::LinkLost:
      return F("SIN ENLACE");
  }

  return F("DESCONOCIDO");
}

const __FlashStringHelper *forecastScreenLabel(ForecastTrend trend) {
  switch (trend) {
    case ForecastTrend::Waiting:
      return F("ESPERA");
    case ForecastTrend::Learning:
      return F("ANALIZA");
    case ForecastTrend::Stable:
      return F("ESTABLE");
    case ForecastTrend::StormAlert:
      return F("TORMENTA");
    case ForecastTrend::Improving:
      return F("MEJORA");
    case ForecastTrend::SensorError:
      return F("ERROR");
    case ForecastTrend::LinkLost:
      return F("ENLACE");
  }

  return F("N/D");
}

void updateTemperatureLeds() {
  if (!hasPacket || latestPacket.sensorStatus != 1) {
    digitalWrite(Config::LED_YELLOW_PIN, LOW);
    digitalWrite(Config::LED_BLUE_PIN, LOW);
    return;
  }

  digitalWrite(Config::LED_YELLOW_PIN,
               latestPacket.temperatureC >= Config::HOT_TEMPERATURE_C ? HIGH
                                                                       : LOW);
  digitalWrite(Config::LED_BLUE_PIN,
               latestPacket.temperatureC <= Config::COLD_TEMPERATURE_C ? HIGH
                                                                        : LOW);
}

void updateRedLed(ForecastTrend trend) {
  const bool shouldBlink =
      trend == ForecastTrend::StormAlert || trend == ForecastTrend::Improving;

  if (!shouldBlink) {
    redLedState = false;
    digitalWrite(Config::LED_RED_PIN, LOW);
    return;
  }

  const unsigned long nowMs = millis();
  if (nowMs - lastBlinkToggleMs >= Config::RED_BLINK_INTERVAL_MS) {
    lastBlinkToggleMs = nowMs;
    redLedState = !redLedState;
    digitalWrite(Config::LED_RED_PIN, redLedState ? HIGH : LOW);
  }
}

void updateStormOverlayState(ForecastTrend trend, float deltaHpa) {
  if (trend == ForecastTrend::StormAlert &&
      previousForecastTrend != ForecastTrend::StormAlert) {
    stormAlertOverlayUntilMs = millis() + Config::STORM_ALERT_SCREEN_MS;
    lastStormDeltaHpa = deltaHpa;
  }

  previousForecastTrend = trend;
}

bool shouldShowStormOverlay(ForecastTrend trend) {
  return trend == ForecastTrend::StormAlert &&
         millis() < stormAlertOverlayUntilMs;
}

void drawStormIcon(int16_t originX, int16_t originY) {
  display.fillCircle(originX + 16, originY + 18, 11, SSD1306_WHITE);
  display.fillCircle(originX + 30, originY + 14, 14, SSD1306_WHITE);
  display.fillCircle(originX + 45, originY + 19, 10, SSD1306_WHITE);
  display.fillRoundRect(originX + 12, originY + 18, 38, 12, 6, SSD1306_WHITE);

  display.fillTriangle(originX + 30, originY + 26, originX + 22, originY + 48,
                       originX + 31, originY + 48, SSD1306_BLACK);
  display.fillTriangle(originX + 31, originY + 48, originX + 25, originY + 48,
                       originX + 37, originY + 60, SSD1306_BLACK);
  display.drawLine(originX + 13, originY + 35, originX + 9, originY + 45,
                   SSD1306_WHITE);
  display.drawLine(originX + 47, originY + 35, originX + 51, originY + 45,
                   SSD1306_WHITE);
  display.drawLine(originX + 54, originY + 35, originX + 58, originY + 45,
                   SSD1306_WHITE);
}

void drawStormAlertScreen() {
  if (!displayReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 6, SSD1306_WHITE);
  drawStormIcon(4, 2);

  display.setTextSize(1);
  display.setCursor(67, 8);
  display.print(F("ALERTA"));
  display.setCursor(67, 20);
  display.print(F("TORMENTA"));
  display.setCursor(67, 36);
  display.print(F("Presion"));
  display.setCursor(67, 46);
  display.print(F("cayendo"));

  display.setCursor(6, 54);
  display.print(F("dP "));
  display.print(lastStormDeltaHpa, 2);
  display.print(F(" hPa"));

  display.display();
}

void drawWaitingScreen(const __FlashStringHelper *message) {
  if (!displayReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("MeteoStation RX"));
  display.println();
  display.println(message);
  display.println();
  display.println(F("Esperando datos"));
  display.println(F("desde ESP32 exterior"));
  display.display();
}

void drawDataScreen(ForecastTrend trend, float deltaHpa,
                    unsigned long referenceAgeMs) {
  if (!displayReady) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print(latestPacket.temperatureC, 1);
  display.print(F("C"));

  display.setTextSize(1);
  display.setCursor(0, 18);
  display.print(F("H: "));
  display.print(latestPacket.humidityPct, 1);
  display.print(F("%  P: "));
  display.print(latestPacket.pressureHpa, 1);

  display.setCursor(0, 28);
  display.print(F("Temp: "));
  display.print(temperatureLabel(latestPacket.temperatureC));

  display.setCursor(0, 38);
  display.print(F("Tiempo: "));
  display.print(forecastScreenLabel(trend));

  display.setCursor(0, 48);
  display.print(F("dP: "));
  display.print(deltaHpa, 2);
  display.print(F(" hPa"));

  display.setCursor(0, 58);
  display.print(F("Ventana: "));
  display.print(referenceAgeMs / 1000UL);
  display.print(F(" s"));

  display.display();
}

void printPacketToSerial(ForecastTrend trend, float deltaHpa) {
  Serial.print(F("Paquete #"));
  Serial.print(latestPacket.sequence);
  Serial.print(F(" | T="));
  Serial.print(latestPacket.temperatureC, 1);
  Serial.print(F(" C | H="));
  Serial.print(latestPacket.humidityPct, 1);
  Serial.print(F(" % | P="));
  Serial.print(latestPacket.pressureHpa, 1);
  Serial.print(F(" hPa | Estado="));
  Serial.print(forecastLabel(trend));
  Serial.print(F(" | dP="));
  Serial.println(deltaHpa, 2);
}

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData,
                int len) {
#else
void onDataRecv(const uint8_t *macAddr, const uint8_t *incomingData, int len) {
#endif
  if (len != static_cast<int>(sizeof(SensorPacket))) {
    Serial.print(F("Paquete descartado, tamano inesperado: "));
    Serial.println(len);
    return;
  }

  memcpy(&latestPacket, incomingData, sizeof(latestPacket));
  lastPacketMs = millis();
  hasPacket = true;

  if (latestPacket.sensorStatus == 1) {
    pushPressureSample(latestPacket.pressureHpa, lastPacketMs);
  }

  float deltaHpa = 0.0f;
  unsigned long referenceAgeMs = 0;
  const ForecastTrend trend = evaluateForecast(deltaHpa, referenceAgeMs);
  updateStormOverlayState(trend, deltaHpa);
  updateTemperatureLeds();
  printPacketToSerial(trend, deltaHpa);

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  (void)info;
#else
  (void)macAddr;
#endif
}

void initDisplay() {
  displayReady = display.begin(SSD1306_SWITCHCAPVCC, Config::OLED_ADDRESS);
  if (!displayReady) {
    Serial.println(F("No se detecta la OLED SSD1306"));
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void initEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.print(F("MAC receptor: "));
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println(F("Error iniciando ESP-NOW"));
    return;
  }

  esp_now_register_recv_cb(onDataRecv);
}

void setupPins() {
  pinMode(Config::LED_YELLOW_PIN, OUTPUT);
  pinMode(Config::LED_BLUE_PIN, OUTPUT);
  pinMode(Config::LED_RED_PIN, OUTPUT);

  digitalWrite(Config::LED_YELLOW_PIN, LOW);
  digitalWrite(Config::LED_BLUE_PIN, LOW);
  digitalWrite(Config::LED_RED_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  delay(500);

  setupPins();
  Wire.begin(Config::I2C_SDA_PIN, Config::I2C_SCL_PIN);
  initDisplay();

  Serial.println();
  Serial.println(F("Nodo interior OLED"));

  drawWaitingScreen(F("Iniciando receptor"));
  initEspNow();
  drawWaitingScreen(F("ESP-NOW listo"));
}

void loop() {
  float deltaHpa = 0.0f;
  unsigned long referenceAgeMs = 0;
  const ForecastTrend trend = evaluateForecast(deltaHpa, referenceAgeMs);

  updateTemperatureLeds();
  updateRedLed(trend);

  if (!hasPacket) {
    drawWaitingScreen(F("Sin paquetes aun"));
  } else if (trend == ForecastTrend::LinkLost) {
    drawWaitingScreen(F("Enlace perdido"));
  } else if (latestPacket.sensorStatus != 1) {
    drawWaitingScreen(F("Error de sensor"));
  } else if (shouldShowStormOverlay(trend)) {
    drawStormAlertScreen();
  } else {
    drawDataScreen(trend, deltaHpa, referenceAgeMs);
  }

  delay(50);
}
