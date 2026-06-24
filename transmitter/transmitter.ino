#include <RCSwitch.h>

RCSwitch sw433 = RCSwitch();

const int PIN_BTN_FIJO    = 18;  // botón código fijo
const int PIN_BTN_ROLLING = 19;  // botón rolling code
const int PIN_TX          = 4;   // DATA del FS1000A

// Código base fijo
const unsigned long CODIGO_FIJO = 5592405UL;  // 0x555555, protocolo 1

// Estado rolling code
unsigned long rollingBase = 0xABCD00UL;
int rollingIndice = 0;

void enviarFijo() {
  Serial.println(F("Enviando FIJO x3..."));
  for (int i = 0; i < 3; i++) {
    sw433.send(CODIGO_FIJO, 24);
    Serial.print(F("  -> "));
    Serial.println(CODIGO_FIJO);
    delay(400);
  }
  Serial.println(F("Listo."));
}

void enviarRolling() {
  Serial.println(F("Enviando ROLLING x3..."));
  for (int i = 0; i < 3; i++) {
    // Cada envío tiene un código distinto
    unsigned long cod = rollingBase + (rollingIndice * 0x1F3AUL);
    sw433.send(cod, 24);
    Serial.print(F("  -> "));
    Serial.println(cod);
    rollingIndice++;
    delay(400);
  }
  Serial.println(F("Listo."));
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN_FIJO,    INPUT_PULLUP);
  pinMode(PIN_BTN_ROLLING, INPUT_PULLUP);

  sw433.enableTransmit(PIN_TX);
  sw433.setProtocol(1);
  sw433.setPulseLength(350);
  sw433.setRepeatTransmit(3);

  Serial.println(F("ESP32 TX listo"));
  Serial.println(F("BTN 18 = Fijo | BTN 19 = Rolling"));
}

void loop() {
  if (digitalRead(PIN_BTN_FIJO) == LOW) {
    delay(50);
    if (digitalRead(PIN_BTN_FIJO) == LOW) {
      enviarFijo();
      while (digitalRead(PIN_BTN_FIJO) == LOW);
    }
  }

  if (digitalRead(PIN_BTN_ROLLING) == LOW) {
    delay(50);
    if (digitalRead(PIN_BTN_ROLLING) == LOW) {
      enviarRolling();
      while (digitalRead(PIN_BTN_ROLLING) == LOW);
    }
  }
}