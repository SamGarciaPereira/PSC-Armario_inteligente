#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "";
const char* password = "";

// Web server configuration
const char* serverUrl = "http://your-nodejs-server:3000";
WebServer server(80);

// Pin definitions for ESP32
#define FINGERPRINT_RX 16  // GPIO16
#define FINGERPRINT_TX 17  // GPIO17

HardwareSerial mySerial(2);  // Using Serial2
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

void setup() {
  // Initialize debug serial
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }
  Serial.println("\n\nIniciando sistema biométrico...");

  // Initialize fingerprint sensor serial
  mySerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
  delay(1000);  // Give sensor time to initialize

  // Test sensor communication
  Serial.println("Testando comunicação com sensor...");
  
  // Try to initialize sensor multiple times
  uint8_t attempts = 0;
  bool sensorFound = false;
  
  while (attempts < 3 && !sensorFound) {
    Serial.printf("Tentativa %d de inicialização...\n", attempts + 1);
    
    if (finger.begin()) {
      sensorFound = true;
      Serial.println("Sensor inicializado com sucesso!");
      
      // Get sensor parameters
      uint32_t param = finger.getParameters();
      Serial.print("Status: 0x"); Serial.println(param, HEX);
      Serial.print("Sensor ID: "); Serial.println(finger.getSensorID());
      Serial.print("Capacity: "); Serial.println(finger.getCapacity());
      Serial.print("Security level: "); Serial.println(finger.getSecurityLevel());
      Serial.print("Device address: "); Serial.println(finger.getDeviceAddress(), HEX);
      Serial.print("Packet length: "); Serial.println(finger.getPacketLength());
      Serial.print("Baud rate: "); Serial.println(finger.getBaudRate());
    } else {
      Serial.println("Falha na comunicação. Verificando configurações...");
      Serial.println("1. Verificando conexões físicas:");
      Serial.println("   - VCC conectado ao 3.3V");
      Serial.println("   - GND conectado ao GND");
      Serial.println("   - TX do sensor conectado ao GPIO16 (RX2)");
      Serial.println("   - RX do sensor conectado ao GPIO17 (TX2)");
      Serial.println("2. Verificando configurações:");
      Serial.println("   - Baud rate: 57600");
      Serial.println("   - Protocolo: 8N1");
      Serial.println("   - Nível lógico: 3.3V");
    }
    
    attempts++;
    if (!sensorFound && attempts < 3) {
      Serial.println("Aguardando 2 segundos antes da próxima tentativa...");
      delay(2000);
    }
  }

  if (!sensorFound) {
    Serial.println("\nERRO: Não foi possível estabelecer comunicação com o sensor!");
    Serial.println("Por favor, verifique:");
    Serial.println("1. Conexões físicas");
    Serial.println("2. Alimentação do sensor (3.3V)");
    Serial.println("3. Nível lógico dos sinais");
    Serial.println("4. Baud rate do sensor");
    Serial.println("\nReinicie o ESP32 após fazer as correções necessárias.");
    while (1) {
      delay(1000);  // Halt execution
    }
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConectado ao WiFi");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/scan", HTTP_GET, handleScan);
  server.on("/enroll", HTTP_POST, handleEnroll);
  
  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

void loop() {
  server.handleClient();
  checkFingerprint();
  delay(100);
}

void handleRoot() {
  String html = "<html><body>";
  html += "<h1>Sistema Biométrico ESP32</h1>";
  html += "<p>Status: Ativo</p>";
  html += "<p><a href='/scan'>Verificar Digital</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleScan() {
  uint8_t result = getFingerprintID();
  if (result == FINGERPRINT_OK) {
    sendToServer(finger.fingerID, "success");
    server.send(200, "text/plain", "Digital reconhecida");
  } else {
    server.send(404, "text/plain", "Digital não reconhecida");
  }
}

void handleEnroll() {
  if (server.hasArg("matricula")) {
    String matricula = server.arg("matricula");
    uint8_t result = enrollFingerprint();
    if (result == FINGERPRINT_OK) {
      server.send(200, "text/plain", "Digital cadastrada com sucesso");
    } else {
      server.send(500, "text/plain", "Erro ao cadastrar digital");
    }
  } else {
    server.send(400, "text/plain", "Matrícula não fornecida");
  }
}

void sendToServer(int id, const char* status) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<200> doc;
    doc["matricula"] = id;
    doc["status"] = status;
    
    String jsonString;
    serializeJson(doc, jsonString);

    int httpResponseCode = http.POST(jsonString);
    if (httpResponseCode > 0) {
      Serial.println("Dados enviados com sucesso");
    } else {
      Serial.println("Erro ao enviar dados");
    }
    http.end();
  }
}

uint8_t getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return p;
  
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return p;
  
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.print("ID encontrado: ");
    Serial.println(finger.fingerID);
    return p;
  }
  return p;
}

uint8_t enrollFingerprint() {
  uint8_t p = -1;
  
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    delay(50);
  }
  
  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) return p;
  
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    delay(50);
  }
  
  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) return p;
  
  p = finger.createModel();
  if (p != FINGERPRINT_OK) return p;
  
  p = finger.storeModel(finger.fingerID);
  return p;
} 