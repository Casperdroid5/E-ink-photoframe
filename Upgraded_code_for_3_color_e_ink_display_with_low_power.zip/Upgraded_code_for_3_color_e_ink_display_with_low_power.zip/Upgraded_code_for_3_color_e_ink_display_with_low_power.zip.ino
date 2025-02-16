/* 
   E-ink Display Controller with improved SD card handling
   For 4.2 inch 3-color e-ink display with SD card reader
*/

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <SdFat.h>

// Function prototypes and constants
bool drawBitmapFromSD(const char *filename, int16_t x, int16_t y, bool with_color = true);
static const uint16_t input_buffer_pixels = 20;
static const uint16_t max_row_width = 640;
static const uint16_t max_palette_pixels = 256;

// Pin Definitions
#define SD_CS 6        // SD card chip select
#define EPD_CS 10      // E-paper display chip select
#define EPD_DC 8       // E-paper DC
#define EPD_RST 9      // E-paper reset
#define EPD_BUSY 7     // E-paper busy
#define PIN_ADVANCE 2  // Interrupt pin for manual advance
#define PIN_MOSFET 4   // Power control MOSFET

// Configuration
#define NUM_BITMAPS 5
#define K_SLEEPCYCLES 9412u

// Display Configuration
#define GxEPD2_DRIVER_CLASS GxEPD2_420c  // GDEW042Z15 400x300

// Global Objects
#if defined(__AVR)
GxEPD2_DRIVER_CLASS display(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY);
#endif
SdFat SD;

// Buffers
uint8_t input_buffer[3 * input_buffer_pixels];
uint8_t output_row_mono_buffer[max_row_width / 8];
uint8_t output_row_color_buffer[max_row_width / 8];
uint8_t mono_palette_buffer[max_palette_pixels / 8];
uint8_t color_palette_buffer[max_palette_pixels / 8];

// Global Variables
static uint16_t sleepCycles = K_SLEEPCYCLES;
static uint8_t idxBitmap = 0;
char szBitmapName[10];
volatile bool bWDT = false;
volatile bool bINT0 = false;
volatile unsigned long lastInterruptTime = 0;
volatile unsigned long interruptCount = 0;
int16_t w2, h2;

// Helper functions
bool initSD() {
  digitalWrite(SD_CS, HIGH);
  delay(100);  // Give SD card time to stabilize

  // Toggle SD CS a few times to reset card's state machine
  for (int i = 0; i < 3; i++) {
    digitalWrite(SD_CS, LOW);
    delayMicroseconds(50);
    digitalWrite(SD_CS, HIGH);
    delayMicroseconds(50);
  }

  return SD.begin(SD_CS);
}

// Interrupt Service Routines
ISR(WDT_vect) {
  wdt_disable();
  bWDT = true;
}

ISR(INT0_vect) {
  unsigned long currentTime = millis();
  if (currentTime - lastInterruptTime > 250) {
    EIMSK &= ~_BV(INT0);  // Disable INT0 interrupt
    bINT0 = true;
    interruptCount++;
    lastInterruptTime = currentTime;
  }
}

void setup() {
  // Initialize power control
  pinMode(PIN_MOSFET, OUTPUT);
  digitalWrite(PIN_MOSFET, HIGH);
  delay(100);  // Increased delay for stability

  Serial.begin(115200);
  Serial.println();
  delay(100);
  Serial.println(F("Starting setup..."));

  // Initialize display
  display.init(115200);
  Serial.println(F("Display initialized"));

  // Initialize SD card
  Serial.print(F("Initializing SD card..."));
  if (!initSD()) {
    Serial.println(F("SD failed!"));
    return;
  }
  Serial.println(F("SD OK!"));

  // Configure interrupt pin
  pinMode(PIN_ADVANCE, INPUT_PULLUP);
  EICRA = (1 << ISC01);  // Falling edge
  EIMSK |= (1 << INT0);  // Enable INT0

  // Verify BMP file exists
  SdFile file;
  if (!file.open("1.bmp", FILE_READ)) {
    Serial.println(F("Missing BMP file!"));
    while (1)
      ;
  }
  file.close();

  w2 = (display.WIDTH / 2) - 200;
  h2 = (display.HEIGHT / 2) - 150;
  idxBitmap = 1;

  // Draw initial image
  if (drawBitmapFromSD("1.bmp", w2, h2)) {
    Serial.println(F("Initial image drawn"));
  }

  Serial.println(F("Setup complete"));
}

