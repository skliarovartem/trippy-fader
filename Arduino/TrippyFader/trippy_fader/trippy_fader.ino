#include "MACKIE.h"
#include "ResponsiveAnalogRead.h"
#include <MIDIUSB.h>
#include "usb_rename.h"

USBRename dummy = USBRename("Trippy Fader", "Nerd Musician", "0001");

int channel = 0;

void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = { 0x09, 0x90 | channel, pitch, velocity };
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = { 0x08, 0x80 | channel, pitch, velocity };
  MidiUSB.sendMIDI(noteOff);
}

void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t packet = { 0x0B, 0xB0 | channel, control, value };
  MidiUSB.sendMIDI(packet);
}

const byte N_BUTTONS = 12;
const byte BUTTON_ARDUINO_PIN[N_BUTTONS] = { 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };

byte MESSAGE_VAL[N_BUTTONS] = {
  0x11,
  MUTE_1,
  SOLO_1,
  0x4B,
  UNDO,
  PLAY,
  0x57,
  LEFT,
  STOP,
  RECORD,
  RIGHT,
  LOOP_InOut
};


unsigned long buttonsPreviousMillis = 0;
const unsigned long buttonsInterval = 30;
unsigned long debounceDelay = 60;

int buttonCState[N_BUTTONS] = { HIGH };
int buttonPState[N_BUTTONS] = { HIGH };
unsigned long lastDebounceTime[N_BUTTONS] = { 0 };
byte velocity[N_BUTTONS] = { 127 };
byte toggleVelocity[N_BUTTONS] = { 127 };

void buttons() {
  readButtonStates();
  handleButtons();
}

void readButtonStates() {
  for (int i = 0; i < N_BUTTONS; i++) {
    buttonCState[i] = digitalRead(BUTTON_ARDUINO_PIN[i]);
  }
}

void handleButtons() {
  for (int i = 0; i < N_BUTTONS; i++) {
    if ((millis() - lastDebounceTime[i]) > debounceDelay) {
      if (buttonPState[i] != buttonCState[i]) {
        lastDebounceTime[i] = millis();
        if (buttonCState[i] == LOW) {  // Button is pressed
          velocity[i] = 127;
        } else {
          velocity[i] = 0;  // Button is released
          Serial.println(i);
        }
        noteOn(channel, MESSAGE_VAL[i], 127 - velocity[i]);
        MidiUSB.flush();
      }
      buttonPState[i] = buttonCState[i];
    }
  }
}

const int faderMax = 0;
const int faderMin = 1023;
int prevFaderValue = 0;
int repeatFaderPosition = 0;
int rawValue, oldValue;
const byte POT_ARDUINO_PIN = A0;
float snapMultiplier = 0.01;
ResponsiveAnalogRead responsivePot;

void potentiometers() {
  responsivePot.update();
  int value = responsivePot.getRawValue();
  int val = map(value, faderMin, faderMax, 0, 127);
  if (abs(value - prevFaderValue) > 4) {
    prevFaderValue = value;
    repeatFaderPosition = 0;
    controlChange(channel, 0x50, val);
  } else if (repeatFaderPosition < 3) {
    prevFaderValue = value;
    repeatFaderPosition = repeatFaderPosition + 1;
    controlChange(channel, 0x50, val);
  }
}

void MIDIread() {
  midiEventPacket_t rx = MidiUSB.read();
  if (rx.header == 4 && rx.byte1 == 99 && rx.byte2 == 99) {
    handleSelectedTrack(rx);
  }
}

void handleSelectedTrack(midiEventPacket_t rx) {
  repeatFaderPosition = 10;
  moveToFirstSession();
  moveToTrack(rx.byte3);
  MidiUSB.flush();
}

void moveToTrack(int moveToTrack) {
  for (int i = 0; i < moveToTrack; i++) {
    noteOn(channel, BANK_RIGHT, 127);
    noteOn(channel, BANK_RIGHT, 0);
  }
  return;
}

void moveToFirstSession() {
  for (int i = 0; i < 60; i++) {
    noteOn(channel, BANK_LEFT, 127);
    noteOn(channel, BANK_LEFT, 0);
  }
  MidiUSB.flush();
}
void setup() {
  Serial.begin(9600);
  Serial.println("Trippy Fader On");
  responsivePot = ResponsiveAnalogRead(POT_ARDUINO_PIN, true, snapMultiplier);
  moveToFirstSession();
  for (int i = 0; i < N_BUTTONS; i++) {
    pinMode(BUTTON_ARDUINO_PIN[i], INPUT_PULLUP);
  }
}

void loop() {

  MIDIread();
  unsigned long buttonsCurrentMillis = millis();
  if (buttonsCurrentMillis - buttonsPreviousMillis >= buttonsInterval) {
    buttonsPreviousMillis = buttonsCurrentMillis;
    buttons();
  }
  potentiometers();
}