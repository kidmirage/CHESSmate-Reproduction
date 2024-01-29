#define PA0 0
#define PA1 1
#define PA2 2
#define PA3 3
#define PA4 4
#define PA5 5
#define PA6 6
#define PA7 7

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

// Keep track of what virtual keyboard row has been enabled.
bool keyboard_row = 0;

// Remember what the last write to the sbc port was.
uint8_t sbc_value = 0;

extern "C" {
  uint16_t getpc();
  uint8_t getop();
  void exec6502(int32_t tickcount);
  void reset6502();

  void printhex(uint16_t val) {
    Serial.print(val, HEX);
    Serial.println();
  }

/****
  *  This is the key part of the CHESSmate "emulation". Pretend to be a 6530-24 RRIOT chip.
  *
  *  Translate the reads and writes to and from the program to the 6530-24 RRIOT registers
  *  and map the bits to the actual switches, 7-segment displays, and LEDs.
  *
  ****/
  
  // Handle read from RRIOT registers.
  uint8_t readRRIOT(uint16_t address) {
    if (address == 2) {
      // Reading from PORTB. The only input is from the 6504 _IRQ.
      // Polling for interrupt?
      return sbc_value;
    } else if (address == 0) {
      // Checking for keypress.
      uint8_t result = 0;
      if (keyboard_row == 1) {
        if (digitalRead(PA0) == LOW) {
          result = 0b00100000;
        } else if (digitalRead(PA1) == LOW) {
          result = 0b00010000;
        } else if (digitalRead(PA2) == LOW) {
          result = 0b00001000;
        } else if (digitalRead(PA3) == LOW) {
          result = 0b00000100;
        } else if (digitalRead(PA4) == LOW) {
          result = 0b00000010;
        } else if (digitalRead(PA5) == LOW) {
          result = 0b00000001;
        }
      } else if (keyboard_row == 2) {
        if (digitalRead(PA6) == LOW) {
          result = 0b01000000;
        } else if (digitalRead(PA7) == LOW) {
          result = 0b00100000;
        } else if (digitalRead(CLEAR_KEY) == LOW) {
          result = 0b00010000;
        } else if (digitalRead(ENTER_KEY) == LOW) {
          result = 0b00001000;
        }
      }
      return result;
    }
  }
  
  // Handle write to RRIOT registers.
  void writeRRIOT(uint16_t address, uint8_t value) {
    if (address == 0) {
      // PA0 - PA7 Data Register (SAD)
      // Maps to Pro Mini pins 0 - 7
      uint8_t check_bit = 0b00000001;
      for (int i = 0; i < 7; i++) {
        if (value & check_bit > 0) {
          digitalWrite(i, HIGH);
        } else {
          digitalWrite(i, LOW);
        }
        check_bit <<= 1;
      }
      // Check for CHESSmate LOSES.
      if (value & check_bit > 0) {
          digitalWrite(LOSES_LED, HIGH);
      } else {
          digitalWrite(LOSES_LED, LOW);
      }
    } else if (address == 1) {
      // PA0 - PA7 Data Direction Register (PADD)
      // Brute force for now. Think about PORTD implementation.
      uint8_t check_bit = 0b00000001;
      for (int i = 0; i < 8; i++) {
        if (value & check_bit > 0) {
          pinMode(i, OUTPUT);
        } else {
          pinMode(i, INPUT_PULLUP);
        }
        check_bit <<= 1;
      }
    } else if (address == 2) {
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
      switch(value & 0x07) {
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
      if (value & 0b00001000 > 0) {
          digitalWrite(CHECK_LED, HIGH);
      } else {
          digitalWrite(CHECK_LED, LOW);
      }
      // Check for BLACK or WHITE.
      if (value & 0b00010000 > 0) {
          digitalWrite(B_W_LED, HIGH);
      } else {
          digitalWrite(B_W_LED, LOW);
      }
    } else if (address == 3) {
      // PB0 - PB7 Data Direction Register (PBDD)
      // Does not directly map to specific pins. Will always be output.
      Serial.print("PPDD set to: ");
      printhex(value);
    } else {
      Serial.print("More...");
    }
  }
}

void setup () {
  Serial.begin (9600);
  while (!Serial) {
    delay(10); // Wait for serial port to connect.
  }
  Serial.println ();

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

  reset6502();
}

void loop () {
  // If timing is enabled, this value is in 6502 clock ticks. 
  // Otherwise, simply instruction count.
  exec6502(100); 

  // Check for NEW GAME. (Reset)
  if (digitalRead(NEW_GAME_KEY) == LOW) {
    reset6502();
  }    
}

