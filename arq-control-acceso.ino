#include <Keypad.h>
#include <LiquidCrystal.h>
#include <Servo.h>

// =====================================================
// LCD 16x2 EN MODO PARALELO
// =====================================================
#define LCD_RS 40
#define LCD_E  41
#define LCD_D4 42
#define LCD_D5 43
#define LCD_D6 44
#define LCD_D7 45

LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// =====================================================
// SERVO
// =====================================================
Servo cerradura;

// =====================================================
// PINES DE SALIDA
// =====================================================
#define PIN_SERVO 9
#define PIN_BUZZER 8
#define PIN_LED_VERDE 6
#define PIN_LED_ROJO 7

// =====================================================
// PINES DE SENSORES
// =====================================================
#define PIN_PUERTA 30
#define PIN_INTRUSION 31

// se deja la puerta desactivad mientras.
#define USAR_SENSOR_PUERTA false
#define USAR_SENSOR_INTRUSION true

// =====================================================
// TIEMPOS DEL SISTEMA
// =====================================================
#define TIEMPO_APERTURA 7000UL   // 7 segundos
#define TIEMPO_ERROR 1500UL      // 1.5 segundos
#define TIEMPO_ALARMA 8000UL     // 8 segundos
#define TIEMPO_BLOQUEO 10000UL   // 10 segundos

#define MAX_INTENTOS 3
#define MAX_USOS_CLAVE 4

// =====================================================
// POSICIONES DEL SERVO
// =====================================================
#define SERVO_CERRADO 0
#define SERVO_ABIERTO 90

// =====================================================
// TECLADO 4x4
// =====================================================
#define FILAS 4
#define COLUMNAS 4

#define PIN_FILA_1  22
#define PIN_FILA_2  23
#define PIN_FILA_3  24
#define PIN_FILA_4  25

#define PIN_COLUMNA_1  26
#define PIN_COLUMNA_2  27
#define PIN_COLUMNA_3  28
#define PIN_COLUMNA_4  29

char teclas[FILAS][COLUMNAS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

byte pinesFilas[FILAS] = {
  PIN_FILA_1,
  PIN_FILA_2,
  PIN_FILA_3,
  PIN_FILA_4
};

byte pinesColumnas[COLUMNAS] = {
  PIN_COLUMNA_1,
  PIN_COLUMNA_2,
  PIN_COLUMNA_3,
  PIN_COLUMNA_4
};

Keypad teclado = Keypad(makeKeymap(teclas), pinesFilas, pinesColumnas, FILAS, COLUMNAS);

// =====================================================
// USUARIOS Y PERFILES
// =====================================================
#define LONGITUD_CLAVE 8

struct Usuario {
  const char* perfil;
  char clave[LONGITUD_CLAVE + 1];
  byte usos;
};

Usuario usuarios[] = {
  {"SEGURIDAD", "1111", 0},
  {"OPERARIO",  "2222", 0},
  {"COORD.",    "3333", 0},
  {"GERENTE",   "4444", 0}
};

#define TOTAL_USUARIOS (sizeof(usuarios) / sizeof(usuarios[0]))

// =====================================================
// MAQUINA DE ESTADOS
// =====================================================
enum EstadoSistema {
  ST_IDLE,
  ST_ERR,
  ST_OPEN,
  ST_ALR,
  ST_LOCK,
  ST_CHANGE
};

EstadoSistema estadoActual = ST_IDLE;

// =====================================================
// VARIABLES DEL SISTEMA
// =====================================================
String claveIngresada = "";
String mensaje = "Ingrese clave";
String perfilActivo = "Ninguno";

byte intentosFallidos = 0;
int usuarioEnCambio = -1;

unsigned long tiempoCambioEstado = 0;
unsigned long tiempoLCD = 0;
unsigned long tiempoBlink = 0;

bool salidaBlink = false;
bool forzarLCD = true;

// =====================================================
// RELOJ DEL SISTEMA
// =====================================================
unsigned long relojSistema() {
  return millis();
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_ROJO, OUTPUT);

  pinMode(PIN_PUERTA, INPUT_PULLUP);
  pinMode(PIN_INTRUSION, INPUT_PULLUP);

  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED_VERDE, LOW);
  digitalWrite(PIN_LED_ROJO, LOW);

  cerradura.attach(PIN_SERVO);
  cerradura.write(SERVO_CERRADO);

  lcd.begin(16, 2);
  lcd.clear();

  cambiarEstado(ST_IDLE);
}

// =====================================================
// LOOP PRINCIPAL
// =====================================================
void loop() {
  leerTeclado();
  revisarSensoresSeguridad();
  actualizarEstado();
  controlarAlarmaVisual();
  actualizarLCD();
}

