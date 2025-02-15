
#ifndef __EPD_5IN65F_H__
#define __EPD_5IN65F_H__

#include <Arduino.h>
#include <SPI.h>

// Display resolution
#define EPD_WIDTH       600
#define EPD_HEIGHT      448

// ESP32 Pin definitions
#define RST_PIN         12  // Reset pin
#define DC_PIN          13  // Data/Command control pin
#define CS_PIN          15  // Chip select pin
#define BUSY_PIN        14  // Busy status pin
#define PWR_PIN         27  // Power control pin

#define UWORD   unsigned int
#define UBYTE   unsigned char
#define UDOUBLE  unsigned long

// Color definitions
#define EPD_5IN65F_BLACK   0x0  /// 000
#define EPD_5IN65F_WHITE   0x1  /// 001
#define EPD_5IN65F_GREEN   0x2  /// 010
#define EPD_5IN65F_BLUE    0x3  /// 011
#define EPD_5IN65F_RED     0x4  /// 100
#define EPD_5IN65F_YELLOW  0x5  /// 101
#define EPD_5IN65F_ORANGE  0x6  /// 110
#define EPD_5IN65F_CLEAN   0x7  /// 111

class Epd {
public:
    Epd();
    ~Epd();
    int Init(void);
    void Reset(void);
    void SendCommand(unsigned char command);
    void SendData(unsigned char data);
    void BusyHigh(void);
    void BusyLow(void);
    void Display(const UBYTE *image);
    void DisplayPart(const UBYTE *image, UWORD xstart, UWORD ystart, 
                    UWORD image_width, UWORD image_height);
    void Clear(UBYTE color);
    void Sleep(void);

private:
    void DigitalWrite(int pin, int value);
    int DigitalRead(int pin);
    void DelayMs(unsigned int delaytime);
    void SpiTransfer(unsigned char data);
    
    unsigned int reset_pin;
    unsigned int dc_pin;
    unsigned int cs_pin;
    unsigned int busy_pin;
    unsigned long width;
    unsigned long height;
    SPIClass *spi;
};

#endif

// epd5in65f.cpp
#include "epd5in65f.h"

Epd::Epd() {
    reset_pin = RST_PIN;
    dc_pin = DC_PIN;
    cs_pin = CS_PIN;
    busy_pin = BUSY_PIN;
    width = EPD_WIDTH;
    height = EPD_HEIGHT;
    
    // Initialize SPI
    spi = &SPI;
    
    // Configure pins
    pinMode(reset_pin, OUTPUT);
    pinMode(dc_pin, OUTPUT);
    pinMode(cs_pin, OUTPUT);
    pinMode(busy_pin, INPUT);
    pinMode(PWR_PIN, OUTPUT);
    
    digitalWrite(PWR_PIN, HIGH);
    digitalWrite(cs_pin, HIGH);
}

Epd::~Epd() {
}

void Epd::DigitalWrite(int pin, int value) {
    digitalWrite(pin, value);
}

int Epd::DigitalRead(int pin) {
    return digitalRead(pin);
}

void Epd::DelayMs(unsigned int delaytime) {
    delay(delaytime);
}

void Epd::SpiTransfer(unsigned char data) {
    digitalWrite(cs_pin, LOW);
    spi->transfer(data);
    digitalWrite(cs_pin, HIGH);
}

int Epd::Init(void) {
    // Initialize SPI for ESP32
    spi->begin(18, 19, 23, cs_pin); // SCLK, MISO, MOSI, SS
    spi->beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));
    
    Reset();
    BusyHigh();
    
    SendCommand(0x00);
    SendData(0xEF);
    SendData(0x08);
    SendCommand(0x01);
    SendData(0x37);
    SendData(0x00);
    SendData(0x23);
    SendData(0x23);
    SendCommand(0x03);
    SendData(0x00);
    SendCommand(0x06);
    SendData(0xC7);
    SendData(0xC7);
    SendData(0x1D);
    SendCommand(0x30);
    SendData(0x3C);
    SendCommand(0x41);
    SendData(0x00);
    SendCommand(0x50);
    SendData(0x37);
    SendCommand(0x60);
    SendData(0x22);
    SendCommand(0x61);
    SendData(0x02);
    SendData(0x58);
    SendData(0x01);
    SendData(0xC0);
    SendCommand(0xE3);
    SendData(0xAA);
    
    DelayMs(100);
    SendCommand(0x50);
    SendData(0x37);
    
    return 0;
}

void Epd::BusyHigh(void) {
    while(!(DigitalRead(BUSY_PIN)));
}

void Epd::BusyLow(void) {
    while(DigitalRead(BUSY_PIN));
}

void Epd::Reset(void) {
    DigitalWrite(reset_pin, LOW);
    DelayMs(1);
    DigitalWrite(reset_pin, HIGH);
    DelayMs(200);
}

void Epd::SendCommand(unsigned char command) {
    DigitalWrite(dc_pin, LOW);
    SpiTransfer(command);
}

void Epd::SendData(unsigned char data) {
    DigitalWrite(dc_pin, HIGH);
    SpiTransfer(data);
}

void Epd::Display(const UBYTE *image) {
    SendCommand(0x61);//Set Resolution setting
    SendData(0x02);
    SendData(0x58);
    SendData(0x01);
    SendData(0xC0);
    SendCommand(0x10);
    
    for(int i=0; i<height; i++) {
        for(int j=0; j<width/2; j++) {
            SendData(image[j + ((width/2)*i)]);
        }
    }
    
    SendCommand(0x04);//0x04
    BusyHigh();
    SendCommand(0x12);//0x12
    BusyHigh();
    SendCommand(0x02);  //0x02
    BusyLow();
    DelayMs(200);
}

void Epd::DisplayPart(const UBYTE *image, UWORD xstart, UWORD ystart, 
                     UWORD image_width, UWORD image_height) {
    SendCommand(0x61);//Set Resolution setting
    SendData(0x02);
    SendData(0x58);
    SendData(0x01);
    SendData(0xC0);
    SendCommand(0x10);
    
    for(int i=0; i<height; i++) {
        for(int j=0; j<width/2; j++) {
            if(i<image_height+ystart && i>=ystart && j<(image_width+xstart)/2 && j>=xstart/2) {
                SendData(pgm_read_byte(&image[(j-xstart/2) + (image_width/2*(i-ystart))]));
            }
            else {
                SendData(0x11);
            }
        }
    }
    
    SendCommand(0x04);//0x04
    BusyHigh();
    SendCommand(0x12);//0x12
    BusyHigh();
    SendCommand(0x02);  //0x02
    BusyLow();
    DelayMs(200);
}

void Epd::Clear(UBYTE color) {
    SendCommand(0x61);//Set Resolution setting
    SendData(0x02);
    SendData(0x58);
    SendData(0x01);
    SendData(0xC0);
    SendCommand(0x10);
    
    for(int i=0; i<width/2; i++) {
        for(int j=0; j<height; j++) {
            SendData((color<<4)|color);
        }
    }
    
    SendCommand(0x04);//0x04
    BusyHigh();
    SendCommand(0x12);//0x12
    BusyHigh();
    SendCommand(0x02);  //0x02
    BusyLow();
    DelayMs(500);
}

void Epd::Sleep(void) {
    DelayMs(100);
    SendCommand(0x07);
    SendData(0xA5);
    DelayMs(100);
    DigitalWrite(reset_pin, 0);
}