/*
  PROYECTO 3 - Brazo robotico: estacion de asistencia con busqueda y verificacion RFID  (v2, SIN PCA9685)
  Arduino UNO R4 WiFi + servos directos (Servo.h) + RC522 + 5 sensores IR + app App Inventor por BLE

  Cambios contra la v1:
    - Sin PCA9685: los servos van directo a A0, A1, A5, A3, A4 (base, hombro, codo, muneca, pinza; solo la senal; codo en A5 desde 2026-11-04);
      los 5V/GND de los servos salen
      de la fuente por el Wago, NUNCA del pin 5V del Arduino. GND de la fuente unido al GND del Arduino.
    - La calibracion (poses, pinza y UID de las etiquetas de los cubos) se guarda en la EEPROM: ya no hay que pegar
      lineas en el codigo ni volver a subirlo. Sobrevive a apagar y a volver a cargar el programa.
    - Arranque escalonado: los servos se conectan uno por uno para no tirar la fuente con el pico de corriente.
    - Arreglado: "trabajando" se quedaba en true si un comando de calibracion salia con error
      (servo invalido / estacion invalida) y a partir de ahi todo respondia OCUPADO.

  Que hace:
    - Lee siempre los 5 sensores IR y sabe que slots estan ocupados.
    - "DAME n": visita solo los slots ocupados; toma el cubo, lo lleva al checkpoint, lee su UID.
        si es el cubo n -> lo lleva a la entrega;  si no -> lo regresa a su slot y sigue.
        si se acaban los slots -> avisa que no lo encontro.
    - "ORDENA" (extra): deja el cubo 1 en el slot 1, el 2 en el 2, ... usando la entrega como lugar de espera.
    - Avisa cada paso por BLE (y por el Monitor Serie) para que la app muestre el estado y hable.

  Librerias (Herramientas > Administrar bibliotecas):
    Servo (Arduino), MFRC522 (GithubCommunity), ArduinoBLE          -- EEPROM ya viene con la placa

  Calibracion (Monitor Serie a 115200, 'Nueva linea'), ver GUIA_PROYECTO.pdf, seccion 7:
    1) J 5 ang  hasta que la pinza abra/cierre bien;  PINZA ABIERTA / PINZA CERRADA
    2) J s ang  para mover cada servo;  GUARDAR HOME (reposo y altura para girar);  GUARDAR e  en cada estacion
    3) UID n  con el cubo n encima del checkpoint (solo si se cambia una etiqueta: ya vienen los de fabrica)
    4) TABLA  para revisar todo lo guardado
*/
#include <stddef.h>
#include <SPI.h>
#include <EEPROM.h>
#include <Servo.h>
#include <MFRC522.h>
#include <ArduinoBLE.h>

// =====================================================================  CONFIGURACION
// ---------- Servos directos al Arduino (solo la senal; 5V y GND desde la fuente) ----------
enum Articulacion { BASE, HOMBRO, CODO, MUNECA, GIRO, PINZA, N_SERVOS };
const char *NOMBRE_SERVO[N_SERVOS] = {"base", "hombro", "codo", "muneca", "giro", "pinza"};
const uint8_t SIN_SERVO = 255;                     // 255 = el brazo no tiene esa articulacion
// Pines de los servos:
//   1 = cableado final (GUIA_CONEXION.pdf): base A0, hombro A1, codo A5, muneca A3, pinza A4
//   0 = cableado viejo (antes del 26 de octubre): base D7, hombro D8, codo A0, muneca A1, pinza A2
#define SERVOS_ORDEN_NUEVO 1
#if SERVOS_ORDEN_NUEVO
// 2026-11-04: CODO pasado de A2 a A5 (se quedo la punta de un Dupont atorada en A2)
const uint8_t PIN_SERVO[N_SERVOS] = {A0, A1, A5, A3, SIN_SERVO, A4};   // (A4/A5 como pines digitales: no se usa I2C)
#else
const uint8_t PIN_SERVO[N_SERVOS] = {7, 8, A0, A1, SIN_SERVO, A2};
#endif
const int MIN_US[N_SERVOS]        = {500, 500, 500, 500, 500, 500};     // pulso a 0 grados
const int MAX_US[N_SERVOS]        = {2500, 2500, 2500, 2500, 2500, 2500};  // pulso a 180 grados
const uint8_t ORDEN_ARRANQUE[] = {PINZA, MUNECA, CODO, HOMBRO, BASE};   // uno por uno al encender
const unsigned long MS_ENTRE_SERVOS = 600;

const uint8_t MS_POR_PASO = 25;                    // velocidad: mas grande = mas lento y suave (antes 12)
const float   GRADOS_POR_PASO = 1.0;

// ---------- Estaciones (mismo orden que en la placa impresa) ----------
enum Estacion { ENTREGA, S1, S2, S3, S4, S5, CHECKPOINT, N_EST };
const char *NOMBRE_EST[N_EST] = {"ENTREGA", "S1", "S2", "S3", "S4", "S5", "CP"};

// Pose = angulos de base, hombro, codo, muneca, giro (la pinza va aparte).
struct Pose { float a[5]; };

// VALORES DE EJEMPLO: solo se usan si la EEPROM esta vacia (o despues de BORRAR SI).
// Lo bueno se calibra con GUARDAR y queda en la EEPROM.
const Pose POSE_ALTA_DEF[N_EST] = {  // brazo arriba de la estacion, a altura segura (~7 cm sobre la placa)
  /* ENTREGA */ {{180, 100, 70, 90, 90}},
  /* S1      */ {{150, 100, 70, 90, 90}},
  /* S2      */ {{120, 100, 70, 90, 90}},
  /* S3      */ {{ 60, 100, 70, 90, 90}},
  /* S4      */ {{ 30, 100, 70, 90, 90}},
  /* S5      */ {{  0, 100, 70, 90, 90}},
  /* CP      */ {{ 90, 100, 70, 90, 90}},
};
const Pose POSE_BAJA_DEF[N_EST] = {  // pinza a la altura de agarre / de soltar
  /* ENTREGA */ {{180, 70, 100, 90, 90}},
  /* S1      */ {{150, 70, 100, 90, 90}},
  /* S2      */ {{120, 70, 100, 90, 90}},
  /* S3      */ {{ 60, 70, 100, 90, 90}},
  /* S4      */ {{ 30, 70, 100, 90, 90}},
  /* S5      */ {{  0, 70, 100, 90, 90}},
  /* CP      */ {{ 90, 85, 85, 90, 90}},   // el checkpoint esta mas alto (arriba de la torre, ~5 cm)
};
const Pose POSE_HOME_DEF = {{90, 110, 60, 90, 90}};
const float PINZA_ABIERTA_DEF = 40;                // grados: abierta ~51 mm (el cubo mide 40)
const float PINZA_CERRADA_DEF = 95;                // grados: aprieta el cubo sin forzar el servo

