 
#include <Adafruit_Fingerprint.h> 
#include <HardwareSerial.h> 
 
HardwareSerial mySerial(2); 
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial); 
 
void setup() { 
  Serial.begin(115200); 
  mySerial.begin(57600); 
  if (finger.begin()) { 
    Serial.println("Leitor biométrico iniciado!"); 
  } else { 
    Serial.println("Erro no leitor biométrico!"); 
    while (1) { delay(1); } 
  } 
} 
 
void loop() { 
  getFingerprintID(); 
  delay(100); 
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
  } else { 
    Serial.println("Digital não reconhecida."); 
  } 
  return p; 
} 
 