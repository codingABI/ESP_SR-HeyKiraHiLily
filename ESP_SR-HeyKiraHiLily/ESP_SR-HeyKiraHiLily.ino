/*
 * Project: ESP_SR-HeyKiraHiLily (Ding30)
 *
 * An offline ESP_SR Speech Recognition example for Arduino IDE and ESP32-S3 with a way to change the Wake Words.
 * The Sketch based on "ESP_SR\Basic.ino" from arduino-esp32
 * See https://github.com/codingABI/ESP_SR-HeyKiraHiLily for requirements and more details.
 *
 * License: CC0
 * Copyright (c) 2026 codingABI
 *
 * created by codingABI https://github.com/codingABI/ESP_SR-HeyKiraHiLily
 *
 * Hardware:
 * - ESP32-S3 N16R8 (Board manager: ESP32S3 Dev Module, PSRAM: OPI PSRAM, Flash Size: 16 MB (128Mb), Partition: ESP SR 16M)
 * - INMP441
 * - passive buzzer
 *
 * History:
 * 14.09.2026, Initial version
 */

#include <Arduino.h>
#include "ESP_I2S.h"
#include "ESP_SR.h"

// Pins for I2S INMP441 Microphone
#define I2S_PIN_BCK 17 // SCK/BCK
#define I2S_PIN_DIN 16 // SD/DIN
#define I2S_PIN_WS  47 // WS

#define BUZZER_PIN 38 // Passive buzzer
#define LIGHT_PIN 97 // 97 = virtual pin to control the builtin rgb led as a single led

// ESP_SR requires 16bit audio with 16kHz sample rate
#define I2S_SAMPLE_RATE 16000
#define I2S_DATA_WIDTH  I2S_DATA_BIT_WIDTH_16BIT

// Mono microphone on mono bus
#define SR_INPUT_FORMAT "M"
#define SR_INPUT_CHANNELS SR_CHANNELS_MONO
#define I2S_OUTPUT_CHANNELS I2S_SLOT_MODE_MONO

I2SClass i2s;

// Command IDs
enum {
  SR_CMD_LIGHT_ON,
  SR_CMD_LIGHT_OFF,
  SR_CMD_SOUND_ON,
};

// Command phrases. Can have multiple phrases for given command ID
static const sr_cmd_t sr_commands[] = {
  {SR_CMD_LIGHT_ON, "Turn on the light"},
  {SR_CMD_LIGHT_ON, "Switch on the light"},
  {SR_CMD_LIGHT_ON, "Lights on"},
  {SR_CMD_LIGHT_OFF, "Turn off the light"},
  {SR_CMD_LIGHT_OFF, "Switch off the light"},
  {SR_CMD_LIGHT_OFF, "Lights off"},
  {SR_CMD_LIGHT_OFF, "Go dark"},
  {SR_CMD_SOUND_ON, "Turn on the sound"},
  {SR_CMD_SOUND_ON, "Fire"},
};

enum beepTypes { DEFAULTBEEP, SHORTBEEP, LONGBEEP, HIGHSHORTBEEP, LASER }; // Beep types

// Stop passive buzzer
void stopBeep() {
  ledcWrite(BUZZER_PIN, 0);
  ledcDetach(BUZZER_PIN);
}

// Beep with passive buzzer
void beep(int type=DEFAULTBEEP) {
  switch(type) {
    case DEFAULTBEEP: // 500 Hz for 200ms
      ledcAttach(BUZZER_PIN,500,8);
      ledcWrite(BUZZER_PIN, 128);
      delay(200);
      stopBeep();
      break;
    case SHORTBEEP: // 1 kHz for 100ms
    {
      ledcAttach(BUZZER_PIN,1000,8);
      ledcWrite(BUZZER_PIN, 128);
      delay(100);
      stopBeep();
      break;
    }
    case LONGBEEP: // 250 Hz for 400ms
      ledcAttach(BUZZER_PIN,250,8);
      ledcWrite(BUZZER_PIN, 128);
      delay(400);
      stopBeep();
      break;
    case HIGHSHORTBEEP: { // High and short beep
      ledcAttach(BUZZER_PIN,5000,8);
      ledcWrite(BUZZER_PIN, 128);
      delay(100);
      stopBeep();
      break;
    }
    case LASER: { // Laser like sound
      int i = 5000; // Start frequency in Hz (goes down to 300 Hz)
      int j = 300; // Start duration in microseconds (goes up to 5000 microseconds)
      ledcAttach(BUZZER_PIN,i,8);
      while (i>300) {
        i -=50;
        j +=50;
        ledcWriteTone(BUZZER_PIN,i);
        delayMicroseconds(j+1000);
      }
      stopBeep();
      break;
    }
  }
  delay(100);
}

// Speech recognition event
void onSrEvent(sr_event_t event, int command_id, int phrase_id) {
  switch (event) {
    case SR_EVENT_WAKEWORD:
      Serial.println("WakeWord Detected!");
      if (strlen(SR_INPUT_FORMAT) == 1) {  // Mono recognition does not get CHANNEL event
        ESP_SR.setMode(SR_MODE_COMMAND);   // Switch to Command detection
        beep(SHORTBEEP);
      }
      break;
    case SR_EVENT_WAKEWORD_CHANNEL:
      Serial.printf("WakeWord Channel %d Verified!\n", command_id);
      ESP_SR.setMode(SR_MODE_COMMAND);  // Switch to Command detection
      break;
    case SR_EVENT_TIMEOUT:
      Serial.println("Timeout Detected!");
      ESP_SR.setMode(SR_MODE_WAKEWORD);  // Switch back to WakeWord detection
      beep(LONGBEEP);
      break;
    case SR_EVENT_COMMAND:
      Serial.printf("Command ID %d Detected!\n", command_id);
      switch (command_id) {
        case SR_CMD_LIGHT_ON:
          Serial.println("Light On");
          digitalWrite(LIGHT_PIN, HIGH);
          break;
        case SR_CMD_LIGHT_OFF:
          Serial.println("Light Off");
          digitalWrite(LIGHT_PIN, LOW);
          break;
        case SR_CMD_SOUND_ON:
          Serial.println("Sound On");
          beep(LASER);
          break;
        default: Serial.printf("Unknown Command ID %d!\n", command_id); break;
      }
      ESP_SR.setMode(SR_MODE_COMMAND);  // Allow for more commands to be given, before timeout
      break;
    default: Serial.println("Unknown Event!"); break;
  }
}

void setup() {
  Serial.begin(115200);

  Serial.print("Light pin ");
  Serial.println(LIGHT_PIN);
  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, LOW);

  i2s.setTimeout(1000);

  // I2S Microphone
  // INMP441 I2S Microphone (24bit mic requires data transform to 16bit)
  i2s.setPins(I2S_PIN_BCK, I2S_PIN_WS, -1, I2S_PIN_DIN);
  i2s.begin(I2S_MODE_STD, I2S_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_OUTPUT_CHANNELS, I2S_STD_SLOT_LEFT);
  i2s.configureRX(I2S_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_32BIT, I2S_OUTPUT_CHANNELS, I2S_RX_TRANSFORM_32_TO_16);

  ESP_SR.onEvent(onSrEvent);
  ESP_SR.begin(i2s, sr_commands, sizeof(sr_commands) / sizeof(sr_cmd_t), SR_INPUT_CHANNELS, SR_MODE_WAKEWORD, SR_INPUT_FORMAT);
}

void loop() {
  delay(100);
}