// ---------- Sensores IR (FC-51: salida LOW cuando detecta) ----------
const uint8_t PIN_IR[5] = {2, 3, 4, 5, 6};        // slot 1..5
const uint8_t IR_DETECTA = LOW;

// ---------- RC522 por SPI, todo junto en D9..D13 (alimentado a 3.3 V) ----------
// En el orden de la placa del lector: SDA=D10, SCK=D13, MOSI=D11, MISO=D12, IRQ sin conectar, GND, RST=D9, 3.3V.
// SCK, MOSI y MISO no se pueden mover: son los pines SPI del Arduino (fijos por hardware).
const uint8_t RC522_SS = 10, RC522_RST = 9;

// ---------- Cubos ----------
const char *NOMBRE_CUBO[5] = {"CUBO 1", "CUBO 2", "CUBO 3", "CUBO 4", "CUBO 5"};
const int DESCONOCIDO = 9;          // tag que no esta en la tabla
const int SIN_LECTURA = 0;

// UID de las etiquetas adhesivas de cada cubo (leidas con prueba_rfid el 2026-10-26, MIFARE Ultralight).
// Son los valores de fabrica: se usan si en la EEPROM no hay uno registrado para ese cubo, o despues
// de BORRAR SI. Con el comando UID n se pueden cambiar sin tocar el codigo.
// Ojo: el 1 y el 3 (2E / 2F) y el 2 y el 4 (53 / 52) solo cambian en un byte.
const byte UID_CUBO_DEF[5][7] = {
  {0xFF, 0x0F, 0x2E, 0x49, 0xBA, 0x00, 0x00},   // cubo 1
  {0xFF, 0x0F, 0x53, 0x4A, 0xBA, 0x00, 0x00},   // cubo 2
  {0xFF, 0x0F, 0x2F, 0x49, 0xBA, 0x00, 0x00},   // cubo 3
  {0xFF, 0x0F, 0x52, 0x4A, 0xBA, 0x00, 0x00},   // cubo 4
  {0xFF, 0x0F, 0x30, 0x49, 0xBA, 0x00, 0x00},   // cubo 5
};
const byte LARGO_UID_DEF = 7;

// ---------- BLE (los mismos UUID van en la app) ----------
#define UUID_SERVICIO "19b10000-e8f2-537e-4f6c-d104768a1214"
#define UUID_COMANDO  "19b10001-e8f2-537e-4f6c-d104768a1214"   // la app ESCRIBE aqui: "DAME 3", "ORDENA", ...
#define UUID_ESTADO   "19b10002-e8f2-537e-4f6c-d104768a1214"   // el Arduino NOTIFICA aqui: "BUSCANDO:3", ...
const char *NOMBRE_BLE = "BrazoRFID";

// =====================================================================  CALIBRACION EN EEPROM
struct UIDCubo { byte uid[7]; byte largo; };
struct Calibracion {
  uint32_t marca;
  Pose alta[N_EST];
  Pose baja[N_EST];
  Pose home;
  float pinzaAbierta, pinzaCerrada;
  UIDCubo cubo[5];
  uint16_t suma;                    // si no cuadra, la EEPROM tiene basura -> valores de ejemplo
};
const uint32_t MARCA_EEPROM = 0xB4A20002;          // cambiarla si se cambia la estructura
Calibracion cal;

uint16_t sumaCal(const Calibracion &c) {
  const byte *p = (const byte *)&c;
  uint16_t s = 0;
  for (size_t i = 0; i < offsetof(Calibracion, suma); i++) s = s * 31 + p[i];
  return s;
}

void calPorDefecto() {
  memset(&cal, 0, sizeof(cal));
  for (int e = 0; e < N_EST; e++) { cal.alta[e] = POSE_ALTA_DEF[e]; cal.baja[e] = POSE_BAJA_DEF[e]; }
  cal.home = POSE_HOME_DEF;
  cal.pinzaAbierta = PINZA_ABIERTA_DEF;
  cal.pinzaCerrada = PINZA_CERRADA_DEF;
  for (int c = 0; c < 5; c++) uidDeFabrica(c);
}

void uidDeFabrica(int c) {
  memcpy(cal.cubo[c].uid, UID_CUBO_DEF[c], LARGO_UID_DEF);
  cal.cubo[c].largo = LARGO_UID_DEF;
}

bool cargarCal() {
  EEPROM.get(0, cal);
  if (cal.marca == MARCA_EEPROM && cal.suma == sumaCal(cal)) return true;
  calPorDefecto();
  return false;
}

void guardarCal() {
  cal.marca = MARCA_EEPROM;
  cal.suma = sumaCal(cal);
  EEPROM.put(0, cal);
  Serial.println(F("  (guardado en EEPROM)"));
}

// =====================================================================  OBJETOS Y ESTADO
Servo servo[N_SERVOS];
bool conectado[N_SERVOS];           // ya se le hizo attach (arranque escalonado)
MFRC522 rfid(RC522_SS, RC522_RST);
BLEService svcBLE(UUID_SERVICIO);
BLEStringCharacteristic chComando(UUID_COMANDO, BLEWrite | BLEWriteWithoutResponse, 20);
BLEStringCharacteristic chEstado(UUID_ESTADO, BLERead | BLENotify, 20);

