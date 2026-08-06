// bateria.ino
// Batería: LiPo 2S con divisor 10k/4.7k (pin A0).
// Mide el VCC real con el bandgap interno para no depender de 5V exactos.

const int PIN_VBAT = A0;
const int BATERIA_BAJA = 40;   // umbral de aviso de batería baja (%)

// VCC real en mV, referenciado al bandgap interno de 1.1V.
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

// Porcentaje de batería: pct = (vBat - 6.5V) / (8.4V - 6.5V) * 100.
int medirBateria()
{
  long vcc = leerVcc();                          // mV
  long vBat = (long)analogRead(PIN_VBAT) * vcc * 147L / (1023L * 47L);  // divisor 10k/4.7k
  long pct = (vBat - 6500L) * 100L / 1900L;      // 6.5..8.4V
  return (int)constrain(pct, 0, 100);
}