#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <utility/imumaths.h>

Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x29, &Wire);

// Button configuration
const int BUTTON_PIN = 2;  // Change to your button pin
bool buttonPressed = false;
bool lastButtonState = false;

// Mouse sensitivity and smoothing
const float sensitivity = 2.0;  // Adjust cursor speed
const float alpha = 0.3;        // Smoothing factor (0.0-1.0)
float smoothDX = 0, smoothDY = 0;

// Previous quaternion for delta calculation
imu::Quaternion prevQuat;
bool quatInitialized = false;

// Dead zone to eliminate sensor noise
const float deadZone = 0.5; // degrees

void setup() {
  Serial.begin(115200);
  Serial.println("BNO055 Hand Mouse - Delta Movement");
  
  // Setup button pin
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize BNO055
  if (!bno.begin()) {
    Serial.println("{\"error\":\"No BNO055 detected!\"}");
    while (1);
  }
  
  // Use external crystal for better accuracy
  bno.setExtCrystalUse(true);
  delay(1000);
  
  Serial.println("{\"status\":\"BNO055 ready - Press and hold button to move cursor\"}");
}

void loop() {
  // Read button state
  buttonPressed = !digitalRead(BUTTON_PIN); // Assuming active LOW button
  
  // Button press/release events
  if (buttonPressed && !lastButtonState) {
    // Button just pressed - initialize reference quaternion
    prevQuat = bno.getQuat();
    quatInitialized = true;
    smoothDX = 0;
    smoothDY = 0;
    Serial.println("{\"event\":\"button_press\"}");
  }
  else if (!buttonPressed && lastButtonState) {
    // Button just released
    quatInitialized = false;
    Serial.println("{\"event\":\"button_release\"}");
  }
  
  lastButtonState = buttonPressed;
  
  // Only calculate movement when button is pressed
  if (buttonPressed && quatInitialized) {
    // Get current quaternion
    imu::Quaternion currentQuat = bno.getQuat();
    
    // Calculate relative rotation from previous frame
    imu::Quaternion deltaQuat = currentQuat * prevQuat.conjugate();
    
    // Convert to Euler angles for easier interpretation
    imu::Vector<3> deltaEuler = deltaQuat.toEuler();
    
    // Extract yaw and pitch changes (in radians)
    float deltaYaw = deltaEuler.x();   // Left/Right rotation
    float deltaPitch = deltaEuler.z(); // Up/Down tilt
    
    // Convert to degrees
    deltaYaw *= 180.0 / M_PI;
    deltaPitch *= 180.0 / M_PI;
    
    // Handle angle wraparound
    if (deltaYaw > 180) deltaYaw -= 360;
    if (deltaYaw < -180) deltaYaw += 360;
    if (deltaPitch > 180) deltaPitch -= 360;
    if (deltaPitch < -180) deltaPitch += 360;
    
    // Apply dead zone
    if (abs(deltaYaw) < deadZone) deltaYaw = 0;
    if (abs(deltaPitch) < deadZone) deltaPitch = 0;
    
    // Calculate mouse movement deltas
    float rawDX = deltaYaw * sensitivity;
    float rawDY = -deltaPitch * sensitivity; // Invert Y for natural feel
    
    // Apply smoothing
    smoothDX = alpha * rawDX + (1 - alpha) * smoothDX;
    smoothDY = alpha * rawDY + (1 - alpha) * smoothDY;
    
    // Only send movement if there's significant motion
    if (abs(smoothDX) > 0.1 || abs(smoothDY) > 0.1) {
      // Output delta movement for mouse control
      Serial.print("{\"mouse\":{");
      Serial.print("\"dx\":"); Serial.print((int)smoothDX);
      Serial.print(",\"dy\":"); Serial.print((int)smoothDY);
      Serial.print(",\"button\":true");
      Serial.println("}}");
    }
    
    // Update previous quaternion for next delta calculation
    prevQuat = currentQuat;
  }
  else {
    // Button not pressed - send idle status
    Serial.println("{\"mouse\":{\"dx\":0,\"dy\":0,\"button\":false}}");
  }
  
  delay(10); // 100 Hz update rate for smooth movement
}

// Optional: Add calibration function
void calibrateSensor() {
  Serial.println("{\"status\":\"Calibrating - move device in figure-8 pattern\"}");
  
  uint8_t sys, gyro, accel, mag = 0;
  do {
    bno.getCalibration(&sys, &gyro, &accel, &mag);
    Serial.print("Calibration: Sys=");
    Serial.print(sys); Serial.print(" Gyro=");
    Serial.print(gyro); Serial.print(" Accel=");
    Serial.print(accel); Serial.print(" Mag=");
    Serial.println(mag);
    delay(100);
  } while (sys < 3); // Wait until system calibration is good
  
  Serial.println("{\"status\":\"Calibration complete!\"}");
}
