/**
 * @file SistemaSeguridad_ArduinoMega.ino
 * @brief Sistema de control de acceso, configuración, monitoreo ambiental e intrusión.
 *
 * Proyecto de Arquitectura Computacional basado en Arduino Mega 2560.
 * El sistema utiliza teclado matricial, LCD 16x2, RFID RC522, sensores analógicos,
 * servomotor, buzzer, LED RGB y una máquina de estados finitos.
 *
 * No se utiliza delay(). La temporización se realiza mediante tareas no bloqueantes
 * y una función de reloj encapsulada.
 */

#include <Keypad.h>
#include <LiquidCrystal.h>
#include <Servo.h>
#include <SPI.h>
#include <MFRC522.h>
#include <EEPROM.h>

// =====================================================
// PINES DEL SISTEMA
// =====================================================

// LCD 16x2 paralelo: RS, E, D4, D5, D6, D7
#define LCD_RS 12
#define LCD_E  11
#define LCD_D4 5
#define LCD_D5 4
#define LCD_D6 3
#define LCD_D7 2

// Teclado 4x4
#define PIN_FILA_1 25
#define PIN_FILA_2 27
#define PIN_FILA_3 29
#define PIN_FILA_4 31

#define PIN_COLUMNA_1 33
#define PIN_COLUMNA_2 35
#define PIN_COLUMNA_3 37
#define PIN_COLUMNA_4 39

// Actuadores
#define PIN_SERVO 8
#define PIN_BUZZER 6

// LED RGB
#define PIN_LED_ROJO 26
#define PIN_LED_VERDE 24
#define PIN_LED_AZUL 22

// Botón físico
#define PIN_BOTON 10

// Sensores
#define PIN_TEMP A8
#define PIN_LUZ A7
#define PIN_HALL A6
#define PIN_SONIDO A5

// RFID RC522
#define PIN_RFID_RST 9
#define PIN_RFID_SS 53

// =====================================================
// CONFIGURACIÓN GENERAL
// =====================================================

#define FILAS 4
#define COLUMNAS 4

#define TOTAL_ROLES 4
#define ROL_NINGUNO 255

#define LEN_CLAVE 4
#define LEN_ENTRADA 8
#define UID_MAX 21

#define MASTER_CLAVE "0000"

#define MAX_INTENTOS 3
#define MAX_USOS_CREDENCIAL 4

#define SERVO_CERRADO 0
#define SERVO_ABIERTO 90

// Si tu buzzer suena al revés, invierte estos dos valores.
#define BUZZER_ON HIGH
#define BUZZER_OFF LOW

#define LED_ON HIGH
#define LED_OFF LOW

// =====================================================
// TIEMPOS DEL SISTEMA
// =====================================================

#define T_LCD 300UL
#define T_SENSORES 200UL

#define T_APERTURA_SERVO 7000UL
#define T_ERROR 1500UL
#define T_ALARMA_INTENTOS 8000UL
#define T_BLOQUEO 10000UL

#define T_MONITOR_AMBIENTAL 5000UL
#define T_MONITOR_INTRUSOS 2000UL

#define T_ALARMA_AMBIENTAL 4000UL
#define T_ALARMA_INTRUSOS 2000UL

#define T_VENTANA_ALARMAS 12000UL

#define T_ALARMA_LED_ON 300UL
#define T_ALARMA_LED_OFF 700UL

#define T_BLOQUEO_LED_ON 100UL
#define T_BLOQUEO_LED_OFF 500UL

#define T_DEBOUNCE_BOTON 30UL

// =====================================================
// UMBRALES DE SENSORES
// =====================================================

#define UMBRAL_TEMP_BAJA 33.0
#define UMBRAL_LUZ_BAJA 100

#define UMBRAL_HALL 600
#define UMBRAL_SONIDO 600

// =====================================================
// EEPROM
// =====================================================

#define EEPROM_ADDR 0
#define EEPROM_MAGIC 90

// =====================================================
// OBJETOS
// =====================================================

LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
Servo cerradura;
MFRC522 rfid(PIN_RFID_SS, PIN_RFID_RST);

// =====================================================
// TECLADO
// =====================================================

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
// ESTRUCTURAS
// =====================================================

/**
 * @brief Información de cada usuario/rol del sistema.
 */
struct UsuarioData {
  char nombre[12];
  char clave[LEN_CLAVE + 1];
  char uid[UID_MAX];
  byte horaInicio;
  byte horaFin;
  byte usosClave;
  byte usosRFID;
};

/**
 * @brief Estructura persistente en EEPROM.
 */
struct ConfigEEPROM {
  byte magic;
  UsuarioData usuarios[TOTAL_ROLES];
};

/**
 * @brief Estados principales del sistema.
 */
