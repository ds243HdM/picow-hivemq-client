# picow-iot

This is a small example project that demonstrates connecting to an MQTT broker with a Pico W.  It uses lwIP's MQTT client and mbedtls to connect to a broker and transmit a message to a topic.

## Important note about SNI

Some cloud providers (e.g. AWS, HiveMQ) require that SNI be enabled on the client.  There does not appear to be a way to configure lwip's MQTT app client to set up SNI. This code uses a patched version of LWIP to support SNI.

If you want to connect to a cloud provider that requires SNI you'll need to apply the patch to the copy of LWIP thats used by the Pico SDK.

To do so, in your expansion of `pico-sdk`, run

```bash
cd lib/lwip
git apply path/to/mqtt-sni.patch
```

Then, re-build your application making sure it uses your patched changes.

## Setup

### pico-sdk

You should just need to set up pico-sdk as you would for pico-examples.

### cmake

Configure cmake with the following variables, the same as pico-examples.
- PICO_SDK_PATH
- PICO_BOARD
- WIFI_SSID
- WIFI_PASSWORD

### crypto_consts.h custom header

The build relies on a simple header file (crypto_consts.h) to provide cryptographic keys and certificates as well.

See crytpo_consts_example.h for a setup for AWS IoT and Mosquitto test servers.

## TODO
- Support additional configurations that work with background/FreeRTOS 

## Extra ANLEITUNG von Dominik

### 1. SNI-Patch installieren

Falls der Patch aus der README siehe oben allein das Problem nicht löst:

Kopiere die Datei `altcp_tls_mbedtls.c` aus diesem Projekt-Folder in deine SDK:

Beispiel-Path (Base of path could be different on your device):

```bash
cp altcp_tls_mbedtls.c ~/.pico-sdk/sdk/2.2.0/src/rp2_common/pico_lwip/altcp_tls_mbedtls.c
```

important part of path is `/rp2_common/pico_lwip/altcp_tls_mbedtls.c`

### 2. Konfiguration vorbereiten

Kopiere die Example-Konfigurationsdatei `crypto_consts.example.h` und fülle sie mit deinen Werten aus:

Deine eigene Datei sollte `crypto_consts.h` heißen.

```bash
cp crypto_consts.example.h crypto_consts.h
# Bearbeite crypto_consts.h und ergänze:
# - WIFI_SSID und WIFI_PASSWORD
# - MQTT_USERNAME und MQTT_PASSWORD
# - MQTT_SERVER_HOST
# - HIVE_MQ_CRYPTO_CA (dein Root-Zertifikat)
```

Das Hive MQ Crypto CA ist bereits im example, kann aber folgendermaßen generiert werden:
``openssl s_client -showcerts -connect <TLS MQTT URL (URL:Port)> </dev/null``

### 3. Build

Führe diese Befehle im `build/` Ordner aus:

```bash
cd build
rm -rf *
cmake -DPICO_SDK_PATH=$HOME/.pico-sdk/sdk/2.2.0 -DPICO_BOARD=pico_w ..
make -j4
```

Das fertige `.uf2` File findest du in `build/picow_iot.uf2`
