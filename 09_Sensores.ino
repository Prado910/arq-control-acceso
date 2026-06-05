// =====================================================
// SENSORES
// =====================================================

/**
 * @brief Lee sensores y evalúa condiciones de alarma.
 */
void tareaSensores() {
  unsigned long ahora = relojSistema();

  if (ahora - tSensores < T_SENSORES) {
    return;
  }

  tSensores = ahora;

  int lecturaTemp = analogRead(PIN_TEMP);

  if (lecturaTemp <= 0) {
    lecturaTemp = 1;
  }

  if (lecturaTemp >= 1023) {
    lecturaTemp = 1022;
  }

  float r1 = 10000.0;
  float c1 = 0.0010222847;
  float c2 = 0.0002531646;
  float c3 = 0.0;

  float r2 = r1 * (1023.0 / (float)lecturaTemp - 1.0);
  float logR2 = log(r2);

  valorTemp = 1.0 / (c1 + c2 * logR2 + c3 * logR2 * logR2 * logR2);
  valorTemp = valorTemp - 273.15;

  valorLuz = analogRead(PIN_LUZ);
  valorHall = analogRead(PIN_HALL);
  valorSonido = analogRead(PIN_SONIDO);

  if (estadoActual == ST_MONITOR_AMBIENTAL) {
    if (valorTemp < UMBRAL_TEMP_BAJA && valorLuz < UMBRAL_LUZ_BAJA) {
      dispararAlarma(ALR_AMBIENTAL);
    }
  }

  if (estadoActual == ST_MONITOR_INTRUSOS) {
    bool condicionIntrusion = valorHall > UMBRAL_HALL && valorSonido > UMBRAL_SONIDO;

    if (condicionIntrusion && !condicionIntrusionAnterior) {
      conteoIntrusion++;
    }

    condicionIntrusionAnterior = condicionIntrusion;

    if (conteoIntrusion >= 3) {
      conteoIntrusion = 0;
      condicionIntrusionAnterior = false;
      dispararAlarma(ALR_INTRUSOS);
    }
  }
}
