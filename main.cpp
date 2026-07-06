#include <Arduino.h>
#include <Adafruit_Protomatter.h>
#include <Fonts/FreeSansBold9pt7b.h> // Large, bold numeric readout

// ---- HUB75 wiring (Using Pico W safe pinout) ----
uint8_t rgbPins[]  = {0, 1, 2, 3, 4, 5}; // R1, G1, B1, R2, G2, B2
uint8_t addrPins[] = {6, 7, 8, 9};       // A, B, C, D (4 lines = 32px tall)
uint8_t clockPin   = 10;                 // CLK
uint8_t latchPin   = 11;                 // LAT
uint8_t oePin      = 12;                 // OE

// 64 wide, 4-bit color depth, single RGB chain, 4 address lines, double-buffered
Adafruit_Protomatter matrix(
  64,                        // Width in pixels
  4,                         // Bit depth (color fidelity)
  1, rgbPins,                // # of RGB pin sets, and the pins
  4, addrPins,               // # of address pins, and the pins
  clockPin, latchPin, oePin,
  true                       // Double-buffering ON
);

// Define static colors
const uint16_t COLOR_PURPLE = matrix.color565(140, 0, 220); 
const uint16_t COLOR_BLACK  = matrix.color565(0, 0, 0);
const uint16_t COLOR_WHITE  = matrix.color565(255, 255, 255);

// Screen operational states
enum DisplayMode {
  MODE_STATIC_BORDER,
  MODE_RAINBOW_BORDER,
  MODE_BIRTHDAY_SCROLL,
  MODE_DISCONNECTED          
};
DisplayMode currentMode = MODE_STATIC_BORDER;
DisplayMode savedModeBeforeDisconnect = MODE_STATIC_BORDER; 

// State tracking variables
char activeDisplayString[24] = "0.000"; 
const char* birthdayText = "Happy Birthday Avram!";
int16_t scrollXPosition = 64; 

float rainbowPhase = 0.0;
float currentGlowFactor = 1.0; 
float redGlowFactor = 1.0;     
unsigned long lastAnimUpdate = 0;
unsigned long lastScrollUpdate = 0;

// Helper function to generate a color from a wheel angle with adjustable brightness
uint16_t getWheelColor(uint8_t WheelPos, float intensity) {
  WheelPos = 255 - WheelPos;
  uint8_t r = 0, g = 0, b = 0;

  if(WheelPos < 85) {
    r = 255 - WheelPos * 3;
    b = WheelPos * 3;
  } else if(WheelPos < 170) {
    WheelPos -= 85;
    g = WheelPos * 3;
    b = 255 - WheelPos * 3;
  } else {
    WheelPos -= 170;
    r = WheelPos * 3;
    g = 255 - WheelPos * 3;
  }

  return matrix.color565((uint8_t)(r * intensity), (uint8_t)(g * intensity), (uint8_t)(b * intensity));
}

// Layout rendering engine
void updateScreenLayout() {
  matrix.fillScreen(0); 

  if (currentMode == MODE_BIRTHDAY_SCROLL) {
    matrix.setFont(NULL); 
    matrix.setTextSize(2); 
    matrix.setTextColor(COLOR_WHITE); 
    matrix.setTextWrap(false); 
    matrix.setCursor(scrollXPosition, 9);
    matrix.print(birthdayText);
  } 
  else {
    // ---- Render Border Grid Profile ----
    for (int y = 0; y < matrix.height(); y++) {
      for (int x = 0; x < matrix.width(); x++) {
        if (y < 3 || y >= 23 || x < 3 || x >= 61) {
          if (currentMode == MODE_RAINBOW_BORDER) {
            float angle = atan2(y - 15.5, x - 31.5);
            if (angle < 0) angle += 2 * PI;
            uint8_t colorIdx = (uint8_t)((angle / (2 * PI)) * 255.0 + rainbowPhase) & 255;
            matrix.drawPixel(x, y, getWheelColor(colorIdx, currentGlowFactor));
          } 
          else if (currentMode == MODE_DISCONNECTED) {
            uint16_t dynamicRed = matrix.color565((uint8_t)(255 * redGlowFactor), 0, 0);
            matrix.drawPixel(x, y, dynamicRed);
          } 
          else {
            matrix.drawPixel(x, y, COLOR_PURPLE);
          }
        }
      }
    }

    // ---- Render "Tesla" inside bottom border ----
    matrix.setFont(NULL); 
    matrix.setTextSize(1);
    
    if (currentMode == MODE_RAINBOW_BORDER) {
      matrix.setTextColor(COLOR_WHITE);
    } else {
      matrix.setTextColor(COLOR_BLACK);
    }
    
    matrix.setCursor(18, 24);
    matrix.print("Tesla");

    // ---- Render Big Live Value inside upper canvas ----
    matrix.setFont(&FreeSansBold9pt7b); 
    matrix.setTextSize(1);              
    matrix.setTextColor(COLOR_WHITE); 
    
    int16_t x1, y1;
    uint16_t totalWidth, totalHeight;

    const char* textToPrint = (currentMode == MODE_DISCONNECTED) ? "---" : activeDisplayString;
    matrix.getTextBounds(textToPrint, 0, 0, &x1, &y1, &totalWidth, &totalHeight);

    // Dynamic centered layout calculations
    int16_t xStart = (matrix.width() - totalWidth) / 2;
    int16_t yBase  = ((20 - totalHeight) / 2) - y1 + 3; 
    
    // Straightforward static text draw (Clean & Stable)
    matrix.setCursor(xStart, yBase);
    matrix.print(textToPrint);
  }

  matrix.show();
}

