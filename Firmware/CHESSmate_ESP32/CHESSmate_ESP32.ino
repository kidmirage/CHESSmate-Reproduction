/** CHESSmate emulator for Arduino.
 *  
 *  The 6502 emulator is from Mike Chambers.
 *
 *    http://forum.arduino.cc/index.php?topic=193216.0
 *
 *  Project inspired by Oscar Vermeulen's KIM-1 Uno project.
 *
 *    https://obsolescence.wixsite.com/obsolescence/kim-uno-summary-c1uuh
 *
 *  CHESSmate code added by Michael Gardi.
 *
 *    https://hackaday.io/project/194011-commodore-chessmate-reproduction
 *
 *  This Sketch is specifically for the ESP32 WROOM.
 */

// IO pins used on the arduino.
#define PA0 15
#define PA1 12
#define PA2 14
#define PA3 27
#define PA4 26
#define PA5 25
#define PA6 33
#define PA7 32

#define ENTER_KEY     13
#define CLEAR_KEY     23
#define NEW_GAME_KEY  2

#define CHECK_LED     0
#define LOSES_LED     4
#define B_W_LED       16

#define DISPLAY_1     17
#define DISPLAY_2     5
#define DISPLAY_3     18
#define DISPLAY_4     19

#define BUZZER_1      21
#define BUZZER_2      22

// Keep track of what "virtual" keyboard row has been enabled.
int keyboard_row = 0;

// Remember what the last write to the SBC (B data port) was.
uint8_t sbc_value = 0;

extern "C" { 
  void exec6502(int32_t tickcount);
  void reset6502();
}

void setup () {
  Serial.begin (115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB
  }
  delay(100);
  Serial.println ("CHESSmate Starting");
  
  // Set modes for pins that won't change.
  pinMode(NEW_GAME_KEY, INPUT_PULLUP);
  pinMode(CLEAR_KEY, INPUT_PULLUP);
  pinMode(ENTER_KEY, INPUT_PULLUP);

  pinMode(CHECK_LED, OUTPUT);
  pinMode(LOSES_LED, OUTPUT);
  pinMode(B_W_LED, OUTPUT);

  pinMode(DISPLAY_1, OUTPUT);
  pinMode(DISPLAY_2, OUTPUT);
  pinMode(DISPLAY_3, OUTPUT);
  pinMode(DISPLAY_4, OUTPUT);

  pinMode(BUZZER_1, OUTPUT);
  pinMode(BUZZER_2, OUTPUT);

  // Set the program counter to the start of the CHESSmate program, set stack pointer, clear registers and flags.
  reset6502();
}

void loop () {
  // If timing is enabled, this value is in 6502 clock ticks.
  // Otherwise, simply instruction count.
  exec6502(10000);
}

