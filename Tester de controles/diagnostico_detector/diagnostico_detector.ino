// diagnostico_detector.ino
// Detector de protocolo RF en MODO DIAGNOSTICO DE RAFAGA.
// Copia de seguridad del firmware del producto (detector_de_protocolo_rf) con
// el modo de diagnostico por Serial que imprime una rafaga completa de tramas:
//   - ventana de 700 ms desde la primera trama
//   - hasta 8 tramas guardadas (limite de SRAM del Nano)
//   - por cada trama: pulso, bps, protocolo, bits, codigo + metricas raw
//   - gap entre tramas, sync, promedios de simbolos y la primera trama cruda
//
// Sirve para comparar la senal de un control real contra la del ESP32 (Tester
// de controles) y contra lo que reproduce el TXCar.
//
// Hardware (igual que el producto):
//   Arduino Nano, RX 433MHz DATA -> D2 (INT0), boton reset -> D6 (INPUT_PULLUP)
//   OLED 128x64 por I2C (A4/A5), Serial a 9600.
//
// Uso:
//   1. Subir este sketch al Nano.
//   2. Abrir el monitor serial a 9600.
//   3. Apretar el boton (o resetear) para iniciar una captura.
//   4. Accionar el control/ESP/TXCar una vez.
//   5. Al cerrar la ventana de 700 ms imprime el resumen y queda esperando
//      reset (boton) para la proxima captura.

#include <RCSwitch.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "logo_bitech.h"

// U8g2 en modo pagina de 128 bytes (vs 1024 de Adafruit)
U8G2_SSD1306_128X64_NONAME_2_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

RCSwitch sw433 = RCSwitch();

const int PIN_RESET = 6;

// ---- Constantes de deteccion ----
const int MAX_CAPTURAS = 5;     // maximo de capturas antes de decidir
const int MIN_IGUALES = 3;      // minimo de tramas iguales para dar COMPATIBLE
const int BAUD_MIN = 400;       // ventana de bitrate soportada (baud)
const int BAUD_MAX = 800;
const int RECEIVE_TOLERANCE = 50;  // % de tolerancia de RCSwitch (default 60)
const unsigned long TIMEOUT_SLEEP = 120000UL;  // autoapagado por inactividad (2 min)
const bool MODO_DIAGNOSTICO_RAFAGA = true;     // imprime por Serial una rafaga completa
const unsigned long VENTANA_RAFAGA_MS = 700;   // tiempo de captura desde la primera trama
const int MAX_TRAMAS_RAFAGA = 8;               // limite para cuidar SRAM del Nano

// ---- Struct de paquete ----
struct Paquete
{
  unsigned long codigo;
  int protocolo;
  int bits;
  int pulso;
  int baud;
};

struct MetricasRaw
{
  unsigned int cambios;
  unsigned long duracionUs;
  unsigned int syncA;
  unsigned int syncB;
  unsigned int ceroA;
  unsigned int ceroB;
  unsigned int unoA;
  unsigned int unoB;
  uint8_t cantCero;
  uint8_t cantUno;
};

struct TramaRafaga
{
  unsigned int tMs;
  Paquete paquete;
  MetricasRaw metricas;
};

Paquete capturas[MAX_CAPTURAS];
int indice = 0;
bool esperandoReset = false;
bool reintentando = false;
unsigned long ultimaActividad = 0;
unsigned int rawDiagnostico[RCSWITCH_MAX_CHANGES];
int rawDiagnosticoCant = 0;
unsigned int rawPrimeraTrama[RCSWITCH_MAX_CHANGES];
int rawPrimeraTramaCant = 0;
TramaRafaga tramasRafaga[MAX_TRAMAS_RAFAGA];
int cantTramasRafaga = 0;
int totalTramasRafaga = 0;
bool rafagaActiva = false;
unsigned long inicioRafaga = 0;

// -----------------------------------------------------------

// Factores de pulso por protocolo RCSwitch (sync, zero, one), en PROGMEM.
// La duracion de la trama se deriva de estos y del pulso medido, sin depender
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
    return 0;                           // evitar division por cero

  return (int)(1000000UL * bits / sumaUs);
}

bool protocoloInvertido(int protocolo)
{
  return protocolo == 6 || protocolo == 9 || protocolo == 10 ||
         protocolo == 11 || protocolo == 12;
}

int cantidadRawEsperada(int bits, int protocolo)
{
  int cantidad = bits * 2 + (protocoloInvertido(protocolo) ? 2 : 1);
  if (cantidad < 0)
    return 0;
  if (cantidad > RCSWITCH_MAX_CHANGES)
    return RCSWITCH_MAX_CHANGES;
  return cantidad;
}

void copiarRawDiagnostico(int cantidad)
{
  unsigned int *raw = sw433.getReceivedRawdata();
  rawDiagnosticoCant = cantidad;
  for (int i = 0; i < rawDiagnosticoCant; i++)
    rawDiagnostico[i] = raw[i];
}

