// =====================================================
// CONTROL DE ACCESO
// =====================================================

/**
 * @brief Confirma clave ingresada desde INICIO.
 */
void confirmarClaveInicio() {
  if (idxEntrada < LEN_CLAVE) {
    accesoFallido("Min 4 digitos");
    return;
  }

  String clave = String(entrada);

  if (clave == MASTER_CLAVE) {
    intentosFallidos = 0;
    setEstado(ST_CONFIG);
    return;
  }

  int rol = buscarUsuarioPorClave(clave);

  if (rol >= 0) {
    if (config.usuarios[rol].usosClave >= MAX_USOS_CREDENCIAL) {
      accesoFallido("Clave vencida");
      return;
    }

    if (!enHorario(rol)) {
      accesoFallido("Fuera horario");
      return;
    }

    accesoAutorizado((byte)rol, false);
  }

  else {
    accesoFallido("Clave erronea");
  }
}

/**
 * @brief Confirma clave maestra para entrar a configuración desde monitoreo.
 */
void confirmarMasterParaConfig() {
  if (String(entrada) == MASTER_CLAVE) {
    setEstado(ST_CONFIG);
  }

  else {
    limpiarEntrada();
    mensajeTemporal = "Master incorrecta";
    tMensaje = relojSistema() + T_ERROR;
  }
}

/**
 * @brief Concede acceso a un rol autorizado.
 * @param rol Índice del usuario.
 * @param porRFID true si el acceso fue por RFID.
 */
void accesoAutorizado(byte rol, bool porRFID) {
  rolActivo = rol;
  intentosFallidos = 0;

  if (porRFID) {
    config.usuarios[rol].usosRFID++;
  } else {
    config.usuarios[rol].usosClave++;
  }

  guardarConfiguracion();

  abrirCerradura();
  setEstado(ST_MONITOR_AMBIENTAL);

  // Mostrar usuario que ingresó
  mensajeTemporal = "ING:" + String(config.usuarios[rol].nombre);
  tMensaje = relojSistema() + T_MENSAJE_INGRESO;
}

/**
 * @brief Procesa un fallo de acceso.
 * @param motivo Mensaje a mostrar.
 */
void accesoFallido(String motivo) {
  limpiarEntrada();
  intentosFallidos++;

  mensajeTemporal = motivo;
  tMensaje = relojSistema() + T_ERROR;

  setLED(false, false, true);

  if (intentosFallidos >= MAX_INTENTOS) {
    mensajeTemporal = "Max intentos";

    // LED rojo SOLO cuando el sistema se bloquea
    setLED(true, false, false);

    dispararAlarma(ALR_INTENTOS);
    return;
  }
}

/**
 * @brief Busca un usuario por clave.
 * @param clave Clave ingresada.
 * @return Índice del usuario o -1.
 */
int buscarUsuarioPorClave(String clave) {
  for (byte i = 0; i < TOTAL_ROLES; i++) {
    if (clave == String(config.usuarios[i].clave)) {
      return i;
    }
  }

  return -1;
}

/**
 * @brief Busca un usuario por UID RFID.
 * @param uid UID leído.
 * @return Índice del usuario o -1.
 */
int buscarUsuarioPorUID(String uid) {
  for (byte i = 0; i < TOTAL_ROLES; i++) {
    if (String(config.usuarios[i].uid).length() > 0 && uid == String(config.usuarios[i].uid)) {
      return i;
    }
  }

  return -1;
}

/**
 * @brief Verifica si una clave ya está registrada.
 * @param clave Clave a validar.
 * @return true si la clave está repetida.
 */
bool claveRepetida(String clave) {
  if (clave == MASTER_CLAVE) {
    return true;
  }

  for (byte i = 0; i < TOTAL_ROLES; i++) {
    if (clave == String(config.usuarios[i].clave)) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Verifica si el rol puede ingresar en la hora actual.
 * @param rol Índice del rol.
 * @return true si está dentro del horario permitido.
 */
bool enHorario(byte rol) {
  byte inicio = config.usuarios[rol].horaInicio;
  byte fin = config.usuarios[rol].horaFin;

  if (inicio <= fin) {
    return horaActual >= inicio && horaActual <= fin;
  }

  return horaActual >= inicio || horaActual <= fin;
}
