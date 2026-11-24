/*
  Prueba del lector RFID RC522, solo (sin servos ni sensores IR).
  Arduino UNO R4 WiFi, mismos pines que estacion_rfid_v2:
    SDA -> D10   SCK -> D13   MOSI -> D11   MISO -> D12   IRQ sin conectar
    GND -> GND   RST -> D9    3.3V -> 3.3V  (NUNCA a 5 V)

  Abre el Monitor Serie a 115200. Al arrancar revisa que el lector responda; luego,
  cada vez que acercas una etiqueta o llavero, imprime su UID y su tipo.
  Comandos (Monitor Serie con "Nueva linea"):
    v   volver a revisar la conexion con el lector
    a   prueba de la antena (autoprueba interna del RC522)
*/
#include <SPI.h>
#include <MFRC522.h>

const uint8_t PIN_SS = 10;
const uint8_t PIN_RST = 9;

MFRC522 rfid(PIN_SS, PIN_RST);
unsigned long lecturas = 0;
unsigned long reinicios = 0;

// Versiones que de verdad devuelve un RC522: 0x91 o 0x92 = original; 0x88, 0x18, 0x12, 0xB2 = clones que sirven.
bool versionValida(byte v) {
  return v == 0x91 || v == 0x92 || v == 0x88 || v == 0x18 || v == 0x12 || v == 0xB2 || v == 0x82;
}

// Estado de la antena: los dos bits de abajo de TxControlReg deben estar en 1 (antena prendida).
void revisarAntena() {
  byte tx = rfid.PCD_ReadRegister(rfid.TxControlReg);
  byte ganancia = (rfid.PCD_ReadRegister(rfid.RFCfgReg) >> 4) & 0x07;
  Serial.print(F("Antena: "));
  Serial.print((tx & 0x03) == 0x03 ? F("prendida") : F("APAGADA"));
  Serial.print(F(" (TxControl 0x"));
  Serial.print(tx, HEX);
  Serial.print(F(") · ganancia "));
  Serial.print(ganancia);
  Serial.println(F(" de 7"));
  if ((tx & 0x03) != 0x03) {
    rfid.PCD_AntennaOn();
    Serial.println(F("  (se volvio a prender)"));
  }
}

// Un intento de despertar a cualquier etiqueta cercana, con el resultado tal cual.
void intentoDeLectura() {
  byte atqa[2];
  byte tam = sizeof(atqa);
  MFRC522::StatusCode r = rfid.PICC_WakeupA(atqa, &tam);
  Serial.print(F("  intento: "));
  Serial.print(rfid.GetStatusCodeName(r));
  if (r == MFRC522::STATUS_TIMEOUT) Serial.print(F("  (no hay etiqueta que conteste)"));
  else if (r == MFRC522::STATUS_OK) Serial.print(F("  (algo contesto!)"));
  else Serial.print(F("  (algo contesta pero llega con errores: muy lejos, chueca o de otro tipo)"));
  Serial.println();
}

// Escribe valores conocidos en un registro que no hace nada peligroso y los vuelve a leer.
// Si la comunicación por SPI está bien, lo que se lee es igual a lo que se escribió.
void pruebaRegistros() {
  const byte valores[] = {0x00, 0xFF, 0x55, 0xAA, 0x0F, 0xF0};
  byte original = rfid.PCD_ReadRegister(rfid.ModWidthReg);
  int malos = 0;
  Serial.println(F("Prueba de escribir y leer (debe salir igual):"));
  for (byte i = 0; i < sizeof(valores); i++) {
    rfid.PCD_WriteRegister(rfid.ModWidthReg, valores[i]);
    byte leido = rfid.PCD_ReadRegister(rfid.ModWidthReg);
    Serial.print(F("  escribo 0x"));
    if (valores[i] < 0x10) Serial.print('0');
    Serial.print(valores[i], HEX);
    Serial.print(F(" -> leo 0x"));
    if (leido < 0x10) Serial.print('0');
    Serial.print(leido, HEX);
    if (leido != valores[i]) {
      malos++;
      Serial.print(F("   MAL (bits distintos: "));
      Serial.print((byte)(leido ^ valores[i]), BIN);
      Serial.print(')');
    }
    Serial.println();
  }
  rfid.PCD_WriteRegister(rfid.ModWidthReg, original);
  if (malos == 0) {
    Serial.println(F("  -> la comunicacion SPI esta bien"));
  } else {
    Serial.println(F("  -> la comunicacion SPI llega danada:"));
    Serial.println(F("     revisa MISO (D12) y SCK (D13) primero, luego MOSI (D11) y SDA (D10);"));
    Serial.println(F("     que ningun cable este en el pin de al lado, que entren firmes, y cables cortos."));
  }
}

