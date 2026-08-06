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
    u8g2.print(p.baud);
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
    u8g2.print(p.baud);
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
    u8g2.print(F("Solo 400-800 baud"));
  } while (u8g2.nextPage());
  delay(2500);
  mostrarEspera();
}