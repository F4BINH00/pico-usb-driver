# pico_usb – Driver Linux + Firmware + App

**Autores:**
- Fábio Rodrigues Borges Filho
- Rodrigo dos Santos Albuquerque

---

1) **Driver Linux (kernel module)** com operações **read/write** em endpoints **bulk**.
2) **Firmware** (TinyUSB) para uma placa em modo **USB Device** (Raspberry Pi Pico / RP2040).
3) **Aplicação de usuário** que conversa com o dispositivo através de um arquivo de dispositivo em `/dev`.

## Protocolo simples (host ↔ device)

O firmware implementa uma interface **USB Vendor Class** com 2 endpoints Bulk:
- **OUT (host → device):** recebe comandos/payload (usado por `write()` no driver).
- **IN (device → host):** envia respostas (usado por `read()` no driver).

VID/PID do dispositivo:
- **VID:** `0xCAFE`
- **PID:** `0x4001`

Comandos (primeiro byte do pacote):
- `0x01` `LED_ON`  → liga LED e responde `OK:LED_ON`
- `0x02` `LED_OFF` → desliga LED e responde `OK:LED_OFF`
- `0x03` `BLINK`   → bytes: `[0x03, reps, period_ms/10]`, responde `OK:BLINK`
- `0x10` `ECHO`    → ecoa o payload recebido (útil para testar **read**)

## Driver Linux (driver/)
```bash
make        # Compila o módulo pico_usb.ko
make clean  # Remove arquivos de compilação

make load   # Carrega o driver no kernel (insmod)
make remove # Remove o driver do kernel (rmmod)
make reload # Remove e carrega novamente o driver
