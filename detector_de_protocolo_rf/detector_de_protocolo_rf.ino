#include <RCSwitch.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "logo_bitech.h"

// U8g2 en modo página de 128 bytes (vs 1024 de Adafruit)
U8G2_SSD1306_128X64_NONAME_2_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

RCSwitch sw433 = RCSwitch();

const int PIN_RESET = 6;
const int PIN_VBAT = A0;

// ---- Struct de paquete ----
struct Paquete
{
  unsigned long codigo;
  int protocolo;
  int bits;
};

Paquete capturas[3];
int indice = 0;
bool esperandoReset = false;

// -----------------------------------------------------------

int medirBateria()
{
  int raw = analogRead(PIN_VBAT);
  float vMedido = raw * (5.0 / 1023.0);
  float vBat = vMedido * (10.0 + 4.7) / 4.7;
  int pct = (int)((vBat - 6.5) / (8.4 - 6.5) * 100.0);
  return constrain(pct, 0, 100);
}

// Dibuja cabecera: batería + línea separadora
// U8g2 trabaja dentro de firstPage/nextPage, no necesita clearDisplay
void dibujarCabecera(int bat)
{
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

void mostrarEspera()
{
  int bat = medirBateria();
  u8g2.firstPage();
  do
  {
    dibujarCabecera(bat);
    u8g2.setCursor(0, 23);
    u8g2.print(F("Apriete el control"));
    u8g2.setCursor(0, 33);
    u8g2.print(F("del porton 3 veces"));
    u8g2.setCursor(0, 47);
    u8g2.print(F("Detectados: "));
    u8g2.print(min(indice, 3));
    u8g2.print(F("/3"));
    u8g2.setCursor(0, 57);
    u8g2.print(F("433 MHz activo"));
  } while (u8g2.nextPage());
}

void mostrarCompatible(Paquete &p)
{
  int bat = medirBateria();
  u8g2.firstPage();
  do
  {
    dibujarCabecera(bat);
    u8g2.setCursor(0, 22);
    u8g2.print(F("COMPATIBLE"));
    u8g2.drawHLine(0, 24, 128);
    u8g2.setCursor(0, 34);
    u8g2.print(F("Freq:433.92 MHz"));
    u8g2.setCursor(0, 43);
    u8g2.print(F("Proto:"));
    u8g2.print(p.protocolo);
    u8g2.setCursor(0, 52);
    u8g2.print(F("Bits:"));
    u8g2.print(p.bits);
    u8g2.setCursor(0, 61);
    u8g2.print(F("Cod:"));
    u8g2.print(p.codigo);
  } while (u8g2.nextPage());
}

void mostrarIncompatible(Paquete &p)
{
  int bat = medirBateria();
  u8g2.firstPage();
  do
  {
    dibujarCabecera(bat);
    u8g2.setCursor(0, 22);
    u8g2.print(F("NO COMPATIBLE"));
    u8g2.drawHLine(0, 24, 128);
    u8g2.setCursor(0, 34);
    u8g2.print(F("Rolling Code"));
    u8g2.setCursor(0, 43);
    u8g2.print(F("Freq:433.92 MHz"));
    u8g2.setCursor(0, 52);
    u8g2.print(F("Proto:"));
    u8g2.print(p.protocolo);
    u8g2.setCursor(0, 61);
    u8g2.print(F("Bits:"));
    u8g2.print(p.bits);
  } while (u8g2.nextPage());
}

// -----------------------------------------------------------

void resetearDetector()
{
  indice = 0;
  esperandoReset = false;
  for (int i = 0; i < 3; i++)
    capturas[i] = {0, 0, 0};
  sw433.resetAvailable();
  mostrarEspera();
}

bool paquetesIguales(Paquete &a, Paquete &b)
{
  return (a.codigo == b.codigo) &&
         (a.protocolo == b.protocolo) &&
         (a.bits == b.bits);
}

void procesarResultado()
{
  esperandoReset = true;

  bool esFijo = paquetesIguales(capturas[0], capturas[1]) &&
                paquetesIguales(capturas[1], capturas[2]);

  // El último paquete capturado es capturas[(indice-1) % 3]
  Paquete &ultimo = capturas[(indice - 1) % 3];

  if (esFijo)
    mostrarCompatible(ultimo);
  else
    mostrarIncompatible(ultimo);
}

// -----------------------------------------------------------

void setup()
{
  Serial.begin(9600);
  pinMode(PIN_RESET, INPUT_PULLUP);

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);

  // logo
  u8g2.firstPage();
  do
  {
    u8g2.drawXBMP(0, 0, LOGO_W, LOGO_H, logo_bitech);
  } while (u8g2.nextPage());

  delay(2500);

  // titulo
  u8g2.firstPage();
  do
  {
    u8g2.setFont(u8g2_font_10x20_tf);
    u8g2.setCursor(10, 28);
    u8g2.print(F("RF ANALYZER"));
    u8g2.setFont(u8g2_font_9x15_tf);
    u8g2.setCursor(22, 52);
    u8g2.print(F("433 MHz"));
  } while (u8g2.nextPage());

  sw433.enableReceive(0);
  mostrarEspera();
}

void loop()
{
  // reset button polling
  if (digitalRead(PIN_RESET) == LOW)
  {
    delay(50);
    if (digitalRead(PIN_RESET) == LOW)
    {
      resetearDetector();
      while (digitalRead(PIN_RESET) == LOW)
        ;
    }
  }

  if (esperandoReset)
    return;

  if (sw433.available())
  {
    unsigned long cod = sw433.getReceivedValue();
    int proto = sw433.getReceivedProtocol();
    int bits = sw433.getReceivedBitlength();
    sw433.resetAvailable();

    if (cod == 0)
      return;

    capturas[indice % 3] = {cod, proto, bits};
    indice++;

    Serial.print(F("["));
    Serial.print(indice);
    Serial.print(F("] cod:"));
    Serial.print(cod);
    Serial.print(F(" proto:"));
    Serial.print(proto);
    Serial.print(F(" bits:"));
    Serial.println(bits);

    mostrarEspera();

    if (indice >= 3)
      procesarResultado();
  }
}