float angulo[N_SERVOS];             // donde esta cada servo ahora
bool ocupado[5];                    // slot ocupado (ya filtrado)
bool leerSiempre = true;            // lectura continua del RFID en reposo (LEER SI/NO)
bool sinIR = false;                 // modo de prueba sin sensores IR: los slots se dicen con SINIR 11000
uint8_t contIR[5];
bool trabajando = false;            // true mientras atiende cualquier comando
bool cancelar = false;              // lo prende el comando PARA
bool traeCubo = false;              // la pinza trae un cubo

// =====================================================================  UTILIDADES
void enviar(const String &msg) {    // estado hacia la app (max 20 caracteres) y al Monitor Serie
  Serial.print(F(">> ")); Serial.println(msg);
  chEstado.writeValue(msg);
}

String textoSlots() {
  String s = "SLOTS:";
  for (int i = 0; i < 5; i++) s += ocupado[i] ? '1' : '0';
  return s;
}

void leerIR() {                     // filtro: 5 lecturas iguales seguidas (~100 ms) para cambiar de estado
  static unsigned long t = 0;
  if (sinIR) return;                 // modo de prueba: ocupado[] lo pone el comando SINIR
  if (millis() - t < 20) return;
  t = millis();
  bool cambio = false;
  for (int i = 0; i < 5; i++) {
    bool ahora = digitalRead(PIN_IR[i]) == IR_DETECTA;
    if (ahora != ocupado[i]) {
      if (++contIR[i] >= 5) { ocupado[i] = ahora; contIR[i] = 0; cambio = true; }
    } else contIR[i] = 0;
  }
  if (cambio && !trabajando) enviar(textoSlots());
}

void procesarComando(String cmd);

void servicio() {                   // se llama todo el tiempo, tambien a media maniobra
  BLE.poll();
  leerIR();
  if (chComando.written()) procesarComando(chComando.value());
  if (Serial.available()) {
    String l = Serial.readStringUntil('\n');
    procesarComando(l);
  }
}

void esperar(unsigned long ms) {
  unsigned long t0 = millis();
  while (millis() - t0 < ms) servicio();
}

// =====================================================================  SERVOS
void escribirServo(int s, float grados) {
  if (PIN_SERVO[s] == SIN_SERVO) return;
  grados = constrain(grados, 0, 180);
  angulo[s] = grados;
  if (!conectado[s]) return;
  int us = MIN_US[s] + (long)(grados * (MAX_US[s] - MIN_US[s]) / 180.0);
  servo[s].writeMicroseconds(us);
}

// Conecta los servos uno por uno en su posicion de reposo: cada servo pega un jalon de corriente al
// arrancar, y si arrancan los 5 juntos la fuente cae y el Arduino se reinicia.
void arrancarServos() {
  for (uint8_t i = 0; i < sizeof(ORDEN_ARRANQUE); i++) {
    int s = ORDEN_ARRANQUE[i];
    if (PIN_SERVO[s] == SIN_SERVO) continue;
    servo[s].attach(PIN_SERVO[s], MIN_US[s], MAX_US[s]);
    conectado[s] = true;
    escribirServo(s, angulo[s]);
    Serial.print(F("  servo ")); Serial.print(NOMBRE_SERVO[s]); Serial.print(F(" listo en el pin "));
    if (PIN_SERVO[s] >= A0) { Serial.print('A'); Serial.println(PIN_SERVO[s] - A0); } else { Serial.print('D'); Serial.println(PIN_SERVO[s]); }
    delay(MS_ENTRE_SERVOS);
  }
}

// Mueve todas las articulaciones a la vez, en pasos chicos (movimiento progresivo, sin saltos).
bool moverA(const Pose &p, float pinza) {
  float ini[N_SERVOS], fin[N_SERVOS];
  for (int s = 0; s < N_SERVOS; s++) ini[s] = angulo[s];
  for (int s = 0; s < 5; s++) fin[s] = p.a[s];
  fin[PINZA] = pinza;
  float maxDelta = 0;
  for (int s = 0; s < N_SERVOS; s++) if (PIN_SERVO[s] != SIN_SERVO) maxDelta = max(maxDelta, abs(fin[s] - ini[s]));
  int pasos = max(1, (int)(maxDelta / GRADOS_POR_PASO));
  for (int k = 1; k <= pasos; k++) {
    float f = (float)k / pasos;
    f = f * f * (3 - 2 * f);                       // arranca y frena suave
    for (int s = 0; s < N_SERVOS; s++) escribirServo(s, ini[s] + (fin[s] - ini[s]) * f);
    esperar(MS_POR_PASO);
    if (cancelar) return false;
  }
  return true;
}

Pose poseActual() {
  Pose p;
  for (int s = 0; s < 5; s++) p.a[s] = angulo[s];
  return p;
}

bool moverPinza(float grados) { return moverA(poseActual(), grados); }

int enEstacion = -1;                                 // estacion donde esta el brazo ahorita (-1 = en el aire)

bool irHome() {
  float pz = traeCubo ? cal.pinzaCerrada : cal.pinzaAbierta;
  if (enEstacion >= 0) { Pose h = poseActual(); h.a[HOMBRO] = cal.home.a[HOMBRO]; if (!moverA(h, pz)) return false; }
  enEstacion = -1;
  return moverA(cal.home, pz);
}

// Traslado seguro: sube a la postura de HOME (altura para girar sin chocar con la torre),
// gira la base en el aire y luego llega a la estacion (arriba).
// En TODAS las estaciones se mueve por partes, como en el checkpoint (el hombro va al ultimo):
// al bajar: codo y muneca primero, luego hombro; al subir: hombro primero, luego codo y muneca.
bool irArribaDe(int e) {
  float pz = traeCubo ? cal.pinzaCerrada : cal.pinzaAbierta;
  Pose subir = poseActual();
  if (enEstacion >= 0) {                             // salir de una estacion: primero sube el hombro
    Pose h = poseActual(); h.a[HOMBRO] = cal.home.a[HOMBRO];
    if (!moverA(h, pz)) return false;
  }
  enEstacion = -1;
  subir.a[HOMBRO] = cal.home.a[HOMBRO];
  subir.a[CODO] = cal.home.a[CODO];
  subir.a[MUNECA] = cal.home.a[MUNECA];
  if (!moverA(subir, pz)) return false;
  Pose girar = subir;
  girar.a[BASE] = cal.alta[e].a[BASE];
  if (!moverA(girar, pz)) return false;
  {                                                  // llegar a la estacion: primero codo y muneca
    Pose c = girar; c.a[CODO] = cal.alta[e].a[CODO]; c.a[MUNECA] = cal.alta[e].a[MUNECA];
    if (!moverA(c, pz)) return false;
  }
  if (!moverA(cal.alta[e], pz)) return false;
  enEstacion = e;
  return true;
}

