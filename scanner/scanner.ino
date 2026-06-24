#include <RCSwitch.h>
#include <U8g2lib.h>
#include <Wire.h>

// U8g2 en modo página de 128 bytes (vs 1024 de Adafruit)
U8G2_SSD1306_128X64_NONAME_2_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

RCSwitch sw433 = RCSwitch();

const int PIN_RESET = 6;
const int PIN_VBAT  = A0;

unsigned long codigos[3] = {0, 0, 0};
int  indice        = 0;
int  bitsDetectados = 0;
bool esperandoReset = false;
bool es433         = false;

// -----------------------------------------------------------

int medirBateria() {
  int raw = analogRead(PIN_VBAT);
  float vMedido = raw * (5.0 / 1023.0);
  float vBat    = vMedido * (10.0 + 4.7) / 4.7;
  int pct = (int)((vBat - 6.5) / (8.4 - 6.5) * 100.0);
  return constrain(pct, 0, 100);
}

// Dibuja cabecera: batería + línea separadora
// U8g2 trabaja dentro de firstPage/nextPage, no necesita clearDisplay
void dibujarCabecera(int bat) {
  // Texto batería
  u8g2.setCursor(0, 8);
  u8g2.print(F("BAT:"));
  u8g2.print(bat);
  u8g2.print('%');

  // Ícono batería (esquina superior derecha)
  u8g2.drawFrame(106, 1, 18, 7);
  u8g2.drawBox(124, 3, 2, 3);
  int ancho = (int)(14.0 * bat / 100.0);
  if (ancho > 0)
    u8g2.drawBox(107, 2, ancho, 5);

  // Línea separadora
  u8g2.drawHLine(0, 11, 128);
}

// -----------------------------------------------------------

void mostrarEspera() {
  int bat = medirBateria();
  u8g2.firstPage();
  do {
    dibujarCabecera(bat);
    u8g2.setCursor(0, 23);  u8g2.print(F("Apriete el control"));
    u8g2.setCursor(0, 33);  u8g2.print(F("del porton 3 veces"));
    u8g2.setCursor(0, 47);  u8g2.print(F("Detectados: "));
                            u8g2.print(min(indice, 3));
                            u8g2.print(F("/3"));
    u8g2.setCursor(0, 57);  u8g2.print(F("433 MHz activo"));
  } while (u8g2.nextPage());
}

void mostrarCompatible(unsigned long codigo, int protocolo) {
  int bat = medirBateria();
  u8g2.firstPage();
  do {
    dibujarCabecera(bat);
    u8g2.setCursor(0, 22);  u8g2.print(F("COMPATIBLE"));
    u8g2.drawHLine(0, 24, 128);
    u8g2.setCursor(0, 34);  u8g2.print(F("Freq:433.92 MHz"));
    u8g2.setCursor(0, 43);  u8g2.print(F("Proto:")); u8g2.print(protocolo);
    u8g2.setCursor(0, 52);  u8g2.print(F("Bits:"));  u8g2.print(bitsDetectados);
    u8g2.setCursor(0, 61);  u8g2.print(F("Cod:"));   u8g2.print(codigo);
  } while (u8g2.nextPage());
}

void mostrarIncompatible(int protocolo) {
  int bat = medirBateria();
  u8g2.firstPage();
  do {
    dibujarCabecera(bat);
    u8g2.setCursor(0, 22);  u8g2.print(F("NO COMPATIBLE"));
    u8g2.drawHLine(0, 24, 128);
    u8g2.setCursor(0, 34);  u8g2.print(F("Rolling Code"));
    u8g2.setCursor(0, 43);  u8g2.print(F("Freq:433.92 MHz"));
    u8g2.setCursor(0, 52);  u8g2.print(F("Proto:")); u8g2.print(protocolo);
    u8g2.setCursor(0, 61);  u8g2.print(F("Bits:"));  u8g2.print(bitsDetectados);
  } while (u8g2.nextPage());
}

// -----------------------------------------------------------

void resetearDetector() {
  indice         = 0;
  bitsDetectados = 0;
  esperandoReset = false;
  es433          = false;
  codigos[0] = codigos[1] = codigos[2] = 0;
  sw433.resetAvailable();
  mostrarEspera();
}

void procesarResultado(unsigned long codigo, int protocolo) {
  esperandoReset = true;
  bool esFijo = (codigos[0] == codigos[1]) && (codigos[1] == codigos[2]);
  if (esFijo)
    mostrarCompatible(codigo, protocolo);
  else
    mostrarIncompatible(protocolo);
}

// -----------------------------------------------------------

void setup() {
  Serial.begin(9600);
  pinMode(PIN_RESET, INPUT_PULLUP);

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);  // fuente pequeña, liviana

  // Pantalla de inicio
  u8g2.firstPage();
  do {
    u8g2.setCursor(10, 20); u8g2.print(F("DETECTOR RF 433"));
    u8g2.setCursor(20, 35); u8g2.print(F("v4.0 - Iniciando"));
  } while (u8g2.nextPage());

  delay(3000);

  sw433.enableReceive(0);  // INT0 = pin 2

  mostrarEspera();
}

void loop() {
  // Botón reset con debounce
  if (digitalRead(PIN_RESET) == LOW) {
    delay(50);
    if (digitalRead(PIN_RESET) == LOW) {
      resetearDetector();
      while (digitalRead(PIN_RESET) == LOW);
    }
  }

  if (esperandoReset) return;

  if (sw433.available()) {
    unsigned long cod = sw433.getReceivedValue();
    int proto         = sw433.getReceivedProtocol();
    bitsDetectados    = sw433.getReceivedBitlength();
    sw433.resetAvailable();

    if (cod == 0) return;

    codigos[indice % 3] = cod;
    indice++;

    mostrarEspera();

    if (indice >= 3)
      procesarResultado(cod, proto);
  }
}