void loop() {
  if (bINT0) {
    Serial.println(F("Processing button interrupt"));
    bINT0 = false;

    digitalWrite(PIN_MOSFET, HIGH);
    delay(100);  // Increased delay for stability

    // Reinitialize SD card
    if (!initSD()) {
      Serial.println(F("SD reinit failed!"));
      digitalWrite(PIN_MOSFET, LOW);
      EIMSK |= _BV(INT0);
      return;
    }

    do {
      idxBitmap++;
      if (idxBitmap > NUM_BITMAPS) idxBitmap = 1;
      sprintf(szBitmapName, "%d.bmp", idxBitmap);
      Serial.print(F("Trying file: "));
      Serial.println(szBitmapName);
    } while (!drawBitmapFromSD(szBitmapName, w2, h2));

    EIMSK |= _BV(INT0);
  } else if (bWDT) {
    bWDT = false;

    if (sleepCycles > 0) {
      sleepCycles--;
    } else {
      digitalWrite(PIN_MOSFET, HIGH);
      delay(100);

      // Reinitialize SD card
      if (!initSD()) {
        Serial.println(F("SD reinit failed!"));
        digitalWrite(PIN_MOSFET, LOW);
        return;
      }

      do {
        idxBitmap++;
        if (idxBitmap > NUM_BITMAPS) idxBitmap = 1;
        sprintf(szBitmapName, "%d.bmp", idxBitmap);
        Serial.print(F("Trying file: "));
        Serial.println(szBitmapName);
      } while (!drawBitmapFromSD(szBitmapName, w2, h2));

      sleepCycles = K_SLEEPCYCLES;
    }
  }

  digitalWrite(PIN_MOSFET, LOW);

  MCUSR = 0;
  WDTCSR |= _BV(WDCE) | _BV(WDE);
  WDTCSR = _BV(WDIE) | _BV(WDP3) | _BV(WDP0);
  wdt_reset();

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sei();
  sleep_mode();
}

uint16_t read16(SdFile &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read();
  ((uint8_t *)&result)[1] = f.read();
  return result;
}

uint32_t read32(SdFile &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read();
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read();
  return result;
}

