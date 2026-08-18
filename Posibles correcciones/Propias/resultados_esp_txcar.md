# Resultados: caso 51252245 (TXCar)

Prueba realizada para aislar por qué el TXCar no reproducía el control `51252245`
(clonado desde el control real) de modo que abriera el portón.

## Contexto

- Control real `51252245` (HT6P2002): el portón abre con él.
- Clonado en TXCar desde el **control real**: el TXCar NO abre el portón.
- Re-enseñado en TXCar desde el **ESP32 (señal limpia)**: patrón correcto.

## Resultados medidos (ráfagas con el detector)

| Métrica | TXCar (aprendió del control REAL) | TXCar (aprendió del ESP32) | TXCar que FUNCIONA (123594517) |
|---|---|---|---|
| tramas_total | 5 | **7** | 7 |
| Gaps | 113–114ms con **hueco de 228ms** | **111–112ms uniformes** | 112–114ms uniformes |
| sync_high | 400–500 (inestable) | **448–476 (estable)** | 476–504 |
| cero_a / cero_b | 565–597 / 979–1009 | 531–576 / 959–1001 | 527–557 / 1008–1040 |
| uno_a / uno_b | 1043–1108 / 449–508 | 1048–1092 / 440–486 | 1039–1074 / 482–516 |
| Código | 51252245 | 51252245 | 123594517 |

## Conclusión (cuál es el problema)

El problema **no es el código `51252245` ni el TXCar**: es el **proceso de
aprendizaje del TXCar cuando se le enseña desde el control real**.

El TXCar copia mal la señal RF del control original (HT6P2002), generando una
copia "sucia": pocas tramas (5), un hueco doble de 228ms entre tramas y jitter
de sincronización. Esa copia defectuosa es la que el receptor del portón rechaza.

Al enseñarle desde una señal limpia (ESP32 con RCSwitch), el TXCar reproduce
`51252245` con el mismo patrón que el clon que sí funciona: 7 tramas, gaps
parejos 111–112ms y sincronización estable.

**Solución validada:** enseñarle al TXCar desde la señal limpia
(detector → ESP32 → TXCar), no desde el control real.

## Secuencia exacta del fix

1. **Detector** en modo diagnóstico (`Tester de controles/diagnostico_detector`):
   conectado a una PC (A), monitor serial a 9600.
2. **ESP32** con el simulador (`Tester de controles/control_simulado`):
   conectado a otra PC (B), monitor serial a 9600.
3. En el ESP32: `m` para ver el menú. Elegir el código con `a..g` (presets de
   código) o ajustar pulso/repeticiones con `p <us>` / `r <n>`.
4. Verificar con `x` (1 pulsación) que el detector capta el
   `DIAGNOSTICO RAFAGA` con el código correcto y timings limpios.
5. Poner el TXCar en modo aprendizaje (según su manual).
6. En el ESP32: `z 5` — transmite el código durante 5 segundos apuntando al
   TXCar.
7. Verificar la reproducción del TXCar con el detector: debe dar ≥7 tramas y
   gaps parejos (patrón "limpio").
8. Probar contra el portón.

## Datos de referencia

- `51252245` = protocolo 6 (HT6P20B), 28 bits, pulso 342us → preset `a`.
- Presets del simulador: `a` 51252245, `b` 123594517, `c` 58918405, `d` 89478485,
  `e` 209517045, `f` 99438357 (534 bps), `g` 136178533 (638 bps).