void calcularMetricasRaw(Paquete &p, MetricasRaw &m)
{
  unsigned long ceroA = 0;
  unsigned long ceroB = 0;
  unsigned long unoA = 0;
  unsigned long unoB = 0;
  int cantCero = 0;
  int cantUno = 0;
  bool invertido = protocoloInvertido(p.protocolo);
  int inicioDatos = invertido ? 2 : 1;
  unsigned long duracionTotal = 0;

  for (int i = 0; i < rawDiagnosticoCant; i++)
    duracionTotal += rawDiagnostico[i];

  for (int i = 0; i < p.bits; i++)
  {
    int pos = inicioDatos + i * 2;
    if (pos + 1 >= rawDiagnosticoCant)
      break;

    int bitPos = p.bits - 1 - i;
    bool bitUno = bitPos >= 0 && bitPos < 32 && (p.codigo & (1UL << bitPos));
    if (bitUno)
    {
      unoA += rawDiagnostico[pos];
      unoB += rawDiagnostico[pos + 1];
      cantUno++;
    }
    else
    {
      ceroA += rawDiagnostico[pos];
      ceroB += rawDiagnostico[pos + 1];
      cantCero++;
    }
  }

  m.cambios = rawDiagnosticoCant;
  m.duracionUs = duracionTotal;
  m.syncA = rawDiagnosticoCant > 0 ? rawDiagnostico[0] : 0;
  m.syncB = rawDiagnosticoCant > 1 ? rawDiagnostico[1] : 0;
  m.ceroA = cantCero > 0 ? ceroA / cantCero : 0;
  m.ceroB = cantCero > 0 ? ceroB / cantCero : 0;
  m.unoA = cantUno > 0 ? unoA / cantUno : 0;
  m.unoB = cantUno > 0 ? unoB / cantUno : 0;
  m.cantCero = cantCero;
  m.cantUno = cantUno;
}

void resetearRafaga()
{
  cantTramasRafaga = 0;
  totalTramasRafaga = 0;
  rafagaActiva = false;
  inicioRafaga = 0;
  rawPrimeraTramaCant = 0;
}

void registrarTramaRafaga(Paquete &p)
{
  unsigned long ahora = millis();
  if (!rafagaActiva)
  {
    rafagaActiva = true;
    inicioRafaga = ahora;
    cantTramasRafaga = 0;
    totalTramasRafaga = 0;
  }

  totalTramasRafaga++;
  if (cantTramasRafaga >= MAX_TRAMAS_RAFAGA)
    return;

  if (cantTramasRafaga == 0)
  {
    rawPrimeraTramaCant = rawDiagnosticoCant;
    for (int i = 0; i < rawPrimeraTramaCant; i++)
      rawPrimeraTrama[i] = rawDiagnostico[i];
  }

  TramaRafaga &t = tramasRafaga[cantTramasRafaga];
  t.tMs = (unsigned int)(ahora - inicioRafaga);
  t.paquete = p;
  calcularMetricasRaw(p, t.metricas);
  cantTramasRafaga++;
}

void imprimirTramaRafaga(TramaRafaga &t, int numero)
{
  Paquete &p = t.paquete;
  MetricasRaw &m = t.metricas;
  bool invertido = protocoloInvertido(p.protocolo);

  Serial.print(F("#"));
  Serial.print(numero);
  Serial.print(F(" t_ms:"));
  Serial.print(t.tMs);
  Serial.print(F(" pulsos:"));
  Serial.print(p.pulso);
  Serial.print(F(" bps:"));
  Serial.print(p.baud);
  Serial.print(F(" protocolo:"));
  Serial.print(p.protocolo);
  Serial.print(F(" bits:"));
  Serial.print(p.bits);
  Serial.print(F(" codigo:"));
  Serial.println(p.codigo);

  Serial.print(F("raw_cambios:"));
  Serial.print(m.cambios);
  Serial.print(F(" duracion_raw_us:"));
  Serial.println(m.duracionUs);

  if (invertido)
  {
    Serial.print(F("sync_low_us:"));
    Serial.print(m.syncA);
    Serial.print(F(" sync_high_us:"));
    Serial.println(m.syncB);
  }
  else
  {
    Serial.print(F("sync_us:"));
    Serial.println(m.syncA);
  }

  Serial.print(F("senal_invertida:"));
  Serial.print(invertido ? 1 : 0);
  Serial.print(F(" inicio_datos_raw:"));
  Serial.println(invertido ? 2 : 1);

  Serial.print(F("cero_prom_a_us:"));
  Serial.print(m.ceroA);
  Serial.print(F(" cero_prom_b_us:"));
  Serial.print(m.ceroB);
  Serial.print(F(" cero_cant:"));
  Serial.println(m.cantCero);

  Serial.print(F("uno_prom_a_us:"));
  Serial.print(m.unoA);
  Serial.print(F(" uno_prom_b_us:"));
  Serial.print(m.unoB);
  Serial.print(F(" uno_cant:"));
  Serial.println(m.cantUno);
}

