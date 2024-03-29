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
 */

// IO pins used on the arduino.
#define PA0 7
#define PA1 6
#define PA2 5
#define PA3 4
#define PA4 3
#define PA5 2
#define PA6 1
#define PA7 0

#define ENTER_KEY     8
#define CLEAR_KEY     9
#define NEW_GAME_KEY  10

#define CHECK_LED     11
#define LOSES_LED     12
#define B_W_LED       13

#define DISPLAY_1     A0
#define DISPLAY_2     A1
#define DISPLAY_3     A2
#define DISPLAY_4     A3

#define BUZZER_1      A4
#define BUZZER_2      A5

// Keep track of what "virtual" keyboard row has been enabled.
int keyboard_row = 0;

// Remember what the last write to the SBC (B data port) was.
uint8_t sbc_value = 0;

// Have to invert the bits going to the 7-segment displays.
uint8_t lookup[16] = {
  0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
  0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf,
};

// Useful fnctions from the emulator C code.
extern "C" {
  uint16_t getpc();
  uint8_t getop();
  void exec6502(int32_t tickcount);
  void reset6502();

  void printhex(uint16_t val) {
    //Serial.print(val, HEX);
    //Serial.println();
  }

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
      PORTD = ((lookup[value & 0b1111] << 4) | lookup[value >> 4]) & 0xFE;

      // Check for CHESSmate LOSES.
      if ((value & 0b10000000) > 0) {
        digitalWrite(LOSES_LED, HIGH);
      } else {
        digitalWrite(LOSES_LED, LOW);
      }
    } else if (address == 1) {          // Port A Data Direction
      // PA0 - PA7 Data Direction Register (PADD)
      if (value == 128) {
        PORTD = 0x00;     // All inputs.
        PORTD |= 0xFF;    // Input pullups.
      } else {
        DDRD = value;
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

void setup () {
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
