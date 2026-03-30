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

Esta opcion es ahora la configuracion por defecto del proyecto.

Conexion del `BME280` al ESP32 exterior:

- `ESP32 3V3` -> `BME280 VCC`
- `ESP32 GND` -> `BME280 GND`
- `ESP32 GPIO 21` -> `BME280 SDA`
- `ESP32 GPIO 22` -> `BME280 SCL`
- `ESP32 GPIO 33` -> no conectar
- `BME280 CSB` -> `3V3`
- `BME280 SDO` -> `GND` para direccion `0x76`

Con esta opcion el proyecto funciona, pero el `BME280` se queda alimentado todo el tiempo. El ESP32 exterior dormira, pero el sensor no se apagara.

No tienes que cambiar nada del codigo para usar esta opcion.

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
- `BME280 CSB` -> `3V3`
- `BME280 SDO` -> `GND` para direccion `0x76`

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
BME280 CSB -------------------- 3V3
BME280 SDO -------------------- GND
```

En el codigo actual el pin de control es `GPIO 33`. Si algun dia quieres cambiarlo, esta en `SENSOR_POWER_PIN` dentro de `esp32_bme_tx/esp32_bme_tx.ino`.

Para activar esta opcion en el codigo, cambia esto en `esp32_bme_tx/esp32_bme_tx.ino`:

```cpp
constexpr bool SENSOR_POWER_SWITCH_ENABLED = true;
```

## ESP32 interior: OLED y LEDs

### Pantalla OLED

Conexion de la OLED al ESP32 interior:

- `ESP32 3V3` -> `OLED VCC`
- `ESP32 GND` -> `OLED GND`
- `ESP32 GPIO 21` -> `OLED SDA`
- `ESP32 GPIO 22` -> `OLED SCL`

Tu OLED de `4 pines` se conecta solo con esos cuatro cables:

- `GND`
- `VCC`
- `SCL`
- `SDA`

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
- `BME280 CSB`: ponlo a `3V3`
- `BME280 SDO`: ponlo a `GND`

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

## Configuracion inicial con PowerShell

Si prefieres trabajar por terminal en Windows, este es el flujo bueno en `PowerShell`.

### 1. Abrir PowerShell en la carpeta del proyecto

```powershell
$ProjectRoot = "C:\ruta\al\repositorio\MeteoStation"
Set-Location $ProjectRoot
```

### 2. Preparar `arduino-cli` la primera vez

Si ya tienes `arduino-cli`, el core `ESP32` y las librerias instaladas, puedes saltar este paso.

```powershell
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib install "Adafruit BME280 Library" "Adafruit Unified Sensor" "Adafruit SSD1306" "Adafruit GFX Library"
```

Para comprobar que `arduino-cli` esta bien instalado:

```powershell
arduino-cli version
arduino-cli board list
```

### 3. Grabar el receptor interior

Conecta el ESP32 interior y mira que puerto `COM` usa:

```powershell
arduino-cli board list
```

Te saldra algo parecido a esto:

```text
Port Protocol Type              Board Name        FQBN
COMx serial   Serial Port (USB) ESP32 Dev Module esp32:esp32:esp32
```

Guarda ese puerto en una variable. Por ejemplo:

```powershell
$ReceiverPort = "COMx"
arduino-cli compile --fqbn esp32:esp32:esp32 esp32_oled_rx
arduino-cli upload -p $ReceiverPort --fqbn esp32:esp32:esp32 esp32_oled_rx
```

### 4. Leer la MAC del receptor

Abre el monitor serie con `DTR` y `RTS` desactivados para que el ESP32 no se resetee cada vez:

```powershell
arduino-cli monitor -p $ReceiverPort -c baudrate=115200,dtr=off,rts=off
```

Veras una linea como esta:

```text
MAC receptor: 24:6F:28:12:34:56
```

Esa es la direccion que necesita el emisor.

### 5. Poner esa MAC en el emisor

Tienes dos formas.

Forma rapida con script de `PowerShell`:

```powershell
$ReceiverMac = "24:6F:28:12:34:56"
.\scripts\set_receiver_mac.ps1 $ReceiverMac
```

Si Windows no te deja ejecutar scripts `.ps1`, usa:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\set_receiver_mac.ps1 $ReceiverMac
```

Forma manual:

Abre `esp32_bme_tx/esp32_bme_tx.ino` y busca esta linea:

```cpp
uint8_t RECEIVER_MAC[6] = {0x24, 0x6F, 0x28, 0x00, 0x00, 0x00};
```

Si la MAC que viste fue:

```text
24:6F:28:12:34:56
```

la linea debe quedar asi:

```cpp
uint8_t RECEIVER_MAC[6] = {0x24, 0x6F, 0x28, 0x12, 0x34, 0x56};
```