// =====================================================================  MANIOBRAS
bool tomarDe(int e) {
  if (!irArribaDe(e)) return false;
  if (!moverPinza(cal.pinzaAbierta)) return false;
  if (!moverA(cal.baja[e], cal.pinzaAbierta)) return false;
  if (!moverPinza(cal.pinzaCerrada)) return false;
  esperar(250);
  traeCubo = true;
  return moverA(cal.alta[e], cal.pinzaCerrada);
}

bool soltarEn(int e) {
  if (!irArribaDe(e)) return false;
  if (!moverA(cal.baja[e], cal.pinzaCerrada)) return false;
  if (!moverPinza(cal.pinzaAbierta)) return false;
  traeCubo = false;
  esperar(200);
  return moverA(cal.alta[e], cal.pinzaAbierta);
}

// ---------- RFID ----------
// Si el lector se reinició solo (un parpadeo de 3.3 V, GND o RST), su antena queda apagada y
// ya no lee nada: se detecta por TxControlReg y se vuelve a configurar.
void asegurarLector() {
  byte v = rfid.PCD_ReadRegister(rfid.VersionReg);
  if (v == 0x00 || v == 0xFF) return;                 // no contesta ahorita: no se puede hacer nada
  if ((rfid.PCD_ReadRegister(rfid.TxControlReg) & 0x03) != 0x03) {
    rfid.PCD_Init();
    rfid.PCD_SetAntennaGain(rfid.RxGain_max);
    Serial.println(F("  !! el RC522 se reinicio solo (revisa la soldadura de sus pines); se volvio a configurar"));
  }
}

bool leerUID(byte *uid, byte &largo, unsigned long espera) {
  unsigned long t0 = millis();
  asegurarLector();
  while (millis() - t0 < espera) {
    servicio();
    if (cancelar) return false;
    byte atqa[2]; byte tam = sizeof(atqa);
    // WakeupA tambien despierta a una tarjeta que ya se leyo antes (quedo en HALT)
    bool hay = rfid.PICC_WakeupA(atqa, &tam) == MFRC522::STATUS_OK || rfid.PICC_IsNewCardPresent();
    if (hay && rfid.PICC_ReadCardSerial()) {
      largo = min((byte)rfid.uid.size, (byte)7);
      memcpy(uid, rfid.uid.uidByte, largo);
      rfid.PICC_HaltA();
      return true;
    }
    delay(30);
  }
  return false;
}

int cuboDeUID(const byte *uid, byte largo) {
  for (int c = 0; c < 5; c++)
    if (cal.cubo[c].largo == largo && memcmp(cal.cubo[c].uid, uid, largo) == 0) return c + 1;
  return DESCONOCIDO;
}

bool uidVacio(int c) {
  for (int i = 0; i < cal.cubo[c].largo; i++) if (cal.cubo[c].uid[i]) return false;
  return true;
}

void imprimirUID(const byte *uid, byte largo) {
  Serial.print(F("{"));
  for (byte i = 0; i < largo; i++) {
    Serial.print(F("0x")); if (uid[i] < 0x10) Serial.print('0');
    Serial.print(uid[i], HEX); if (i < largo - 1) Serial.print(F(", "));
  }
  Serial.print(F("}, ")); Serial.print(largo);
}

// Con el cubo en la pinza: lo asienta en la torre, lee el UID y lo vuelve a subir.
int verificarEnCheckpoint() {
  if (!irArribaDe(CHECKPOINT)) return SIN_LECTURA;
  if (!moverA(cal.baja[CHECKPOINT], cal.pinzaCerrada)) return SIN_LECTURA;
  // suelta el cubo en la torre para que asiente parejo sobre el lector, lee y lo vuelve a agarrar
  moverPinza(cal.pinzaAbierta); esperar(300);
  byte uid[7], largo = 0;
  bool ok = leerUID(uid, largo, 2000);
  if (!ok && !cancelar) {                           // segundo intento: lo agarra, lo levanta y lo vuelve a asentar
    moverPinza(cal.pinzaCerrada); esperar(250);
    moverA(cal.alta[CHECKPOINT], cal.pinzaCerrada);
    moverA(cal.baja[CHECKPOINT], cal.pinzaCerrada);
    moverPinza(cal.pinzaAbierta); esperar(300);
    ok = leerUID(uid, largo, 2000);
  }
  moverPinza(cal.pinzaCerrada); esperar(250);       // lo vuelve a agarrar
  moverA(cal.alta[CHECKPOINT], cal.pinzaCerrada);
  if (!ok) return SIN_LECTURA;
  Serial.print(F("   UID leido: ")); imprimirUID(uid, largo); Serial.println();
  return cuboDeUID(uid, largo);
}

// Confirma con el IR que el slot quedo como se espera (true = ocupado)
bool slotQueda(int slot, bool esperado) {
  if (sinIR) { ocupado[slot - 1] = esperado; return true; }   // sin IR: se da por hecho
  unsigned long t0 = millis();
  while (millis() - t0 < 1200) { servicio(); if (ocupado[slot - 1] == esperado) return true; }
  return false;
}

// Toma un cubo de un slot igual que PRUEBA. El IR ya NO hace soltar el cubo: un sensor puede ver
// el cubo del slot de al lado y el brazo soltaba un cubo bien agarrado. Si el IR sigue viendo algo
// solo se avisa; el lector RFID del checkpoint confirma despues si de verdad trae el cubo.
bool tomarDeSlot(int slot) {
  if (!tomarDe(slot)) return false;
  if (!slotQueda(slot, false)) enviar("AVISO:IR" + String(slot));
  return true;
}