// Required functions from the emulator C code.
extern "C" { 
  /********************************************************************************************
   *   This is the key part of the CHESSmate "emulation". Pretend to be a 6530-24 RRIOT chip.
   *
   *   Translate the reads and writes to and from the program to the 6530-24 RRIOT registers
   *   and map the bits to the actual switches, 7-segment displays, and LEDs.
   *
   ********************************************************************************************/
  // Handle reads from RRIOT registers.
  uint8_t readRRIOT(uint16_t address) {
    if (address == 2) {             // Port A Data
      // Reading from PORTB. Return the previous value of PORTB.
      return sbc_value;
    } else if (address == 0) {      // Port B Data
      // Check for NEW GAME. (Reset)
      if (digitalRead(NEW_GAME_KEY) == LOW) {
        reset6502();
      }
      // Default result, no keys pressed.
      uint8_t result = 0b01111111;
  
      // Checking for keypress.
      if (keyboard_row == 1) {
        if (digitalRead(PA7) == LOW) {
          result = 0b01011111;
        } else if (digitalRead(PA6) == LOW) {
          result = 0b01101111;
        } else if (digitalRead(PA5) == LOW) {
          result = 0b01110111;
        } else if (digitalRead(PA4) == LOW) {
          result = 0b01111011;
        } else if (digitalRead(PA3) == LOW) {
          result = 0b01111101;
        } else if (digitalRead(PA2) == LOW) {
          result = 0b01111110;
        }
      } else if (keyboard_row == 2) {
        if (digitalRead(PA1) == LOW) {
          result = 0b00111111;
        } else if (digitalRead(PA0) == LOW) {
          result = 0b01011111;
        } else if (digitalRead(CLEAR_KEY) == LOW) {
          result = 0b01101111;
        } else if (digitalRead(ENTER_KEY) == LOW) {
          result = 0b01110111;
        }
      }
      return result;
    }
  }

  // Handle writes to RRIOT registers.
  void writeRRIOT(uint16_t address, uint8_t value) {
    if (address == 0) {                 // Port A Data
      // PA0 - PA7 Data Register (SAD)
      // Maps to Pro Mini pins 0 - 7
      if ((value & 0b00000001) > 0) {
        digitalWrite(PA0, HIGH);
      } else {
        digitalWrite(PA0, LOW);
      }
      if ((value & 0b00000010) > 0) {
        digitalWrite(PA1, HIGH);
      } else {
        digitalWrite(PA1, LOW);
      }
      if ((value & 0b00000100) > 0) {
        digitalWrite(PA2, HIGH);
      } else {
        digitalWrite(PA2, LOW);
      }
      if ((value & 0b00001000) > 0) {
        digitalWrite(PA3, HIGH);
      } else {
        digitalWrite(PA3, LOW);
      }
      if ((value & 0b00010000) > 0) {
        digitalWrite(PA4, HIGH);
      } else {
        digitalWrite(PA4, LOW);
      }
      if ((value & 0b00100000) > 0) {
        digitalWrite(PA5, HIGH);
      } else {
        digitalWrite(PA5, LOW);
      }
      if ((value & 0b01000000) > 0) {
        digitalWrite(PA6, HIGH);
      } else {
        digitalWrite(PA6, LOW);
      }
      if ((value & 0b10000000) > 0) {
        digitalWrite(PA7, HIGH);
      } else {
        digitalWrite(PA7, LOW);
      }
      // Check for CHESSmate LOSES.
      if ((value & 0b10000000) > 0) {
        digitalWrite(LOSES_LED, HIGH);
      } else {
        digitalWrite(LOSES_LED, LOW);
      }
    } else if (address == 1) {          // Port A Data Direction
      // PA0 - PA7 Data Direction Register (PADD)
      if (value == 128) {
        pinMode(PA0, INPUT_PULLUP);
        pinMode(PA1, INPUT_PULLUP);
        pinMode(PA2, INPUT_PULLUP);
        pinMode(PA3, INPUT_PULLUP);
        pinMode(PA4, INPUT_PULLUP);
        pinMode(PA5, INPUT_PULLUP);
        pinMode(PA6, INPUT_PULLUP);
        pinMode(PA7, INPUT_PULLUP);
      } else {
        pinMode(PA0, OUTPUT);
        pinMode(PA1, OUTPUT);
        pinMode(PA2, OUTPUT);
        pinMode(PA3, OUTPUT);
        pinMode(PA4, OUTPUT);
        pinMode(PA5, OUTPUT);
        pinMode(PA6, OUTPUT);
        pinMode(PA7, OUTPUT);;
      }
    } else if (address == 2) {          // Port B Data
      // PB0 - PB7 Data Register (SBD)
      // Remember the last value written to this register.
      sbc_value = value;
  
      // The bottom 3 bits of the value control 8 IO lines. Only 1 will be turned on.
      digitalWrite(DISPLAY_1, LOW);
      digitalWrite(DISPLAY_2, LOW);
      digitalWrite(DISPLAY_3, LOW);
      digitalWrite(DISPLAY_4, LOW);
      keyboard_row = 0;
      digitalWrite(BUZZER_1, LOW);
      digitalWrite(BUZZER_2, LOW);
  
      // Determine which line to enable.
      switch (value & 0x07) {
        case 0:
          digitalWrite(DISPLAY_1, HIGH);
          break;
        case 1:
          digitalWrite(DISPLAY_2, HIGH);
          break;
        case 2:
          digitalWrite(DISPLAY_3, HIGH);
          break;
        case 3:
          digitalWrite(DISPLAY_4, HIGH);
          break;
        case 4:
          keyboard_row = 1;
          break;
        case 5:
          keyboard_row = 2;
          break;
        case 6:
          digitalWrite(BUZZER_1, HIGH);
          break;
        case 7:
          digitalWrite(BUZZER_2, HIGH);
          break;
        default:
          break;
      }
      // Check for CHECK.
      if ((value & 0b00001000) > 0) {
          digitalWrite(CHECK_LED, HIGH);
      } else {
          digitalWrite(CHECK_LED, LOW);
      }
      // Check for BLACK or WHITE.
      if ((value & 0b00010000) > 0) {
        digitalWrite(B_W_LED, LOW);
      } else {
        digitalWrite(B_W_LED, HIGH);
      }
    } else if (address == 3) {            // Port B Data Direction
      // PB0 - PB7 Data Direction Register (PBDD)
      // Does not directly map to specific pins. Will always be output.
    }
  }
} 
