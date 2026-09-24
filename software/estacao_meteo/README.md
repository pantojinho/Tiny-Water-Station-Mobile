# Estação Meteorológica Mini — Waveshare ESP32-S3-Touch-AMOLED-1.64

Firmware Arduino completo de estação meteorológica para a placa
**Waveshare ESP32-S3-Touch-AMOLED-1.64** (AMOLED 280×456 QSPI + touch FT3168 +
IMU QMI8658) com sensor externo **Adafruit BMP581** (temperatura + pressão, I²C 0x47).

![board](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.64)

## Funcionalidades

- **Relógio NTP** com fuso automático (geo-IP via ip-api.com)
- **Previsão do tempo** Open-Meteo (temperatura externa, mín/máx, código WMO + emoji)
- **Alerta de chuva**: varra as próximas 12 h; probabilidade ≥ 40 % → banner "leva guarda-chuva"
- **Sensores locais**: BMP581 (temp/pressão), altitude barométrica (vs MSL em tempo real), QMI8658 (giro/accel)
- **Umidade relativa** via Open-Meteo
- **UI touch LVGL 9** — 3 abas (Clima / Sensores / Config) com ícones OpenMoji
- **Web server** em `http://<ip>/` (ou `estacao.local`): monitor ao vivo + configuração (WiFi, lat/lon, fuso) — APIs `/api/status` e `/api/config`
- **Log em cartão microSD** (`/estacao.csv`, 1 linha/min) — CSV pronto pra planilha
- AP de configuração (`EstacaoMeteo` / `12345678`) se o WiFi falhar

## Hardware

| Função | Pino |
|---|---|
| I²C (BMP581 + touch + IMU) | SDA=GPIO47, SCL=GPIO48 |
| Display AMOLED (QSPI) | CS=9, CLK=10, D0..D3=11..14, RST=21 |
| microSD | CS=38, MOSI=39, MISO=40, CLK=41 |
| BMP581 alimentação | 3V3 e GND do header |

O painel CO5300 é montado **180°** — o firmware usa `rot=2` no construtor e inverte as
coordadas do touch (veja "Armadilhas").

## Build & Flash (arduino-cli)

```bash
arduino-cli lib install "lvgl" "Adafruit BMP5xx Library" "ArduinoJson" "GFX Library for Arduino"
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,UploadSpeed=921600" .
arduino-cli upload -p /dev/ttyACM0 --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,UploadSpeed=921600" .
```

> `lv_conf.h` do projeto é copiado automaticamente para `~/Arduino/libraries/` na 1ª build
> (ou copie manualmente — o LVGL procura lá quando não há flag de include).

## Arquivos

| Arquivo | O que é |
|---|---|
| `estacao_meteo.ino` | firmware principal (UI + sensores + web + SD) |
| `lv_conf.h` | configuração LVGL (RGB565, fontes Montserrat) |
| `icon_*.h` | emojis OpenMoji 72×72 em RGB565 (CC-BY-SA 4.0) |
| `icons.py` | gerador dos `icon_*.h` a partir de `assets/*.png` |
| `assets/` | PNGs origem (OpenMoji) |
| `test_display/` | sketch mínimo de teste de display+touch |

## Armadilhas dessa placa (learnings)

1. **Nunca chame `gfx->setRotation()` em runtime** — o CO5300 não suporta
   reprogramação do MADCTL após init → listras/corrupção. Painel fixo no construtor.
2. **QMI8658**: habilitar auto-incremento I²C (CTRL1=0x60); dados em 0x35..0x3F.
3. **Adafruit BMP5xx**: `pressure` já retorna hPa.
4. **Touch FT3168** retorna coordenadas do painel nativo — inverter para rot=2.
5. Serial CDC do S3 exige DTR ativo (pyserial: `p.dtr = True`).

## Origem

Derivado do projeto de telemetria [Tiny-Water-Station-Mobile](https://github.com/pantojinho/Tiny-Water-Station-Mobile)
(reaproveitado: BMP581 da BOM, estrutura de log em SD e leitura periódica de sensores).

## Licença

MIT
