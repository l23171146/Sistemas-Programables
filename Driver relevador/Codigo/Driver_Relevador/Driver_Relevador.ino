/*
  Practica 4.1.1-A: Driver de software para un relevador.
  Arduino UNO R4 WiFi + modulo puente H L298N + motorreductor + LED.
  Equipo: Rosa Guadalupe Romero Navarro, Javier Ramon Quintero Valdez
          y Gabriel Alejandro Barron Sarabia.

  Motor (activo en ALTO): D7 -> IN1 del L298N; IN2 a GND; jumpers ENA y 5V-EN puestos.
  LED (activo en BAJO):   5V -> anodo del LED; catodo -> 220 ohm -> D6.
  Potencia: pila de 9 V a +12V y GND del L298N (la practica pide 2x18650, ~7.4 V).
  GND del L298N unido al GND del Arduino (tierra comun obligatoria).

  Comandos por el Monitor Serie (115200 baudios, cualquier ajuste de linea):
    M1 / M0  encender / apagar el motor
    L1 / L0  encender / apagar el LED
    E        estado de ambos
  Varias ordenes en una linea, separadas por coma o espacio, se ejecutan seguidas:
  "M1,M0" sirve para la prueba de seguridad 1 (la segunda debe rechazarse).

  El programa principal nunca escribe en los pines: todo pasa por la clase Relevador.
  No utiliza delay().
*/

#include <ctype.h>

const uint8_t PIN_MOTOR = 7;
const uint8_t PIN_FOCO = 6;              // LED externo (PIN_LED ya es el LED de la placa, D13).
const uint32_t TIEMPO_MINIMO_MS = 1000;   // Entre dos cambios del mismo relevador.
const uint32_t FIN_DE_ORDEN_MS = 50;       // Silencio que cierra una orden sin fin de linea.
const uint8_t LARGO_ORDEN = 8;


// ---------------------------------------------------------------- Modelo del relevador
class Relevador {
  public:
    Relevador(uint8_t pin, bool activoEnBajo, uint32_t tiempoMinimo = TIEMPO_MINIMO_MS)
      : pin_(pin), activoEnBajo_(activoEnBajo), encendido_(false),
        ultimoCambio_(0), tiempoMinimo_(tiempoMinimo) {}

    // Estado seguro: el nivel de "apagado" se escribe antes y despues de volver
    // salida el pin, para que el actuador no parpadee al arrancar.
    void iniciar() {
      digitalWrite(pin_, nivel(false));
      pinMode(pin_, OUTPUT);
      digitalWrite(pin_, nivel(false));
      encendido_ = false;
      ultimoCambio_ = millis();             // El arranque cuenta como un cambio.
    }

    bool encender() { return cambiar(true); }
    bool apagar() { return cambiar(false); }
    bool estaEncendido() const { return encendido_; }

  private:
    uint8_t pin_;
    bool activoEnBajo_;
    bool encendido_;
    uint32_t ultimoCambio_;
    uint32_t tiempoMinimo_;

    // Traduce encendido/apagado al nivel del pin segun la logica del actuador.
    uint8_t nivel(bool encender) const {
      return (encender != activoEnBajo_) ? HIGH : LOW;
    }

    bool cambiar(bool encender) {
      if (encender == encendido_) {
        return true;                        // Ya esta asi: no hay nada que conmutar.
      }
      uint32_t ahora = millis();
      if (ahora - ultimoCambio_ < tiempoMinimo_) {
        return false;                       // Rechazada: el pin no cambia.
      }
      digitalWrite(pin_, nivel(encender));
      encendido_ = encender;
      ultimoCambio_ = ahora;
      return true;
    }
};


Relevador motor(PIN_MOTOR, false);          // L298N: activo en alto.
Relevador led(PIN_FOCO, true);              // LED al reves: activo en bajo.

char orden[LARGO_ORDEN + 1];
uint8_t largo = 0;
uint32_t ultimoCaracter = 0;


// ---------------------------------------------------------------- Monitor Serie
// Estas funciones no reciben un Relevador como parametro: Tinkercad declara las
// funciones al inicio del archivo, antes de la clase, y no reconoceria el tipo.
const char *textoEstado(bool encendido) {
  return encendido ? "ENCENDIDO" : "APAGADO";
}

void mostrarEstado() {
  Serial.print("Estado -> Motor: ");
  Serial.print(textoEstado(motor.estaEncendido()));
  Serial.print(" | LED: ");
  Serial.println(textoEstado(led.estaEncendido()));
}

void ordenar(bool esMotor, bool encender) {
  Relevador &r = esMotor ? motor : led;
  const char *nombre = esMotor ? "Motor" : "LED";
  bool yaEstaba = (r.estaEncendido() == encender);
  bool logrado = encender ? r.encender() : r.apagar();
  if (!logrado) {
    Serial.print("Orden rechazada (");
    Serial.print(nombre);
    Serial.println("): espere 1 s entre cambios");
    return;
  }
  Serial.print(nombre);
  Serial.print(yaEstaba ? " ya estaba " : " ");
  Serial.println(textoEstado(r.estaEncendido()));
}

void ejecutar(const char *texto) {
  if (strcmp(texto, "M1") == 0)      ordenar(true, true);
  else if (strcmp(texto, "M0") == 0) ordenar(true, false);
  else if (strcmp(texto, "L1") == 0) ordenar(false, true);
  else if (strcmp(texto, "L0") == 0) ordenar(false, false);
  else if (strcmp(texto, "E") == 0)  mostrarEstado();
  else {
    Serial.print("Comando desconocido: ");
    Serial.print(texto);
    Serial.println("  (use M1, M0, L1, L0 o E)");
  }
}

void cerrarOrden() {
  if (largo > 0) {
    orden[largo] = '\0';
    ejecutar(orden);
  }
  largo = 0;
}

// Junta los caracteres sin bloquear. La orden termina con fin de linea, coma o
// espacio o, si el Monitor Serie esta en "Sin ajuste de linea", tras 50 ms sin caracteres.
void leerSerie() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    ultimoCaracter = millis();
    if (c == '\n' || c == '\r' || c == ',' || c == ' ') {
      cerrarOrden();
    } else if (largo < LARGO_ORDEN) {
      orden[largo++] = (char)toupper((unsigned char)c);
    }
  }
  if (largo > 0 && millis() - ultimoCaracter >= FIN_DE_ORDEN_MS) {
    cerrarOrden();
  }
}


// ---------------------------------------------------------------- Programa principal
void setup() {
  motor.iniciar();                          // Primero el estado seguro, luego lo demas.
  led.iniciar();
  Serial.begin(115200);
  uint32_t inicio = millis();
  while (!Serial && millis() - inicio < 2000) {
    // Espera al Monitor Serie hasta 2 s, sin delay(); los actuadores ya estan apagados.
  }
  Serial.println();
  Serial.println("Practica 4.1.1-A: driver de software para un relevador");
  Serial.println("Sistema iniciado en estado seguro.");
  mostrarEstado();
  Serial.println("Comandos: M1, M0 (motor) | L1, L0 (LED) | E (estado) | varios: M1,M0");
}

void loop() {
  leerSerie();
}
