# Central NFC de Filamentos

Estação de bancada para controlar os rolos de filamento das impressoras
**Snapmaker U1** e **Bambu Lab H2S (AMS 2 Pro)**.

Cada rolo recebe uma tag NFC (NTAG213, formato OpenSpool). A central lê a tag, pesa o rolo
e diz em qual impressora e slot ele está. O consumo de cada impressão é descontado no Spoolman.

## Hardware

| Peça | Função |
|---|---|
| ESP32-2432S028R ("CYD", tela 2,8") | cérebro + tela touch |
| PN532 (I2C) | lê e grava as tags NFC |
| HX711 + célula de carga 10 kg | balança |
| Tags NTAG213 | uma por rolo |

## Etapas

1. **[Teste do hardware](docs/01-teste-do-hardware.md)** — ligar tudo na bancada e conferir. ← *agora*
2. Medidas reais → arquivos STL da caixa
3. Firmware da central (telas, Wi-Fi, Spoolman)
4. Servidor: Spoolman + ponte Bambu (H2S) + Bespok3d (U1)

## Estrutura

- `firmware/teste-hardware/` — firmware de teste (PlatformIO). Binários prontos em `bin/`.
- `docs/` — guias passo a passo.
