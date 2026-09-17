parte 2 // ============================================================
// PARTE 2 — TEMPORIZACIÓN NO BLOQUEANTE CON millis()
// LED1: 500 ms | LED2: 1000 ms | LED3: 1500 ms
// Tarea extra (reto opcional): imprime "Hola Mundo" cada 3000 ms,
// totalmente independiente del código de los LEDs.
// ============================================================

const int LED1 = 2;
const int LED2 = 3;
const int LED3 = 4;

// Estado actual (encendido/apagado) de cada LED
bool estadoLED1 = LOW;
bool estadoLED2 = LOW;
bool estadoLED3 = LOW;

// Marca de tiempo del último cambio de cada tarea
unsigned long previoLED1 = 0;
unsigned long previoLED2 = 0;
unsigned long previoLED3 = 0;
unsigned long previoSerial = 0;

// Intervalos de cada tarea (en milisegundos)
const unsigned long INTERVALO_LED1 = 500;
const unsigned long INTERVALO_LED2 = 1000;
const unsigned long INTERVALO_LED3 = 1500;
const unsigned long INTERVALO_SERIAL = 3000;

void setup() {
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);

  Serial.begin(9600);
}

void loop() {
  unsigned long ahora = millis();

  // --- Tarea LED1 (cada 500 ms) ---
  if (ahora - previoLED1 >= INTERVALO_LED1) {
    previoLED1 = ahora;
    estadoLED1 = !estadoLED1;
    digitalWrite(LED1, estadoLED1);
  }

  // --- Tarea LED2 (cada 1000 ms) ---
  if (ahora - previoLED2 >= INTERVALO_LED2) {
    previoLED2 = ahora;
    estadoLED2 = !estadoLED2;
    digitalWrite(LED2, estadoLED2);
  }

  // --- Tarea LED3 (cada 1500 ms) ---
  if (ahora - previoLED3 >= INTERVALO_LED3) {
    previoLED3 = ahora;
    estadoLED3 = !estadoLED3;
    digitalWrite(LED3, estadoLED3);
  }

  // --- Tarea extra: mensaje serial cada 3 s ---
  if (ahora - previoSerial >= INTERVALO_SERIAL) {
    previoSerial = ahora;
    Serial.println("Hola Mundo");
  }
}
