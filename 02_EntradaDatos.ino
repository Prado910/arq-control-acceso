// =====================================================
// ENTRADA DE DATOS
// =====================================================

/**
 * @brief Limpia el buffer de entrada del teclado.
 */
void limpiarEntrada() {
  for (byte i = 0; i <= LEN_ENTRADA; i++) {
    entrada[i] = '\0';
  }

  idxEntrada = 0;
}

/**
 * @brief Agrega un dígito al buffer de entrada.
 * @param tecla Dígito presionado.
 */
void agregarDigito(char tecla) {
  if (idxEntrada < LEN_ENTRADA) {
    entrada[idxEntrada] = tecla;
    idxEntrada++;
    entrada[idxEntrada] = '\0';
  }
}

/**
 * @brief Devuelve la clave digitada en forma de asteriscos.
 * @return Cadena enmascarada.
 */
String claveEnmascarada() {
  String salida = "";

  for (byte i = 0; i < idxEntrada; i++) {
    salida += "*";
  }

  return salida;
}