enum EstadoSistema {
  ST_INICIO,
  ST_CONFIG,
  ST_MONITOR_AMBIENTAL,
  ST_MONITOR_INTRUSOS,
  ST_ALARMA,
  ST_BLOQUEO
};

/**
 * @brief Fases internas del estado CONFIG.
 */
enum FaseConfig {
  CFG_MENU,
  CFG_HORARIO_ROL,
  CFG_HORARIO_DATOS,
  CFG_CLAVE_ROL,
  CFG_CLAVE_DATOS,
  CFG_RFID_ROL,
  CFG_RFID_SCAN,
  CFG_HORA_ACTUAL,
  CFG_AUTH_MASTER
};

/**
 * @brief Origen de la alarma activa.
 */
enum OrigenAlarma {
  ALR_NINGUNA,
  ALR_AMBIENTAL,
  ALR_INTRUSOS,
  ALR_INTENTOS
};

// =====================================================
// VARIABLES GLOBALES
// =====================================================

ConfigEEPROM config;

EstadoSistema estadoActual = ST_INICIO;
FaseConfig faseConfig = CFG_MENU;
OrigenAlarma origenAlarma = ALR_NINGUNA;

char entrada[LEN_ENTRADA + 1];
byte idxEntrada = 0;

byte intentosFallidos = 0;
byte rolActivo = ROL_NINGUNO;
byte rolConfig = ROL_NINGUNO;

byte horaActual = 12;

float valorTemp = 0.0;
int valorLuz = 0;
int valorHall = 0;
int valorSonido = 0;

byte conteoIntrusion = 0;
bool condicionIntrusionAnterior = false;

byte conteoAlarmas = 0;
unsigned long inicioVentanaAlarmas = 0;

unsigned long tEstado = 0;
unsigned long tLCD = 0;
unsigned long tSensores = 0;
unsigned long tBlink = 0;
unsigned long tServo = 0;
unsigned long tMensaje = 0;
unsigned long tBotonCambio = 0;

bool servoAbierto = false;
bool salidaBlink = false;
bool botonLecturaAnterior = false;
bool botonEstadoEstable = false;

String mensajeTemporal = "";

// =====================================================
// PROTOTIPOS
// =====================================================

unsigned long relojSistema();

void cargarConfiguracion();
void inicializarConfiguracionDefecto();
void guardarConfiguracion();

void limpiarEntrada();
void agregarDigito(char tecla);
String claveEnmascarada();

void setEstado(EstadoSistema nuevoEstado);
void tareaTeclado();
void tareaBoton();
void tareaRFID();
void tareaSensores();
void tareaServo();
void tareaAlarma();
void tareaLCD();
void actualizarMaquinaEstados();

void procesarTeclaInicio(char tecla);
void procesarTeclaConfig(char tecla);
void procesarTeclaMonitoreo(char tecla);

void confirmarClaveInicio();
void confirmarMasterParaConfig();
void accesoAutorizado(byte rol, bool porRFID);
void accesoFallido(String motivo);

int buscarUsuarioPorClave(String clave);
int buscarUsuarioPorUID(String uid);
bool claveRepetida(String clave);
bool enHorario(byte rol);

void abrirCerradura();
void cerrarCerradura();

void iniciarConfigMenu();
void iniciarAuthMasterConfig();
void guardarHorarioConfig();
void guardarClaveConfig();
void guardarRFIDConfig(String uid);
void guardarHoraActualConfig();

void dispararAlarma(OrigenAlarma origen);
void registrarAlarmaSensor();
void setLED(bool rojo, bool verde, bool azul);

String leerUID();
void imprimirLinea(byte fila, String texto);
unsigned long tiempoRestante(unsigned long duracion);
String nombreEstado();

// =====================================================
// RELOJ DEL SISTEMA
// =====================================================

/**
 * @brief Devuelve el tiempo actual del sistema.
 * @return Tiempo en milisegundos.
 *
 * La palabra millis() solo aparece en esta función para mantener
 * la temporización encapsulada.
 */
unsigned long relojSistema() {
  return millis();
}

// =====================================================
// SETUP
// =====================================================

/**
 * @brief Inicializa periféricos, EEPROM, LCD, RFID, pines y estado inicial.
 */
void setup() {
  Serial.begin(9600);

  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_ROJO, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);
  pinMode(PIN_LED_AZUL, OUTPUT);

  pinMode(PIN_BOTON, INPUT_PULLUP);

  pinMode(PIN_TEMP, INPUT);
  pinMode(PIN_LUZ, INPUT);
  pinMode(PIN_HALL, INPUT);
  pinMode(PIN_SONIDO, INPUT);

  digitalWrite(PIN_BUZZER, BUZZER_OFF);
  setLED(false, false, false);

  cerradura.attach(PIN_SERVO);
  cerrarCerradura();

  lcd.begin(16, 2);
  lcd.clear();

  SPI.begin();
  rfid.PCD_Init();

  cargarConfiguracion();
  limpiarEntrada();

  setEstado(ST_INICIO);
}

