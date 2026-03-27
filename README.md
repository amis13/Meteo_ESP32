# MeteoStation para dos ESP32

Este proyecto usa dos ESP32 separados:

- `esp32_bme_tx`: va fuera de la cabina y lleva el `BME280`
- `esp32_oled_rx`: va dentro de la cabina y lleva la `OLED 0.96"` y los LEDs

Los dos ESP32 se comunican por `ESP-NOW`, asi que no hace falta router ni WiFi del barco.

## Material necesario

- 2 placas `ESP32`
- 1 modulo `BME280` por I2C
- 1 pantalla `OLED SSD1306 0.96"` por I2C
- 1 LED amarillo
- 1 LED azul
- 1 LED rojo
- 3 resistencias de `220` a `330 ohm` para los LEDs
- opcional: 1 `MOSFET P-channel` y 1 resistencia de `100k` para ahorrar aun mas bateria en el sensor exterior

## Librerias Arduino

Instala estas librerias desde el Gestor de Librerias del IDE de Arduino:

- `Adafruit BME280 Library`
- `Adafruit Unified Sensor`
- `Adafruit SSD1306`
- `Adafruit GFX Library`

## Conexion clara

## ESP32 exterior: opcion facil

Esta es la forma mas simple de empezar. Recomendable si primero quieres ver que todo funciona.

Conexion del `BME280` al ESP32 exterior:

- `ESP32 3V3` -> `BME280 VCC`
- `ESP32 GND` -> `BME280 GND`
- `ESP32 GPIO 21` -> `BME280 SDA`
- `ESP32 GPIO 22` -> `BME280 SCL`
- `ESP32 GPIO 33` -> no conectar

Con esta opcion el proyecto funciona, pero el `BME280` se queda alimentado todo el tiempo. El ESP32 exterior dormira, pero el sensor no se apagara.

## ESP32 exterior: opcion ahorro de bateria

Esta es la opcion para gastar menos bateria.

### Que es un MOSFET

Un `MOSFET` es, dicho simple, un interruptor electronico. En este proyecto sirve para que el ESP32 encienda el `BME280` solo unos segundos, mida, envie los datos y luego lo apague antes de dormir.

Si no quieres complicarte con esto, usa la opcion facil de arriba.

### Conexion con MOSFET

Usa un `MOSFET P-channel` como interruptor del positivo del `BME280`.

Conexion:

- `ESP32 3V3` -> `Source` del `MOSFET`
- `Drain` del `MOSFET` -> `BME280 VCC`
- `ESP32 GND` -> `BME280 GND`
- `ESP32 GPIO 21` -> `BME280 SDA`
- `ESP32 GPIO 22` -> `BME280 SCL`
- `ESP32 GPIO 33` -> `Gate` del `MOSFET` pasando antes por una resistencia de `100` a `220 ohm`
- `Gate` del `MOSFET` -> `3V3` con una resistencia de `100k`

Resumen de funcionamiento:

- `GPIO 33` en `LOW` -> el sensor se enciende
- `GPIO 33` en `HIGH` -> el sensor se apaga

Esquema simplificado:

```text
ESP32 3V3 -------------------- Source MOSFET
                               |
                               +---- Drain MOSFET ---- BME280 VCC

ESP32 GPIO33 -- resistencia --- Gate MOSFET
ESP32 3V3 ---- resistencia 100k-^

ESP32 GND --------------------- BME280 GND
ESP32 GPIO21 ------------------ BME280 SDA
ESP32 GPIO22 ------------------ BME280 SCL
```

En el codigo actual el pin de control es `GPIO 33`. Si algun dia quieres cambiarlo, esta en `SENSOR_POWER_PIN` dentro de `esp32_bme_tx/esp32_bme_tx.ino`.

## ESP32 interior: OLED y LEDs

### Pantalla OLED

Conexion de la OLED al ESP32 interior:

- `ESP32 3V3` -> `OLED VCC`
- `ESP32 GND` -> `OLED GND`
- `ESP32 GPIO 21` -> `OLED SDA`
- `ESP32 GPIO 22` -> `OLED SCL`

