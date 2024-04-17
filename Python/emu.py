import os
import pygame
import sys
from cpu import CPU
from mmu import MMU
from pygame import key
import time

class Emulator:
  
    """
    
    Contains 6502 CPU that runs the CHESSmate application.
    
    """
    # Define the memory spaces that CHESSmate uses.
    CHESSMATE_RAM =     0x0000
    CHESSMATE_ROM =     0xF000
    RRIOT_REGISTERS =   0x8B00
    RRIOT_RAM =         0x8B80
    RRIOT_ROM =         0x8C00
    
    RAM_SIZE =          2048
    ROM_SIZE =          4096
    RRIOT_REG_SIZE =    16
    RRIOT_RAM_SIZE =    64
    RRIOT_ROM_SIZE =    1024

    # Keep track of what "virtual" keyboard row has been enabled.
    keyboard_row = 0

    # Remember what the last write to the SBC (B data port) was.
    sbc_value = 0
    
    # The display surface.
    screen = None
    
    # The current character selected for output to a 7-Segment display.
    current_character = ' '
    current_showing = [' ', ' ', ' ', ' ']
    
    # The current 7-Segment display selected.
    current_display = -1
   
    # Create a font to display in the 7-Segment display windows.
    font = None
    
    # Define the display "windows" for the displays and LEDs.
    DISPLAYs = [(187,125,60,80), (289,125,60,80), (454,125,60,80), (559,125,60,80)]
    D1,D2,D3,D4 = 0,1,2,3
    
    LEDs = [(138,281,35,27), (304,281,35,27), (468,281,35,27), (632,281,35,27)]
    LCHECK,LLOSES,LWHITE,LBLACK = 0,1,2,3
    
    # Save the "window" patch used to erase the displays and LEDs.
    SAVE_DISPLAYs = []
    SAVE_LEDs = []
    
    # Define the coordinates for all 19 buttons.
    BUTTONs = [(82,483,61,61), (164,483,61,61), (248,483,61,61), (328,483,61,61), 
               (409,483,61,61), (492,483,61,61), (577,483,61,61), (659,483,61,61), 
               (82,587,61,61), (164,587,61,61), (248,587,61,61), (328,587,61,61), 
               (409,587,61,61), (492,587,61,61), (577,587,61,61), (659,587,61,61), 
               (124,379,61,61), (435,379,61,61), (616,379,61,61)]
    BA,BB,BC,BD,BE,BF,BG,BH = 0,1,2,3,4,5,6,7
    B1,B2,B3,B4,B5,B6,B7,B8 = 8,9,10,11,12,13,14,15
    BNEW,BCLEAR,BENTER = 16,17,18
    
    # Define the allowable keys.
    allowable_keys = [pygame.K_a, pygame.K_b, pygame.K_c, pygame.K_d, pygame.K_e, pygame.K_f, pygame.K_g, pygame.K_h, 
                      pygame.K_1, pygame.K_2, pygame.K_3, pygame.K_4, pygame.K_5, pygame.K_6, pygame.K_7, pygame.K_8,
                      pygame.K_ESCAPE, pygame.K_BACKSPACE, pygame.K_RETURN]
    
    # Remember the last button clicked.  May have to make a buffer.
    current_button = -1
    clear_on_next_button = False
    
    # Sound files.
    sound_enabled = True
    new_button = None
    clear_button = None
    enter_button = None
    a1_button = None
    b2_button = None
    c3_button = None
    d4_button = None
    e5_button = None
    f6_button = None
    g7_button = None
    h8_button = None
    error = None
    check = None
    checkmate = None
   
    # Manage playing of sounds.
    new_sound = -1
    last_sound = 0
    
    dir_path = None

    
    # Setup the cpu to run the CHESSmate 6502 based program.
    def __init__(self):
 
        self.dir_path = os.path.dirname(os.path.realpath(__file__))
        chessmate = open(self.dir_path+"/ROMs/CHESSmate.hex", "r")  # 4K CHESSmate code.
        opening_book = open(self.dir_path+"/ROMs/OpeningBook.hex", "r")  # 1K CHESSmate opening biik moves.
        
        self.sbc_value = 1
        
        # Define blocks of memory.  Each tuple is
        # (start_address, length, readOnly=True, value=None, valueOffset=0)
        self.mmu = MMU([
                (self.CHESSMATE_RAM, self.RAM_SIZE), # Create RAM with 2K.
                (self.CHESSMATE_ROM, self.ROM_SIZE, True, chessmate), # CHESSmate program.
                (self.RRIOT_REGISTERS, self.RRIOT_REG_SIZE, False, None, 0, self.manage_RRIOT), # Control the IO through these "registers".
                (self.RRIOT_RAM, self.RRIOT_RAM_SIZE), # Small cache of RAM on RRIOT chip. 
                (self.RRIOT_ROM, self.RRIOT_ROM_SIZE, True, opening_book),
        ])
         
        # Create the CPU with the MMU and the starting program counter address.
        self.cpu =  CPU(self.mmu, self.CHESSMATE_ROM)
        self.cpu.set_music_callback(self.music_callback)
         
    # Handle reads from the RRIOT control registers.
    def read_byte(self, address):

        if address == self.RRIOT_REGISTERS+2:   # Port B Data
            # Reading from PORTB. Return the previous value of PORTB.
            return self.sbc_value
        elif address == self.RRIOT_REGISTERS:   # Port A Data
            # Check for NEW GAME. (Reset)
            if self.current_button == self.BNEW:
                self.cpu =  CPU(self.mmu, self.CHESSMATE_ROM)
                self.cpu.set_music_callback(self.music_callback)
                self.clear_display()
            else:
                # Checking for key pressed.
                if self.current_button >= 0:
                    # Default result, no keys pressed.
                    result = 0b01111111
                    if self.keyboard_row == 1:
                        if self.current_button in [self.BA,self.B1]:
                            result = 0b01011111
                        elif self.current_button in [self.BB,self.B2]:
                            result = 0b01101111
                        elif self.current_button in [self.BC,self.B3]:
                            result = 0b01110111
                        elif self.current_button in [self.BD,self.B4]:
                            result = 0b01111011
                        elif self.current_button in [self.BE,self.B5]:
                            result = 0b01111101
                        elif self.current_button in [self.BF,self.B6]:
                            result = 0b01111110
                    elif self.keyboard_row == 2:
                        if self.current_button in [self.BG,self.B7]:
                            result = 0b00111111
                        elif self.current_button in [self.BH,self.B8]:
                            result = 0b01011111
                        elif self.current_button == self.BCLEAR:
                            result = 0b01101111
                        elif self.current_button == self.BENTER:
                            self.clear_display()
                            self.clear_on_next_button = True
                            return 0b01110111
                    if result != 0b01111111:
                        if self.clear_on_next_button:
                            self.clear_display()
                            self.clear_on_next_button = False
                    return result
                    
        return 0b01111111
        
    # Handle writes to the RRIOT control registers.
    def write_byte(self, address, value):

        if address == self.RRIOT_REGISTERS:     # Port A Data
            # Determine what character is to be written to a 7-Segment display.
            test = value & 0b01111111 # Strip off the high bit.
            if test == 0b00000000:
                self.current_character = ' '
            elif test == 0b00001000:
                self.current_character = '_'
            elif test == 0b00111111:
                self.current_character = '0'
            elif test == 0b00000110:
                self.current_character = '1'
            elif test == 0b01011011:
                self.current_character = '2'
            elif test == 0b01001111:
                self.current_character = '3'
            elif test == 0b01100110:
                self.current_character = '4'
            elif test == 0b01101101:
                self.current_character = '5'
            elif test == 0b01111101:
                self.current_character = '6'
            elif test == 0b00000111:
                self.current_character = '7'
            elif test == 0b01111111:
                self.current_character = '8'
            elif test == 0b01101111:
                self.current_character = '9'
            elif test == 0b01110111:
                self.current_character = 'A'
            elif test == 0b01111100:
                self.current_character = 'B'
            elif test == 0b00111001:
                self.current_character = 'C'
            elif test == 0b01011110:
                self.current_character = 'D'
            elif test == 0b01111001:
                self.current_character = 'E'
            elif test == 0b01110001:
                self.current_character = 'F'
            elif test == 0b00111101:
                self.current_character = 'G'
            elif test == 0b01110110:
                self.current_character = 'H'
            elif test == 0b01000000:
                self.current_character = '-'
            elif test == 0b01010011:
                self.current_character = 'S'
            elif test > 0:
                print("ERR:",bin(test))
                
            # If there is a current display active show the current character there.
            if self.current_display >= 0 and self.current_character != ' ':
                self.toggle_display(self.current_display, self.current_character)
                    
            # Check for CHESSmate LOSES.
            if (value & 0b10000000) > 0:
                self.toggle_led(self.LLOSES, True)
            else:
                self.toggle_led(self.LLOSES, False)
        elif address == self.RRIOT_REGISTERS+1:    # Port A Data Direction
            # Do nothing.
            pass
        elif address == self.RRIOT_REGISTERS+2:     # Port B Data
            # Remember the last value written to this register.
            self.sbc_value = value
            
            # Keep track of what keyboard row is being asked for.
            self.keyboard_row = 0
            
            # The bottom 3 bits of the value control 8 IO lines. 
            # Only 1 will be turned on.
            # Determine which line to enable.
            test = value & 0x07
            if test < 4:
                self.current_display = test
            else:
                self.current_display = -1
                
                # Set the keyboard row being interrogated.
                if test ==  4:
                    self.keyboard_row = 1
                elif test ==  5:
                    self.keyboard_row = 2
                    
                # Lines 6 and 7 control the buzzer. We can use these bits to determine when a sound starts.
                if test in [6,7]:
                    # Waiting for a new sound.
                    if self.new_sound == -1:
                        # New sound started. 
                        self.new_sound = 0
                        
                        # Record the time the sound started.
                        self.last_sound = int(time.time() * 1000)
                        
                        # Blink display.
                        if self.current_button == -1:
                            self.clear_display()
                        
                        # The zero page value at address 0x69 is the sound number.
                        sound = self.cpu.peekByte(0x69)
                        
                        # Have to differentiate between an H8 and ERROR sound?
                        stack = self.cpu.stackPeekByte(-1)
                        
                        if sound == 1:
                            self.play_sound(self.a1_button)
                        elif sound == 2:
                            self.play_sound(self.b2_button)
                        elif sound == 3:
                            self.play_sound(self.c3_button)
                        elif sound == 4:
                            self.play_sound(self.d4_button)
                        elif sound == 5:
                            self.play_sound(self.e5_button)
                        elif sound == 6:
                            self.play_sound(self.f6_button)
                        elif sound == 7:
                            self.play_sound(self.g7_button)
                        elif sound == 8:
                            if stack == 0x92 or stack == 0xA0:
                                self.play_sound(self.error)
                            else:
                                self.play_sound(self.h8_button)
                                self.clear_display()
                        elif sound == 9:
                            self.play_sound(self.clear_button)
                        elif sound == 10:
                            self.play_sound(self.enter_button)
                        elif sound == 16:
                            self.play_sound(self.check)
                        elif sound == 32:
                            if stack == 26:
                                self.play_sound(self.checkmate)
                            else:
                                self.play_sound(self.new_button)
                        else:
                            # Don't know the sound number yet.
                            self.play_sound(self.checkmate)
                    elif self.new_sound == 0:
                        # Check for a pause to know that sound has ended.
                        test_time = int(time.time() * 1000)
                        if (test_time - self.last_sound) > 2:
                            # Wait for next sound.
                            self.new_sound = -1
                        else:
                            self.last_sound = test_time
                            
                        
            # Check for CHECK.
            if (value & 0b00001000) > 0:
                self.toggle_led(self.LCHECK, True)
            else:
                self.toggle_led(self.LCHECK, False)
            
            # Check for BLACK or WHITE.
            if (value & 0b00010000) > 0:
                self.toggle_led(self.LBLACK, False)
                self.toggle_led(self.LWHITE, True)
            else:
                self.toggle_led(self.LBLACK,True)
                self.toggle_led(self.LWHITE, False)

        elif address == self.RRIOT_REGISTERS+3:     # Port B Data Direction
            # Does not directly map to specific pins. Will always be 0x7F.
            pass

    # Handle the callback from the mmu when accessing the RRIOT control registers.
    def manage_RRIOT(self, address, value):
        if value != None:
            self.write_byte(address, value)
        else:
            return self.read_byte(address)
        
    # Determine if the coordinates passed lie within a rectangle.
    def button_pressed(self, r, x, y):
        if x>=r[0] and x<=r[0]+r[2] and y>=r[1] and y<=r[1]+r[3]:
            return True
        return False
    
    # Toggle one of the LEDs.
    def toggle_led(self, led, on):
        if on:
            pygame.draw.circle(self.screen, (255,0,0), (self.LEDs[led][0]+15,self.LEDs[led][1]+15), 11)
        else:
            self.screen.blit(self.SAVE_LEDs[led], (self.LEDs[led][0], self.LEDs[led][1]))
    
    # Toggle one of the 7-Segment displays.
    def toggle_display(self, display, value):
        if value != self.current_showing[display]:
            self.screen.blit(self.SAVE_DISPLAYs[display], (self.DISPLAYs[display][0], self.DISPLAYs[display][1]))
            character = self.font.render(value, 0, (255, 0, 0))
            if value == 'S':
                character = pygame.transform.rotate(character, 180)  # Flip the text vertically.
            self.screen.blit(character, (self.DISPLAYs[display][0]+5, self.DISPLAYs[display][1]+10))
            self.current_showing[display] = value
    
    # Blank all four 7-Segment displays.   
    def clear_display(self):
        self.toggle_display(0, ' ')
        self.toggle_display(1, ' ')
        self.toggle_display(2, ' ')
        self.toggle_display(3, ' ')
        
    # Play the sound passed if sound has been enabled.
    def play_sound(self, sound):
        if self.sound_enabled:
            pygame.mixer.Sound.play(sound)
    
    # Gets called when the play sound subroutine entered.    
    def music_callback(self):
        pass
 
    # Run CHESSmate.    
    def run(self):
        # Initialize the PyGame environment.
        pygame.init()
        pygame.font.init()
        try:
            pygame.mixer.init()
            # Load game sounds.
            self.new_button = pygame.mixer.Sound(self.dir_path+"/Sounds/New Button.wav")
            self.clear_button = pygame.mixer.Sound(self.dir_path+"/Sounds/Clear Button.wav")
            self.enter_button = pygame.mixer.Sound(self.dir_path+"/Sounds/Enter Button.wav")
            self.a1_button = pygame.mixer.Sound(self.dir_path+"/Sounds/A1 Button.wav")
            self.b2_button = pygame.mixer.Sound(self.dir_path+"/Sounds/B2 Button.wav")
            self.c3_button = pygame.mixer.Sound(self.dir_path+"/Sounds/C3 Button.wav")
            self.d4_button = pygame.mixer.Sound(self.dir_path+"/Sounds/D4 Button.wav")
            self.e5_button = pygame.mixer.Sound(self.dir_path+"/Sounds/E5 Button.wav")
            self.f6_button = pygame.mixer.Sound(self.dir_path+"/Sounds/F6 Button.wav")
            self.g7_button = pygame.mixer.Sound(self.dir_path+"/Sounds/G7 Button.wav")
            self.h8_button = pygame.mixer.Sound(self.dir_path+"/Sounds/H8 Button.wav")
            self.error = pygame.mixer.Sound(self.dir_path+"/Sounds/Error.wav")
            self.check = pygame.mixer.Sound(self.dir_path+"/Sounds/Check.wav")
            self.checkmate = pygame.mixer.Sound(self.dir_path+"/Sounds/Checkmate.wav")
        except:
            # Could not initialize the sound system.
            self.sound_enabled = False

        # Get the CHESSmate membrane image.
        bg = pygame.image.load(self.dir_path+"/Images/Membrane Image (800x738).png")

        # Screen constants.
        SCREEN_SIZE = bg.get_width(), bg.get_height()
        SCREEN_ATTRIBUTES = 0

        # Open a window. 
        self.screen = pygame.display.set_mode(SCREEN_SIZE, SCREEN_ATTRIBUTES)
        pygame.display.set_caption('CHESSmate')
        self.screen.blit(bg, (0,0))
            
        # Testing font.
        self.sysfont = pygame.font.Font(None, 32)
    
        # Create a font to display in the 7-Segment display windows.
        self.font = pygame.font.Font(self.dir_path+"/Font/DSEG7ClassicMini-BoldItalic.ttf", 64)
        
        # Save the backgrounds from the display "windows".
        for i in range(4):
            self.SAVE_DISPLAYs.append(self.screen.subsurface(self.DISPLAYs[i]).copy())
        for i in range(4):
            self.SAVE_LEDs.append(self.screen.subsurface(self.LEDs[i]).copy())
        
        # Main loop.
        while True:
            # Clear button press.
            self.current_button = -1
            
            # Handle PyGame events.
            for event in pygame.event.get():
                # Click the close x.
                if event.type == pygame.QUIT:
                    pygame.quit()
                    sys.exit()
                
                # Handle key pressed events.
                elif event.type == pygame.KEYDOWN:
                    key = event.key
                    if key in self.allowable_keys:
                        self.current_button = self.allowable_keys.index(key)
                        
                # Handle mouse events.
                elif pygame.mouse.get_pressed()[0] == True:
                    # Check for button clicked.
                    pos = pygame.mouse.get_pos()
                    for i in range(len(self.BUTTONs)):
                        if self.button_pressed(self.BUTTONs[i], pos[0], pos[1]):
                            self.current_button = i
                            break
                                   
            # Show the changes to the screen.
            pygame.display.flip()
            for _ in range(10000):
                self.cpu.step()
           
