/*
  Control de acceso con RFID RC522 (SPI) - Arduino UNO/Nano

  CONEXIONES (RC522 -> Arduino)  ¡ALIMENTAR CON 3.3 V, NO 5 V!
    SDA (SS)  -> D10
    SCK       -> D13
    MOSI      -> D11
    MISO      -> D12
    RST       -> D9
    3.3V      -> 3.3V
    GND       -> GND
    IRQ       -> sin conectar

  LEDs (cada uno con resistencia de 220 ohm a GND):
    LED verde -> D7
    LED rojo  -> D6
*/

#include <SPI.h>
#include <MFRC522.h>

// ---------- Pines ----------
const byte SS_PIN  = 10;
const byte RST_PIN = 9;
const byte LED_VERDE = 7;
const byte LED_ROJO  = 6;

// ---------- Configuración ----------
const unsigned long TIEMPO_LED = 2000;  // ms que permanece encendido el LED

// UID autorizado: CAMBIA estos bytes por el UID de tu tarjeta
// (léelo primero con el Monitor Serie)
const byte UID_AUTORIZADO[] = {0x01, 0x35, 0xD5, 0x1C};
const byte TAM_UID_AUTORIZADO = sizeof(UID_AUTORIZADO);

MFRC522 rfid(SS_PIN, RST_PIN);

// ---------- Control de tiempo con millis() ----------
bool verdeEncendido = false;
bool rojoEncendido  = false;
unsigned long inicioVerde = 0;
unsigned long inicioRojo  = 0;

// Verifica comunicación leyendo el registro de versión del chip
bool lectorResponde() {
  byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  Serial.print(F("VersionReg = 0x"));
  Serial.println(version, HEX);
  // 0x00 o 0xFF significa que no hay comunicación SPI (ej. MISO desconectado)
  return (version != 0x00 && version != 0xFF);
}

void imprimirUID(const MFRC522::Uid &uid) {
  Serial.print(F("UID:"));
  for (byte i = 0; i < uid.size; i++) {
    Serial.print(' ');
    if (uid.uidByte[i] < 0x10) Serial.print('0');
    Serial.print(uid.uidByte[i], HEX);
  }
  Serial.println();
}

bool uidAutorizado(const MFRC522::Uid &uid) {
  if (uid.size != TAM_UID_AUTORIZADO) return false;
  return memcmp(uid.uidByte, UID_AUTORIZADO, TAM_UID_AUTORIZADO) == 0;
}

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }

  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_ROJO, LOW);

  SPI.begin();
  rfid.PCD_Init();
  delay(50);

  if (lectorResponde()) {
    Serial.println(F("Lector RC522 detectado: comunicacion SPI OK"));
    Serial.println(F("Acerque una tarjeta..."));
  } else {
    Serial.println(F("ERROR: no hay comunicacion con el lector RC522"));
    Serial.println(F("Revise cableado (MISO, MOSI, SCK, SS, RST) y alimentacion 3.3 V"));
  }
}

void loop() {
  unsigned long ahora = millis();

  // --- Apagado automático de los LEDs (sin bloquear) ---
  if (verdeEncendido && (ahora - inicioVerde >= TIEMPO_LED)) {
    digitalWrite(LED_VERDE, LOW);
    verdeEncendido = false;
  }
  if (rojoEncendido && (ahora - inicioRojo >= TIEMPO_LED)) {
    digitalWrite(LED_ROJO, LOW);
    rojoEncendido = false;
  }

  // --- Lectura de tarjetas (siempre activa) ---
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial())   return;

  imprimirUID(rfid.uid);

  if (uidAutorizado(rfid.uid)) {
    Serial.println(F("ACCESO PERMITIDO"));
    digitalWrite(LED_VERDE, HIGH);
    verdeEncendido = true;
    inicioVerde = millis();          // reinicia el conteo de 2 s
    digitalWrite(LED_ROJO, LOW);     // apaga el rojo si estaba encendido
    rojoEncendido = false;
  } else {
    Serial.println(F("ACCESO DENEGADO"));
    digitalWrite(LED_ROJO, HIGH);
    rojoEncendido = true;
    inicioRojo = millis();
    // El LED verde NO se toca: si estaba encendido, sigue su propio conteo
  }

  rfid.PICC_HaltA();       // detiene la tarjeta actual
  rfid.PCD_StopCrypto1();  // termina la comunicación segura
}
