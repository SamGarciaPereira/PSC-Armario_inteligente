#define SENSOR_IR_PIN 34 
 
void setup() { 
  Serial.begin(115200); 
  pinMode(SENSOR_IR_PIN, INPUT); 
} 
 
void loop() { 
  int sensorValue = digitalRead(SENSOR_IR_PIN); 
  if (sensorValue == LOW) { 
    Serial.println("Notebook presente."); 
  } else { 
    Serial.println("Notebook ausente!"); 
  } 
  delay(500); 
}