// =====================================================
// LOOP PRINCIPAL
// =====================================================

/**
 * @brief Bucle principal basado en tareas no bloqueantes.
 */
void loop() {
  tareaBoton();
  tareaTeclado();
  tareaRFID();
  tareaSensores();
  tareaServo();
  actualizarMaquinaEstados();
  tareaAlarma();
  tareaLCD();
}

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

// =====================================================
// ESTADOS
// =====================================================

/**
 * @brief Cambia el estado principal del sistema.
 * @param nuevoEstado Estado destino.
 */
void setEstado(EstadoSistema nuevoEstado) {
  estadoActual = nuevoEstado;
  tEstado = relojSistema();
  tBlink = relojSistema();
  salidaBlink = false;
  mensajeTemporal = "";

  switch (estadoActual) {
    case ST_INICIO:
      limpiarEntrada();
      rolActivo = ROL_NINGUNO;
      setLED(false, false, true);
      digitalWrite(PIN_BUZZER, BUZZER_OFF);
      break;

    case ST_CONFIG:
      limpiarEntrada();
      iniciarConfigMenu();
      setLED(false, true, true);
      digitalWrite(PIN_BUZZER, BUZZER_OFF);
      break;

    case ST_MONITOR_AMBIENTAL:
      limpiarEntrada();
      setLED(false, true, false);
      digitalWrite(PIN_BUZZER, BUZZER_OFF);
      break;

    case ST_MONITOR_INTRUSOS:
      limpiarEntrada();
      setLED(false, false, true);
      digitalWrite(PIN_BUZZER, BUZZER_OFF);
      break;

    case ST_ALARMA:
      limpiarEntrada();
      setLED(true, false, false);
      digitalWrite(PIN_BUZZER, BUZZER_ON);
      break;

    case ST_BLOQUEO:
      limpiarEntrada();
      cerrarCerradura();
      setLED(true, false, false);
      digitalWrite(PIN_BUZZER, BUZZER_OFF);
      break;
  }
}

/**
 * @brief Actualiza transiciones temporizadas de la máquina de estados.
 */
void actualizarMaquinaEstados() {
  unsigned long ahora = relojSistema();
  unsigned long transcurrido = ahora - tEstado;

  if (mensajeTemporal.length() > 0 && ahora >= tMensaje) {
    mensajeTemporal = "";
  }

  switch (estadoActual) {
    case ST_INICIO:
      break;

    case ST_CONFIG:
      break;

    case ST_MONITOR_AMBIENTAL:
      if (transcurrido >= T_MONITOR_AMBIENTAL) {
        setEstado(ST_MONITOR_INTRUSOS);
      }
      break;

    case ST_MONITOR_INTRUSOS:
      if (transcurrido >= T_MONITOR_INTRUSOS) {
        setEstado(ST_MONITOR_AMBIENTAL);
      }
      break;

    case ST_ALARMA:
      if (origenAlarma == ALR_AMBIENTAL && transcurrido >= T_ALARMA_AMBIENTAL) {
        setEstado(ST_MONITOR_AMBIENTAL);
      }

      else if (origenAlarma == ALR_INTRUSOS && transcurrido >= T_ALARMA_INTRUSOS) {
        setEstado(ST_MONITOR_INTRUSOS);
      }

      else if (origenAlarma == ALR_INTENTOS && transcurrido >= T_ALARMA_INTENTOS) {
        setEstado(ST_BLOQUEO);
      }
      break;

    case ST_BLOQUEO:
      if (transcurrido >= T_BLOQUEO) {
        intentosFallidos = 0;
        setEstado(ST_INICIO);
      }
      break;
  }
}

// =====================================================
// TAREA: BOTÓN
// =====================================================

/**
 * @brief Lee el botón con antirebote por software.
 */