void setup(void) {
  Serial.begin(9600);
  Serial.setTimeout(5); // Prevents blocking reads from lagging animations

  ProtomatterStatus status = matrix.begin();
  if(status != PROTOMATTER_OK) {
    for(;;); 
  }

  updateScreenLayout();
  Serial.println("--- Giant Text Dynamic Color Tesla Ready ---");
}

void loop(void) {
  // Check physical connection state
  if (!Serial) {
    if (currentMode != MODE_DISCONNECTED) {
      savedModeBeforeDisconnect = currentMode; 
      currentMode = MODE_DISCONNECTED;
      updateScreenLayout();
    }
  } else {
    if (currentMode == MODE_DISCONNECTED) {
      currentMode = savedModeBeforeDisconnect; 
      updateScreenLayout();
    }
  }

  // Parse Serial stream incoming commands
  if (Serial && Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.length() > 0) {
      // 1. Safely intercept/ignore initialization strings
      if (input.startsWith("c0")) {
        Serial.println("Status: Config packet handled.");
      } 
      // 2. Intercept LabRAD shutdown message
      else if (input.startsWith("f(")) {
        currentMode = MODE_DISCONNECTED;
        Serial.println("Status: Script disconnected gracefully");
      }
      else if (input.equalsIgnoreCase("m1")) {
        currentMode = MODE_RAINBOW_BORDER;
        Serial.println("Status: Mode shifted to GLOWING RAINBOW");
      } 
      else if (input.equalsIgnoreCase("m0")) {
        currentMode = MODE_STATIC_BORDER;
        Serial.println("Status: Mode shifted to STATIC PURPLE");
      } 
      else if (input.equalsIgnoreCase("a0")) {
        currentMode = MODE_BIRTHDAY_SCROLL;
        scrollXPosition = matrix.width(); 
        Serial.println("Status: Mode shifted to BIRTHDAY SCROLL");
      } 
      else {
        // Safe context assumption: The payload is now a pure float value
        float number = input.toFloat();

        if (number < 0.0f || number > 99.999f) {
          Serial.println("Error: Number out of bounds (0.000 to 99.999)");
        } else {
          char numberBuffer[16];
          dtostrf(number, 1, 3, numberBuffer);
          sprintf(activeDisplayString, "%s", numberBuffer); 
          
          if (currentMode == MODE_BIRTHDAY_SCROLL) {
            currentMode = MODE_STATIC_BORDER;
          }

          Serial.print("Updated Number: ");
          Serial.println(activeDisplayString);
        }
      }
      updateScreenLayout();
    }
  }

  unsigned long currentMillis = millis();

  // Background UI Animation handler (targeting ~60Hz ticks)
  if (currentMillis - lastAnimUpdate >= 15) {
    lastAnimUpdate = currentMillis;
    bool processRedraw = false;

    if (currentMode == MODE_RAINBOW_BORDER) {
      rainbowPhase += 4.5f; 
      if (rainbowPhase >= 256.0f) rainbowPhase -= 256.0f;
      
      float wave = sin((2.0 * PI * currentMillis) / 500.0); 
      currentGlowFactor = 0.625f + (wave * 0.375f); 
      processRedraw = true;
    }
    else if (currentMode == MODE_DISCONNECTED) {
      float slowRedWave = sin((2.0 * PI * currentMillis) / 3000.0);
      redGlowFactor = 0.65f + (slowRedWave * 0.35f);
      processRedraw = true;
    }

    if (processRedraw) {
      updateScreenLayout();
    }
  }

  // Text marquee dynamic timing handler
  if (currentMode == MODE_BIRTHDAY_SCROLL) {
    if (currentMillis - lastScrollUpdate >= 50) {
      lastScrollUpdate = currentMillis;
      scrollXPosition--; 
      if (scrollXPosition < -252) {
        scrollXPosition = matrix.width(); 
      }
      updateScreenLayout();
    }
  }
}
