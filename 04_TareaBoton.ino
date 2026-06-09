// =====================================================
// TAREA: BOTÓN
// =====================================================

/**
 * @brief Lee el botón con antirebote por software.
 */
void tareaBoton() {
  unsigned long ahora = relojSistema();
  bool lectura = digitalRead(PIN_BOTON) == LOW;

  if (lectura != botonLecturaAnterior) {
    tBotonCambio = ahora;
    botonLecturaAnterior = lectura;
  }

  if ((ahora - tBotonCambio) >= T_DEBOUNCE_BOTON) {
    if (lectura != botonEstadoEstable) {
      botonEstadoEstable = lectura;

      if (botonEstadoEstable) {
        // El botón SOLO vuelve a inicio desde CONFIG o BLOQUEO
        if (estadoActual == ST_CONFIG || estadoActual == ST_BLOQUEO) {
          intentosFallidos = 0;
          limpiarEntrada();
          cerrarCerradura();
          setLED(false, false, false);
          setEstado(ST_INICIO);
        }
      }
    }
  }
}
