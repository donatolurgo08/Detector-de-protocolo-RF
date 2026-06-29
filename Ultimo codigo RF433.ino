#include <U8g2lib.h>
#include <Wire.h>

U8G2_SSD1306_128X64_NONAME_2_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// =====================================================================
// Pines
// =====================================================================
const int PIN_DATA  = 2;
const int PIN_RESET = 6;
const int PIN_VBAT  = A0;

// =====================================================================
// Estado de deteccion
// =====================================================================
int  indice        = 0;
int  bitsDetectados = 0;
bool esperandoReset = false;

// Para rolling code comparamos cantidad de pulsos, no el contenido
// (el contenido cambia cada vez, la cantidad no)
// ACA HAY UN ERROR, se deben comparar el contenido
unsigned int pulsosCapturados[3] = {0, 0, 0};
unsigned int hashesCapturados[3] = {0, 0, 0};


unsigned long ultimoEventoMs            = 0;
const unsigned long UMBRAL_PULSACION_MS = 700;

// =====================================================================
// Captura cruda por ISR en D2/INT0
// Datos reales del control:
//   Trama real:     ~240 pulsos consistentes
//   Ruido/basura:   pulsos muy variables (>1000 o <50)
// =====================================================================
const unsigned long SILENCIO_US  = 15000UL; // >15ms = fin de trama
const unsigned int  PULSOS_MIN   = 50;       // filtra ruido corto
const unsigned int  PULSOS_MAX   = 600;      // filtra rafagas largas de ruido

// Tolerancia para comparar cantidad de pulsos entre pulsaciones
// (permite variacion de +/- TOLERANCIA_PULSOS entre tramas del mismo control)
const unsigned int  TOLERANCIA_PULSOS = 20;

volatile unsigned long rawHashAcc      = 2166136261UL;
volatile unsigned int  rawPulsosAcc    = 0;
volatile unsigned long rawUltimoFlanco = 0;
volatile unsigned long rawHashSnap     = 0;
volatile unsigned int  rawPulsosSnap   = 0;
volatile bool          rawListo        = false;

void ISR_data() {
  unsigned long ahora    = micros();
  unsigned long duracion = ahora - rawUltimoFlanco;
  rawUltimoFlanco = ahora;

  if (duracion > SILENCIO_US) {
    if (rawPulsosAcc >= PULSOS_MIN && rawPulsosAcc <= PULSOS_MAX && !rawListo) {
      rawHashSnap   = rawHashAcc;
      rawPulsosSnap = rawPulsosAcc;
      rawListo      = true;
    }
    rawHashAcc   = 2166136261UL;
    rawPulsosAcc = 0;
    return;
  }

  // Ignora pulsos extremadamente cortos (<10us), son glitches electricos
  if (duracion < 10) return;

  unsigned int bucket = (unsigned int)(duracion / 50);
  rawHashAcc ^= (unsigned long)bucket;
  rawHashAcc *= 16777619UL;
  if (rawPulsosAcc < 60000) rawPulsosAcc++;
}

// =====================================================================
// Bateria
// =====================================================================
int medirBateria() {
  long suma = 0;
  for (int i = 0; i < 8; i++) {
    suma += analogRead(PIN_VBAT);
    delay(1);
  }
  float vPin = (suma / 8.0f) * (5.0f / 1023.0f);
  float vBat = vPin * (14.7f / 4.7f);
  int pct = (int)((vBat - 6.5f) / (8.4f - 6.5f) * 100.0f);
  return constrain(pct, 0, 100);
}

// =====================================================================
// Pantalla
// =====================================================================
void dibujarCabecera(int bat) {
  u8g2.setCursor(0, 8);
  u8g2.print(F("BAT:"));
  u8g2.print(bat);
  u8g2.print('%');

  u8g2.drawFrame(106, 1, 18, 7);
  u8g2.drawBox(124, 3, 2, 3);
  int ancho = constrain((int)(14.0f * bat / 100.0f), 0, 14);
  if (ancho > 0)
    u8g2.drawBox(107, 2, ancho, 5);

  u8g2.drawHLine(0, 11, 128);
}

void mostrarEspera() {
  int bat = medirBateria();
  u8g2.firstPage();
  do {
    dibujarCabecera(bat);
    u8g2.setCursor(0, 23); u8g2.print(F("Apriete el control"));
    u8g2.setCursor(0, 33); u8g2.print(F("del porton 3 veces"));
    u8g2.setCursor(0, 47); u8g2.print(F("Detectados: "));
                           u8g2.print(min(indice, 3));
                           u8g2.print(F("/3"));
    u8g2.setCursor(0, 57); u8g2.print(F("433 MHz activo"));
  } while (u8g2.nextPage());
}

void mostrarCompatible(unsigned int pulsos) {
  int bat = medirBateria();
  u8g2.firstPage();
  do {
    dibujarCabecera(bat);
    u8g2.setCursor(0, 22); u8g2.print(F("** COMPATIBLE **"));
    u8g2.drawHLine(0, 24, 128);
    u8g2.setCursor(0, 34); u8g2.print(F("Freq: 433.92 MHz"));
    u8g2.setCursor(0, 43); u8g2.print(F("Tipo: Cod. Fijo"));
    u8g2.setCursor(0, 52); u8g2.print(F("Bits: ")); u8g2.print(pulsos);
    u8g2.setCursor(0, 61); u8g2.print(F("Clonable: SI"));
  } while (u8g2.nextPage());
}