// =====================================================================  TAREA 1: BUSCAR Y ENTREGAR
void buscarCubo(int n) {
  enviar("BUSCANDO:" + String(n));
  bool revisar[5];
  for (int i = 0; i < 5; i++) revisar[i] = ocupado[i];   // foto de los slots ocupados al empezar
  enviar(textoSlots());
  bool entregado = false;

  // orden de revision: primero el slot con el mismo numero del cubo pedido, luego los demas (1..5)
  int orden[5] = {n, 0, 0, 0, 0};
  for (int s = 1, k = 1; s <= 5; s++) if (s != n) orden[k++] = s;
  for (int i = 0; i < 5 && !entregado; i++) {
    int slot = orden[i];
    if (!revisar[slot - 1] || !ocupado[slot - 1]) continue;   // vacio -> ni se acerca
    enviar("TOMANDO:" + String(slot));
    if (!tomarDeSlot(slot)) break;
    enviar("VERIFICANDO:" + String(slot));
    int c = verificarEnCheckpoint();
    if (cancelar) break;
    if (c == n) {
      enviar("CORRECTO:" + String(n));
      enviar("ENTREGANDO:" + String(n));
      if (!soltarEn(ENTREGA)) break;
      enviar("ENTREGADO:" + String(n));
      entregado = true;
    } else {
      if (c == SIN_LECTURA) enviar("SIN_LECTURA:" + String(slot));
      else if (c == DESCONOCIDO) enviar("DESCONOCIDO:" + String(slot));
      else enviar("INCORRECTO:" + String(c));
      enviar("REGRESANDO:" + String(slot));
      if (!soltarEn(slot)) break;
      if (!slotQueda(slot, true)) enviar("ERROR:SLOT" + String(slot));
    }
  }
  if (cancelar) enviar("CANCELADO");
  else if (!entregado) enviar("NO_ENCONTRADO:" + String(n));
  cancelar = false;
  irHome();
}

// =====================================================================  TAREA 2 (EXTRA): ORDENAR
// tiene[1..5] = cubo en cada slot (0 vacio, 1..5, 9 desconocido); espera = cubo en la zona de entrega.
// La misma logica se probo en una simulacion con los 4,051 acomodos posibles (0 errores, max 7 movimientos).
int tiene[6];
int espera = 0;

bool moverCubo(int de, int a) {     // de/a: 1..5 = slot, 0 = ENTREGA
  enviar("MOVIENDO:" + String(de) + ">" + String(a));
  bool ok = (de == 0) ? tomarDe(ENTREGA) : tomarDeSlot(de);
  if (!ok) return false;
  if (!soltarEn(a == 0 ? ENTREGA : a)) return false;
  int c = (de == 0) ? espera : tiene[de];
  if (de == 0) espera = 0; else tiene[de] = 0;
  if (a == 0) espera = c; else tiene[a] = c;
  return true;
}

void ordenarCubos() {
  enviar("ORDENANDO");
  espera = 0;                                       // la zona de entrega debe estar vacia al empezar
  // 1) inventario: que cubo hay en cada slot
  for (int s = 1; s <= 5; s++) {
    tiene[s] = 0;
    if (!ocupado[s - 1]) continue;
    enviar("REVISANDO:" + String(s));
    if (!tomarDeSlot(s)) goto fin;
    int c = verificarEnCheckpoint();
    if (cancelar) goto fin;
    tiene[s] = (c == SIN_LECTURA) ? DESCONOCIDO : c;
    enviar("HAY:" + String(s) + "=" + String(tiene[s]));
    if (!soltarEn(s)) goto fin;
  }
  // 2) acomodo
  for (int vuelta = 0; vuelta < 30; vuelta++) {
    if (cancelar) goto fin;
    if (espera >= 1 && espera <= 5 && tiene[espera] == 0) {          // el que espera ya tiene lugar
      if (!moverCubo(0, espera)) goto fin; continue;
    }
    int j = 0;
    for (int k = 1; k <= 5; k++)
      if (tiene[k] >= 1 && tiene[k] <= 5 && tiene[k] != k && tiene[tiene[k]] == 0) { j = k; break; }
    if (j) { if (!moverCubo(j, tiene[j])) goto fin; continue; }         // a su lugar directo
    int malo = 0, desc = 0, vacio = 0;
    for (int k = 1; k <= 5; k++) {
      if (tiene[k] != 0 && tiene[k] != k && !malo) malo = k;
      if (tiene[k] == DESCONOCIDO && !desc) desc = k;
      if (tiene[k] == 0 && !vacio) vacio = k;
    }
    if (!malo) break;                                                  // ya esta ordenado
    if (desc && espera == 0) { if (!moverCubo(desc, 0)) goto fin; }
    else if (desc && vacio) { if (!moverCubo(desc, vacio)) goto fin; }
    else if (vacio)         { if (!moverCubo(malo, vacio)) goto fin; }
    else if (espera == 0)   { if (!moverCubo(malo, 0)) goto fin; }
    else { enviar("ERROR:SIN_LUGAR"); goto fin; }
  }
  for (int k = 1; k <= 5; k++)                                         // el desconocido termina en la entrega
    if (tiene[k] == DESCONOCIDO && espera == 0) { moverCubo(k, 0); break; }
  for (int c = 1; c <= 5; c++) if (tiene[c] != c) enviar("FALTA:" + String(c));
  if (espera == DESCONOCIDO) enviar("DESCONOCIDO:E");
  enviar("ORDENADO");
fin:
  if (cancelar) enviar("CANCELADO");
  cancelar = false;
  irHome();
}

// =====================================================================  COMANDOS
int estacionDeTexto(String t) {
  t.trim(); t.toUpperCase();
  if (t == "E" || t == "ENTREGA" || t == "0") return ENTREGA;
  if (t == "CP" || t == "CHECK" || t == "CHECKPOINT") return CHECKPOINT;
  if (t.startsWith("S")) t = t.substring(1);
  int n = t.toInt();
  return (n >= 1 && n <= 5) ? n : -1;
}

int numeroDeCubo(String t) {        // acepta "3", "TRES", o "CUBO 3"
  t.trim(); t.toUpperCase();
  const char *palabras[5] = {"UNO", "DOS", "TRES", "CUATRO", "CINCO"};
  for (int i = 0; i < 5; i++) {
    if (t == palabras[i]) return i + 1;
    if (t == NOMBRE_CUBO[i]) return i + 1;
  }
  int n = t.toInt();
  return (n >= 1 && n <= 5) ? n : 0;
}