void tareaBoton() {
  unsigned long ahora = relojSistema();
  bool lectura = digitalRead(PIN_BOTON) == LOW;

  if (lectura != botonLecturaAnterior) {
    tBotonCambio = ahora;
    botonLecturaAnterior = lectura;
  }

  if ((ahora - tBotonCambio) >= T_DEBOUNCE_BOTON) {
    if (lectura != botonEstadoEstable) {
      botonEstadoEstable = lectura;

      if (botonEstadoEstable) {
        intentosFallidos = 0;
        cerrarCerradura();
        setEstado(ST_INICIO);
      }
    }
  }
}

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
    iniciarAuthMasterConfig();
  }

  else if (estadoActual == ST_MONITOR_INTRUSOS && tecla == '#') {
    iniciarAuthMasterConfig();
  }
}

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

  setLED(true, false, false);

  if (intentosFallidos >= MAX_INTENTOS) {
    mensajeTemporal = "Max intentos";
    dispararAlarma(ALR_INTENTOS);
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

// =====================================================
// SENSORES
// =====================================================

/**
 * @brief Lee sensores y evalúa condiciones de alarma.
 */
void tareaSensores() {
  unsigned long ahora = relojSistema();

  if (ahora - tSensores < T_SENSORES) {
    return;
  }

  tSensores = ahora;

  int lecturaTemp = analogRead(PIN_TEMP);

  if (lecturaTemp <= 0) {
    lecturaTemp = 1;
  }

  if (lecturaTemp >= 1023) {
    lecturaTemp = 1022;
  }

  float r1 = 10000.0;
  float c1 = 0.001129148;
  float c2 = 0.000234125;
  float c3 = 0.0000000876741;

  float r2 = r1 * (1023.0 / (float)lecturaTemp - 1.0);
  float logR2 = log(r2);

  valorTemp = 1.0 / (c1 + c2 * logR2 + c3 * logR2 * logR2 * logR2);
  valorTemp = valorTemp - 273.15;

  valorLuz = analogRead(PIN_LUZ);
  valorHall = analogRead(PIN_HALL);
  valorSonido = analogRead(PIN_SONIDO);

  if (estadoActual == ST_MONITOR_AMBIENTAL) {
    if (valorTemp < UMBRAL_TEMP_BAJA && valorLuz < UMBRAL_LUZ_BAJA) {
      dispararAlarma(ALR_AMBIENTAL);
    }
  }

  if (estadoActual == ST_MONITOR_INTRUSOS) {
    bool condicionIntrusion = valorHall > UMBRAL_HALL && valorSonido > UMBRAL_SONIDO;

    if (condicionIntrusion && !condicionIntrusionAnterior) {
      conteoIntrusion++;
    }

    condicionIntrusionAnterior = condicionIntrusion;

    if (conteoIntrusion >= 3) {
      conteoIntrusion = 0;
      condicionIntrusionAnterior = false;
      dispararAlarma(ALR_INTRUSOS);
    }
  }
}

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

// =====================================================
// ALARMAS
// =====================================================

/**
 * @brief Activa una alarma según su origen.
 * @param origen Tipo de alarma.
 */
void dispararAlarma(OrigenAlarma origen) {
  origenAlarma = origen;

  if (origen == ALR_AMBIENTAL || origen == ALR_INTRUSOS) {
    registrarAlarmaSensor();

    if (conteoAlarmas >= 3) {
      conteoAlarmas = 0;
      mensajeTemporal = "3 alarmas";
      tMensaje = relojSistema() + T_ERROR;
      setEstado(ST_INICIO);
      return;
    }
  }

  setEstado(ST_ALARMA);
}

/**
 * @brief Registra alarmas consecutivas dentro de una ventana de 12 segundos.
 */
void registrarAlarmaSensor() {
  unsigned long ahora = relojSistema();

  if (inicioVentanaAlarmas == 0 || ahora - inicioVentanaAlarmas > T_VENTANA_ALARMAS) {
    inicioVentanaAlarmas = ahora;
    conteoAlarmas = 0;
  }

  conteoAlarmas++;
}

/**
 * @brief Controla LED rojo y buzzer durante alarma o bloqueo.
 */
void tareaAlarma() {
  unsigned long ahora = relojSistema();

  if (estadoActual == ST_ALARMA) {
    digitalWrite(PIN_BUZZER, BUZZER_ON);

    unsigned long periodoActual = salidaBlink ? T_ALARMA_LED_ON : T_ALARMA_LED_OFF;

    if (ahora - tBlink >= periodoActual) {
      tBlink = ahora;
      salidaBlink = !salidaBlink;

      digitalWrite(PIN_LED_ROJO, salidaBlink ? LED_ON : LED_OFF);
      digitalWrite(PIN_LED_VERDE, LED_OFF);
      digitalWrite(PIN_LED_AZUL, LED_OFF);
    }

    return;
  }

  if (estadoActual == ST_BLOQUEO) {
    digitalWrite(PIN_BUZZER, BUZZER_OFF);

    unsigned long periodoActual = salidaBlink ? T_BLOQUEO_LED_ON : T_BLOQUEO_LED_OFF;

    if (ahora - tBlink >= periodoActual) {
      tBlink = ahora;
      salidaBlink = !salidaBlink;

      digitalWrite(PIN_LED_ROJO, salidaBlink ? LED_ON : LED_OFF);
      digitalWrite(PIN_LED_VERDE, LED_OFF);
      digitalWrite(PIN_LED_AZUL, LED_OFF);
    }

    return;
  }
}

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