### LEDs

Cada LED debe llevar su resistencia de `220` a `330 ohm`.

Conexion recomendada:

- `ESP32 GPIO 26` -> resistencia -> pata larga del LED amarillo
- pata corta del LED amarillo -> `GND`
- `ESP32 GPIO 27` -> resistencia -> pata larga del LED azul
- pata corta del LED azul -> `GND`
- `ESP32 GPIO 25` -> resistencia -> pata larga del LED rojo
- pata corta del LED rojo -> `GND`

Regla rapida para no liarte con los LEDs:

- pata larga = lado positivo
- pata corta = lado negativo

## Que hace cada pin

### ESP32 exterior

- `GPIO 21`: datos I2C del `BME280`
- `GPIO 22`: reloj I2C del `BME280`
- `GPIO 33`: solo se usa si montas el corte de alimentacion del sensor
- `3V3`: alimentacion del `BME280` o del `MOSFET`
- `GND`: masa comun

### ESP32 interior

- `GPIO 21`: datos I2C de la `OLED`
- `GPIO 22`: reloj I2C de la `OLED`
- `GPIO 26`: LED amarillo
- `GPIO 27`: LED azul
- `GPIO 25`: LED rojo
- `3V3`: alimentacion de la `OLED`
- `GND`: masa comun

## Configuracion inicial

1. Carga primero `esp32_oled_rx/esp32_oled_rx.ino` en el ESP32 interior.
2. Abre el monitor serie a `115200`.
3. Copia la `MAC receptor` que aparecera al arrancar.
4. Abre `esp32_bme_tx/esp32_bme_tx.ino`.
5. Busca `RECEIVER_MAC`.
6. Sustituye esa MAC por la MAC real del ESP32 interior.
7. Carga `esp32_bme_tx/esp32_bme_tx.ino` en el ESP32 exterior.

## Funcionamiento

- El ESP32 exterior despierta cada `60 segundos`
- enciende el `BME280`
- mide temperatura, humedad y presion
- envia los datos al ESP32 interior
- vuelve a `deep sleep`

En el interior:

- la OLED muestra temperatura, humedad y presion
- el LED amarillo se enciende si hace calor
- el LED azul se enciende si hace frio
- el LED rojo parpadea si la presion cambia de forma importante
- si la presion baja, aparece `ALERTA TORMENTA`
- si la presion sube, aparece `MEJORA`
- si entra en alerta de tormenta, sale un icono de tormenta en pantalla durante unos segundos

## Ajustes utiles

En `esp32_oled_rx/esp32_oled_rx.ino` puedes cambiar:

- `HOT_TEMPERATURE_C`
- `COLD_TEMPERATURE_C`
- `FAST_DEMO_MODE`
- `TREND_WINDOW_MS`
- `STORM_DROP_THRESHOLD_HPA`
- `IMPROVEMENT_RISE_THRESHOLD_HPA`

En `esp32_bme_tx/esp32_bme_tx.ino` puedes cambiar:

- `SLEEP_INTERVAL_US`
- `SEND_TIMEOUT_MS`
- `SENSOR_POWER_PIN`
- `SENSOR_POWER_ACTIVE_HIGH`
- `SENSOR_POWER_STABILIZE_MS`

`FAST_DEMO_MODE = true` hace que la alerta salte antes para pruebas. Para uso real en el barco te conviene ponerlo en `false`.

## Recomendaciones para montar en el barco

- Coloca el `BME280` en una caja ventilada
- no lo cierres hermetico, porque el sensor necesita notar el aire exterior
- evita sol directo sobre el sensor
- protege la placa de salpicaduras
- si el sensor va fuera, intenta que quede en sombra y con aire circulando

## Si quieres hacerlo sin complicarte

Si ahora mismo no quieres usar `MOSFET`, haz esto:

- conecta el `BME280` con la opcion facil
- deja `GPIO 33` sin conectar
- prueba primero que el sistema mide y recibe datos

Cuando ya lo tengas funcionando, si quieres mas autonomia de bateria, pasas a la opcion con `MOSFET`.
