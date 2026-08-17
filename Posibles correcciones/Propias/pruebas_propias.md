# Pruebas propias (con el detector de protocolo RF)

Pruebas orientadas al control que falla (`51252245`, HT6P2002) y su clon en TXCar.

## A. Con el firmware actual (sin tocar código)

1. **Consistencia del clon que falla**: 5–10 pulsaciones del TXCar clonado de `51252245` → ¿siempre ~5 tramas y jitter alto? ¿O cambia según cómo se aprieta?
2. **Mantener apretado 2–3 s** en el TXCar clonado → ver `tramas_total`. Si sube de 5 a 8–10, el control manda más tramas al mantener → el problema es cantidad de repeticiones.
3. **Baseline comparativo**: hacer lo mismo con el clon que sí funciona (`123594517`) → 5 pulsaciones para tener patrón (7 tramas, gaps parejos, timings limpios).
4. **Control remoto con `pruebas/transmitter`**: el ESP32 puede regenerar exactamente el código `51252245` (proto 6, 28 bits) variando `setRepeatTransmit(4/5/7/10)` y el pulso (342 vs 539 us). Esto aísla si el detector pierde tramas por culpa del TXCar o del receptor barato. Comparar qué recibe el detector en cada configuración.

## B. Con firmware extendido

5. **Ventana de 2000 ms** → capturar la pulsación completa sin cortar la cuenta.
6. **Métricas de jitter** en el resumen (min/max/σ de símbolos 0 y 1 + tramas por pulsación) → cuantificar si el clon que falla es "sucio" de forma consistente.
7. **Prueba de re-aprendizaje**: grabar el portón usando el clon TXCar. Si abre → es tema de instalación/aprendizaje, no del clonador.