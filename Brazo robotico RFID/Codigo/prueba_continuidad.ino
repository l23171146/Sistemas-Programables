/*
  PRUEBA DE CONTINUIDAD - cables del brazo
  Arduino UNO R4 WiFi

  Como funciona:
    El pin de prueba usa INPUT_PULLUP. Si el cable une ese pin con GND
    lee LOW (hay continuidad). Si el cable esta abierto o hace falso
    contacto lee HIGH. Se revisa miles de veces por segundo, asi que
    tambien cuenta los cortes muy cortos que pasan al menear el cable.

  Conexion:
    - FUENTE DE LOS SERVOS APAGADA y el cable desconectado en sus dos
      puntas (del servo, del Wago y del Arduino).
    - Una punta del cable -> PIN_PRUEBA (D7)
    - La otra punta       -> cualquier GND del Arduino

  Comandos (monitor serie a 9600):
    r  -> reiniciar el contador de cortes
*/

const uint8_t PIN_PRUEBA = 7;           // D7 esta libre en el diagrama actual
const unsigned long REPORTE_MS = 500;   // cada cuanto imprime el estado

bool conectadoAntes = false;
unsigned long cortes = 0;
unsigned long ultimoReporte = 0;


void setup() {

  Serial.begin(9600);

  while (!Serial) {
    ;
  }

  pinMode(PIN_PRUEBA, INPUT_PULLUP);

  conectadoAntes = (digitalRead(PIN_PRUEBA) == LOW);

  Serial.println(F("=== PRUEBA DE CONTINUIDAD ==="));
  Serial.println(F("Cable entre D7 y GND. Menealo y mira si aparecen cortes."));
  Serial.println(F("r = reiniciar contador"));
  Serial.println();
}


void loop() {

  bool conectado = (digitalRead(PIN_PRUEBA) == LOW);

  // Cada vez que pasa de conectado a abierto cuenta un corte,
  // aunque dure menos de un milisegundo.
  if (conectadoAntes && !conectado) {
    cortes++;
  }

  conectadoAntes = conectado;

  if (millis() - ultimoReporte >= REPORTE_MS) {

    ultimoReporte = millis();

    Serial.print(conectado ? F("CONTINUIDAD OK") : F("SIN CONTINUIDAD"));
    Serial.print(F("   cortes: "));
    Serial.println(cortes);
  }

  if (Serial.available() > 0) {

    char c = Serial.read();

    if (c == 'r' || c == 'R') {
      cortes = 0;
      Serial.println(F("Contador en 0."));
    }
  }
}
