/*
  PRUEBA DE FUNCIONALIDAD Y LIMITES - Kit Brazo Robotico
  Arduino UNO R4 WiFi
  v2 - limites configurables + parser de comandos mas robusto

  NUMERACION DE SERVOS:

    Servo 1 -> Gripper
    Servo 2 -> Muneca
    Servo 3 -> Codo
    Servo 4 -> Hombro
    Servo 5 -> Base

  Comandos:
    1 90        -> Gripper a 90 grados
    2 90        -> Muneca a 90 grados
    3 90        -> Codo a 90 grados
    4 90        -> Hombro a 90 grados
    5 90        -> Base a 90 grados

    c           -> Centrar todos
    s           -> Prueba de limites (usa los limites configurados, NO 0-180 fijo)
    info        -> Mostrar posiciones y limites actuales
    lim n mn mx -> Configurar limite min/max del servo n (ej: "lim 3 20 160")
    m           -> Mostrar menu

  IMPORTANTE:
    - Servos alimentados con fuente externa de 5V.
    - GND de la fuente y GND del Arduino deben estar unidos FISICAMENTE
      (revisa esto primero si el brazo se comporta erratico).
    - Encuentra los limites reales de cada servo ANTES de correr 's',
      moviendolos a mano de poco en poco con el comando "<n> <angulo>".
*/

#include <Servo.h>

// ---------- CONFIGURACION DE PINES ----------
// Segun Diagrama_conexiones.png (2026-10-26): A0 base, A1 hombro,
// A5 codo (antes A2: punta de Dupont atorada, 2026-11-04), A3 muneca, A4 pinza (A4/A5 como pines digitales, no se usa I2C).

const uint8_t PIN_GRIPPER = A4;
const uint8_t PIN_MUNECA  = A3;
const uint8_t PIN_CODO    = A5;
const uint8_t PIN_HOMBRO  = A1;
const uint8_t PIN_BASE    = A0;

const uint8_t NUM_SERVOS = 5;

const uint8_t pines[NUM_SERVOS] = {
  PIN_GRIPPER,
  PIN_MUNECA,
  PIN_CODO,
  PIN_HOMBRO,
  PIN_BASE
};

const char* nombres[NUM_SERVOS] = {
  "Gripper",
  "Muneca",
  "Codo",
  "Hombro",
  "Base"
};

Servo servos[NUM_SERVOS];

int posicionActual[NUM_SERVOS];

// ---------- LIMITES DE SEGURIDAD ----------
// OJO: estos valores son un punto de partida conservador (30-150).
// AJUSTALOS a los limites REALES de tu brazo con el comando "lim n mn mx"
// una vez que los hayas encontrado a mano. No los dejes en 0-180 a menos
// que hayas confirmado que el joint realmente puede llegar ahi sin topar.
// La base (ultimo) gira libre, sin topes: usa todo el rango 0-180.
int limiteMin[NUM_SERVOS] = { 30, 30, 30, 30, 0 };
int limiteMax[NUM_SERVOS] = { 150, 150, 150, 150, 180 };

const int PASO = 1;
const int RETARDO_MS = 20;


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(9600);

  while (!Serial) {
    ;
  }

  // Inicializar los 5 servos UNO POR UNO con un pequeno retardo entre
  // cada uno, para no pedirle a la fuente 5 arranques de corriente
  // simultaneos justo al encender.
  for (uint8_t i = 0; i < NUM_SERVOS; i++) {

    // La base usa el rango de pulso completo (500-2500 us, igual que
    // estacion_rfid_v2) para girar mas que con el de fabrica (544-2400).
    if (pines[i] == PIN_BASE) {
      servos[i].attach(pines[i], 500, 2500);
    } else {
      servos[i].attach(pines[i]);
    }

    posicionActual[i] = 90;

    servos[i].write(90);

    delay(150);
  }

  Serial.println(F("=== PRUEBA DE BRAZO ROBOTICO v2 ==="));

  mostrarMenu();
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  if (Serial.available() > 0) {

    String entrada = Serial.readStringUntil('\n');

    entrada.trim();

    procesarComando(entrada);
  }
}


// ============================================================
// MENU
// ============================================================

