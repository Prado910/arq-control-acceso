// =====================================================
// TAREA: TECLADO
// =====================================================

/**
 * @brief Lee el teclado y envía la tecla al manejador correspondiente.
 */
void tareaTeclado() {
  char tecla = teclado.getKey();

  if (!tecla) {
    return;
  }

  if (estadoActual == ST_INICIO) {
    procesarTeclaInicio(tecla);
  }

  else if (estadoActual == ST_CONFIG) {
    procesarTeclaConfig(tecla);
  }

  else if (estadoActual == ST_MONITOR_AMBIENTAL || estadoActual == ST_MONITOR_INTRUSOS) {
    procesarTeclaMonitoreo(tecla);
  }
}

/**
 * @brief Procesa teclas en estado INICIO.
 * @param tecla Tecla presionada.
 */
void procesarTeclaInicio(char tecla) {
  if (tecla >= '0' && tecla <= '9') {
    agregarDigito(tecla);
  }

  else if (tecla == '*') {
    limpiarEntrada();
    mensajeTemporal = "Clave borrada";
    tMensaje = relojSistema() + T_ERROR;
  }

  else if (tecla == '#') {
    confirmarClaveInicio();
  }
}

/**
 * @brief Procesa teclas en el estado CONFIG.
 * @param tecla Tecla presionada.
 */
void procesarTeclaConfig(char tecla) {
  if (tecla == '*') {
    iniciarConfigMenu();
    return;
  }

  if (faseConfig == CFG_AUTH_MASTER) {
    if (tecla >= '0' && tecla <= '9') {
      agregarDigito(tecla);
    }

    if (tecla == '#') {
      confirmarMasterParaConfig();
    }

    return;
  }

  if (faseConfig == CFG_MENU) {
    if (tecla == '1') {
      faseConfig = CFG_HORARIO_ROL;
      limpiarEntrada();
    }

    else if (tecla == '2') {
      faseConfig = CFG_CLAVE_ROL;
      limpiarEntrada();
    }

    else if (tecla == '3') {
      faseConfig = CFG_RFID_ROL;
      limpiarEntrada();
    }

    else if (tecla == '4') {
      faseConfig = CFG_HORA_ACTUAL;
      limpiarEntrada();
    }

    else if (tecla == 'D') {
      setEstado(ST_MONITOR_AMBIENTAL);
    }

    return;
  }

  if (faseConfig == CFG_HORARIO_ROL) {
    if (tecla >= '1' && tecla <= '4') {
      rolConfig = tecla - '1';
      faseConfig = CFG_HORARIO_DATOS;
      limpiarEntrada();
    }

    return;
  }

  if (faseConfig == CFG_HORARIO_DATOS) {
    if (tecla >= '0' && tecla <= '9') {
      agregarDigito(tecla);
    }

    if (idxEntrada == 4) {
      guardarHorarioConfig();
    }

    return;
  }

  if (faseConfig == CFG_CLAVE_ROL) {
    if (tecla >= '1' && tecla <= '4') {
      rolConfig = tecla - '1';
      faseConfig = CFG_CLAVE_DATOS;
      limpiarEntrada();
    }

    return;
  }

  if (faseConfig == CFG_CLAVE_DATOS) {
    if (tecla >= '0' && tecla <= '9') {
      agregarDigito(tecla);
    }

    if (idxEntrada == LEN_CLAVE) {
      guardarClaveConfig();
    }

    return;
  }

  if (faseConfig == CFG_RFID_ROL) {
    if (tecla >= '1' && tecla <= '4') {
      rolConfig = tecla - '1';
      faseConfig = CFG_RFID_SCAN;
      limpiarEntrada();
    }

    return;
  }

  if (faseConfig == CFG_HORA_ACTUAL) {
    if (tecla >= '0' && tecla <= '9') {
      agregarDigito(tecla);
    }

    if (idxEntrada == 2) {
      guardarHoraActualConfig();
    }

    return;
  }
}

/**
 * @brief Procesa teclas durante monitoreo.
 * @param tecla Tecla presionada.
 */
void procesarTeclaMonitoreo(char tecla) {
  if (estadoActual == ST_MONITOR_AMBIENTAL && tecla == '*') {
    limpiarEntrada();
    cerrarCerradura();
    setEstado(ST_INICIO);
    return;
  }

  else if (estadoActual == ST_MONITOR_INTRUSOS && tecla == '#') {
    limpiarEntrada();
    cerrarCerradura();
    setEstado(ST_INICIO);
    return;
  }
}
