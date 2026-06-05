// =====================================================
// ALARMAS
// =====================================================

/**
 * @brief Activa una alarma según su origen.
 * @param origen Tipo de alarma.
 */
void dispararAlarma(OrigenAlarma origen) {
  origenAlarma = origen;

  if (origen == ALR_AMBIENTAL || origen == ALR_INTRUSOS) {
    registrarAlarmaSensor();

    if (conteoAlarmas >= 3) {
      conteoAlarmas = 0;
      mensajeTemporal = "3 alarmas";
      tMensaje = relojSistema() + T_ERROR;
      setEstado(ST_INICIO);
      return;
    }
  }

  setEstado(ST_ALARMA);
}

/**
 * @brief Registra alarmas consecutivas dentro de una ventana de 12 segundos.
 */
void registrarAlarmaSensor() {
  unsigned long ahora = relojSistema();

  if (inicioVentanaAlarmas == 0 || ahora - inicioVentanaAlarmas > T_VENTANA_ALARMAS) {
    inicioVentanaAlarmas = ahora;
    conteoAlarmas = 0;
  }

  conteoAlarmas++;
}

/**
 * @brief Controla LED rojo y buzzer durante alarma o bloqueo.
 */
void tareaAlarma() {
  unsigned long ahora = relojSistema();

  if (estadoActual == ST_ALARMA) {
    digitalWrite(PIN_BUZZER, BUZZER_ON);

    unsigned long periodoActual = salidaBlink ? T_ALARMA_LED_ON : T_ALARMA_LED_OFF;

    if (ahora - tBlink >= periodoActual) {
      tBlink = ahora;
      salidaBlink = !salidaBlink;

      digitalWrite(PIN_LED_ROJO, salidaBlink ? LED_ON : LED_OFF);
      digitalWrite(PIN_LED_VERDE, LED_OFF);
      digitalWrite(PIN_LED_AZUL, LED_OFF);
    }

    return;
  }

  if (estadoActual == ST_BLOQUEO) {
    digitalWrite(PIN_BUZZER, BUZZER_OFF);

    unsigned long periodoActual = salidaBlink ? T_BLOQUEO_LED_ON : T_BLOQUEO_LED_OFF;

    if (ahora - tBlink >= periodoActual) {
      tBlink = ahora;
      salidaBlink = !salidaBlink;

      digitalWrite(PIN_LED_ROJO, salidaBlink ? LED_ON : LED_OFF);
      digitalWrite(PIN_LED_VERDE, LED_OFF);
      digitalWrite(PIN_LED_AZUL, LED_OFF);
    }

    return;
  }
}
