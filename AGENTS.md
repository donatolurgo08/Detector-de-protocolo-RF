# AGENTS.md

Proyecto en español: firmware en C++ para **Arduino Nano** que detecta protocolos RF 433MHz (código fijo vs rolling code). Todo el código, comentarios y textos de UI están en español: mantené ese idioma en cambios nuevos.

## Estructura

- `detector_de_protocolo_rf/` — **código del producto** (lo que se compila/flashea). Contiene el sketch `.ino` y `logo_bitech.h` (logo 128x64 en PROGMEM).
- `pruebas/` — sketches prototipo/experimentales (`scanner` v4.0, `transmitter` v5.0), versiones anteriores de la misma idea. **No editarlos** al mejorar el producto; trabajá solo sobre `detector_de_protocolo_rf/`.
- `3D/` — archivos STL del gabinete. `docs/` — documentación, diagramas y material de referencia.

## Compilar / flashear

No hay Makefile ni build CLI. Se compila desde **Arduino IDE** (o `arduino-cli`): abrir la carpeta `detector_de_protocolo_rf`, placa **Arduino Nano**. Librerías externas requeridas (instalar desde Library Manager):

- `RCSwitch` (sui77/rc-switch)
- `U8g2lib` (olikraus/u8g2)

En esta máquina ya está instalado `rc-switch` en `~/Arduino/libraries/`.

## Gotchas que no hay que "arreglar"

- **Display U8g2 en modo página**: `U8G2_SSD1306_128X64_NONAME_2_HW_I2C` — el `_2` es buffer de 128 bytes (ahorra SRAM en el Nano de 2KB). Todo el dibujo va dentro del loop `firstPage()/do{...}while(nextPage())` y **no usa `clearDisplay()`**.
- **Memoria**: SRAM muy limitada → usar `F()` en todos los strings y `PROGMEM` para bitmaps. No "optimizar" estas elecciones.

## Hardware (pines definidos en el código)

- Recepción RF: `sw433.enableReceive(0)` → INT0 = pin **D2**.
- Botón reset: pin **6**, `INPUT_PULLUP`, activo en LOW, con debounce de 50ms.
- Batería: **A0**, divisor 10k/4.7k, LiPo 2S (6.5–8.4V). Pct = `constrain((vBat-6.5)/(8.4-6.5)*100, 0, 100)`.
- OLED por I2C por hardware (A4/A5). Debug por `Serial` a 9600.

## Lógica del detector

Captura frames en `capturas[5]`. Con **3 tramas iguales** (comparación tolerante `mismaTrama`: admite ±1 bit de diferencia por glitches del receptor) → `COMPATIBLE` (código fijo). Si al llegar a 3 no coinciden pide reintento hasta **5** y decide por mayoría (`obtenerMayoria`, ≥3 iguales); si no hay mayoría → `NO COMPATIBLE` (rolling code). Además filtra por **bitrate**: `calcularBaud()` (1e6/(multProtocolo*pulsoUs)) debe caer en `BAUD_MIN..BAUD_MAX` (500–700), si no muestra `BITRATE NO SOPORTADO` y no cuenta el frame. RCSwitch devuelve `0` cuando falla la decodificación: ese paquete se descarta. `esperandoReset=true` congela el loop hasta que se apriete el botón.

## Convenciones de estilo

- Nombres de función en PascalCase en español: `medirBateria()`, `dibujarCabecera()`, `mostrarEspera()`, `resetearDetector()`.
- En el sketch principal las llaves van en línea propia. Los prototipos de `pruebas/` usan otra variante — no tomarlos como referencia de estilo.

## Roadmap (README)

Autoapagado/sleep del micro tras inactividad, soporte 315MHz, PCB dedicada. No hay CI, tests ni linter en el repo.
