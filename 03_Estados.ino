// =====================================================
// ESTADOS
// =====================================================

/**
 * @brief Cambia el estado principal del sistema.
 * @param nuevoEstado Estado destino.
 */
void setEstado(EstadoSistema nuevoEstado) {
  estadoActual = nuevoEstado;
  tEstado = relojSistema();
  tBlink = relojSistema();
  salidaBlink = false;
  mensajeTemporal = "";

  switch (estadoActual) {
    case ST_INICIO:
      limpiarEntrada();
      rolActivo = ROL_NINGUNO;
      setLED(false, false, true);
      digitalWrite(PIN_BUZZER, BUZZER_OFF);
      break;

    case ST_CONFIG:
      limpiarEntrada();
      iniciarConfigMenu();
      setLED(false, true, true);
      digitalWrite(PIN_BUZZER, BUZZER_OFF);
      break;

    case ST_MONITOR_AMBIENTAL:
      limpiarEntrada();
      setLED(false, true, false);
      digitalWrite(PIN_BUZZER, BUZZER_OFF);
      break;

    case ST_MONITOR_INTRUSOS:
      limpiarEntrada();
      setLED(false, false, true);
      digitalWrite(PIN_BUZZER, BUZZER_OFF);
      break;

    case ST_ALARMA:
      limpiarEntrada();
      setLED(true, false, false);
      digitalWrite(PIN_BUZZER, BUZZER_ON);
      break;

    case ST_BLOQUEO:
      limpiarEntrada();
      cerrarCerradura();
      setLED(true, false, false);
      digitalWrite(PIN_BUZZER, BUZZER_OFF);
      break;
  }
}

/**
 * @brief Actualiza transiciones temporizadas de la máquina de estados.
 */
void actualizarMaquinaEstados() {
  unsigned long ahora = relojSistema();
  unsigned long transcurrido = ahora - tEstado;

  if (mensajeTemporal.length() > 0 && ahora >= tMensaje) {
    mensajeTemporal = "";
  }

  switch (estadoActual) {
    case ST_INICIO:
      break;

    case ST_CONFIG:
      break;

    case ST_MONITOR_AMBIENTAL:
      if (transcurrido >= T_MONITOR_AMBIENTAL) {
        setEstado(ST_MONITOR_INTRUSOS);
      }
      break;

    case ST_MONITOR_INTRUSOS:
      if (transcurrido >= T_MONITOR_INTRUSOS) {
        setEstado(ST_MONITOR_AMBIENTAL);
      }
      break;

    case ST_ALARMA:
      if (origenAlarma == ALR_AMBIENTAL && transcurrido >= T_ALARMA_AMBIENTAL) {
        setEstado(ST_MONITOR_AMBIENTAL);
      }

      else if (origenAlarma == ALR_INTRUSOS && transcurrido >= T_ALARMA_INTRUSOS) {
        setEstado(ST_MONITOR_INTRUSOS);
      }

      else if (origenAlarma == ALR_INTENTOS && transcurrido >= T_ALARMA_INTENTOS) {
        setEstado(ST_BLOQUEO);
      }
      break;

    case ST_BLOQUEO:
      if (transcurrido >= T_BLOQUEO) {
        intentosFallidos = 0;
        setEstado(ST_INICIO);
      }
      break;
  }
}
