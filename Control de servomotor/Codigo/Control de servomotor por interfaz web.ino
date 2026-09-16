#include <WiFiS3.h>
#include <Servo.h>

// --- Datos de tu red WiFi ---
char ssid[] = "Hellsings";
char pass[] = "Ramon213";

WiFiServer server(80);
Servo miServo;

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // espera a que el Monitor Serial esté listo
  }

  miServo.attach(9);       // Pin de señal del servo
  miServo.write(90);       // Posición inicial (centro)

  // Conexión a la red WiFi
  Serial.print("Conectando a ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    Serial.println(WiFi.status());
  }

  Serial.println("\nWiFi conectado.");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());   // <-- Esta IP la usarás en el navegador

  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (!client) return;

  String peticion = "";
  while (client.connected() && client.available()) {
    char c = client.read();
    peticion += c;
    if (c == '\n') break;   // solo necesitamos la primera línea
  }

  // Buscar si viene un ángulo en la URL: /servo?angulo=90
  int idx = peticion.indexOf("angulo=");
  if (idx != -1) {
    int fin = peticion.indexOf(' ', idx);
    String valorStr = peticion.substring(idx + 7, fin);
    int angulo = valorStr.toInt();
    angulo = constrain(angulo, 0, 180);
    miServo.write(angulo);
    Serial.print("Ángulo recibido: ");
    Serial.println(angulo);
  }

  // Enviar la página HTML de respuesta
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println("Connection: close");
  client.println();
  client.println(paginaHTML());
  client.println();

  delay(1);
  client.stop();
}

String paginaHTML() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>Control de Servo</title></head><body>";
  html += "<h2>Control de Servomotor</h2>";
  html += "<input type='range' min='0' max='180' value='90' id='slider' ";
  html += "oninput='document.getElementById(\"valor\").innerHTML=this.value' ";
  html += "onchange='enviar(this.value)' style='width:80%'>";
  html += "<p>Ángulo: <span id='valor'>90</span>°</p>";
  html += "<script>";
  html += "function enviar(v){ fetch('/servo?angulo=' + v); }";
  html += "</script></body></html>";
  return html;
}
