#include <RCSwitch.h>
#include <U8g2lib.h>
#include <Wire.h>
#include "logo_bitech.h"

// U8g2 en modo página de 128 bytes (vs 1024 de Adafruit)
U8G2_SSD1306_128X64_NONAME_2_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

RCSwitch sw433 = RCSwitch();

const int PIN_RESET = 6;
const int PIN_VBAT = A0;

// ---- Constantes de detección ----
const int MAX_CAPTURAS = 5;     // máximo de capturas antes de decidir
const int MIN_IGUALES = 3;      // mínimo de tramas iguales para dar COMPATIBLE
const int BAUD_MIN = 500;       // ventana de bitrate soportada (baud)
const int BAUD_MAX = 700;
const int RECEIVE_TOLERANCE = 50;  // % de tolerancia de RCSwitch (default 60)

// Multiplicador para pasar de pulso (µs) a bitrate: cantidad de pulsos por bit
// según el protocolo de RCSwitch (índice = protocolo - 1)
static const uint8_t PROGMEM MULT_POR_PROTOCOLO[] = { 4, 3, 15, 4, 3, 3, 7, 23, 23, 4, 3, 3 };

// ---- Struct de paquete ----
struct Paquete
{
  unsigned long codigo;
  int protocolo;
  int bits;
  int pulso;
};

Paquete capturas[MAX_CAPTURAS];
int indice = 0;
bool esperandoReset = false;
bool reintentando = false;
unsigned long ultimoAvisoBitrate = 0;

// -----------------------------------------------------------

int medirBateria()
{
  int raw = analogRead(PIN_VBAT);
  float vMedido = raw * (5.0 / 1023.0);
  float vBat = vMedido * (10.0 + 4.7) / 4.7;
  int pct = (int)((vBat - 6.5) / (8.4 - 6.5) * 100.0);
  return constrain(pct, 0, 100);
}

// Bitrate (baud) a partir del pulso medido por RCSwitch y el protocolo.
// baud = 1e6 / (pulsosPorBit * pulsoUs)
int calcularBaud(int pulsoUs, int protocolo)
{
  if (protocolo < 1 || protocolo > 12)
    return 0;
  uint8_t mult = pgm_read_byte(&MULT_POR_PROTOCOLO[protocolo - 1]);
  return (int)(1000000L / ((long)mult * pulsoUs));
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
    if (reintentando)
    {
      u8g2.print(indice);
      u8g2.print(F("/5"));
    }
    else
    {
      u8g2.print(min(indice, 3));
      u8g2.print(F("/3"));
    }
    u8g2.setCursor(0, 57);
    if (reintentando)
      u8g2.print(F("No coinciden, repita"));
    else
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
    u8g2.print(F("Baud:"));
    u8g2.print(calcularBaud(p.pulso, p.protocolo));
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
    u8g2.print(F("Baud:"));
    u8g2.print(calcularBaud(p.pulso, p.protocolo));
    u8g2.setCursor(0, 52);
    u8g2.print(F("Proto:"));
    u8g2.print(p.protocolo);
    u8g2.setCursor(0, 61);
    u8g2.print(F("Bits:"));
    u8g2.print(p.bits);
  } while (u8g2.nextPage());
}

void mostrarBitrateNoSoportado(int baud, int pulso)
{
  int bat = medirBateria();
  u8g2.firstPage();
  do
  {
    dibujarCabecera(bat);
    u8g2.setCursor(0, 22);
    u8g2.print(F("BITRATE NO SOPORTADO"));
    u8g2.drawHLine(0, 24, 128);
    u8g2.setCursor(0, 34);
    u8g2.print(F("Baud:"));
    u8g2.print(baud);
    u8g2.setCursor(0, 43);
    u8g2.print(F("Pulso:"));
    u8g2.print(pulso);
    u8g2.print(F(" us"));
    u8g2.setCursor(0, 52);
    u8g2.print(F("Solo 500-700 baud"));
  } while (u8g2.nextPage());
  delay(2500);
  mostrarEspera();
}

// -----------------------------------------------------------

void resetearDetector()
{
  indice = 0;
  esperandoReset = false;
  reintentando = false;
  ultimoAvisoBitrate = 0;
  for (int i = 0; i < MAX_CAPTURAS; i++)
    capturas[i] = {0, 0, 0, 0};
  sw433.resetAvailable();
  mostrarEspera();
}

// Comparación tolerante: tolera un bit de diferencia (glitches del receptor).
// - Bits iguales: el código debe coincidir.
// - Bits difieren en 1: coincide si el código es idéntico (glitch al final en
//   protocolos invertidos) o si coincide al descartar el bit espurio (codigo>>1).
bool mismaTrama(Paquete &a, Paquete &b)
{
  if (a.protocolo != b.protocolo)
    return false;
  int diffBits = abs(a.bits - b.bits);
  if (diffBits > 1)
    return false;
  if (diffBits == 0)
    return a.codigo == b.codigo;
  if (a.bits > b.bits)
    return (a.codigo == b.codigo) || ((a.codigo >> 1) == b.codigo);
  return (a.codigo == b.codigo) || ((b.codigo >> 1) == a.codigo);
}

// Busca la trama que se repite al menos MIN_IGUALES veces (mayoría de votos).
bool obtenerMayoria(Paquete &ganador)
{
  for (int i = 0; i < indice; i++)
  {
    int cont = 0;
    for (int j = 0; j < indice; j++)
    {
      if (mismaTrama(capturas[i], capturas[j]))
        cont++;
    }
    if (cont >= MIN_IGUALES)
    {
      ganador = capturas[i];
      return true;
    }
  }
  return false;
}

void procesarResultado(Paquete &ganador, bool esFijo)
{
  esperandoReset = true;

  if (esFijo)
    mostrarCompatible(ganador);
  else
    mostrarIncompatible(ganador);
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

  delay(2500);
  u8g2.setFont(u8g2_font_6x10_tf);
  sw433.setReceiveTolerance(RECEIVE_TOLERANCE);
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
    int pulso = sw433.getReceivedDelay();
    sw433.resetAvailable();

    if (cod == 0)
      return;

    int baud = calcularBaud(pulso, proto);

    Serial.print(F("["));
    Serial.print(indice + 1);
    Serial.print(F("] cod:"));
    Serial.print(cod);
    Serial.print(F(" proto:"));
    Serial.print(proto);
    Serial.print(F(" bits:"));
    Serial.print(bits);
    Serial.print(F(" pulso:"));
    Serial.print(pulso);
    Serial.print(F(" baud:"));
    Serial.println(baud);

    if (baud < BAUD_MIN || baud > BAUD_MAX)
    {
      if (millis() - ultimoAvisoBitrate > 3000)
      {
        ultimoAvisoBitrate = millis();
        mostrarBitrateNoSoportado(baud, pulso);
      }
      return;
    }

    capturas[indice] = {cod, proto, bits, pulso};
    indice++;

    if (indice >= MIN_IGUALES)
    {
      Paquete ganador;
      if (obtenerMayoria(ganador))
      {
        procesarResultado(ganador, true);
        return;
      }
      if (indice >= MAX_CAPTURAS)
      {
        procesarResultado(capturas[indice - 1], false);
        return;
      }
      reintentando = true;
    }

    mostrarEspera();
  }
}