void mostrarMenu() {

  Serial.println(F("\nComandos disponibles:"));
  Serial.println(F("  m               -> Mostrar este menu"));
  Serial.println(F("  c               -> Centrar todos los servos (90 grados)"));
  Serial.println(F("  s               -> Barrido de LIMITES configurados (no 0-180 fijo)"));
  Serial.println(F("  <n> <angulo>    -> Mover servo n al angulo indicado"));
  Serial.println(F("  lim <n> <mn> <mx> -> Configurar limites del servo n"));
  Serial.println(F("  info            -> Mostrar posiciones y limites actuales"));
  Serial.println();

  for (uint8_t i = 0; i < NUM_SERVOS; i++) {
    Serial.print(i + 1);
    Serial.print(F(" = "));
    Serial.println(nombres[i]);
  }

  Serial.println();
}


// ============================================================
// VALIDACION DE COMANDOS (defensa contra datos corruptos en Serial)
// ============================================================

// Revisa que el string solo contenga digitos, espacios, o un signo
// menos al inicio de un numero. Si hay CUALQUIER caracter raro
// (tipico de ruido electrico en la linea serial), se rechaza el
// comando completo en vez de intentar parsearlo a la fuerza.
bool esComandoNumericoValido(const String &s) {

  if (s.length() == 0) return false;

  bool huboDigito = false;

  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];

    bool esDigito = (c >= '0' && c <= '9');
    bool esEspacio = (c == ' ');
    bool esMenos = (c == '-');

    if (esDigito) huboDigito = true;

    if (!esDigito && !esEspacio && !esMenos) {
      return false; // caracter no esperado -> comando sospechoso, se descarta
    }
  }

  return huboDigito;
}

// Cuenta cuantos "tokens" separados por espacio tiene el string
int contarTokens(const String &s) {
  int tokens = 0;
  bool enToken = false;

  for (unsigned int i = 0; i < s.length(); i++) {
    if (s[i] != ' ') {
      if (!enToken) {
        tokens++;
        enToken = true;
      }
    } else {
      enToken = false;
    }
  }

  return tokens;
}


// ============================================================
// PROCESAR COMANDO
// ============================================================

void procesarComando(String cmd) {

  if (cmd.length() == 0) {
    return;
  }

  if (cmd.equalsIgnoreCase("m")) {
    mostrarMenu();
  }

  else if (cmd.equalsIgnoreCase("c")) {
    centrarTodos();
  }

  else if (cmd.equalsIgnoreCase("s")) {
    probarLimitesTodos();
  }

  else if (cmd.equalsIgnoreCase("info")) {
    mostrarInfo();
  }

  else if (cmd.startsWith("lim ") || cmd.startsWith("LIM ")) {
    procesarComandoLimite(cmd);
  }

  else {

    // Comando de movimiento "<n> <angulo>"
    // Se valida ESTRICTO antes de mover nada: si el string trae basura
    // (ruido de linea, bytes a medias, etc.) se rechaza sin mover ningun
    // servo, en vez de arriesgarse a interpretar un numero equivocado.
    if (!esComandoNumericoValido(cmd) || contarTokens(cmd) != 2) {
      Serial.print(F("Comando no reconocido o corrupto, se ignora: \""));
      Serial.print(cmd);
      Serial.println(F("\""));
      return;
    }

    int espacio = cmd.indexOf(' ');

    int servoSel = cmd.substring(0, espacio).toInt();
    int angulo   = cmd.substring(espacio + 1).toInt();

    moverServoSuave(servoSel, angulo);
  }
}


// ============================================================
// COMANDO: CONFIGURAR LIMITES  ("lim n mn mx")
// ============================================================

void procesarComandoLimite(String cmd) {

  // quita el "lim " del inicio para validar el resto como numerico
  String resto = cmd.substring(4);
  resto.trim();

  if (!esComandoNumericoValido(resto) || contarTokens(resto) != 3) {
    Serial.println(F("Formato invalido. Usa: lim <servo 1-5> <min> <max>"));
    return;
  }

  int primerEspacio = resto.indexOf(' ');
  int segundoEspacio = resto.indexOf(' ', primerEspacio + 1);

  int servoSel = resto.substring(0, primerEspacio).toInt();
  int nuevoMin = resto.substring(primerEspacio + 1, segundoEspacio).toInt();
  int nuevoMax = resto.substring(segundoEspacio + 1).toInt();

  if (servoSel < 1 || servoSel > NUM_SERVOS) {
    Serial.println(F("Numero de servo invalido (1-5)."));
    return;
  }

  if (nuevoMin < 0 || nuevoMax > 180 || nuevoMin >= nuevoMax) {
    Serial.println(F("Rango invalido. Debe cumplir 0 <= min < max <= 180."));
    return;
  }

  int indice = servoSel - 1;

  limiteMin[indice] = nuevoMin;
  limiteMax[indice] = nuevoMax;

  Serial.print(F("Limites actualizados para Servo "));
  Serial.print(servoSel);
  Serial.print(F(" ("));
  Serial.print(nombres[indice]);
  Serial.print(F("): "));
  Serial.print(nuevoMin);
  Serial.print(F(" - "));
  Serial.println(nuevoMax);

  // Si la posicion actual quedo fuera del nuevo rango, la ajusta
  if (posicionActual[indice] < nuevoMin || posicionActual[indice] > nuevoMax) {
    moverServoSuave(servoSel, constrain(posicionActual[indice], nuevoMin, nuevoMax));
  }
}