void mostrarIncompatible(unsigned int pulsos) {
  int bat = medirBateria();
  u8g2.firstPage();
  do {
    dibujarCabecera(bat);
    u8g2.setCursor(0, 22); u8g2.print(F("NO COMPATIBLE"));
    u8g2.drawHLine(0, 24, 128);
    u8g2.setCursor(0, 34); u8g2.print(F("Rolling Code"));
    u8g2.setCursor(0, 43); u8g2.print(F("Freq: 433.92 MHz"));
    u8g2.setCursor(0, 52); u8g2.print(F("Bits: ")); u8g2.print(pulsos);
    u8g2.setCursor(0, 61); u8g2.print(F("Clonable: NO"));
  } while (u8g2.nextPage());
}

// =====================================================================
// Logica principal
// =====================================================================
void resetearDetector() {
  indice         = 0;
  bitsDetectados = 0;
  esperandoReset = false;
  pulsosCapturados[0] = pulsosCapturados[1] = pulsosCapturados[2] = 0;
  hashesCapturados[0] = hashesCapturados[1] = hashesCapturados[2] = 0;

  noInterrupts();
  rawHashAcc   = 2166136261UL;
  rawPulsosAcc = 0;
  rawListo     = false;
  interrupts();

  ultimoEventoMs = 0;

  Serial.println(F("--- RESET ---"));
  mostrarEspera();
}

// Compara si dos cantidades de pulsos son similares (dentro de tolerancia)
bool pulsosSimilares(unsigned int a, unsigned int b) {
  int diff = (int)a - (int)b;
  if (diff < 0) diff = -diff;
  return diff <= (int)TOLERANCIA_PULSOS;
}

void registrarDeteccion(unsigned long huella, unsigned int pulsos) {
  unsigned long ahora = millis();

  if ((ahora - ultimoEventoMs) < UMBRAL_PULSACION_MS) return;
  ultimoEventoMs = ahora;

  pulsosCapturados[indice % 3] = pulsos;
  hashesCapturados[indice % 3] = huella;
  indice++;

  Serial.print(F("Pulsacion #")); Serial.print(indice);
  Serial.print(F("  huella="));   Serial.print(huella);
  Serial.print(F("  pulsos="));   Serial.println(pulsos);
  Serial.print("Pulsos capturados: ");
  Serial.println(pulsos);

  Serial.print("Hash: ");
  Serial.println(huella);

  if (indice < 3) {
    mostrarEspera();
    return;
  }

  // compara pulsos Y hashes
  bool pulsosOK = pulsosSimilares(pulsosCapturados[0], pulsosCapturados[1]) &&
                  pulsosSimilares(pulsosCapturados[1], pulsosCapturados[2]);

  if (!pulsosOK) return; // ruido, ignorar sin cambiar pantalla

  bool hashesIguales = (hashesCapturados[0] == hashesCapturados[1]) &&
                      (hashesCapturados[1] == hashesCapturados[2]);

  esperandoReset = true;
  bitsDetectados = pulsos;

  if (hashesIguales)
      mostrarCompatible(pulsos);
  else
      mostrarIncompatible(pulsos);
}

// =====================================================================
// Setup
// =====================================================================
void setup() {
  Serial.begin(9600);
  pinMode(PIN_RESET, INPUT_PULLUP);
  pinMode(PIN_DATA, INPUT);

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);

  u8g2.firstPage();
  do {
    u8g2.setCursor(10, 20); u8g2.print(F("DETECTOR RF 433"));
    u8g2.setCursor(15, 35); u8g2.print(F("v10.0 - Listo"));
    u8g2.setCursor(5,  50); u8g2.print(F("DATA2 -> D2"));
  } while (u8g2.nextPage());
  delay(2500);

  attachInterrupt(digitalPinToInterrupt(PIN_DATA), ISR_data, CHANGE);

  mostrarEspera();
  Serial.println(F("Listo. Aprieta el control 3 veces."));
}

// =====================================================================
// Loop
// =====================================================================
void loop() {
  // Boton RESET con debounce
  if (digitalRead(PIN_RESET) == LOW) {
    delay(50);
    if (digitalRead(PIN_RESET) == LOW) {
      resetearDetector();
      while (digitalRead(PIN_RESET) == LOW);
      delay(50);
    }
  }

  if (esperandoReset) return;

  if (rawListo) {
    noInterrupts();
    unsigned long huella = rawHashSnap;
    unsigned int  pulsos = rawPulsosSnap;
    rawListo = false;
    interrupts();

    Serial.print(F("[ISR] huella="));
    Serial.print(huella);
    Serial.print(F("  pulsos="));
    Serial.println(pulsos);

    registrarDeteccion(huella, pulsos);
  }
}
