// =====================================================
// CONFIGURACIÓN
// =====================================================

/**
 * @brief Inicia el menú principal de configuración.
 */
void iniciarConfigMenu() {
  faseConfig = CFG_MENU;
  rolConfig = ROL_NINGUNO;
  limpiarEntrada();
}

/**
 * @brief Solicita clave maestra para acceder a CONFIG desde monitoreo.
 */
void iniciarAuthMasterConfig() {
  faseConfig = CFG_AUTH_MASTER;
  setEstado(ST_CONFIG);
  faseConfig = CFG_AUTH_MASTER;
  limpiarEntrada();
  mensajeTemporal = "";
}

/**
 * @brief Guarda horario de un rol desde el teclado.
 *
 * Formato: HHHH
 * Ejemplo: 0817 significa inicio 08 y fin 17.
 */
void guardarHorarioConfig() {
  byte hInicio = (entrada[0] - '0') * 10 + (entrada[1] - '0');
  byte hFin = (entrada[2] - '0') * 10 + (entrada[3] - '0');

  if (hInicio > 23 || hFin > 23) {
    mensajeTemporal = "Hora invalida";
    tMensaje = relojSistema() + T_ERROR;
    limpiarEntrada();
    return;
  }

  config.usuarios[rolConfig].horaInicio = hInicio;
  config.usuarios[rolConfig].horaFin = hFin;
  guardarConfiguracion();

  mensajeTemporal = "Horario guardado";
  tMensaje = relojSistema() + T_ERROR;

  iniciarConfigMenu();
}

/**
 * @brief Guarda una nueva clave para el rol seleccionado.
 */
void guardarClaveConfig() {
  String nuevaClave = String(entrada);

  if (claveRepetida(nuevaClave)) {
    mensajeTemporal = "Clave repetida";
    tMensaje = relojSistema() + T_ERROR;
    limpiarEntrada();
    return;
  }

  for (byte i = 0; i < LEN_CLAVE; i++) {
    config.usuarios[rolConfig].clave[i] = entrada[i];
  }

  config.usuarios[rolConfig].clave[LEN_CLAVE] = '\0';
  config.usuarios[rolConfig].usosClave = 0;

  guardarConfiguracion();

  mensajeTemporal = "Clave guardada";
  tMensaje = relojSistema() + T_ERROR;

  iniciarConfigMenu();
}

/**
 * @brief Guarda un UID RFID para el rol seleccionado.
 * @param uid UID leído por el RC522.
 */
void guardarRFIDConfig(String uid) {
  uid.toCharArray(config.usuarios[rolConfig].uid, UID_MAX);
  config.usuarios[rolConfig].usosRFID = 0;

  guardarConfiguracion();

  mensajeTemporal = "RFID guardado";
  tMensaje = relojSistema() + T_ERROR;

  iniciarConfigMenu();
}

/**
 * @brief Guarda la hora actual simulada.
 */
void guardarHoraActualConfig() {
  byte h = (entrada[0] - '0') * 10 + (entrada[1] - '0');

  if (h > 23) {
    mensajeTemporal = "Hora invalida";
    tMensaje = relojSistema() + T_ERROR;
    limpiarEntrada();
    return;
  }

  horaActual = h;

  mensajeTemporal = "Hora actual OK";
  tMensaje = relojSistema() + T_ERROR;

  iniciarConfigMenu();
}
