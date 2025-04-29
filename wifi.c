#include <WiFi.h> 
#include <HTTPClient.h> 

const char* ssid = "visitantes"; 
const char* password = ""; 
const char* serverName = "http://armario_inteligente:8000/dados"; 

void setup() { 
    Serial.begin(115200); 
    WiFi.begin(ssid, password); 
    while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
} 
    Serial.println("Wi-Fi conectado!"); 
} 

void loop() { 
    if (WiFi.status() == WL_CONNECTED) { 
    HTTPClient http; 
    http.begin(serverName); 
    http.addHeader("Content-Type", "application/json"); 
    String jsonData = "{"teste":"Mensagem de Teste do ESP32"}"; 
    int httpResponseCode = http.POST(jsonData); 
    Serial.println(httpResponseCode); 
    http.end(); 
    } 
    delay(5000); 
} 
