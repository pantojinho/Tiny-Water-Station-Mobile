# Estação Meteorológica Mini — Waveshare ESP32-S3-Touch-AMOLED-1.64

Firmware Arduino completo de estação meteorológica para a placa
**Waveshare ESP32-S3-Touch-AMOLED-1.64** (AMOLED 280×456 QSPI + touch FT3168 +
IMU QMI8658) com sensor externo **Adafruit BMP581** (temperatura + pressão, I²C 0x47).

![board](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.64)

## Funcionalidades

- **Relógio NTP** com fuso da cidade escolhida; geo-IP apenas como configuração inicial
- **Previsão do tempo** Open-Meteo (temperatura externa, mín/máx, código WMO e ícones de sol, lua, nuvens, chuva e tempestade no painel web)
- **Alerta de chuva**: varra as próximas 12 h; probabilidade ≥ 40 % → banner "leva guarda-chuva"
- **Sensores locais**: BMP581 (temp/pressão), altitude barométrica (vs MSL em tempo real), QMI8658 (giro/accel)
- **Umidade relativa** via Open-Meteo
- **Tela horizontal LVGL 9**, 456×280, USB à direita, sem menus ou leitura do touch; temperatura do BMP581 em destaque, clima em tamanho menor e IP em faixa inferior
- **Web server** em `http://<ip>/` (ou `estacao.local`): monitor ao vivo + busca de cidade e configuração (WiFi, lat/lon, fuso) — APIs `/api/status` e `/api/config`
- **Log em cartão microSD** (`/estacao.csv`, 1 linha/min) — CSV pronto pra planilha
- AP de configuração (`EstacaoMeteo` / `12345678`), endereço `http://192.168.4.1/`: imediato sem rede salva; após aproximadamente 20 s sem conectar no início; após 60 s desconectado durante o uso

## Hardware

| Função | Pino |
|---|---|
| I²C (BMP581 + touch + IMU) | SDA=GPIO47, SCL=GPIO48 |
| Display AMOLED (QSPI) | CS=9, CLK=10, D0..D3=11..14, RST=21 |
| microSD | CS=38, MOSI=39, MISO=40, CLK=41 |
| BMP581 alimentação | 3V3 e GND do header |

O painel CO5300 permanece na resolução nativa 280×456 e rotação de hardware 0.
O firmware gira os pixels em software e usa uma interface lógica 456×280 fixa.
Coloque a placa com o conector USB à direita. Não há rotação automática.
As áreas de atualização são alinhadas em pares de pixels antes da rotação,
como exige o CO5300, para evitar rastros nas atualizações parciais.
O painel web também destaca a temperatura local do BMP581; o tempo da
cidade aparece como informação secundária, identificado como Internet.

## Build & Flash (arduino-cli)

```bash
arduino-cli lib install "lvgl@9.2.2" "Adafruit BMP5xx Library@1.0.2" "ArduinoJson@7.4.2" "GFX Library for Arduino@1.6.8"
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,UploadSpeed=921600" .
arduino-cli upload -p /dev/ttyACM0 --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB,UploadSpeed=921600" .
```

> Copie `lv_conf.h` para `~/Arduino/libraries/lv_conf.h` antes de compilar;
> o Arduino CLI não faz essa cópia automaticamente.

## Acesso e configuração

O touchscreen está desativado: nenhum dispositivo de entrada LVGL é criado,
e o FT3168 não é consultado. Configure tudo pelo endereço exibido na tela.
As consultas HTTP da previsão rodam em tarefa separada para não bloquear a UI.

Se a rede salva estiver indisponível, conecte o celular/computador ao Wi-Fi
**EstacaoMeteo**, senha **12345678**, e abra **http://192.168.4.1/**.
Preencha a rede e a senha em Configurações. Ao salvar uma mudança de Wi-Fi,
a página reinicia a estação; conecte à rede escolhida e use o novo IP da tela.
A localização e a rede ficam salvas na memória após desligar.

Os fusos brasileiros `America/...` mais comuns são convertidos para regras POSIX
antes de configurar o relógio do ESP32. Para outro local, informe diretamente
uma regra POSIX no campo de fuso horário.

## Escolher a cidade

Abra `http://<ip>/` no navegador, entre em **Configurações**, digite a cidade e
clique em **Buscar**. Escolha o resultado correto conferindo estado e país, depois
clique em **Salvar alterações**. A página preenche latitude, longitude e fuso
horário com os dados do Open-Meteo. A previsão e o relógio atualizam sem reiniciar;
a escolha fica salva após desligar o aparelho. Se a busca estiver indisponível,
latitude, longitude e fuso também podem ser informados manualmente. Uma troca de rede Wi-Fi salva pela página reinicia a estação automaticamente.

## Arquivos

| Arquivo | O que é |
|---|---|
| `main.cpp` | firmware principal (UI + sensores + web + SD) |
| `estacao_meteo.ino` | entrada vazia para Arduino IDE/CLI; main.cpp é compilado automaticamente |
| `platformio.ini` | configuração de compilação pelo PlatformIO |
| `web_page.h` | painel web e formulário de configuração |
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
4. **Rotação horizontal**: o flush gira tanto a área quanto os pixels RGB565;
   alterar apenas a resolução ou a rotação lógica não gira os pixels do CO5300.
5. Serial CDC do S3 exige DTR ativo (pyserial: `p.dtr = True`).

## Origem

Derivado do projeto de telemetria [Tiny-Water-Station-Mobile](https://github.com/pantojinho/Tiny-Water-Station-Mobile)
(reaproveitado: BMP581 da BOM, estrutura de log em SD e leitura periódica de sensores).

## Licença

MIT
