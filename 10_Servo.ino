// =====================================================
// SERVO
// =====================================================

/**
 * @brief Abre la cerradura durante un tiempo programado.
 */
void abrirCerradura() {
  cerradura.write(SERVO_ABIERTO);
  servoAbierto = true;
  tServo = relojSistema();
}

/**
 * @brief Cierra la cerradura.
 */
void cerrarCerradura() {
  cerradura.write(SERVO_CERRADO);
  servoAbierto = false;
}

/**
 * @brief Cierra automáticamente el servo sin bloquear el programa.
 */
void tareaServo() {
  if (!servoAbierto) {
    return;
  }

  if (relojSistema() - tServo >= T_APERTURA_SERVO) {
    cerrarCerradura();
  }
}
