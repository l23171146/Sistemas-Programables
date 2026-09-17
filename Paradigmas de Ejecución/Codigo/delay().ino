// ============================================================
// PARTE 1 — EL ANTIPATRÓN: TODO CON delay()
// Intención: LED1 cada 500 ms, LED2 cada 1000 ms, LED3 cada 1500 ms
// PROBLEMA: delay() bloquea TODA la ejecución del programa.
// ============================================================

const int LED1 = 2;
const int LED2 = 3;
const int LED3 = 4;

void setup() {
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  pinMode(LED3, OUTPUT);
}

void loop() {
  // --- LED1: 500 ms encendido, 500 ms apagado ---
  digitalWrite(LED1, HIGH);
  delay(500);
  digitalWrite(LED1, LOW);
  delay(500);

  // --- LED2: 1000 ms encendido, 1000 ms apagado ---
  digitalWrite(LED2, HIGH);
  delay(1000);
  digitalWrite(LED2, LOW);
  delay(1000);

  // --- LED3: 1500 ms encendido, 1500 ms apagado ---
  digitalWrite(LED3, HIGH);
  delay(1500);
  digitalWrite(LED3, LOW);
  delay(1500);
}
