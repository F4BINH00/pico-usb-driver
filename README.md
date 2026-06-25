# 🚀 Pico_USB: Driver Linux & Firmware

Projeto de implementação de comunicação USB entre **Host Linux** e **Raspberry Pi Pico (RP2040)**.

## 👥 Autores
* **Fábio Rodrigues Borges Filho**
* **Rodrigo dos Santos Albuquerque**

## 📖 Visão Geral
Este projeto demonstra a integração entre hardware e sistema operacional através de:
* **Firmware:** Implementado com [TinyUSB] para operar em modo *USB Device*.
* **Driver Linux:** Módulo de Kernel que expõe o hardware via interface de arquivos `/dev`.
* **Aplicação de Usuário:** Software para interação (read/write) com o dispositivo.

## 🔌 Protocolo de Comunicação
Utilizamos a **USB Vendor Class** com dois *Endpoints Bulk*:
* **OUT (Host → Dispositivo):** Envio de comandos/payloads.
* **IN (Dispositivo → Host):** Recebimento de respostas.

**Identificadores:** `VID: 0xCAFE` | `PID: 0x4001`

### Comandos Suportados
| Comando | Hex | Descrição |
| :--- | :--- | :--- |
| `LED_ON` | `0x01` | Liga o LED do Pico |
| `LED_OFF` | `0x02` | Desliga o LED do Pico |
| `BLINK` | `0x03` | Pisca o LED (args: reps, period_ms/10) |
| `ECHO` | `0x10` | Ecoa o payload recebido (Teste de integridade) |

## 🛠️ Como Utilizar

### Driver Linux
Navegue até a pasta `/driver` e utilize o `make`:

```bash
make          # Compila o módulo pico_usb.ko
sudo make load    # Insere o módulo no Kernel (insmod)
sudo make remove  # Remove o módulo (rmmod)
sudo make reload  # Ciclo de recarga