void imprimirPose(const char *titulo, const Pose &p) {
  Serial.print(F("  /* ")); Serial.print(titulo); Serial.print(F(" */ {{"));
  for (int s = 0; s < 5; s++) { Serial.print((int)p.a[s]); if (s < 4) Serial.print(F(", ")); }
  Serial.println(F("}},"));
}

void imprimirTabla() {
  Serial.println(F("\n---- Calibracion guardada ----"));
  Serial.println(F("POSE_ALTA:"));
  for (int e = 0; e < N_EST; e++) imprimirPose(NOMBRE_EST[e], cal.alta[e]);
  Serial.println(F("POSE_BAJA:"));
  for (int e = 0; e < N_EST; e++) imprimirPose(NOMBRE_EST[e], cal.baja[e]);
  imprimirPose("HOME", cal.home);
  Serial.print(F("  pinza abierta = ")); Serial.print(cal.pinzaAbierta);
  Serial.print(F("   cerrada = ")); Serial.println(cal.pinzaCerrada);
  Serial.println(F("UID de los cubos:"));
  for (int c = 0; c < 5; c++) {
    Serial.print(F("  ")); Serial.print(NOMBRE_CUBO[c]); Serial.print(F(": "));
    if (uidVacio(c)) Serial.println(F("(sin registrar)"));
    else { imprimirUID(cal.cubo[c].uid, cal.cubo[c].largo); Serial.println(); }
  }
}

void ayuda() {
  Serial.println(F("\nComandos (Monitor Serie a 115200, 'Nueva linea'):"));
  Serial.println(F("  DAME n        buscar y entregar el cubo n (1-5)     ORDENA   ordenar los cubos"));
  Serial.println(F("  ESTADO        slots ocupados                          HOME     ir a reposo"));
  Serial.println(F("  PARA          detener lo que este haciendo            CENTRAR  todos a 90"));
  Serial.println(F("  -- calibracion (todo se guarda solo en la EEPROM) --"));
  Serial.println(F("  J s ang       mover el servo s (0 base,1 hombro,2 codo,3 muneca,5 pinza)"));
  Serial.println(F("  POSE          imprimir los angulos actuales"));
  Serial.println(F("  GUARDAR e     guarda la pose actual (donde agarra/suelta) en la estacion e (E,1..5,CP)"));
  Serial.println(F("  GUARDAR e ALTA|BAJA   (opcional) poses distintas para llegar y para agarrar"));
  Serial.println(F("  GUARDAR HOME  guarda la pose actual como reposo y altura para girar"));
  Serial.println(F("  PINZA ABIERTA|CERRADA guarda el angulo actual de la pinza"));
  Serial.println(F("  UID n         registra el cubo puesto en el checkpoint como cubo n (UID solo = leer)"));
  Serial.println(F("  TABLA         muestra toda la calibracion      BORRAR SI  vuelve a los valores de ejemplo"));
  Serial.println(F("  IRA e         ir arriba de la estacion e       PRUEBA e   tomar y soltar en e"));
  Serial.println(F("  LEER SI|NO    lectura continua del RFID en reposo (avisa al poner/quitar un cubo)"));
  Serial.println(F("  TOMA e        agarra el cubo de la estacion e  SUELTA e   lo lleva y lo suelta en e"));
  Serial.println(F("  SINIR 11000   prueba sin sensores IR (1 = slot con cubo)   SINIR NO  volver a los IR"));
  Serial.println(F("  CICLO n       sin sensores: toma del slot n, pasa por CP y lo deja en ENTREGA"));
  Serial.println(F("  IR            ver los 5 sensores"));
}

void ejecutarComando(String cmd);

void procesarComando(String cmd) {
  cmd.trim(); cmd.toUpperCase(); cmd.replace(':', ' ');
  if (cmd.length() == 0) return;
  if (cmd == "PARA" || cmd == "STOP") { if (trabajando) cancelar = true; return; }
  if (cmd == "ESTADO") { enviar(trabajando ? "OCUPADO" : "REPOSO"); enviar(textoSlots()); return; }
  if (trabajando) { enviar("OCUPADO"); return; }       // no se aceptan ordenes a media tarea

  // El unico lugar donde se prende y se apaga "trabajando": aunque el comando salga antes con error,
  // al regresar siempre se libera (en la v1 se quedaba trabado en true).
  trabajando = true; cancelar = false;
  ejecutarComando(cmd);
  cancelar = false; trabajando = false;
}