// ============================================================
// CENTRAR TODOS
// ============================================================

void centrarTodos() {

  Serial.println(F("Centrando todos los servos a 90 grados..."));

  for (uint8_t i = 0; i < NUM_SERVOS; i++) {
    moverServoSuave(i + 1, 90);
  }

  Serial.println(F("Listo."));
}


// ============================================================
// MOSTRAR INFORMACION
// ============================================================

void mostrarInfo() {

  Serial.println(F("--- Posicion y limites actuales ---"));

  for (uint8_t i = 0; i < NUM_SERVOS; i++) {
    Serial.print(F("Servo "));
    Serial.print(i + 1);
    Serial.print(F(" - "));
    Serial.print(nombres[i]);
    Serial.print(F(": "));
    Serial.print(posicionActual[i]);
    Serial.print(F(" grados  (limites: "));
    Serial.print(limiteMin[i]);
    Serial.print(F(" - "));
    Serial.print(limiteMax[i]);
    Serial.println(F(")"));
  }
}


// ============================================================
// MOVER SERVO SUAVEMENTE
// ============================================================

void moverServoSuave(int servoSel, int anguloObjetivo) {

  if (servoSel < 1 || servoSel > NUM_SERVOS) {
    Serial.println(F("Numero de servo invalido (1-5)."));
    return;
  }

  int indice = servoSel - 1;

  int anguloAntesDeLimitar = anguloObjetivo;

  anguloObjetivo = constrain(
    anguloObjetivo,
    limiteMin[indice],
    limiteMax[indice]
  );

  if (anguloObjetivo != anguloAntesDeLimitar) {
    Serial.print(F("(angulo recortado a los limites configurados: "));
    Serial.print(anguloObjetivo);
    Serial.println(F(")"));
  }

  int actual = posicionActual[indice];

  if (actual == anguloObjetivo) {
    Serial.println(F("Ya esta en esa posicion."));
    return;
  }

  int paso = (anguloObjetivo > actual) ? PASO : -PASO;

  Serial.print(F("Moviendo Servo "));
  Serial.print(servoSel);
  Serial.print(F(" ("));
  Serial.print(nombres[indice]);
  Serial.print(F(") de "));
  Serial.print(actual);
  Serial.print(F(" a "));
  Serial.println(anguloObjetivo);

  while (actual != anguloObjetivo) {
    actual += paso;
    servos[indice].write(actual);
    delay(RETARDO_MS);
  }

  posicionActual[indice] = anguloObjetivo;

  Serial.println(F("Movimiento completado."));
}


// ============================================================
// PRUEBA DE LIMITES (usa los limites configurados, no 0-180 fijo)
// ============================================================

void probarLimitesTodos() {

  Serial.println(F("=== INICIANDO PRUEBA DE LIMITES ==="));
  Serial.println(F("Se usan los limites CONFIGURADOS, no 0-180."));
  Serial.println(F("Si un servo suena forzado o vibra, detente y revisa sus limites."));

  for (uint8_t i = 0; i < NUM_SERVOS; i++) {

    Serial.print(F("\n-- Probando Servo "));
    Serial.print(i + 1);
    Serial.print(F(" ("));
    Serial.print(nombres[i]);
    Serial.println(F(") --"));

    moverServoSuave(i + 1, 90);
    delay(500);

    Serial.println(F("Yendo hacia el limite minimo configurado..."));
    moverServoSuave(i + 1, limiteMin[i]);
    delay(500);

    Serial.println(F("Yendo hacia el limite maximo configurado..."));
    moverServoSuave(i + 1, limiteMax[i]);
    delay(500);

    Serial.println(F("Regresando a 90 grados..."));
    moverServoSuave(i + 1, 90);
    delay(500);
  }

  Serial.println(F("\n=== PRUEBA DE LIMITES FINALIZADA ==="));
}