void imprimirResumenRafaga()
{
  Serial.println(F("---- DIAGNOSTICO RAFAGA ----"));
  Serial.print(F("ventana_ms:"));
  Serial.print(VENTANA_RAFAGA_MS);
  Serial.print(F(" tramas_guardadas:"));
  Serial.print(cantTramasRafaga);
  Serial.print(F(" tramas_total:"));
  Serial.println(totalTramasRafaga);

  for (int i = 0; i < cantTramasRafaga; i++)
  {
    imprimirTramaRafaga(tramasRafaga[i], i + 1);
    if (i > 0)
    {
      Serial.print(F("gap_desde_anterior_ms:"));
      Serial.println((int)tramasRafaga[i].tMs - (int)tramasRafaga[i - 1].tMs);
    }
  }

  Serial.print(F("raw_primera_trama_us:"));
  for (int i = 0; i < rawPrimeraTramaCant; i++)
  {
    if (i > 0)
      Serial.print(',');
    Serial.print(rawPrimeraTrama[i]);
  }
  Serial.println();

  Serial.println(F("---- FIN RAFAGA ----"));
}

// -----------------------------------------------------------

void resetearDetector()
{
  indice = 0;
  esperandoReset = false;
  reintentando = false;
  resetearRafaga();
  for (int i = 0; i < MAX_CAPTURAS; i++)
    capturas[i] = {0, 0, 0, 0, 0};
  sw433.resetAvailable();
  mostrarEspera();
}

// Comparacion tolerante: tolera un bit de diferencia (glitches del receptor).
bool mismaTrama(Paquete &a, Paquete &b)
{
  if (a.protocolo != b.protocolo)
    return false;

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

// Busca la trama que se repite al menos MIN_IGUALES veces (mayoria de votos).
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
    case 1: case 2: case 4: case 5:          // PT2262 generico y otros
    case 11: case 12:                        // HT12E, SM5212: probables
      return 1;
    default:                                 // 3, 7, 8, 9, 10: no
      return 0;
  }
}

void procesarResultado(Paquete &ganador, bool esFijo)
{
  esperandoReset = true;

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

// Interrupcion de cambio de pin del boton (PD6 = PCINT22): solo se usa para
// despertar el micro desde el sleep. El debounce lo sigue haciendo el loop.
ISR(PCINT2_vect)
{
}

void irASleep()
{
  if (digitalRead(PIN_RESET) == LOW)
    return;

  u8g2.sleepOn();
  sw433.disableReceive();

  PCMSK2 |= (1 << PCINT22);
  PCICR |= (1 << PCIE2);
  PCIFR |= (1 << PCIF2);

  ADCSRA &= ~(1 << ADEN);

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

  wdt_disable();

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
  sleep_disable();

  wdt_enable(WDTO_8S);
  ADCSRA |= (1 << ADEN);

  PCMSK2 &= ~(1 << PCINT22);
  PCICR &= ~(1 << PCIE2);

  u8g2.sleepOff();
  EIFR |= (1 << INTF0);
  sw433.enableReceive(0);

  ultimaActividad = millis();
  resetearDetector();
}

// -----------------------------------------------------------

void setup()
{
  Serial.begin(9600);
  pinMode(PIN_RESET, INPUT_PULLUP);

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.firstPage();
  do
  {
    u8g2.drawXBMP(0, 0, LOGO_W, LOGO_H, logo_bitech);
  } while (u8g2.nextPage());

  delay(1300);

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

  delay(1300);
  u8g2.setFont(u8g2_font_6x10_tf);
  sw433.setReceiveTolerance(RECEIVE_TOLERANCE);
  sw433.enableReceive(0);
  ultimaActividad = millis();
  mostrarEspera();

  wdt_enable(WDTO_8S);
}

void loop()
{
  wdt_reset();

  static bool botonPrevio = false;
  bool botonAhora = (digitalRead(PIN_RESET) == LOW);
  if (!botonPrevio && botonAhora)
  {
    delay(50);
    if (digitalRead(PIN_RESET) == LOW)
    {
      ultimaActividad = millis();
      resetearDetector();
    }
  }
  botonPrevio = botonAhora;

  if (millis() - ultimaActividad > TIMEOUT_SLEEP)
  {
    irASleep();
    return;
  }

  if (esperandoReset)
    return;

  if (MODO_DIAGNOSTICO_RAFAGA && rafagaActiva &&
      millis() - inicioRafaga >= VENTANA_RAFAGA_MS)
  {
    imprimirResumenRafaga();
    esperandoReset = true;
    return;
  }

  if (sw433.available())
  {
    ultimaActividad = millis();

    unsigned long cod = sw433.getReceivedValue();
    int proto = sw433.getReceivedProtocol();
    int bits = sw433.getReceivedBitlength();
    int pulso = sw433.getReceivedDelay();
    int baud = calcularBaud(cod, bits, proto, pulso);
    copiarRawDiagnostico(cantidadRawEsperada(bits, proto));
    sw433.resetAvailable();

    if (cod == 0)
      return;

    Paquete paqueteActual = {cod, proto, bits, pulso, baud};

    if (MODO_DIAGNOSTICO_RAFAGA)
    {
      registrarTramaRafaga(paqueteActual);
      return;
    }

    capturas[indice] = paqueteActual;
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