// =====================================================
// LECTURA DEL TECLADO
// =====================================================
void leerTeclado() {
  char tecla = teclado.getKey();

  if (!tecla) {
    return;
  }

  // Solo se permite digitar clave en estado IDLE
  if (estadoActual != ST_IDLE && estadoActual != ST_CHANGE) {
    return;
  }

  // Ingreso de numeros
  if (tecla >= '0' && tecla <= '9') {
    if (claveIngresada.length() < LONGITUD_CLAVE) {
      claveIngresada += tecla;
      mensaje = "Digitando...";
      forzarLCD = true;
    }
  }
  else if (tecla == '*') { // Borrar clave
    claveIngresada = "";
    mensaje = "Clave borrada";
    forzarLCD = true;
  }
  else if (tecla == '#') { // Confirmar clave
    if (estadoActual == ST_IDLE) {
      confirmarClave();
    } 
    else if (estadoActual == ST_CHANGE) {
      confirmarNuevaClave();
    }
  }
}

// =====================================================
// VALIDACION DE CLAVE
// =====================================================
void confirmarClave() {
  if (claveIngresada.length() < 4) {
    mensaje = "Min 4 digitos";
    claveIngresada = "";
    cambiarEstado(ST_ERR);
    return;
  }

  int indiceUsuario = buscarUsuarioPorClave(claveIngresada);

  if (indiceUsuario >= 0) {
    if (usuarios[indiceUsuario].usos >= MAX_USOS_CLAVE) {
      usuarioEnCambio = indiceUsuario;
      perfilActivo = usuarios[indiceUsuario].perfil;
      mensaje = "Nueva clave";
      claveIngresada = "";
      cambiarEstado(ST_CHANGE);
      return;
    }

    usuarios[indiceUsuario].usos++;
    intentosFallidos = 0;
    perfilActivo = usuarios[indiceUsuario].perfil;

    mensaje = "Acceso OK";
    claveIngresada = "";
    cambiarEstado(ST_OPEN);
  }

  else {
    intentosFallidos++;
    claveIngresada = "";

    if (intentosFallidos >= MAX_INTENTOS) {
      mensaje = "Max intentos";
      cambiarEstado(ST_ALR);
    } else {
      mensaje = "Clave erronea";
      cambiarEstado(ST_ERR);
    }
  }
}

void confirmarNuevaClave() {
  if (usuarioEnCambio < 0) {
    mensaje = "Error usuario";
    claveIngresada = "";
    cambiarEstado(ST_ERR);
    return;
  }

  if (claveIngresada.length() < 4) {
    mensaje = "Min 4 digitos";
    claveIngresada = "";
    forzarLCD = true;
    return;
  }

  if (claveIngresada == String(usuarios[usuarioEnCambio].clave)) {
    mensaje = "No repetir";
    claveIngresada = "";
    forzarLCD = true;
    return;
  }

  int usuarioExistente = buscarUsuarioPorClave(claveIngresada);

  if (usuarioExistente >= 0) {
    mensaje = "Clave en uso";
    claveIngresada = "";
    forzarLCD = true;
    return;
  }

  claveIngresada.toCharArray(usuarios[usuarioEnCambio].clave, LONGITUD_CLAVE + 1);
  usuarios[usuarioEnCambio].usos = 0;

  mensaje = "Clave actualizada";
  claveIngresada = "";
  usuarioEnCambio = -1;

  cambiarEstado(ST_OPEN);
}

int buscarUsuarioPorClave(String clave) {
  for (byte i = 0; i < TOTAL_USUARIOS; i++) {
    if (clave == String(usuarios[i].clave)) {
      return i;
    }
  }

  return -1;
}

// =====================================================
// SENSORES DE SEGURIDAD
// =====================================================
void revisarSensoresSeguridad() {
  if (estadoActual == ST_OPEN || estadoActual == ST_ALR || estadoActual == ST_LOCK) {
    return;
  }

  if (USAR_SENSOR_INTRUSION) {
    bool intrusionDetectada = digitalRead(PIN_INTRUSION) == LOW;

    if (intrusionDetectada) {
      mensaje = "Intrusion!";
      cambiarEstado(ST_ALR);
      return;
    }
  }

  if (USAR_SENSOR_PUERTA) {
    bool puertaAbierta = digitalRead(PIN_PUERTA) == HIGH;

    if (puertaAbierta) {
      mensaje = "Puerta abierta";
      cambiarEstado(ST_ALR);
      return;
    }
  }
}

// =====================================================
// ACTUALIZACION DE ESTADOS
// =====================================================
void actualizarEstado() {
  unsigned long ahora = relojSistema();
  unsigned long transcurrido = ahora - tiempoCambioEstado;

  switch (estadoActual) {
    case ST_IDLE:
      break;

    case ST_ERR:
      if (transcurrido >= TIEMPO_ERROR) {
        mensaje = "Ingrese clave";
        cambiarEstado(ST_IDLE);
      }
      break;

    case ST_OPEN:
      if (transcurrido >= TIEMPO_APERTURA) {
        mensaje = "Cerrada";
        perfilActivo = "Ninguno";
        cambiarEstado(ST_IDLE);
      }
      break;

    case ST_ALR:
      if (transcurrido >= TIEMPO_ALARMA) {
        mensaje = "Bloqueo";
        cambiarEstado(ST_LOCK);
      }
      break;

    case ST_LOCK:
      if (transcurrido >= TIEMPO_BLOQUEO) {
        intentosFallidos = 0;
        mensaje = "Ingrese clave";
        cambiarEstado(ST_IDLE);
      }
      break;

    case ST_CHANGE:
      break;
  }
}

