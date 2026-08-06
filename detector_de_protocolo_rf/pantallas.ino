// pantallas.ino
// Capa de dibujo con U8g2 (modo página). Separada del sketch principal para
// mantener organizado el código. Al estar en la misma carpeta, Arduino
// compila este archivo junto con detector_de_protocolo_rf.ino como una sola
// unidad, así que comparte las mismas variables y funciones globales
// (u8g2, Paquete, indice, reintentando, BATERIA_BAJA, medirBateria(), ...).

// Dibuja cabecera: batería + línea separadora
// U8g2 trabaja dentro de firstPage/nextPage, no necesita clearDisplay
void dibujarCabecera(int bat)
{
  bool bateriaBaja = (bat < BATERIA_BAJA);
  // En batería baja el ícono parpadea (se dibuja solo en el medio ciclo par)
  bool iconoVisible = !bateriaBaja || ((millis() / 500) % 2 == 0);

  // Texto batería
  u8g2.setCursor(0, 8);
  u8g2.print(F("BAT:"));
  u8g2.print(bat);
  u8g2.print('%');
  if (bateriaBaja)
    u8g2.print('!');

  // Ícono batería (esquina superior derecha)
  u8g2.drawFrame(106, 1, 18, 7);
  u8g2.drawBox(124, 3, 2, 3);
  if (iconoVisible)
  {
    int ancho = (int)(14.0 * bat / 100.0);
    if (ancho > 0)
      u8g2.drawBox(107, 2, ancho, 5);
  }

  // Línea separadora
  u8g2.drawHLine(0, 11, 128);
}

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

// Vista genérica de la resultado: mismo layout en todas las vistas, asterisco
// en el campo que señala el problema (titulo viene en flash con F()).
void dibujarResultado(const __FlashStringHelper* titulo, Paquete &p,
                      bool asteriscoBps, bool asteriscoProto)
{
  int bat = medirBateria();
  u8g2.firstPage();
  do
  {
    dibujarCabecera(bat);
    u8g2.setCursor(0, 22);
    u8g2.print(titulo);
    u8g2.drawHLine(0, 24, 128);
    u8g2.setCursor(0, 34);
    u8g2.print(F("BPS:"));
    u8g2.print(p.baud);
    if (asteriscoBps)
    {
      u8g2.print(F(" ("));
      u8g2.print(BAUD_MIN);
      u8g2.print('-');
      u8g2.print(BAUD_MAX);
      u8g2.print(')');
      u8g2.print(F(" X"));
    }
    u8g2.setCursor(0, 43);
    u8g2.print(F("Protocolo:"));
    u8g2.print(p.protocolo);
    if (asteriscoProto)
      u8g2.print(F(" X"));
    u8g2.setCursor(0, 52);
    u8g2.print(F("Bits:"));
    u8g2.print(p.bits);
    u8g2.setCursor(0, 61);
    u8g2.print(F("Cod:"));
    u8g2.print(p.codigo);
  } while (u8g2.nextPage());
}

void mostrarCompatible(Paquete &p)
{
  dibujarResultado(F("COMPATIBLE"), p, false, false);
}

void mostrarProbCompatible(Paquete &p)
{
  dibujarResultado(F("PROB. COMPATIBLE"), p, false, true);
}

// Protocolo sin compatibilidad documentada con RS400TX: asterisco en Protocolo.
void mostrarNoCompatible(Paquete &p)
{
  dibujarResultado(F("NO COMPATIBLE"), p, false, true);
}

// Bitrate fuera de la ventana: asterisco en BPS con el rango actual.
void mostrarNoCompatibleBps(Paquete &p)
{
  dibujarResultado(F("NO COMPATIBLE"), p, true, false);
}

// Rolling code: solo el texto, centrado.
void mostrarRollingCode()
{
  int bat = medirBateria();
  u8g2.firstPage();
  do
  {
    dibujarCabecera(bat);
    u8g2.setCursor(0, 22);
    u8g2.print(F("NO COMPATIBLE"));
    u8g2.drawHLine(0, 24, 128);
    // "ROLLING CODE" = 12 chars x 6px = 72px -> x = (128-72)/2 = 28
    u8g2.setCursor(28, 43);
    u8g2.print(F("ROLLING CODE"));
  } while (u8g2.nextPage());
}