bool drawBitmapFromSD(const char *filename, int16_t x, int16_t y, bool with_color) {
  SdFile file;
  bool valid = false;
  bool flip = true;
  uint8_t lsb, msb;
  uint16_t pn, yrow;
  uint32_t startTime = millis();

  if ((x >= int16_t(display.WIDTH)) || (y >= int16_t(display.HEIGHT)))
    return false;

  Serial.println();
  Serial.println(F("--------PICTURE DATA--------"));
  Serial.print(F("Loading image '"));
  Serial.print(filename);
  Serial.println('\'');

  if (!file.open(filename, FILE_READ)) {
    idxBitmap = 0;
    return false;
  }

  if (read16(file) == 0x4D42) {  // BMP signature
    uint32_t fileSize = read32(file);
    uint32_t creatorBytes = read32(file);
    uint32_t imageOffset = read32(file);
    uint32_t headerSize = read32(file);
    uint32_t width = read32(file);
    uint32_t height = read32(file);
    uint16_t planes = read16(file);
    uint16_t depth = read16(file);
    uint32_t format = read32(file);

    if ((planes == 1) && ((format == 0) || (format == 3))) {
      Serial.print(F("File size: "));
      Serial.println(fileSize);
      Serial.print(F("Image Offset: "));
      Serial.println(imageOffset);
      Serial.print(F("Header size: "));
      Serial.println(headerSize);
      Serial.print(F("Bit Depth: "));
      Serial.println(depth);
      Serial.print(F("Image size: "));
      Serial.print(width);
      Serial.print('x');
      Serial.println(height);

      uint32_t rowSize = (width * depth / 8 + 3) & ~3;
      if (height < 0) {
        height = -height;
        flip = false;
      }

      uint16_t w = width;
      uint16_t h = height;
      if ((x + w - 1) >= int16_t(display.WIDTH))
        w = int16_t(display.WIDTH) - x;
      if ((y + h - 1) >= int16_t(display.HEIGHT))
        h = int16_t(display.HEIGHT) - y;

      if (w <= max_row_width) {
        valid = true;
        uint8_t bitmask = 0xFF;
        uint8_t bitshift = 8 - depth;
        uint16_t red, green, blue;
        bool whitish, colored;

        if (depth == 1) with_color = false;
        if (depth <= 8) {
          if (depth < 8) bitmask >>= depth;
          file.seekSet(imageOffset - (4 << depth));

          for (uint16_t pn = 0; pn < (1 << depth); pn++) {
            blue = file.read();
            green = file.read();
            red = file.read();
            file.read();
            whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80);
            colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));

            if (0 == pn % 8) mono_palette_buffer[pn / 8] = 0;
            mono_palette_buffer[pn / 8] |= whitish << pn % 8;
            if (0 == pn % 8) color_palette_buffer[pn / 8] = 0;
            color_palette_buffer[pn / 8] |= colored << pn % 8;
          }
        }

        display.clearScreen();
        uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;

        for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) {
          uint32_t in_remain = rowSize;
          uint32_t in_idx = 0;
          uint32_t in_bytes = 0;
          uint8_t in_byte = 0;
          uint8_t in_bits = 0;
          uint8_t out_byte = 0xFF;
          uint8_t out_color_byte = 0xFF;
          uint32_t out_idx = 0;

          file.seekSet(rowPosition);
          for (uint16_t col = 0; col < w; col++) {
            if (in_idx >= in_bytes) {
              in_bytes = file.read(input_buffer,
                                   in_remain > sizeof(input_buffer) ? sizeof(input_buffer) : in_remain);
              in_remain -= in_bytes;
              in_idx = 0;
            }

            switch (depth) {
              case 24:
                blue = input_buffer[in_idx++];
                green = input_buffer[in_idx++];
                red = input_buffer[in_idx++];
                whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80);
                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));
                break;

              case 16:
                lsb = input_buffer[in_idx++];
                msb = input_buffer[in_idx++];
                if (format == 0) {
                  blue = (lsb & 0x1F) << 3;
                  green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                  red = (msb & 0x7C) << 1;
                } else {
                  blue = (lsb & 0x1F) << 3;
                  green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                  red = (msb & 0xF8);
                }
                whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80);
                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0));
                break;

              case 1:
              case 4:
              case 8:
                if (0 == in_bits) {
                  in_byte = input_buffer[in_idx++];
                  in_bits = 8;
                }
                pn = (in_byte >> bitshift) & bitmask;
                whitish = mono_palette_buffer[pn / 8] & (0x1 << pn % 8);
                colored = color_palette_buffer[pn / 8] & (0x1 << pn % 8);
                in_byte <<= depth;
                in_bits -= depth;
                break;
            }

            if (whitish) {
              // keep white
            } else if (colored && with_color) {
              out_color_byte &= ~(0x80 >> col % 8);
            } else {
              out_byte &= ~(0x80 >> col % 8);
            }

            if ((7 == col % 8) || (col == w - 1)) {
              output_row_color_buffer[out_idx] = out_color_byte;
              output_row_mono_buffer[out_idx++] = out_byte;
              out_byte = 0xFF;
              out_color_byte = 0xFF;
            }
          }

          yrow = y + (flip ? h - row - 1 : row);
          display.writeImage(output_row_mono_buffer, output_row_color_buffer, x, yrow, w, 1);
        }

        Serial.print(F("loaded in "));
        Serial.print(millis() - startTime);
        Serial.println(F(" ms"));
        display.refresh();
        Serial.println(F("---------------------------"));
        Serial.println();
      }
    }
  }

  file.close();

  if (!valid) {
    Serial.println(F("bitmap format not handled."));
    return false;
  }

  return true;
}