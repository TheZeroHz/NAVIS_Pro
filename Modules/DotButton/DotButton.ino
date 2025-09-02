#include <Bounce2.h>

#define BTN_PIN   4   // Button input (active LOW with pull-up)
#define LED_PIN   2   // LED pin

Bounce btn = Bounce();

unsigned long lastTapTime = 0;
unsigned long holdThreshold = 200;       // ms to detect hold
unsigned long doubleTapThreshold = 400;  // ms between taps
bool holding = false;
int tapCount = 0;
bool waitingForSecondTap = false;

// Blink control - now handled by RTOS task
volatile bool blinkingActive = false;
TaskHandle_t blinkTaskHandle = NULL;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  // Configure LED pin FIRST and ensure it's OFF
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  // Configure strapping pins as inputs with pull-up for safe boot
  // But AVOID setting LED_PIN as INPUT_PULLUP since it's our output
  pinMode(8, INPUT_PULLUP); // GPIO 8 (Onboard LED, I2C SDA) 
  pinMode(9, INPUT_PULLUP); // GPIO 9 (BOOT Button, I2C SCL)
  delay(100);
  
  btn.attach(BTN_PIN, INPUT_PULLUP);
  btn.interval(10);
  
  // Create blink task - it will run continuously
  xTaskCreate(
    blinkTask,          // Task function
    "BlinkTask",        // Task name
    1000,               // Stack size
    NULL,               // Parameters
    1,                  // Priority
    &blinkTaskHandle    // Task handle
  );
  
  Serial.println("Ready: single=OK, double=blink, hold=LED ON");
}

void loop() {
  btn.update();
  unsigned long now = millis();
  
  // Button pressed
  if (btn.fell()) {
    lastTapTime = now;
    holding = true;
    
    if (waitingForSecondTap) {
      // Second tap detected
      tapCount = 2;
      waitingForSecondTap = false;
    } else {
      // First tap
      tapCount = 1;
      waitingForSecondTap = true;
    }
  }
  
  // Button released
  if (btn.rose()) {
    unsigned long heldTime = now - lastTapTime;
    holding = false;  // Clear holding flag immediately
    
    if (heldTime > holdThreshold) {
      // HOLD RELEASED
      Serial.println("Hold released -> LED OFF");
      stopBlinking();
      digitalWrite(LED_PIN, LOW);
      tapCount = 0;
      waitingForSecondTap = false;
    } else {
      // TAP DETECTED
      if (tapCount == 2) {
        // Double tap immediately processed
        Serial.println("Double tap -> blink");
        startBlinking();
        tapCount = 0;
        waitingForSecondTap = false;
      }
      // Single tap will be processed after timeout
    }
  }
  
  // Check for single tap timeout (only if not currently holding)
  if (waitingForSecondTap && !holding && (now - lastTapTime > doubleTapThreshold)) {
    if (tapCount == 1) {
      Serial.println("Single tap -> OK");
      // Don't stop blinking on single tap - just print OK
    }
    tapCount = 0;
    waitingForSecondTap = false;
  }
  
  // While holding
  if (holding && btn.read() == LOW) {
    unsigned long heldTime = now - lastTapTime;
    if (heldTime > holdThreshold) {
      Serial.println("Holding -> LED ON");
      stopBlinking();  // Stop blinking when hold is detected
      digitalWrite(LED_PIN, HIGH);
    }
  }
  
  // Small delay to prevent excessive CPU usage
  vTaskDelay(pdMS_TO_TICKS(5));
}

void startBlinking() {
  blinkingActive = true;
}

void stopBlinking() {
  blinkingActive = false;
}

void blinkTask(void *parameter) {
  while (true) {
    if (blinkingActive) {
      // Blink ON - ensure clean HIGH signal
      digitalWrite(LED_PIN, HIGH);
      vTaskDelay(pdMS_TO_TICKS(1000));  // ON for 200ms (longer for visibility)
      
      // Check if still should be blinking
      if (!blinkingActive) {
        digitalWrite(LED_PIN, LOW);
        continue;
      }
      
      // Blink OFF - ensure clean LOW signal
      digitalWrite(LED_PIN, LOW);
      vTaskDelay(pdMS_TO_TICKS(1000));  // OFF for 200ms (longer for visibility)
    } else {
      // Not blinking - ensure LED is OFF and wait
      digitalWrite(LED_PIN, LOW);
      vTaskDelay(pdMS_TO_TICKS(100));  // Check every 100ms when not blinking
    }
  }
}