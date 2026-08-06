// transmitter.ino
// Generador de pruebas RF 433MHz para ESP32 + modulo transmisor.
//
// Envia frames "a pedido" por Serial USB para probar el detector de
// protocolo RF (detector_de_protocolo_rf). Soporta protocolos, codigos,
// cantidad de bits y pulso configurables, mas presets de codigo fijo,
// rolling code y bitrate fuera de rango.
//
// Hardware:
//   ESP32 DevKit, DATA del modulo transmisor -> GPIO4 (constante PIN_TX)
//   Modulo 433MHz (mismo canal que el detector)
//   Boton: un terminal a GPIO18 (PIN_BOTON) y el otro a GND.
//          Usa la resistencia pull-up interna, no hace falta resistencias.
//          Cada pulsacion envia 1 frame (como apretar 1 vez el control).
//
// Comandos por Serial (a 9600):
//   1..12    preset codigo fijo con ese protocolo RCSwitch (COMPATIBLE)
//           (8 y 9 son muy lentos -> el detector mostrara BITRATE NO
//            SOPORTADO, es el resultado esperado)
//   r        rolling code (NO COMPATIBLE)
//   l|f      bitrate fuera de rango (lento/rapido)
//   e        envia 1 frame del ultimo codigo
//   x        repite el ultimo frame 3 veces
//   m        muestra este menu
//   send <codigo> <bits> <proto> [pulso]   envia un frame (opcional)
//   rep <n>                                repite el ultimo frame n veces
//
// Notas sobre el detector:
//   - Solo cuenta tramas de 400 a 800 baud; fuera -> BITRATE NO SOPORTADO.
//   - El baud se mide sobre el frame completo (sync + bits): el pulso por
//     defecto de cada protocolo ya cae dentro del rango para los presets.
//   - 3 frames iguales -> COMPATIBLE; si varian -> NO COMPATIBLE (rolling).

#include <RCSwitch.h>

const int PIN_TX = 4;
const int PIN_BOTON = 18;

RCSwitch sw433 = RCSwitch();

// ---- Estado ----
unsigned long ultimoCodigo = 0;
int ultimoBits = 24;
int ultimoProto = 1;
int ultimoPulso = 0;   // 0 = usar el del protocolo

// -----------------------------------------------------------

void mostrarMenu()
{
  Serial.println();
  Serial.println(F("==== GENERADOR RF 433 MHz (ESP32) ===="));
  Serial.println(F("1..12 preset codigo fijo del protocolo"));
  Serial.println(F("       (8 y 9: BITRATE NO SOPORTADO esperado)"));
  Serial.println(F("r     rolling code (NO COMPATIBLE)"));
  Serial.println(F("l|f   bitrate fuera de rango (lento|rapido)"));
  Serial.println(F("e     envia 1 frame del ultimo codigo"));
  Serial.println(F("x     repite el ultimo frame 3 veces"));
  Serial.println(F("m     este menu"));
  Serial.println(F("send <cod> <bits> <proto> [pulso]  envia a mano"));
  Serial.println(F("rep <n>                           repite n veces"));
  Serial.println(F("BOTON fisico (GPIO18-GND): cada pulsacion envia 1 frame"));
  Serial.println();
}

// Envia un frame por RCSwitch y deja la linea en reposo.
// rc-switch 2.x: send() ya no recibe protocolo ni pulso; se configuran
// con setProtocol()/setPulseLength() antes de enviar.
void enviarFrame(unsigned long codigo, int bits, int proto, int pulso)
{
  if (proto >= 1 && proto <= 6)
  {
    if (pulso > 0)
      sw433.setProtocol(proto, pulso);   // protocolo con pulso custom (us)
    else
      sw433.setProtocol(proto);          // pulso por defecto del protocolo
  }

  sw433.send(codigo, (unsigned int)bits);

  digitalWrite(PIN_TX, LOW);   // send() ya la deja LOW; queda como refuerzo

  Serial.print(F("Enviado -> cod:"));
  Serial.print(codigo);
  Serial.print(F(" bits:"));
  Serial.print(bits);
  Serial.print(F(" proto:"));
  Serial.print(proto);
  if (pulso > 0)
  {
    Serial.print(F(" pulso:"));
    Serial.print(pulso);
    Serial.print(F("us"));
  }
  Serial.println();

  ultimoCodigo = codigo;
  ultimoBits = bits;
  ultimoProto = proto;
  ultimoPulso = pulso;
}

