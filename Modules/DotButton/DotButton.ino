#include <Bounce2.h>

#define BTN_PIN   4   // Button input (active LOW with pull-up)
#define LED_PIN   3   // LED pin

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
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  
  btn.attach(BTN_PIN, INPUT_PULLUP);
  btn.interval(10);
  
  // Create blink task
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
  
  // Wake up the blink task
  if (blinkTaskHandle != NULL) {
    vTaskResume(blinkTaskHandle);
  }
}

void stopBlinking() {
  blinkingActive = false;
  
  // Suspend the blink task to save CPU
  if (blinkTaskHandle != NULL) {
    vTaskSuspend(blinkTaskHandle);
  }
}

void blinkTask(void *parameter) {
  bool ledState = false;
  
  // Suspend task initially
  vTaskSuspend(NULL);
  
  while (true) {
    if (blinkingActive) {
      // Blink ON
      ledState = true;
      digitalWrite(LED_PIN, HIGH);
      vTaskDelay(pdMS_TO_TICKS(120));  // ON for 120ms
      
      if (!blinkingActive) {
        digitalWrite(LED_PIN, LOW);
        continue;  // Exit if stopped during delay
      }
      
      // Blink OFF
      ledState = false;
      digitalWrite(LED_PIN, LOW);
      vTaskDelay(pdMS_TO_TICKS(120));  // OFF for 120ms
      
      // Continue blinking until manually stopped
    } else {
      // If no blinking needed, suspend the task
      digitalWrite(LED_PIN, LOW);
      vTaskSuspend(NULL);
    }
  }
}
