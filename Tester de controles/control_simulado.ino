// control_simulado.ino
// ESP32 + modulo TX 433MHz: simula un control remoto normal para pruebas.
// Objetivo: ensenarle codigos al TXCar desde el ESP32 y descartar problemas
// del control real. Aislado de transmitter.ino (generador de pruebas) y del
// producto (detector_de_protocolo_rf).
//
// Simula controles HT6P20 (protocolo 6 de RCSwitch, 28 bits, invertido).
// La salida imita un control normal: varias tramas seguidas por pulsacion
// (setRepeatTransmit) y pulso configurable.
//
// Hardware:
//   ESP32 DevKit, DATA del modulo transmisor -> GPIO4 (constante PIN_TX)
//   Boton: un terminal a GPIO18 (PIN_BOTON) y el otro a GND.
//
// Comandos por Serial (a 9600):
//   a..g   preset de codigo (fija el codigo a enviar)
//   p <us> pulso en microsegundos (default 342, como el control real)
//   r <n>  repeticiones internas por envio (default 10, como control real)
//   x      envia 1 pulsacion (n tramas seguidas)
//   z <s>  transmite en continuo durante <s> segundos (para ensenar al TXCar)
//   m      muestra este menu
//   BOTON fisico (GPIO18-GND): cada pulsacion envia 1 pulsacion
//
// Presets (proto 6, 28 bits, terminan en 0101):
//   a   51252245  (control real que falla en TXCar)    18 ceros / 10 unos
//   b   123594517 (control real que funciona en TXCar) 11 ceros / 17 unos
//   c   58918405  (sintetico, muchos ceros)            19 ceros / 9 unos
//   d   89478485  (sintetico, alternado puro)          14 ceros / 14 unos
//   e   209517045 (sintetico, muchos unos)             9 ceros / 19 unos
//   f   99438357  (534 bps, para probar el porton)     12 ceros / 16 unos
//   g   136178533 (638 bps, para probar el porton)     13 ceros / 15 unos

#include <RCSwitch.h>

const int PIN_TX = 4;
const int PIN_BOTON = 18;

const int PROTO = 6;
const int BITS = 28;

struct Preset
{
  const char *nombre;
  unsigned long codigo;
};

static const Preset presets[] = {
  { "a", 51252245UL },
  { "b", 123594517UL },
  { "c", 58918405UL },
  { "d", 89478485UL },
  { "e", 209517045UL },
  { "f", 99438357UL },
  { "g", 136178533UL },
};
const int CANT_PRESETS = sizeof(presets) / sizeof(presets[0]);

// Pulso en us para que el preset "f" (99438357) transmita a ~534 bps.
const int PULSO_PRESET_F = 486;

// Pulso en us para que el preset "g" (136178533) transmita a ~638 bps.
const int PULSO_PRESET_G = 406;

RCSwitch sw433 = RCSwitch();

unsigned long codigoActual = 51252245UL;
int pulsoUs = 342;
int repeticiones = 10;

// -----------------------------------------------------------

void mostrarMenu()
{
  Serial.println();
  Serial.println(F("==== SIMULADOR DE CONTROL HT6P20 (ESP32) ===="));
  Serial.println(F("a..g   preset de codigo (proto6, 28 bits)"));
  Serial.println(F("p <us> pulso en microsegundos (default 342)"));
  Serial.println(F("r <n>  repeticiones por envio (default 10)"));
  Serial.println(F("x      envia 1 pulsacion"));
  Serial.println(F("z <s>  transmite en continuo <s> segundos"));
  Serial.println(F("m      este menu"));
  Serial.println(F("BOTON (GPIO18-GND): cada pulsacion envia 1 envio"));
  Serial.println();
  Serial.print(F("Actual -> codigo:"));
  Serial.print(codigoActual);
  Serial.print(F(" proto:"));
  Serial.print(PROTO);
  Serial.print(F(" bits:"));
  Serial.print(BITS);
  Serial.print(F(" pulso:"));
  Serial.print(pulsoUs);
  Serial.print(F("us repeticiones:"));
  Serial.println(repeticiones);
  Serial.println();
}

// Configura protocolo 6 con el pulso actual y envia 1 pulsacion completa.
void enviarPulsacion()
{
  sw433.setProtocol(PROTO, pulsoUs);
  sw433.setRepeatTransmit(repeticiones);
  sw433.send(codigoActual, BITS);

  Serial.print(F("Enviado -> codigo:"));
  Serial.print(codigoActual);
  Serial.print(F(" proto:"));
  Serial.print(PROTO);
  Serial.print(F(" bits:"));
  Serial.print(BITS);
  Serial.print(F(" pulso:"));
  Serial.print(pulsoUs);
  Serial.print(F("us x"));
  Serial.println(repeticiones);
}

