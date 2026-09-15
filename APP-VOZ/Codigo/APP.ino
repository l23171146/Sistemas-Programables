#include <WiFiS3.h>

const char* ssid = "Motorola";
const char* password = "w2vrdpay";

WiFiServer server(80);
int status = WL_IDLE_STATUS;

// Pines según tu diagrama
const int ENA = 9;   // PWM - velocidad
const int IN1 = 8;   // Dirección
const int IN2 = 7;   // Dirección

int velocidadActual = 150;

// 0 = detenido, 1 = avanzando, -1 = retrocediendo
int direccionActual = 0;

void setup() {
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  detener();

  Serial.begin(115200);
  while (!Serial);

  while (status != WL_CONNECTED) {
    Serial.println("Conectando...");
    status = WiFi.begin(ssid, password);
    delay(5000);
  }

  Serial.println("Conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("Esperando comandos de voz...");
  Serial.println("--------------------------------");

  server.begin();
}

void avanzar() {
  direccionActual = 1;
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, velocidadActual);
}

void retroceder() {
  direccionActual = -1;
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  analogWrite(ENA, velocidadActual);
}

void detener() {
  direccionActual = 0;
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  analogWrite(ENA, 0);
}

void actualizarVelocidad() {
  if (direccionActual == 1) {
    avanzar();
  } else if (direccionActual == -1) {
    retroceder();
  }
}

void loop() {
  WiFiClient client = server.available();

  if (client) {
    String request = "";
    while (client.connected() && client.available()) {
      char c = client.read();
      request += c;
      if (c == '\n') break;
    }

    String respuesta = "OK";

    // Confirmación clara en el Monitor Serie
    Serial.println("--------------------------------");
    Serial.print(">> Mensaje recibido: ");
    Serial.println(request);

    if (request.indexOf("GET /avanzar") >= 0) {
      avanzar();
      respuesta = "Avanzando";
      Serial.println(">> Comando reconocido: AVANZAR");
    } else if (request.indexOf("GET /retroceder") >= 0) {
      retroceder();
      respuesta = "Retrocediendo";
      Serial.println(">> Comando reconocido: RETROCEDER");
    } else if (request.indexOf("GET /detener") >= 0) {
      detener();
      respuesta = "Detenido";
      Serial.println(">> Comando reconocido: DETENER");
    } else if (request.indexOf("GET /vel_baja") >= 0) {
      velocidadActual = 100;
      actualizarVelocidad();
      respuesta = "Velocidad baja";
      Serial.println(">> Comando reconocido: VELOCIDAD BAJA (100)");
    } else if (request.indexOf("GET /vel_media") >= 0) {
      velocidadActual = 180;
      actualizarVelocidad();
      respuesta = "Velocidad media";
      Serial.println(">> Comando reconocido: VELOCIDAD MEDIA (180)");
    } else if (request.indexOf("GET /vel_maxima") >= 0) {
      velocidadActual = 255;
      actualizarVelocidad();
      respuesta = "Velocidad maxima";
      Serial.println(">> Comando reconocido: VELOCIDAD MAXIMA (255)");
    } else {
      respuesta = "Comando desconocido";
      Serial.println(">> ADVERTENCIA: comando NO reconocido");
    }

    Serial.print(">> Enviando respuesta a la app: ");
    Serial.println(respuesta);
    Serial.println("--------------------------------");

    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println();
    client.println(respuesta);

    delay(1);
    client.stop();
  }
}
