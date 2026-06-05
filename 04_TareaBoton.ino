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
        intentosFallidos = 0;
        cerrarCerradura();
        setEstado(ST_INICIO);
      }
    }
  }
}
