#pragma once
// Compile-time hardware configuration shared by every Unit's build.

#include <Adafruit_NeoPixel.h>

// LED strip
#define LED_COUNT 60
#define LED_PIXEL_TYPE (NEO_GRB + NEO_KHZ800)  // WS2812B
#define LED_DATA_PIN 0                         // XIAO ESP32-C6 D0 (GPIO0)

// LD2410C presence sensor (UART)
#define LD2410_RX_PIN 17  // XIAO ESP32-C6 D7 (GPIO17), wired to the sensor's TX
#define LD2410_TX_PIN 16  // XIAO ESP32-C6 D6 (GPIO16), wired to the sensor's RX

// Current budget for the strip, in milliamps
#define MAX_MILLIAMPS 2000

// How long the room must be empty before motion closes the Strip
#define MOTION_ABSENCE_MS 60000UL
