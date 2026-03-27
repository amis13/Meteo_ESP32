#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_sleep.h>
#include <Adafruit_BME280.h>

namespace Config {
constexpr uint8_t I2C_SDA_PIN = 21;
constexpr uint8_t I2C_SCL_PIN = 22;
constexpr uint8_t SENSOR_POWER_PIN = 33;
constexpr bool SENSOR_POWER_ACTIVE_HIGH = false;
constexpr uint64_t SLEEP_INTERVAL_US = 60ULL * 1000ULL * 1000ULL;
constexpr unsigned long SEND_TIMEOUT_MS = 1500;
constexpr unsigned long SENSOR_POWER_STABILIZE_MS = 250;
constexpr float SEA_LEVEL_PRESSURE_HPA = 1013.25f;

// Sustituye esta MAC por la del ESP32 receptor que lleva la OLED.
uint8_t RECEIVER_MAC[6] = {0x24, 0x6F, 0x28, 0x00, 0x00, 0x00};
}  // namespace Config

struct __attribute__((packed)) SensorPacket {
  uint32_t sequence;
  uint32_t uptimeMs;
  int16_t sensorStatus;
  float temperatureC;
  float humidityPct;
  float pressureHpa;
  float altitudeM;
};

Adafruit_BME280 bme;
bool bmeReady = false;
uint32_t packetCounter = 0;
volatile bool sendCompleted = false;
volatile bool sendSucceeded = false;

void setSensorPower(bool enabled) {
  const uint8_t level =
      enabled == Config::SENSOR_POWER_ACTIVE_HIGH ? HIGH : LOW;
  digitalWrite(Config::SENSOR_POWER_PIN, level);
}

void initSensorPower() {
  pinMode(Config::SENSOR_POWER_PIN, OUTPUT);
  setSensorPower(true);
  delay(Config::SENSOR_POWER_STABILIZE_MS);
}

void shutdownSensorBus() {
  Wire.end();
  pinMode(Config::I2C_SDA_PIN, INPUT);
  pinMode(Config::I2C_SCL_PIN, INPUT);
}

bool beginBmeSensor() {
  if (bme.begin(0x76)) {
    return true;
  }
  if (bme.begin(0x77)) {
    return true;
  }
  return false;
}

void printMac(const uint8_t *mac) {
  if (mac == nullptr) {
    Serial.println(F("desconocida"));
    return;
  }

  char macBuffer[18];
  snprintf(macBuffer, sizeof(macBuffer), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println(macBuffer);
}

void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status) {
  sendCompleted = true;
  sendSucceeded = (status == ESP_NOW_SEND_SUCCESS);
  Serial.print(F("Envio a "));
  printMac(macAddr);
  Serial.print(F("Resultado: "));
  Serial.println(sendSucceeded ? F("OK") : F("ERROR"));
}

void initEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.print(F("MAC emisor: "));
  Serial.println(WiFi.macAddress());
  Serial.print(F("MAC receptor configurada: "));
  printMac(Config::RECEIVER_MAC);

  if (esp_now_init() != ESP_OK) {
    Serial.println(F("Error iniciando ESP-NOW"));
    return;
  }

  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, Config::RECEIVER_MAC, sizeof(Config::RECEIVER_MAC));
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (!esp_now_is_peer_exist(Config::RECEIVER_MAC)) {
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println(F("No se pudo registrar el receptor como peer"));
    }
  }
}

SensorPacket buildPacket() {
  SensorPacket packet = {};
  packet.sequence = ++packetCounter;
  packet.uptimeMs = millis();
  packet.sensorStatus = bmeReady ? 1 : 0;

  if (!bmeReady) {
    return packet;
  }

  packet.temperatureC = bme.readTemperature();
  packet.humidityPct = bme.readHumidity();
  packet.pressureHpa = bme.readPressure() / 100.0f;
  packet.altitudeM = bme.readAltitude(Config::SEA_LEVEL_PRESSURE_HPA);

  if (isnan(packet.temperatureC) || isnan(packet.humidityPct) ||
      isnan(packet.pressureHpa)) {
    packet.sensorStatus = 0;
  }

  return packet;
}

bool sendCurrentReading() {
  SensorPacket packet = buildPacket();
  sendCompleted = false;
  sendSucceeded = false;

  esp_err_t result = esp_now_send(Config::RECEIVER_MAC,
                                  reinterpret_cast<uint8_t *>(&packet),
                                  sizeof(packet));

  if (packet.sensorStatus == 1) {
    Serial.print(F("Paquete #"));
    Serial.print(packet.sequence);
    Serial.print(F(" | T="));
    Serial.print(packet.temperatureC, 1);
    Serial.print(F(" C | H="));
    Serial.print(packet.humidityPct, 1);
    Serial.print(F(" % | P="));
    Serial.print(packet.pressureHpa, 1);
    Serial.println(F(" hPa"));
  } else {
    Serial.println(F("BME280 no disponible o lectura invalida"));
  }

  if (result != ESP_OK) {
    Serial.print(F("Error enviando por ESP-NOW: "));
    Serial.println(result);
    return false;
  }

  const unsigned long startWaitMs = millis();
  while (!sendCompleted && millis() - startWaitMs < Config::SEND_TIMEOUT_MS) {
    delay(10);
  }

  if (!sendCompleted) {
    Serial.println(F("Timeout esperando confirmacion de ESP-NOW"));
    return false;
  }

  return sendSucceeded;
}

void enterDeepSleep() {
  shutdownSensorBus();
  setSensorPower(false);
  delay(20);

  Serial.println(F("Entrando en deep sleep"));
  Serial.flush();

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();

  esp_sleep_enable_timer_wakeup(Config::SLEEP_INTERVAL_US);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  initSensorPower();
  Wire.begin(Config::I2C_SDA_PIN, Config::I2C_SCL_PIN);
  bmeReady = beginBmeSensor();

  Serial.println();
  Serial.println(F("Nodo exterior BME280"));
  Serial.println(bmeReady ? F("BME280 detectado") : F("BME280 no detectado"));

  initEspNow();
  sendCurrentReading();
  enterDeepSleep();
}

void loop() {}