bool revisarLector() {
  byte v = rfid.PCD_ReadRegister(rfid.VersionReg);
  Serial.print(F("Version del lector: 0x"));
  Serial.print(v, HEX);
  if (v == 0x00 || v == 0xFF) {
    Serial.println(F("  -> NO RESPONDE"));
    Serial.println(F("   Revisa: 3.3V y GND, SDA en D10, SCK en D13, MOSI en D11, MISO en D12, RST en D9."));
    Serial.println(F("   0x00 suele ser falta de corriente o RST; 0xFF suele ser MISO o SCK flojo."));
    return false;
  }
  if (!versionValida(v)) {
    Serial.println(F("  -> RESPONDE, PERO MAL: ese numero no es de un RC522 (la respuesta llega danada)"));
    pruebaRegistros();
    return false;
  }
  if (v == 0x91 || v == 0x92) Serial.println(F("  -> OK (RC522 original)"));
  else Serial.println(F("  -> OK (clon compatible)"));
  pruebaRegistros();
  revisarAntena();
  return true;
}

void imprimirUID() {
  Serial.print(F("UID: "));
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) Serial.print('0');
    Serial.print(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) Serial.print(' ');
  }
  // Mismo formato que el comando TABLA de estacion_rfid_v2, por si quieres compararlo.
  Serial.print(F("   {"));
  for (byte i = 0; i < rfid.uid.size; i++) {
    Serial.print(F("0x"));
    if (rfid.uid.uidByte[i] < 0x10) Serial.print('0');
    Serial.print(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) Serial.print(F(", "));
  }
  Serial.print(F("}, "));
  Serial.println(rfid.uid.size);
}

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 3000) {}
  Serial.println(F("\n=== Prueba del lector RFID RC522 ==="));
  SPI.begin();
  rfid.PCD_Init();
  delay(50);
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);   // igual que en el programa del brazo
  if (revisarLector()) {
    Serial.println(F("Acerca una etiqueta o llavero al lector (a 1-3 cm)..."));
  }
  Serial.println(F("Comandos: v = revisar conexion, a = prueba de antena"));
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'v' || c == 'V') {
      revisarLector();
    } else if (c == 'a' || c == 'A') {
      Serial.print(F("Autoprueba de la antena: "));
      bool ok = rfid.PCD_PerformSelfTest();     // al terminar hay que reiniciar el lector
      Serial.println(ok ? F("OK") : F("FALLO (puede ser normal en algunos clones)"));
      rfid.PCD_Init();
      rfid.PCD_SetAntennaGain(rfid.RxGain_max);
    }
  }

  // Cada 2 s dice cómo va: en el UNO R4 WiFi los mensajes del arranque se pierden si el
  // Monitor Serie se abre después, así que esto confirma que el programa sí está corriendo.
  // Si el lector se reinició solo (un parpadeo de corriente por pines sin soldar), su antena
  // queda apagada: se vuelve a configurar y se avisa.
  static unsigned long ultimaRevision = 0;
  if (millis() - ultimaRevision > 200) {
    ultimaRevision = millis();
    byte tx = rfid.PCD_ReadRegister(rfid.TxControlReg);
    if (versionValida(rfid.PCD_ReadRegister(rfid.VersionReg)) && (tx & 0x03) != 0x03) {
      rfid.PCD_Init();
      rfid.PCD_SetAntennaGain(rfid.RxGain_max);
      reinicios++;
      Serial.print(F("!! El lector se reinicio solo (contacto flojo). Se volvio a configurar. Veces: "));
      Serial.println(reinicios);
    }
  }

  static unsigned long ultimoAviso = 0;
  if (millis() - ultimoAviso > 2000) {
    ultimoAviso = millis();
    byte v = rfid.PCD_ReadRegister(rfid.VersionReg);
    Serial.print(F("[corriendo] lector 0x"));
    Serial.print(v, HEX);
    Serial.print((v == 0x00 || v == 0xFF) ? F(" NO RESPONDE (revisa cables)")
                 : versionValida(v) ? F(" OK, esperando etiqueta")
                 : F(" RESPUESTA DANADA (escribe v para la prueba completa)"));
    Serial.print(F(" · lecturas: "));
    Serial.println(lecturas);
    intentoDeLectura();
  }

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }
  lecturas++;
  Serial.print(F("\nLectura #"));
  Serial.println(lecturas);
  imprimirUID();
  MFRC522::PICC_Type tipo = rfid.PICC_GetType(rfid.uid.sak);
  Serial.print(F("Tipo: "));
  Serial.println(rfid.PICC_GetTypeName(tipo));
  rfid.PICC_HaltA();          // deja de leer esta hasta que la retires y la acerques otra vez
  rfid.PCD_StopCrypto1();
}
