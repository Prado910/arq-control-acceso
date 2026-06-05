// =====================================================
// RFID
// =====================================================

/**
 * @brief Tarea no bloqueante de lectura RFID.
 */
void tareaRFID() {
  if (estadoActual != ST_INICIO && !(estadoActual == ST_CONFIG && faseConfig == CFG_RFID_SCAN)) {
    return;
  }

  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  String uid = leerUID();

  Serial.print("UID RFID: ");
  Serial.println(uid);

  if (estadoActual == ST_CONFIG && faseConfig == CFG_RFID_SCAN) {
    guardarRFIDConfig(uid);
  }

  else if (estadoActual == ST_INICIO) {
    int rol = buscarUsuarioPorUID(uid);

    if (rol >= 0) {
      if (config.usuarios[rol].usosRFID >= MAX_USOS_CREDENCIAL) {
        accesoFallido("RFID vencido");
      }

      else if (!enHorario(rol)) {
        accesoFallido("Fuera horario");
      }

      else {
        accesoAutorizado((byte)rol, true);
      }
    }

    else {
      accesoFallido("RFID no valido");
    }
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

/**
 * @brief Convierte el UID leído a texto hexadecimal.
 * @return UID en formato String.
 */
String leerUID() {
  String uid = "";

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) {
      uid += "0";
    }

    uid += String(rfid.uid.uidByte[i], HEX);
  }

  uid.toUpperCase();
  return uid;
}
