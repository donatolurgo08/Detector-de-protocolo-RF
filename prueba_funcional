#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    -1
);


const int PIN_RESET = 6;
const int PIN_VBAT = A0;

unsigned long codigos[3] = {0, 0, 0};
int indice = 0;
String freqDetectada = "";
bool esperandoReset = false;
int bitsDetectados = 0;

int medirBateria() {
  int raw = analogRead(PIN_VBAT);
  float vMedido = raw * (5.0 / 1023.0);
  float vBat = vMedido * (10.0 + 4.7) / 4.7;
  int pct = (int)((vBat - 6.5) / (8.4 - 6.5) * 100.0);
  return constrain(pct, 0, 100);
}

void dibujarBateria(int pct) {
  display.drawRect(106, 0, 18, 8, SSD1306_WHITE);
  display.fillRect(124, 2, 2, 4, SSD1306_WHITE);

  int ancho = (int)(14.0 * pct / 100.0);

  if (ancho > 0)
    display.fillRect(108, 2, ancho, 4, SSD1306_WHITE);
}

void cabecera() {
  int bat = medirBateria();

  dibujarBateria(bat);

  display.setCursor(0, 0);
  display.print("BAT:");
  display.print(bat);
  display.print("%");

  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
}

void mostrarEspera() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  cabecera();

  display.setCursor(0, 14);
  display.println("Apriete el control");

  display.setCursor(0, 26);
  display.println("del porton 3 veces");

  display.setCursor(0, 42);
  display.print("Detectados: ");
  display.print(min(indice, 3));
  display.print("/3");

  display.setCursor(0, 54);
  display.print("315, 433 MHz activo");

  display.display();
}

void mostrarCompatible(unsigned long codigo, int protocolo, String freq) {

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  cabecera();

  display.setCursor(0, 13);
  display.println("COMPATIBLE");

  display.drawLine(0, 23, 128, 23, SSD1306_WHITE);

  display.setCursor(0, 27);
  display.print("Freq:");
  display.print(freq);

  display.setCursor(0, 36);
  display.print("Proto:");
  display.print(protocolo);

  display.setCursor(0, 45);
  display.print("Bits:");
  display.print(bitsDetectados);

  display.setCursor(0, 54);
  display.print("Cod:");
  display.print(codigo);

  display.display();
}


void mostrarIncompatible(int protocolo, String freq) {

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  cabecera();

  display.setCursor(0, 13);
  display.println("NO COMPATIBLE");

  display.drawLine(0, 23, 128, 23, SSD1306_WHITE);

  display.setCursor(0, 27);
  display.println("Rolling Code");

  display.setCursor(0, 36);
  display.print("Freq:");
  display.print(freq);

  display.setCursor(0, 45);
  display.print("Proto:");
  display.print(protocolo);

  display.setCursor(0, 54);
  display.print("Bits:");
  display.print(bitsDetectados);

  display.display();
}

void resetearDetector() {
  indice = 0;

  codigos[0] = 0;
  codigos[1] = 0;
  codigos[2] = 0;

  freqDetectada = "";
  bitsDetectados = 0;
  esperandoReset = false;

  mostrarEspera();
}

void procesarResultado(unsigned long codigo, int protocolo) {

  bool esFijo =
      (codigos[0] == codigos[1]) &&
      (codigos[1] == codigos[2]);

  esperandoReset = true;

  if (esFijo) {
    Serial.println(">>> CODIGO FIJO - COMPATIBLE");
    mostrarCompatible(codigo, protocolo, freqDetectada);
  }
  else {
    Serial.println(">>> ROLLING CODE - NO COMPATIBLE");
    mostrarIncompatible(protocolo, freqDetectada);
  }
}

void setup() {

  Serial.begin(9600);

  pinMode(PIN_RESET, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("No se encontro la OLED");
    while (1);
  }

  Serial.println("OLED listo");

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(10, 8);
  display.println("DETECTOR RF 433/315");

  display.setCursor(20, 24);
  display.println("v4.0 - Iniciando");

  display.display();

  delay(1500);

  mostrarEspera();
}

void loop() {

  if (digitalRead(PIN_RESET) == LOW) {
    delay(50);

    if (digitalRead(PIN_RESET) == LOW) {

      resetearDetector();

      while (digitalRead(PIN_RESET) == LOW);
    }
  }

  if (esperandoReset)
    return;

  
}