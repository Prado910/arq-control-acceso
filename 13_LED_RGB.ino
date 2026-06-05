// =====================================================
// LED RGB
// =====================================================

/**
 * @brief Controla el LED RGB.
 * @param rojo Estado del color rojo.
 * @param verde Estado del color verde.
 * @param azul Estado del color azul.
 */
void setLED(bool rojo, bool verde, bool azul) {
  digitalWrite(PIN_LED_ROJO, rojo ? LED_ON : LED_OFF);
  digitalWrite(PIN_LED_VERDE, verde ? LED_ON : LED_OFF);
  digitalWrite(PIN_LED_AZUL, azul ? LED_ON : LED_OFF);
}
