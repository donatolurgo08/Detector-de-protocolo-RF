#include <RCSwitch.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "logo_bitech.h"

// U8g2 en modo página de 128 bytes (vs 1024 de Adafruit)
U8G2_SSD1306_128X64_NONAME_2_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

RCSwitch sw433 = RCSwitch();

const int PIN_RESET = 6;
const int PIN_VBAT = A0;

// ---- Constantes de detección ----
const int MAX_CAPTURAS = 5;     // máximo de capturas antes de decidir
const int MIN_IGUALES = 3;      // mínimo de tramas iguales para dar COMPATIBLE
const int BAUD_MIN = 400;       // ventana de bitrate soportada (baud)
const int BAUD_MAX = 800;
const int RECEIVE_TOLERANCE = 50;  // % de tolerancia de RCSwitch (default 60)
const unsigned long TIMEOUT_SLEEP = 120000UL;  // autoapagado por inactividad (2 min)
const int BATERIA_BAJA = 15;              // umbral de aviso de batería baja (%)
const unsigned long INTERVALO_BATERIA = 1000UL;  // re-medir batería cada 1 s

// ---- Struct de paquete ----
struct Paquete
{
  unsigned long codigo;
  int protocolo;
  int bits;
  int pulso;
  int baud;
};

Paquete capturas[MAX_CAPTURAS];
int indice = 0;
bool esperandoReset = false;
bool reintentando = false;
unsigned long ultimaActividad = 0;
unsigned long ultimaMedicionBateria = 0;
int bateriaCached = -1;   // -1 = aún sin medir

// -----------------------------------------------------------

// Mide VCC en mV usando el bandgap interno de 1.1V como referencia.
// Así la lectura de batería no depende de que VCC sea exactamente 5V.
long leerVcc()
{
  ADMUX = (1 << REFS0) | (1 << MUX3) | (1 << MUX2) | (1 << MUX1);
  delay(2);
  ADCSRA |= (1 << ADSC);
  while (bit_is_set(ADCSRA, ADSC))
    ;
  uint8_t bajo = ADCL;
  uint8_t alto = ADCH;
  return 1125300L / ((alto << 8) | bajo);
}

// Porcentaje de batería: promedio de lecturas, con caché de 1 s.
// pct = (vBat - 6.5V) / (8.4V - 6.5V) * 100, con divisor 10k/4.7k.
int medirBateria()
{
  if (millis() - ultimaMedicionBateria < INTERVALO_BATERIA && bateriaCached >= 0)
    return bateriaCached;
  ultimaMedicionBateria = millis();

  long suma = 0;
  for (int i = 0; i < 8; i++)
    suma += analogRead(PIN_VBAT);
  int raw = (int)(suma / 8);

  long vcc = leerVcc();                          // mV
  long vBat = raw * vcc * 147L / (1023L * 47L);  // divisor 10k/4.7k
  long pct = (vBat - 6500L) * 100L / (8400L - 6500L);
  pct = constrain(pct, 0, 100);

  bateriaCached = (int)pct;
  return bateriaCached;
}

// Factores de pulso por protocolo RCSwitch (sync, zero, one), en PROGMEM.
// La duración de la trama se deriva de estos y del pulso medido, sin depender
// del buffer crudo de timings (que el ISR sigue sobrescribiendo).
struct FactoresProto
{
  uint8_t syncH, syncL;
  uint8_t zeroH, zeroL;
  uint8_t oneH, oneL;
};

static const FactoresProto PROGMEM factoresProto[12] = {
  {  1, 31,  1,  3,  3,  1 },   // 1
  {  1, 10,  1,  2,  2,  1 },   // 2
  { 30, 71,  4, 11,  9,  6 },   // 3
  {  1,  6,  1,  3,  3,  1 },   // 4
  {  6, 14,  1,  2,  2,  1 },   // 5
  { 23,  1,  1,  2,  2,  1 },   // 6
  {  2, 62,  1,  6,  6,  1 },   // 7
  {  3,130,  7, 16,  3, 16 },   // 8
  {130,  7, 16,  7, 16,  3 },   // 9
  { 18,  1,  3,  1,  1,  3 },   // 10
  { 36,  1,  1,  2,  2,  1 },   // 11
  { 36,  1,  1,  2,  2,  1 },   // 12
};

