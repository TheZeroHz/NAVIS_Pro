#include "BleKeyboard.h"

BleKeyboard bleKeyboard;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  
  // Initialize the BLE keyboard
  bleKeyboard.begin();
  
  Serial.println("BLE Keyboard initialized. Waiting for connection...");
}

void loop() {
  if (bleKeyboard.isConnected()) {
    Serial.println("Connected! Sending 'Hello World'");
    
    // Type "Hello World" using buffer method
    const char* message = "Hello World";
    bleKeyboard.write((uint8_t*)message, strlen(message));
    
    // Add a newline (Enter key)
    bleKeyboard.write(KEY_RETURN);
    
    // Wait 5 seconds before sending again
    delay(5000);
  } else {
    Serial.println("Waiting for BLE connection...");
    delay(1000);
  }
}