#include <Bounce2.h>

#define BTN_PIN     4   // Button input (active LOW with pull-up)
#define LED_PIN     2   // LED pin
#define HAPTIC_PIN  10  // Haptic engine pin

Bounce btn = Bounce();

unsigned long lastTapTime = 0;
unsigned long holdThreshold = 200;       // ms to detect hold
unsigned long doubleTapThreshold = 400;  // ms between taps
bool holding = false;
bool holdHapticTriggered = false;  // Flag to prevent repeated hold haptic
int tapCount = 0;
bool waitingForSecondTap = false;

// Blink control - handled by RTOS task
volatile bool blinkingActive = false;
TaskHandle_t blinkTaskHandle = NULL;

// Haptic control - handled by RTOS task
volatile bool hapticActive = false;
volatile int hapticPattern = 0;  // 0=none, 1=single, 2=double, 3=hold
TaskHandle_t hapticTaskHandle = NULL;

void setup() {
  Serial.begin(115200);
 
  pinMode(LED_PIN, OUTPUT);
  pinMode(HAPTIC_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(HAPTIC_PIN, LOW);
 
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

  // Create haptic task
  xTaskCreate(
    hapticTask,         // Task function
    "HapticTask",       // Task name
    1000,               // Stack size
    NULL,               // Parameters
    2,                  // Higher priority than blink
    &hapticTaskHandle   // Task handle
  );
 
  Serial.println("Ready: single=OK+haptic, double=blink+haptic, hold=LED ON+haptic");
}

void loop() {
  btn.update();
  unsigned long now = millis();
 
  // Button pressed
  if (btn.fell()) {
    lastTapTime = now;
    holding = true;
    holdHapticTriggered = false;  // Reset hold haptic flag
   
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
      stopHaptic();
      digitalWrite(LED_PIN, LOW);
      tapCount = 0;
      waitingForSecondTap = false;
      holdHapticTriggered = false;  // Reset for next hold
    } else {
      // TAP DETECTED
      if (tapCount == 2) {
        // Double tap immediately processed
        Serial.println("Double tap -> blink + haptic");
        startBlinking();
        triggerHaptic(2); // Double tap haptic pattern
        tapCount = 0;
        waitingForSecondTap = false;
      }
      // Single tap will be processed after timeout
    }
  }
 
  // Check for single tap timeout (only if not currently holding)
  if (waitingForSecondTap && !holding && (now - lastTapTime > doubleTapThreshold)) {
    if (tapCount == 1) {
      Serial.println("Single tap -> OK + haptic");
      triggerHaptic(1); // Single tap haptic pattern
    }
    tapCount = 0;
    waitingForSecondTap = false;
  }
 
  // While holding
  if (holding && btn.read() == LOW) {
    unsigned long heldTime = now - lastTapTime;
    if (heldTime > holdThreshold && !holdHapticTriggered) {
      Serial.println("Holding -> LED ON + haptic");
      stopBlinking();  // Stop blinking when hold is detected
      digitalWrite(LED_PIN, HIGH);
      triggerHaptic(3); // Hold haptic pattern (only once)
      holdHapticTriggered = true;  // Prevent repeated haptic triggers
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

void triggerHaptic(int pattern) {
  hapticPattern = pattern;
  hapticActive = true;
  
  // Wake up the haptic task
  if (hapticTaskHandle != NULL) {
    vTaskResume(hapticTaskHandle);
  }
}

void stopHaptic() {
  hapticActive = false;
  
  // Suspend the haptic task
  if (hapticTaskHandle != NULL) {
    vTaskSuspend(hapticTaskHandle);
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

void hapticTask(void *parameter) {
  // Suspend task initially
  vTaskSuspend(NULL);
  
  while (true) {
    if (hapticActive) {
      switch (hapticPattern) {
        case 1: // Single tap - 150ms pulse
          digitalWrite(HAPTIC_PIN, HIGH);
          vTaskDelay(pdMS_TO_TICKS(150));  // 150ms pulse
          digitalWrite(HAPTIC_PIN, LOW);
          break;
          
        case 2: // Double tap - two 150ms pulses
          // First pulse
          digitalWrite(HAPTIC_PIN, HIGH);
          vTaskDelay(pdMS_TO_TICKS(150));  // 150ms pulse
          digitalWrite(HAPTIC_PIN, LOW);
          vTaskDelay(pdMS_TO_TICKS(100));  // 100ms gap
          
          // Second pulse
          digitalWrite(HAPTIC_PIN, HIGH);
          vTaskDelay(pdMS_TO_TICKS(150));  // 150ms pulse
          digitalWrite(HAPTIC_PIN, LOW);
          break;
          
        case 3: // Hold - single 300ms pulse
          digitalWrite(HAPTIC_PIN, HIGH);
          vTaskDelay(pdMS_TO_TICKS(300));  // 300ms pulse
          digitalWrite(HAPTIC_PIN, LOW);
          break;
          
        default:
          break;
      }
      
      // Reset haptic state and suspend task
      hapticActive = false;
      hapticPattern = 0;
      vTaskSuspend(NULL);
    } else {
      // If no haptic needed, suspend the task
      digitalWrite(HAPTIC_PIN, LOW);
      vTaskSuspend(NULL);
    }
  }
}