// Bitrate (baud) promedio sobre la trama completa (sync + todos los bits).
// Determinístico: se calcula de los valores ya decodificados por RCSwitch
// (pulso, bits, código y protocolo), no del buffer crudo. Así una misma señal
// siempre da la misma baud (el buffer live variaba con el micro-timing de la
// lectura, dando dos bauds distintos para el mismo envío).
// baud = 1e6 * bits / (pulso * (suma de factores de cada bit + sync))
int calcularBaud(unsigned long codigo, int bits, int protocolo, int pulso)
{
  if (bits <= 0 || pulso <= 0 || protocolo < 1 || protocolo > 12)
    return 0;

  FactoresProto f;
  memcpy_P(&f, &factoresProto[protocolo - 1], sizeof(FactoresProto));

  unsigned long sumaFactor = f.syncH + f.syncL;   // el sync forma parte de la trama
  for (int i = bits - 1; i >= 0; i--)
  {
    if (codigo & (1UL << i))
      sumaFactor += f.oneH + f.oneL;
    else
      sumaFactor += f.zeroH + f.zeroL;
  }

  unsigned long sumaUs = (unsigned long)pulso * sumaFactor;
  if (sumaUs == 0)
    return 0;                           // evitar división por cero

  return (int)(1000000UL * bits / sumaUs);
}

// -----------------------------------------------------------

void resetearDetector()
{
  indice = 0;
  esperandoReset = false;
  reintentando = false;
  for (int i = 0; i < MAX_CAPTURAS; i++)
    capturas[i] = {0, 0, 0, 0, 0};
  sw433.resetAvailable();
  mostrarEspera();
}