// Repite el ultimo frame n veces con pausa, simulando "apretar el control".
void repetirUltimo(int n)
{
  for (int i = 0; i < n; i++)
  {
    Serial.print(F("["));
    Serial.print(i + 1);
    Serial.print(F("/"));
    Serial.print(n);
    Serial.print(F("] "));
    enviarFrame(ultimoCodigo, ultimoBits, ultimoProto, ultimoPulso);
    delay(100);   // pausa entre envios: el detector cuenta capturas separadas
  }
}

// Lee el boton con debounce (50ms) y por flanco de bajada: cada pulsacion
// envia el ultimo frame 1 sola vez, como si se apretara el control.
void atenderBoton()
{
  static bool botonPrevio = true;   // arranca en HIGH (pull-up interno)
  bool botonAhora = (digitalRead(PIN_BOTON) == LOW);

  if (botonPrevio && !botonAhora)   // flanco de bajada
  {
    delay(50);                       // debounce
    if (digitalRead(PIN_BOTON) == LOW)
    {
      Serial.println(F("[BOTON] Enviando 1 frame..."));
      enviarFrame(ultimoCodigo, ultimoBits, ultimoProto, ultimoPulso);
    }
  }
  botonPrevio = botonAhora;
}

// -----------------------------------------------------------

void ejecutarPreset(const char *nombre)
{
  if (strcmp(nombre, "p1") == 0)
  {
    // proto1, 24 bits, pulso 350us (por defecto), ~535 baud -> COMPATIBLE
    ultimoCodigo = 0x00B4B55;
    ultimoBits = 24;
    ultimoProto = 1;
    ultimoPulso = 0;
    repetirUltimo(3);
  }
  else if (strcmp(nombre, "p2") == 0)
  {
    // proto2, pulso 525us (custom), 24 bits, ~551 baud -> COMPATIBLE.
    // Con el pulso por defecto (650us) daba ~444 baud, bajo el minimo.
    ultimoCodigo = 0x00112233;
    ultimoBits = 24;
    ultimoProto = 2;
    ultimoPulso = 525;
    repetirUltimo(3);
  }
  else if (strcmp(nombre, "p3") == 0)
  {
    // proto3, 32 bits, ~551 baud -> COMPATIBLE
    ultimoCodigo = 0x0A0A0A;
    ultimoBits = 32;
    ultimoProto = 3;
    ultimoPulso = 0;
    repetirUltimo(3);
  }
  else if (strcmp(nombre, "p4") == 0)
  {
    // proto4: limitacion de RCSwitch TX<->RX. El sync bajo (6*380=2280us)
    // es menor al nSeparationLimit (4300us) del receptor, asi que nunca
    // decodifica cuando se transmite con la misma libreria. Se deja el
    // preset por si algun modulo lo tolera, pero puede no recibirse.
    Serial.println(F("proto4: probablemente NO lo reciba el detector"));
    ultimoCodigo = 0x00554433;
    ultimoBits = 24;
    ultimoProto = 4;
    ultimoPulso = 0;
    repetirUltimo(3);
  }
  else if (strcmp(nombre, "p5") == 0)
  {
    // proto5, 24 bits, ~521 baud -> COMPATIBLE.
    // Con 12 bits daba ~428 baud, bajo el minimo.
    ultimoCodigo = 0x00ABC;
    ultimoBits = 24;
    ultimoProto = 5;
    ultimoPulso = 0;
    repetirUltimo(3);
  }
  else if (strcmp(nombre, "p6") == 0)
  {
    // proto6, 24 bits, ~555 baud -> COMPATIBLE
    ultimoCodigo = 0x00DEADBE;
    ultimoBits = 24;
    ultimoProto = 6;
    ultimoPulso = 0;
    repetirUltimo(3);
  }
  else if (strcmp(nombre, "p7") == 0)
  {
    // proto7 (HS2303-PT), 24 bits, ~690 baud -> COMPATIBLE
    ultimoCodigo = 0x123456;
    ultimoBits = 24;
    ultimoProto = 7;
    ultimoPulso = 0;
    repetirUltimo(3);
  }
  else if (strcmp(nombre, "p8") == 0)
  {
    // proto8 (Conrad RS-200 RX): pulso 200us, ~175 baud. Es un protocolo
    // muy lento (max ~217 baud): el detector mostrara BITRATE NO SOPORTADO.
    ultimoCodigo = 0x234567;
    ultimoBits = 24;
    ultimoProto = 8;
    ultimoPulso = 0;
    repetirUltimo(3);
  }
  else if (strcmp(nombre, "p9") == 0)
  {
    // proto9 (Conrad RS-200 TX, invertido): ~174 baud, igual de lento que 8.
    // Resultado esperado: BITRATE NO SOPORTADO.
    ultimoCodigo = 0x345678;
    ultimoBits = 24;
    ultimoProto = 9;
    ultimoPulso = 0;
    repetirUltimo(3);
  }
  else if (strcmp(nombre, "p10") == 0)
  {
    // proto10 (1ByOne Doorbell, invertido), 24 bits, ~572 baud -> COMPATIBLE
    ultimoCodigo = 0x456789;
    ultimoBits = 24;
    ultimoProto = 10;
    ultimoPulso = 0;
    repetirUltimo(3);
  }
  else if (strcmp(nombre, "p11") == 0)
  {
    // proto11 (HT12E, invertido), 16 bits, ~697 baud -> COMPATIBLE.
    // Con 24 bits daba ~815 baud (sobre el maximo), por eso 16 bits.
    ultimoCodigo = 0xBEEF;
    ultimoBits = 16;
    ultimoProto = 11;
    ultimoPulso = 0;
    repetirUltimo(3);
  }
  else if (strcmp(nombre, "p12") == 0)
  {
    // proto12 (SM5212, invertido), 24 bits, ~688 baud -> COMPATIBLE
    ultimoCodigo = 0x789ABC;
    ultimoBits = 24;
    ultimoProto = 12;
    ultimoPulso = 0;
    repetirUltimo(3);
  }
  else if (strcmp(nombre, "roll") == 0)
  {
    // Rolling code: el codigo cambia en cada envio -> NO COMPATIBLE
    Serial.println(F("Rolling code: 3 envios con codigo distinto"));
    for (int i = 0; i < 3; i++)
    {
      unsigned long codigo = 0x1000UL + (unsigned long)i * 0x0100UL + i;
      Serial.print(F("["));
      Serial.print(i + 1);
      Serial.print(F("/3] "));
      enviarFrame(codigo, 24, 1, 0);
      delay(100);
    }
  }
  else if (strcmp(nombre, "out1") == 0)
  {
    // Bitrate fuera de rango (lento): proto1 con pulso 500us -> ~375 baud.
    // Se envia el frame 1 sola vez y se fija el pulso con setProtocol().
    Serial.println(F("Fuera de rango (lento): pulso 500us ~375 baud"));
    ultimoCodigo = 0x00B4B55;
    ultimoBits = 24;
    ultimoProto = 1;
    ultimoPulso = 500;
    repetirUltimo(3);
  }
  else if (strcmp(nombre, "out2") == 0)
  {
    // Bitrate fuera de rango (rapido): proto1 con pulso 160us -> ~1170 baud.
    Serial.println(F("Fuera de rango (rapido): pulso 160us ~1170 baud"));
    ultimoCodigo = 0x00B4B55;
    ultimoBits = 24;
    ultimoProto = 1;
    ultimoPulso = 160;
    repetirUltimo(3);
  }
  else
  {
    Serial.println(F("Preset desconocido. Escriba 'help'."));
  }
}