void ejecutarComando(String cmd) {
  String p1 = cmd, resto = "";
  int sp = cmd.indexOf(' ');
  if (sp > 0) { p1 = cmd.substring(0, sp); resto = cmd.substring(sp + 1); resto.trim(); }

  if (p1 == "DAME" || p1 == "CUBO" || numeroDeCubo(cmd) > 0) {
    int n = numeroDeCubo(p1 == "DAME" || p1 == "CUBO" ? resto : cmd);
    if (n == 0) { enviar("ERROR:CUBO?"); return; }
    buscarCubo(n); enviar("REPOSO");
  } else if (p1 == "ORDENA" || p1 == "ORDENAR") {
    ordenarCubos(); enviar("REPOSO");
  } else if (p1 == "HOME") {
    irHome(); enviar("REPOSO");
  } else if (p1 == "CENTRAR") {                     // todos los servos a 90 (para revisar el armado)
    Pose centro = {{90, 90, 90, 90, 90}};
    moverA(centro, 90); enviar("REPOSO");
  } else if (p1 == "J") {
    int sp2 = resto.indexOf(' ');
    if (sp2 < 0) { Serial.println(F("uso: J s ang")); return; }
    int s = resto.substring(0, sp2).toInt();
    float g = resto.substring(sp2 + 1).toFloat();
    if (s < 0 || s >= N_SERVOS || PIN_SERVO[s] == SIN_SERVO) { Serial.println(F("servo invalido")); return; }
    Pose p = poseActual(); float pz = angulo[PINZA];
    if (s == PINZA) pz = g; else p.a[s] = g;
    moverA(p, pz);
    Serial.print(NOMBRE_SERVO[s]); Serial.print(F(" -> ")); Serial.println(angulo[s]);
  } else if (p1 == "POSE") {
    imprimirPose("actual", poseActual()); Serial.print(F("  pinza = ")); Serial.println(angulo[PINZA]);
  } else if (p1 == "GUARDAR") {
    if (resto == "HOME") {
      cal.home = poseActual();
      imprimirPose("HOME", cal.home); guardarCal();
      return;
    }
    int sp2 = resto.indexOf(' ');
    if (sp2 < 0) {                                  // "GUARDAR e" solo: la misma pose en ALTA y BAJA
      int e = estacionDeTexto(resto);               // (el traslado seguro ya lo da la altura de HOME)
      if (e < 0) { Serial.println(F("uso: GUARDAR e   GUARDAR e ALTA|BAJA   o   GUARDAR HOME")); return; }
      cal.alta[e] = cal.baja[e] = poseActual();
      imprimirPose(NOMBRE_EST[e], poseActual()); guardarCal();
      return;
    }
    int e = estacionDeTexto(resto.substring(0, sp2));
    String cual = resto.substring(sp2 + 1);
    if (e < 0) { Serial.println(F("estacion invalida")); return; }
    if (!cual.startsWith("A") && !cual.startsWith("B")) { Serial.println(F("falta ALTA o BAJA")); return; }
    bool alta = cual.startsWith("A");
    (alta ? cal.alta[e] : cal.baja[e]) = poseActual();
    String t = String(NOMBRE_EST[e]) + (alta ? " ALTA" : " BAJA");
    imprimirPose(t.c_str(), poseActual()); guardarCal();
  } else if (p1 == "PINZA") {
    if (resto.startsWith("A")) { cal.pinzaAbierta = angulo[PINZA]; guardarCal(); }
    else if (resto.startsWith("C")) { cal.pinzaCerrada = angulo[PINZA]; guardarCal(); }
    Serial.print(F("  pinza abierta = ")); Serial.print(cal.pinzaAbierta);
    Serial.print(F("   cerrada = ")); Serial.println(cal.pinzaCerrada);
  } else if (p1 == "IRA") {
    int e = estacionDeTexto(resto);
    if (e < 0) { Serial.println(F("estacion invalida")); return; }
    // respeta la pinza como esta: si la cerraste a mano (J 5), viaja cerrada
    traeCubo = abs(angulo[PINZA] - cal.pinzaCerrada) < abs(angulo[PINZA] - cal.pinzaAbierta);
    irArribaDe(e);
  } else if (p1 == "TOMA") {                        // TOMA e: va a la estacion, agarra y se queda ahi con el cubo
    int e = estacionDeTexto(resto);
    if (e < 0) { Serial.println(F("uso: TOMA e  (E, 1..5, CP)")); return; }
    if (traeCubo) { Serial.println(F("ya trae un cubo: usa SUELTA e primero")); return; }
    if (tomarDe(e)) { Serial.print(F("cubo tomado de ")); Serial.println(NOMBRE_EST[e]); }
  } else if (p1 == "SUELTA") {                      // SUELTA e: lleva el cubo que trae a la estacion y lo suelta
    int e = estacionDeTexto(resto);
    if (e < 0) { Serial.println(F("uso: SUELTA e  (E, 1..5, CP)")); return; }
    if (soltarEn(e)) { Serial.print(F("cubo dejado en ")); Serial.println(NOMBRE_EST[e]); }
  } else if (p1 == "PRUEBA") {
    int e = estacionDeTexto(resto);
    if (e < 0) { Serial.println(F("estacion invalida")); return; }
    if (tomarDe(e) && soltarEn(e)) irHome();
  } else if (p1 == "LEER") {
    leerSiempre = !resto.startsWith("N");
    Serial.println(leerSiempre ? F("lectura continua del RFID: SI") : F("lectura continua del RFID: NO"));
  } else if (p1 == "SINIR") {                       // SINIR 10000 = solo el slot 1 tiene cubo; SINIR NO = usar los IR
    if (resto == "NO" || resto == "") { sinIR = false; Serial.println(F("modo SIN IR apagado: se usan los sensores")); return; }
    if (resto.length() != 5) { Serial.println(F("uso: SINIR 11000  (5 digitos, slot 1 a 5; 1 = hay cubo)   o   SINIR NO")); return; }
    sinIR = true;
    for (int i = 0; i < 5; i++) ocupado[i] = resto[i] == '1';
    Serial.print(F("modo SIN IR: ")); Serial.println(textoSlots());
  } else if (p1 == "CICLO") {                       // recorrido completo SIN sensores: slot -> checkpoint -> entrega
    int e = estacionDeTexto(resto);
    if (e < 1 || e > 5) { Serial.println(F("uso: CICLO n  (n = slot 1..5)")); return; }
    Serial.print(F("CICLO: tomar de S")); Serial.print(e); Serial.println(F(", pasar por CP y dejar en ENTREGA"));
    bool ok = tomarDe(e);
    if (ok) ok = irArribaDe(CHECKPOINT) && moverA(cal.baja[CHECKPOINT], cal.pinzaCerrada);
    if (ok) { Serial.println(F("  en el checkpoint (aqui leeria el RFID)")); esperar(1500); ok = moverA(cal.alta[CHECKPOINT], cal.pinzaCerrada); }
    if (ok) ok = soltarEn(ENTREGA);
    if (ok) Serial.println(F("  entregado")); else if (cancelar) Serial.println(F("  cancelado"));
    cancelar = false;
    irHome();
  } else if (p1 == "IR") {
    for (int i = 0; i < 5; i++) { Serial.print(F("slot ")); Serial.print(i + 1); Serial.println(ocupado[i] ? F(": OCUPADO") : F(": vacio")); }
    enviar(textoSlots());
  } else if (p1 == "UID") {
    int n = numeroDeCubo(resto);
    if (resto.length() > 0 && n == 0) { Serial.println(F("uso: UID n  (n = 1..5)")); return; }
    Serial.println(F("Pon el cubo encima del checkpoint... (PARA para salir)"));
    byte uid[7], largo = 0;
    if (!leerUID(uid, largo, 10000)) { Serial.println(F("  no se leyo nada")); return; }
    Serial.print(F("  UID = ")); imprimirUID(uid, largo); Serial.println();
    int c = cuboDeUID(uid, largo);
    if (n == 0) {                                   // solo leer
      if (c == DESCONOCIDO) Serial.println(F("  no esta registrado: usa UID n para guardarlo"));
      else { Serial.print(F("  es el ")); Serial.println(NOMBRE_CUBO[c - 1]); }
      return;
    }
    if (c != DESCONOCIDO && c != n) {               // la misma etiqueta no puede ser dos cubos
      memset(&cal.cubo[c - 1], 0, sizeof(UIDCubo)); cal.cubo[c - 1].largo = 4;
      Serial.print(F("  (se quito del ")); Serial.print(NOMBRE_CUBO[c - 1]); Serial.println(F(")"));
    }
    memset(cal.cubo[n - 1].uid, 0, sizeof(cal.cubo[n - 1].uid));
    memcpy(cal.cubo[n - 1].uid, uid, largo);
    cal.cubo[n - 1].largo = largo;
    Serial.print(F("  registrado como ")); Serial.println(NOMBRE_CUBO[n - 1]);
    guardarCal();
  } else if (p1 == "TABLA") {
    imprimirTabla();
  } else if (p1 == "BORRAR") {
    if (resto != "SI") { Serial.println(F("Esto borra TODA la calibracion (poses, pinza y UID). Para hacerlo: BORRAR SI")); return; }
    calPorDefecto(); guardarCal();
    Serial.println(F("  calibracion borrada: quedan los valores de ejemplo"));
  } else if (p1 == "AYUDA" || p1 == "?") {
    ayuda();
  } else {
    enviar("ERROR:COMANDO?");
  }
}

