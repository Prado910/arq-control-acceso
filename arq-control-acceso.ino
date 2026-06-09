/**
 * @file arq-control-acceso.ino
 * @brief Sistema de control de acceso, configuración, monitoreo ambiental e intrusión.
 *
 * Proyecto de Arquitectura Computacional basado en Arduino Mega 2560.
 * El sistema utiliza teclado matricial, LCD 16x2, RFID RC522, sensores analógicos,
 * servomotor, buzzer, LED RGB y una máquina de estados finitos.
 *
 * No se utiliza delay(). La temporización se realiza mediante tareas no bloqueantes
 * y una función de reloj encapsulada.
 *
 * Versión modular: cada pestaña .ino agrupa un módulo funcional.
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
#define T_MENSAJE_INGRESO 2500UL
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

#define UMBRAL_TEMP_BAJA 28
#define UMBRAL_LUZ_BAJA 400

#define UMBRAL_HALL 600
#define UMBRAL_SONIDO 100

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

bool ledErrorActivo = false;
unsigned long tApagarLedError = 0;

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
  Serial.println(valorLuz);
}