// Comparación tolerante: tolera un bit de diferencia (glitches del receptor).
// - El pulso (µs) debe ser similar: mismo control => mismo ritmo de transmisión.
// - Bits iguales: el código debe coincidir.
// - Bits difieren en 1: coincide si el código es idéntico (glitch al final en
//   protocolos invertidos) o si coincide al descartar el bit espurio (codigo>>1).
bool mismaTrama(Paquete &a, Paquete &b)
{
  if (a.protocolo != b.protocolo)
    return false;

  // Pulso similar (tolerancia 20% + 40us de piso por jitter de la medición)
  long diffPulso = abs((long)a.pulso - b.pulso);
  if (diffPulso > (long)a.pulso * 20 / 100 + 40)
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

// Compatibilidad del protocolo con el receptor RS400TX:
//   2 = confirmado, 1 = probable, 0 = no compatible
int compatibilidadProtocolo(int protocolo)
{
  switch (protocolo)
  {
    case 6:                                  // HT6P20B: confirmado
      return 2;
    case 1: case 2: case 4: case 5:          // PT2262 genérico y otros
    case 11: case 12:                        // HT12E, SM5212: probables
      return 1;
    default:                                 // 3, 7, 8, 9, 10: no
      return 0;
  }
}

void procesarResultado(Paquete &ganador, bool esFijo)
{
  esperandoReset = true;

  // Prioridad en condiciones de carrera: rolling code > bps > protocolo
  if (!esFijo)
  {
    mostrarRollingCode();
    return;
  }

  if (ganador.baud < BAUD_MIN || ganador.baud > BAUD_MAX)
  {
    mostrarNoCompatibleBps(ganador);
    return;
  }

  int compat = compatibilidadProtocolo(ganador.protocolo);
  if (compat == 2)
    mostrarCompatible(ganador);
  else if (compat == 1)
    mostrarProbCompatible(ganador);
  else
    mostrarNoCompatible(ganador);
}

// -----------------------------------------------------------
// Autoapagado por inactividad

// Interrupción de cambio de pin del botón (PD6 = PCINT22): solo se usa para
// despertar el micro desde el sleep. El debounce lo sigue haciendo el loop.
ISR(PCINT2_vect)
{
}

void irASleep()
{
  // No dormir si el botón está presionado
  if (digitalRead(PIN_RESET) == LOW)
    return;

  // Apagar la pantalla OLED
  u8g2.sleepOn();

  // Desactivar la recepción RF: durante el sleep solo despertará el botón
  sw433.disableReceive();

  // Habilitar interrupción por cambio de pin en el botón (grupo PORTD)
  PCMSK2 |= (1 << PCINT22);
  PCICR |= (1 << PCIE2);
  PCIFR |= (1 << PCIF2);   // limpiar flags pendientes

  // Apagar el ADC mientras duerme (ahorra batería)
  ADCSRA &= ~(1 << ADEN);

  // Cerrar carrera: si el botón se apretó mientras se preparaba el sleep,
  // el PCINT ya habría ocurrido y no despertaría. Mejor no dormir.
  if (digitalRead(PIN_RESET) == LOW)
  {
    ADCSRA |= (1 << ADEN);
    PCMSK2 &= ~(1 << PCINT22);
    PCICR &= ~(1 << PCIE2);
    u8g2.sleepOff();
    EIFR |= (1 << INTF0);
    sw433.enableReceive(0);
    return;
  }

  // Desactivar el watchdog durante el sleep (si no, despierta cada 8 s)
  wdt_disable();

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();      // queda dormido hasta que el botón genere la interrupción
  sleep_disable();

  // Despertó: reactivar watchdog y ADC
  wdt_enable(WDTO_8S);
  ADCSRA |= (1 << ADEN);

  // Limpiar interrupción del botón
  PCMSK2 &= ~(1 << PCINT22);
  PCICR &= ~(1 << PCIE2);

  // Reactivar display y recepción
  u8g2.sleepOff();
  EIFR |= (1 << INTF0);   // limpiar flag pendiente de INT0 antes de reactivar
  sw433.enableReceive(0);

  // Reiniciar a la pantalla de espera (re-mide la batería al despertar)
  ultimaActividad = millis();
  ultimaMedicionBateria = 0;
  resetearDetector();
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
  ultimaActividad = millis();
  mostrarEspera();

  // Watchdog: reinicia el micro si el loop se cuelga (>8 s sin wdt_reset)
  wdt_enable(WDTO_8S);
}

void loop()
{
  wdt_reset();

  // Botón reset por flanco de bajada (no bloquea el loop si queda presionado)
  static bool botonPrevio = false;
  bool botonAhora = (digitalRead(PIN_RESET) == LOW);
  if (!botonPrevio && botonAhora)
  {
    delay(50);   // debounce
    if (digitalRead(PIN_RESET) == LOW)
    {
      ultimaActividad = millis();
      resetearDetector();
    }
  }
  botonPrevio = botonAhora;

  // Autoapagado por inactividad: 2 min sin actividad -> sleep
  if (millis() - ultimaActividad > TIMEOUT_SLEEP)
  {
    irASleep();
    return;
  }

  if (esperandoReset)
    return;

  if (sw433.available())
  {
    ultimaActividad = millis();

    unsigned long cod = sw433.getReceivedValue();
    int proto = sw433.getReceivedProtocol();
    int bits = sw433.getReceivedBitlength();
    int pulso = sw433.getReceivedDelay();
    // El baud se calcula de los valores decodificados (snapshots de RCSwitch),
    // no del buffer crudo, que el ISR sigue sobrescribiendo.
    int baud = calcularBaud(cod, bits, proto, pulso);
    sw433.resetAvailable();

    if (cod == 0)
      return;

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

    capturas[indice] = {cod, proto, bits, pulso, baud};
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