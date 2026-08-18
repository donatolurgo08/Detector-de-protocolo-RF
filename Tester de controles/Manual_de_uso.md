# Tester de controles — Manual de uso

Simulador de control remoto HT6P20 (protocolo 6 de RCSwitch, 28 bits, invertido)
para ESP32 + módulo TX 433MHz. Sirve para probar el portón y enseñarle códigos
al TXCar sin depender del control real.

## Hardware

- ESP32 DevKit.
- **Datos del módulo transmisor** → **GPIO4** (`PIN_TX`).
- **Boton**: un terminal a **GPIO18** (`PIN_BOTON`) y el otro a **GND**.
- Módulo transmisor a **433MHz**.

## Serial

- Velocidad: **9600 baud**.
- Enviar cada comando seguido de **Enter** (`\n` o `\r`).

## Comandos

| Comando | Descripción |
|---|---|
| `m` | Muestra el menú y el estado actual. |
| `a` … `g` | Selecciona el preset de código (proto 6, 28 bits). |
| `p <us>` | Fija el pulso en microsegundos (default `342`). |
| `r <n>` | Fija las repeticiones por envío (default `10`). |
| `x` | Envía 1 pulsación (n tramas seguidas). |
| `z <s>` | Transmite en continuo durante `<s>` segundos (para enseñar al TXCar). |
| Botón físico | Cada pulsación envía 1 envío (como un control real). |

### Presets

Todos son **protocolo 6, 28 bits, 433MHz**.

| Letra | Código | Pulso | ~bps | Binario | Comentario |
|---|---|---|---|---|---|
| `a` | 51252245 | 342us | ~758 | `0011000011100000110000010101` | Control real que falla en TXCar |
| `b` | 123594517 | 342us | ~758 | `0111010111011110011100010101` | Control real que funciona en TXCar |
| `c` | 58918405 | 342us | ~758 | `0011100000110000011000000101` | Sintético, muchos ceros |
| `d` | 89478485 | 342us | ~758 | `0101010101010101010101010101` | Sintético, alternado puro |
| `e` | 209517045 | 342us | ~758 | `1100011111001111100111110101` | Sintético, muchos unos |
| `f` | 99438357 | **486us** | ~534 | `0101111011010100111100010101` | 534 bps, para probar el portón |
| `g` | 136178533 | **406us** | ~638 | `1000000111011110101101100101` | 638 bps, para probar el portón |

Nota: `f` y `g` fijan también el pulso automáticamente (486us y 406us). Los
presets `a`–`e` usan el pulso actual (default 342us).

## Ejemplos de uso

- Probar el portón con el código de 534 bps:
  ```
  f
  x
  ```
- Enseñarle ese código al TXCar:
  ```
  f
  z 5
  ```
- Ver el estado actual:
  ```
  m
  ```