void seleccionarPreset(const char *nombre)
{
  for (int i = 0; i < CANT_PRESETS; i++)
  {
    if (strcmp(nombre, presets[i].nombre) == 0)
    {
      codigoActual = presets[i].codigo;

      // Los presets "f" (534 bps) y "g" (638 bps) fijan tambien el pulso
      // para representar el control completo con una sola orden.
      if (strcmp(nombre, "f") == 0)
        pulsoUs = PULSO_PRESET_F;
      if (strcmp(nombre, "g") == 0)
        pulsoUs = PULSO_PRESET_G;

      Serial.print(F("Preset '"));
      Serial.print(nombre);
      Serial.print(F("' -> codigo:"));
      Serial.print(codigoActual);
      Serial.print(F(" proto:"));
      Serial.print(PROTO);
      Serial.print(F(" bits:"));
      Serial.print(BITS);
      Serial.print(F(" pulso:"));
      Serial.print(pulsoUs);
      Serial.print(F("us"));
      if (strcmp(nombre, "f") == 0)
        Serial.print(F(" (~534 bps)"));
      if (strcmp(nombre, "g") == 0)
        Serial.print(F(" (~638 bps)"));
      Serial.println();
      return;
    }
  }
  Serial.println(F("Preset desconocido. Escriba 'm' para el menu."));
}

// Transmite en continuo durante `segundos` segundos: repite pulsaciones con
// pausa corta, como si se mantuviera apretado el boton del control. Sirve
// para ensenarle al TXCar y para verificar captura en el detector.
void enviarContinuo(int segundos)
{
  unsigned long fin = millis() + (unsigned long)segundos * 1000UL;
  Serial.print(F("Transmitiendo continuo por "));
  Serial.print(segundos);
  Serial.println(F(" s..."));
  while (millis() < fin)
  {
    enviarPulsacion();
    delay(50);
  }
  Serial.println(F("Continuo finalizado."));
}

// Lee el boton con debounce (50ms) y por flanco de bajada: cada pulsacion
// envia 1 pulsacion completa del codigo configurado.
void atenderBoton()
{
  static bool botonPrevio = true;
  bool botonAhora = (digitalRead(PIN_BOTON) == LOW);

  if (botonPrevio && !botonAhora)
  {
    delay(50);
    if (digitalRead(PIN_BOTON) == LOW)
    {
      Serial.println(F("[BOTON] Enviando 1 pulsacion..."));
      enviarPulsacion();
    }
  }
  botonPrevio = botonAhora;
}

// -----------------------------------------------------------

void setup()
{
  Serial.begin(9600);
  delay(500);

  pinMode(PIN_TX, OUTPUT);
  digitalWrite(PIN_TX, LOW);

  pinMode(PIN_BOTON, INPUT_PULLUP);

  sw433.enableTransmit(PIN_TX);

  Serial.println(F("Simulador de control HT6P20 listo."));
  mostrarMenu();
}

// Procesa una linea de comando recibida por serial (ya completa, con Enter).
void procesarComando(char *linea)
{
  char *cmd = strtok(linea, " \t\r\n");
  if (cmd == NULL)
    return;

  char c = cmd[0];

  if (c == 'm' || c == '?' || c == 'h' || strcmp(cmd, "help") == 0 ||
      strcmp(cmd, "menu") == 0)
  {
    mostrarMenu();
    return;
  }

  if (c == 'a' || c == 'b' || c == 'c' || c == 'd' || c == 'e' || c == 'f' || c == 'g')
  {
    char preset[2] = { c, '\0' };
    seleccionarPreset(preset);
    return;
  }

  if (c == 'x')
  {
    Serial.println(F("[COMANDO] Enviando 1 pulsacion..."));
    enviarPulsacion();
    return;
  }

  if (c == 'z')
  {
    char *sseg = strtok(NULL, " \t\r\n");
    int segundos = (sseg != NULL) ? atoi(sseg) : 3;
    if (segundos < 1)
      segundos = 1;
    enviarContinuo(segundos);
    return;
  }

  if (c == 'p')
  {
    char *spulso = strtok(NULL, " \t\r\n");
    int nuevo = (spulso != NULL) ? atoi(spulso) : 0;
    if (nuevo > 0)
    {
      pulsoUs = nuevo;
      Serial.print(F("Pulso -> "));
      Serial.print(pulsoUs);
      Serial.println(F("us"));
    }
    else
    {
      Serial.println(F("Uso: p <us>  (ej: p 342)"));
    }
    return;
  }

  if (c == 'r')
  {
    char *srep = strtok(NULL, " \t\r\n");
    int nuevo = (srep != NULL) ? atoi(srep) : 0;
    if (nuevo >= 1)
    {
      repeticiones = nuevo;
      Serial.print(F("Repeticiones -> "));
      Serial.println(repeticiones);
    }
    else
    {
      Serial.println(F("Uso: r <n>  (ej: r 10)"));
    }
    return;
  }

  Serial.print(F("Comando desconocido: '"));
  Serial.print(cmd);
  Serial.println(F("'. Escriba 'm' para el menu."));
}

void loop()
{
  atenderBoton();

  static char linea[32];
  static int n = 0;
  while (Serial.available())
  {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r')
    {
      if (n > 0)
      {
        linea[n] = '\0';
        procesarComando(linea);
        n = 0;
      }
    }
    else if (n < (int)sizeof(linea) - 1)
    {
      linea[n++] = c;
    }
  }
}