### 6. Grabar el emisor exterior

Conecta ahora el ESP32 exterior y vuelve a mirar que puerto `COM` usa:

```powershell
arduino-cli board list
```

Guarda ese puerto en otra variable. Por ejemplo:

```powershell
$TransmitterPort = "COMy"
arduino-cli compile --fqbn esp32:esp32:esp32 esp32_bme_tx
arduino-cli upload -p $TransmitterPort --fqbn esp32:esp32:esp32 esp32_bme_tx
```

### 7. Comprobar que funciona

Para ver el log del receptor:

```powershell
arduino-cli monitor -p $ReceiverPort -c baudrate=115200,dtr=off,rts=off
```

Para ver el log del emisor:

```powershell
arduino-cli monitor -p $TransmitterPort -c baudrate=115200,dtr=off,rts=off
```

En el receptor deberias ver paquetes recibidos. En el emisor deberias ver deteccion del `BME280`, envio correcto y luego entrada en `deep sleep`.

## Scripts de ayuda en PowerShell

He dejado scripts `.ps1` para no repetir comandos largos en Windows.

Si tu politica de ejecucion bloquea scripts, puedes lanzarlos con:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\nombre_del_script.ps1
```

### Ver puertos

```powershell
.\scripts\list_ports.ps1
```

### Grabar el receptor interior

```powershell
.\scripts\flash_rx.ps1 -Port COMx
```

### Ver la MAC del receptor

```powershell
.\scripts\monitor_rx.ps1 -Port COMx
```

Cuando veas una linea como:

```text
MAC receptor: 24:6F:28:12:34:56
```

actualiza la MAC del emisor con:

```powershell
.\scripts\set_receiver_mac.ps1 24:6F:28:12:34:56
```

### Grabar el emisor exterior

```powershell
.\scripts\flash_tx.ps1 -Port COMy
```

### Ver el log del emisor

```powershell
.\scripts\monitor_tx.ps1 -Port COMy
```

### Variables opcionales

Si alguna vez necesitas cambiar la placa o el baudrate sin tocar los scripts:

```powershell
$env:ARDUINO_FQBN = "esp32:esp32:esp32"
$env:MONITOR_BAUDRATE = "115200"
.\scripts\flash_rx.ps1 -Port COMx
.\scripts\monitor_rx.ps1 -Port COMx
```

## Scripts de ayuda en bash o WSL

Tambien siguen disponibles los scripts `.sh`, pero son utiles sobre todo en Linux o en `WSL` si el puerto serie esta realmente visible como `/dev/ttyUSB0` o `/dev/ttyACM0`.

### Ver puertos

```bash
./scripts/list_ports.sh
```

### Grabar el receptor interior

```bash
./scripts/flash_rx.sh /dev/ttyUSB0
```

### Ver la MAC del receptor

```bash
./scripts/monitor_rx.sh /dev/ttyUSB0
```

### Poner la MAC del receptor

```bash
./scripts/set_receiver_mac.sh 24:6F:28:12:34:56
```

### Grabar el emisor exterior

```bash
./scripts/flash_tx.sh /dev/ttyUSB1
```

### Ver el log del emisor

```bash
./scripts/monitor_tx.sh /dev/ttyUSB1
```

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

- `SENSOR_POWER_SWITCH_ENABLED`
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

## Resumen para modulos OLED y BME280 habituales

### OLED de 4 pines

Una OLED I2C de `4 pines` normalmente tiene:

- `GND`
- `VCC`
- `SCL`
- `SDA`

Conectala asi al ESP32 interior:

- `OLED GND` -> `ESP32 GND`
- `OLED VCC` -> `ESP32 3V3`
- `OLED SCL` -> `ESP32 GPIO 22`
- `OLED SDA` -> `ESP32 GPIO 21`

### BME280 de 6 pines

Un `BME280` de `6 pines` normalmente tiene:

- `VCC`
- `GND`
- `SCL`
- `SDA`
- `CSB`
- `SDO`

Conectalo asi al ESP32 exterior:

- `BME280 VCC` -> `ESP32 3V3`
- `BME280 GND` -> `ESP32 GND`
- `BME280 SCL` -> `ESP32 GPIO 22`
- `BME280 SDA` -> `ESP32 GPIO 21`
- `BME280 CSB` -> `ESP32 3V3`
- `BME280 SDO` -> `ESP32 GND`

Regla importante:

- `CSB` a `3V3` para usar modo `I2C`
- `SDO` a `GND` para direccion `0x76`

Si algun dia `0x76` no funcionase, podrias probar con `SDO` a `3V3` para direccion `0x77`, pero el sketch ya intenta ambas automaticamente.