// =====================================================
// CAMBIO DE ESTADO
// =====================================================
void cambiarEstado(EstadoSistema nuevoEstado) {
  estadoActual = nuevoEstado;
  tiempoCambioEstado = relojSistema();
  forzarLCD = true;

  switch (estadoActual) {
    case ST_IDLE:
      cerradura.write(SERVO_CERRADO);
      digitalWrite(PIN_LED_VERDE, LOW);
      digitalWrite(PIN_LED_ROJO, LOW);
      digitalWrite(PIN_BUZZER, LOW);
      claveIngresada = "";
      break;

    case ST_ERR:
      cerradura.write(SERVO_CERRADO);
      digitalWrite(PIN_LED_VERDE, LOW);
      digitalWrite(PIN_LED_ROJO, HIGH);
      digitalWrite(PIN_BUZZER, LOW);
      break;

    case ST_OPEN:
      cerradura.write(SERVO_ABIERTO);
      digitalWrite(PIN_LED_VERDE, HIGH);
      digitalWrite(PIN_LED_ROJO, LOW);
      digitalWrite(PIN_BUZZER, LOW);
      break;

    case ST_ALR:
      cerradura.write(SERVO_CERRADO);
      digitalWrite(PIN_LED_VERDE, LOW);
      break;

    case ST_LOCK:
      cerradura.write(SERVO_CERRADO);
      digitalWrite(PIN_LED_VERDE, LOW);
      digitalWrite(PIN_LED_ROJO, HIGH);
      digitalWrite(PIN_BUZZER, LOW);
      break;

    case ST_CHANGE:
      cerradura.write(SERVO_CERRADO);
      digitalWrite(PIN_LED_VERDE, LOW);
      digitalWrite(PIN_LED_ROJO, HIGH);
      digitalWrite(PIN_BUZZER, LOW);
      break;
  }
}

// =====================================================
// CONTROL DE ALARMA
// =====================================================
void controlarAlarmaVisual() {
  if (estadoActual != ST_ALR) {
    return;
  }

  unsigned long ahora = relojSistema();

  if (ahora - tiempoBlink >= 250) {
    tiempoBlink = ahora;
    salidaBlink = !salidaBlink;

    digitalWrite(PIN_LED_ROJO, salidaBlink);
    digitalWrite(PIN_BUZZER, salidaBlink);
  }
}

// =====================================================
// ACTUALIZACION DEL LCD 16x2
// =====================================================
void actualizarLCD() {
  unsigned long ahora = relojSistema();

  if (!forzarLCD && ahora - tiempoLCD < 300) {
    return;
  }

  tiempoLCD = ahora;
  forzarLCD = false;

  lcd.clear();

  if (estadoActual == ST_IDLE) {
    imprimirLinea(0, "IDLE Int:" + String(intentosFallidos));
    imprimirLinea(1, "Clave:" + claveOculta());
  }

  else if (estadoActual == ST_ERR) {
    imprimirLinea(0, "ERR Int:" + String(intentosFallidos));
    imprimirLinea(1, mensaje);
  }

  else if (estadoActual == ST_OPEN) {
    imprimirLinea(0, "OPEN " + String(tiempoRestante(TIEMPO_APERTURA)) + "s");
    imprimirLinea(1, perfilActivo);
  }

  else if (estadoActual == ST_ALR) {
    imprimirLinea(0, "ALR " + String(tiempoRestante(TIEMPO_ALARMA)) + "s");
    imprimirLinea(1, mensaje);
  }

  else if (estadoActual == ST_LOCK) {
    imprimirLinea(0, "LOCK " + String(tiempoRestante(TIEMPO_BLOQUEO)) + "s");
    imprimirLinea(1, "Espere...");
  }

  else if (estadoActual == ST_CHANGE) {
    imprimirLinea(0, "CAMBIAR CLAVE");
    imprimirLinea(1, "Nueva:" + claveOculta());
  }
}

// =====================================================
// FUNCIONES AUXILIARES DEL LCD
// =====================================================
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

String claveOculta() {
  String oculto = "";

  for (byte i = 0; i < claveIngresada.length(); i++) {
    oculto += "*";
  }

  return oculto;
}

String nombreEstado() {
  switch (estadoActual) {
    case ST_IDLE:
      return "IDLE";

    case ST_ERR:
      return "ERR";

    case ST_OPEN:
      return "OPEN";

    case ST_ALR:
      return "ALR";

    case ST_LOCK:
      return "LOCK";
      
    case ST_CHANGE:
      return "CHG";
  }

  return "----";
}

unsigned long tiempoRestante(unsigned long duracion) {
  unsigned long ahora = relojSistema();
  unsigned long transcurrido = ahora - tiempoCambioEstado;

  if (transcurrido >= duracion) {
    return 0;
  }

  return (duracion - transcurrido + 999) / 1000;
}