// -----------------------------------------------------------

void setup()
{
  Serial.begin(9600);
  delay(500);

  pinMode(PIN_TX, OUTPUT);
  digitalWrite(PIN_TX, LOW);

  pinMode(PIN_BOTON, INPUT_PULLUP);   // boton a GND, sin resistencia externa

  sw433.enableTransmit(PIN_TX);
  sw433.setRepeatTransmit(4);   // ~4 repeticiones por envio, como un control real

  // Preset inicial p1: proto1, 24 bits, pulso por defecto (350us) ~535 baud.
  ultimoCodigo = 0x00B4B55;
  ultimoBits = 24;
  ultimoProto = 1;
  ultimoPulso = 0;

  Serial.println(F("Generador RF 433MHz listo."));
  mostrarMenu();
}

// Procesa una linea de comando recibida por serial (ya completa, con Enter).
// Acepta un caracter por accion y conserva send/rep para enviaos a mano.
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

  // Presets de codigo fijo: 1..12 -> p1..p12 (se usa atoi para aceptar
  // "10", "11" y "12", no solo un caracter).
  int nPreset = atoi(cmd);
  if (nPreset >= 1 && nPreset <= 12)
  {
    char preset[4];
    snprintf(preset, sizeof(preset), "p%d", nPreset);
    ejecutarPreset(preset);
    return;
  }

  if (c == 'r')
  {
    ejecutarPreset("roll");
    return;
  }
  if (c == 'l')
  {
    ejecutarPreset("out1");
    return;
  }
  if (c == 'f')
  {
    ejecutarPreset("out2");
    return;
  }
  if (c == 'e')
  {
    Serial.println(F("[COMANDO] Enviando 1 frame del ultimo codigo..."));
    enviarFrame(ultimoCodigo, ultimoBits, ultimoProto, ultimoPulso);
    return;
  }
  if (c == 'x')
  {
    repetirUltimo(3);
    return;
  }

  if (strcmp(cmd, "send") == 0)
  {
    char *scod = strtok(NULL, " \t\r\n");
    char *sbits = strtok(NULL, " \t\r\n");
    char *sproto = strtok(NULL, " \t\r\n");
    char *spulso = strtok(NULL, " \t\r\n");
    if (scod == NULL || sbits == NULL || sproto == NULL)
    {
      Serial.println(F("Uso: send <codigo> <bits> <proto> [pulso]"));
      return;
    }
    unsigned long codigo = strtoul(scod, NULL, 0);
    int bits = atoi(sbits);
    int proto = atoi(sproto);
    int pulso = (spulso != NULL) ? atoi(spulso) : 0;
    enviarFrame(codigo, bits, proto, pulso);
    return;
  }

  if (strcmp(cmd, "rep") == 0)
  {
    char *sn = strtok(NULL, " \t\r\n");
    int n = (sn != NULL) ? atoi(sn) : 3;
    if (n < 1)
      n = 1;
    repetirUltimo(n);
    return;
  }

  Serial.print(F("Comando desconocido: '"));
  Serial.print(cmd);
  Serial.println(F("'. Escriba 'm' para el menu."));
}

void loop()
{
  // Boton: cada pulsacion envia 1 frame del ultimo codigo configurado.
  atenderBoton();

  // Lee el serial acumulando hasta el Enter: recien ahi procesa la linea
  // completa (evita que el comando se tome caracter por caracter).
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
