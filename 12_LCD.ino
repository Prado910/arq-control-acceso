// =====================================================
// LCD
// =====================================================

/**
 * @brief Actualiza la pantalla LCD sin bloquear.
 */
void tareaLCD() {
  unsigned long ahora = relojSistema();

  if (ahora - tLCD < T_LCD) {
    return;
  }

  tLCD = ahora;
  lcd.clear();

  if (mensajeTemporal.length() > 0) {
    imprimirLinea(0, mensajeTemporal);
    imprimirLinea(1, "Estado:" + nombreEstado());
    return;
  }

  switch (estadoActual) {
    case ST_INICIO:
      imprimirLinea(0, "INICIO H:" + String(horaActual));
      imprimirLinea(1, "Clave:" + claveEnmascarada());
      break;

    case ST_CONFIG:
      if (faseConfig == CFG_MENU) {
        imprimirLinea(0, "CONFIG 1H 2C");
        imprimirLinea(1, "3RFID 4Hora D");
      }

      else if (faseConfig == CFG_AUTH_MASTER) {
        imprimirLinea(0, "Clave maestra");
        imprimirLinea(1, "Clave:" + claveEnmascarada());
      }

      else if (faseConfig == CFG_HORARIO_ROL) {
        imprimirLinea(0, "Horario rol");
        imprimirLinea(1, "1 2 3 4");
      }

      else if (faseConfig == CFG_HORARIO_DATOS) {
        imprimirLinea(0, config.usuarios[rolConfig].nombre);
        imprimirLinea(1, "HHHH:" + String(entrada));
      }

      else if (faseConfig == CFG_CLAVE_ROL) {
        imprimirLinea(0, "Clave rol");
        imprimirLinea(1, "1 2 3 4");
      }

      else if (faseConfig == CFG_CLAVE_DATOS) {
        imprimirLinea(0, config.usuarios[rolConfig].nombre);
        imprimirLinea(1, "Nueva:" + claveEnmascarada());
      }

      else if (faseConfig == CFG_RFID_ROL) {
        imprimirLinea(0, "RFID rol");
        imprimirLinea(1, "1 2 3 4");
      }

      else if (faseConfig == CFG_RFID_SCAN) {
        imprimirLinea(0, config.usuarios[rolConfig].nombre);
        imprimirLinea(1, "Acerque tarjeta");
      }

      else if (faseConfig == CFG_HORA_ACTUAL) {
        imprimirLinea(0, "Hora actual");
        imprimirLinea(1, "HH:" + String(entrada));
      }
      break;

    case ST_MONITOR_AMBIENTAL:
      imprimirLinea(0, "AMB T:" + String(valorTemp, 1) + "C");
      imprimirLinea(1, "Luz:" + String(valorLuz));
      break;

    case ST_MONITOR_INTRUSOS:
      imprimirLinea(0, "INTR H:" + String(valorHall));
      imprimirLinea(1, "S:" + String(valorSonido) + " C:" + String(conteoIntrusion));
      break;

    case ST_ALARMA:
      if (origenAlarma == ALR_AMBIENTAL) {
        imprimirLinea(0, "ALARMA AMB");
        imprimirLinea(1, "T/L fuera rango");
      }

      else if (origenAlarma == ALR_INTRUSOS) {
        imprimirLinea(0, "ALARMA PUERTA");
        imprimirLinea(1, "Hall/Sonido");
      }

      else if (origenAlarma == ALR_INTENTOS) {
        imprimirLinea(0, "ALARMA ACCESO");
        imprimirLinea(1, "Max intentos");
      }
      break;

    case ST_BLOQUEO:
      imprimirLinea(0, "BLOQUEO " + String(tiempoRestante(T_BLOQUEO)) + "s");
      imprimirLinea(1, "Boton = inicio");
      break;
  }
}

/**
 * @brief Imprime una línea ajustada a 16 caracteres.
 * @param fila Fila del LCD.
 * @param texto Texto a imprimir.
 */
void imprimirLinea(byte fila, String texto) {
  lcd.setCursor(0, fila);

  if (texto.length() > 16) {
    texto = texto.substring(0, 16);
  }

  lcd.print(texto);

  for (byte i = texto.length(); i < 16; i++) {
    lcd.print(" ");
  }
}

/**
 * @brief Calcula tiempo restante de un estado temporizado.
 * @param duracion Duración total.
 * @return Segundos restantes.
 */
unsigned long tiempoRestante(unsigned long duracion) {
  unsigned long transcurrido = relojSistema() - tEstado;

  if (transcurrido >= duracion) {
    return 0;
  }

  return (duracion - transcurrido + 999) / 1000;
}

/**
 * @brief Retorna el nombre corto del estado actual.
 * @return Nombre del estado.
 */
String nombreEstado() {
  switch (estadoActual) {
    case ST_INICIO:
      return "INICIO";

    case ST_CONFIG:
      return "CONFIG";

    case ST_MONITOR_AMBIENTAL:
      return "AMB";

    case ST_MONITOR_INTRUSOS:
      return "INTR";

    case ST_ALARMA:
      return "ALARMA";

    case ST_BLOQUEO:
      return "BLOQ";
  }

  return "----";
}
