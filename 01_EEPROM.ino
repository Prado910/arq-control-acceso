// =====================================================
// EEPROM
// =====================================================

/**
 * @brief Carga la configuración desde EEPROM.
 */
void cargarConfiguracion() {
  EEPROM.get(EEPROM_ADDR, config);

  if (config.magic != EEPROM_MAGIC) {
    inicializarConfiguracionDefecto();
    guardarConfiguracion();
  }
}

/**
 * @brief Escribe valores iniciales de usuarios, claves y horarios.
 */
void inicializarConfiguracionDefecto() {
  config.magic = EEPROM_MAGIC;

  strcpy(config.usuarios[0].nombre, "SEGURIDAD");
  strcpy(config.usuarios[0].clave, "1111");
  strcpy(config.usuarios[0].uid, "");
  config.usuarios[0].horaInicio = 0;
  config.usuarios[0].horaFin = 23;
  config.usuarios[0].usosClave = 0;
  config.usuarios[0].usosRFID = 0;

  strcpy(config.usuarios[1].nombre, "OPERARIO");
  strcpy(config.usuarios[1].clave, "2222");
  strcpy(config.usuarios[1].uid, "");
  config.usuarios[1].horaInicio = 6;
  config.usuarios[1].horaFin = 18;
  config.usuarios[1].usosClave = 0;
  config.usuarios[1].usosRFID = 0;

  strcpy(config.usuarios[2].nombre, "COORD");
  strcpy(config.usuarios[2].clave, "3333");
  strcpy(config.usuarios[2].uid, "");
  config.usuarios[2].horaInicio = 7;
  config.usuarios[2].horaFin = 20;
  config.usuarios[2].usosClave = 0;
  config.usuarios[2].usosRFID = 0;

  strcpy(config.usuarios[3].nombre, "GERENTE");
  strcpy(config.usuarios[3].clave, "4444");
  strcpy(config.usuarios[3].uid, "");
  config.usuarios[3].horaInicio = 0;
  config.usuarios[3].horaFin = 23;
  config.usuarios[3].usosClave = 0;
  config.usuarios[3].usosRFID = 0;
}

/**
 * @brief Guarda toda la configuración en EEPROM.
 */
void guardarConfiguracion() {
  EEPROM.put(EEPROM_ADDR, config);
}