// =====================================================================  SETUP / LOOP
void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 2000) {}
  Serial.setTimeout(50);

  // INPUT_PULLUP: un slot sin sensor (o con el cable suelto) lee siempre "vacio" en vez de ruido.
  for (int i = 0; i < 5; i++) pinMode(PIN_IR[i], INPUT_PULLUP);

  if (cargarCal()) {
    Serial.println(F("Calibracion cargada de la EEPROM"));
    // Si algun cubo no tiene etiqueta registrada, se le pone la de fabrica y se guarda.
    bool completados = false;
    for (int c = 0; c < 5; c++) {
      // (no si esa etiqueta ya quedo registrada en otro cubo con UID n: serian dos cubos iguales)
      if (uidVacio(c) && cuboDeUID(UID_CUBO_DEF[c], LARGO_UID_DEF) == DESCONOCIDO) {
        uidDeFabrica(c);
        completados = true;
      }
    }
    if (completados) {
      Serial.println(F("  (se pusieron los UID de fabrica de los cubos que no tenian)"));
      guardarCal();
    }
  } else {
    Serial.println(F("  !! EEPROM sin calibracion: se usan los valores de ejemplo (calibra con GUARDAR)"));
  }

  for (int s = 0; s < 5; s++) angulo[s] = cal.home.a[s];
  angulo[PINZA] = cal.pinzaAbierta;
  arrancarServos();                                   // uno por uno, ya en reposo

  SPI.begin();
  rfid.PCD_Init();
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);           // mas alcance: el tag queda a ~2.6 cm
  byte v = rfid.PCD_ReadRegister(rfid.VersionReg);
  Serial.print(F("RC522 version: 0x")); Serial.println(v, HEX);
  if (v == 0x00 || v == 0xFF) Serial.println(F("  !! No responde el RC522: revisa cables y que este a 3.3 V"));

  bool tablaVacia = true;
  for (int c = 0; c < 5; c++) if (!uidVacio(c)) tablaVacia = false;
  if (tablaVacia) Serial.println(F("  !! No hay cubos registrados: usa UID n con cada uno"));

  if (!BLE.begin()) {
    Serial.println(F("  !! No arranco el BLE"));
  } else {
    BLE.setLocalName(NOMBRE_BLE);
    BLE.setDeviceName(NOMBRE_BLE);
    BLE.setAdvertisedService(svcBLE);
    svcBLE.addCharacteristic(chComando);
    svcBLE.addCharacteristic(chEstado);
    BLE.addService(svcBLE);
    chEstado.writeValue("REPOSO");
    BLE.advertise();
    Serial.print(F("BLE listo como ")); Serial.println(NOMBRE_BLE);
  }
  for (int i = 0; i < 5; i++) ocupado[i] = digitalRead(PIN_IR[i]) == IR_DETECTA;
  ayuda();
  enviar(textoSlots());
}

// Lectura continua del RC522 en reposo (solo Monitor Serie, no se manda a la app): avisa al poner o quitar un cubo.
void monitorRFID() {
  static unsigned long t = 0; static byte ult[7]; static byte ultLargo = 0; static uint8_t fallas = 0;
  if (!leerSiempre || trabajando || millis() - t < 250) return;
  t = millis();
  asegurarLector();
  byte atqa[2]; byte tam = sizeof(atqa);
  bool hay = rfid.PICC_WakeupA(atqa, &tam) == MFRC522::STATUS_OK || rfid.PICC_IsNewCardPresent();
  if (hay && rfid.PICC_ReadCardSerial()) {
    byte largo = min((byte)rfid.uid.size, (byte)7);
    rfid.PICC_HaltA(); fallas = 0;
    if (largo == ultLargo && memcmp(ult, rfid.uid.uidByte, largo) == 0) return;   // el mismo de antes
    memcpy(ult, rfid.uid.uidByte, largo); ultLargo = largo;
    Serial.print(F("RFID: ")); imprimirUID(ult, largo);
    int c = cuboDeUID(ult, largo);
    if (c == DESCONOCIDO) Serial.println(F("  -> no registrado")); else { Serial.print(F("  -> ")); Serial.println(NOMBRE_CUBO[c - 1]); }
  } else if (ultLargo && ++fallas >= 4) {           // ~1 s sin verlo: se quito
    Serial.println(F("RFID: (ya no hay cubo en el lector)")); ultLargo = 0; fallas = 0;
  }
}

void loop() {
  servicio();
  monitorRFID();
}
