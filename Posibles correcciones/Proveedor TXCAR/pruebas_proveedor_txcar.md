# Pruebas para el fabricante/distribuidor del TXCar

Pruebas que debería hacer el proveedor del TXCar para determinar si el problema del control `51252245` (HT6P2002) es un error de instalación del producto o un defecto del clonador.

## Diagnóstico técnico de su producto

1. **Medir con SDR / osciloscopio** la señal emitida por el TXCar para `HT6P2002` vs `HT6P20D`:
   - Forma de onda completa (preámbulo, sync, datos, gaps entre tramas).
   - **Cantidad de repeticiones por pulsación** (nosotros medimos 5 vs 7).
   - **Jitter de cada pulso** (nosotros medimos desvíos de hasta ~30 %).
   - Frecuencia exacta (433.92 MHz o deriva).
2. **Comparar lo que el TXCar dice haber guardado vs lo que transmite** → detectar si pierde bits al aprender o al regenerar.
3. **Test de compatibilidad con HT6P2002**: probar el clon contra receptores reales de learning code para ver en cuáles falla y en cuáles no.
4. **Revisar el algoritmo de aprendizaje/reproducción**: si regenera bien `HT6P20D` pero mal `HT6P2002`, es un bug de su software (ambos son familia HT6P20, deberían tratarse igual).

## Cierre de caso (soporte)

5. **Actualización de firmware del TXCar** si es actualizable, con este caso como repro.
6. **Manual/instalación**: documentar qué encoders soporta de forma confiable y si ciertos receptores exigen re-aprender con el clon.