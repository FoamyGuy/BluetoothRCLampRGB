/*
  Written by Freemansoft Inc.
  Exercise Neopixel (WS2811 or WS2812) shield using the adafruit NeoPixel library
  You need to download the Adafruit NeoPixel library from github, 
  unzip it and put it in your arduino libraries directory
  
  Edited by Foamyguy to support Bluetooth instead of serial over USB. Added some 
  modes that run the lights through a range of colors.
  
  commands include
  rgb   <led 0..39> <red 0..255> <green 0..255> <blue 0..255> <pattern 0..9>: set RGB pattern to  pattern <0:off, 1:continuous>
  rgb   <all -1>    <red 0..255> <green 0..255> <blue 0..255> <pattern 0..9>: set RGB pattern to  pattern <0:off, 1:continuous>
  debug <true|false> log all input to serial
  blank clears all
  demo random colors 
  
 */
#include <Adafruit_NeoPixel.h>
#include <MsTimer2.h>

#include <SoftwareSerial.h>  

  boolean COLOR_SHIFT_MODE = true;
  int MAX_COLOR_VALUE = 160;
  int COLOR_SHIFT_VALUE = 5;
  int redVal = 0;
  int blueVal = MAX_COLOR_VALUE;
  int greenVal = 0;

int bluetoothTx = 2;  // TX-O pin of bluetooth mate, Arduino 2
int bluetoothRx = 3;  // RX-I pin of bluetooth mate, Arduino 3

SoftwareSerial bluetooth(bluetoothTx, bluetoothRx);

boolean logDebug = true;
// pin used to talk to NeoPixel
#define PIN 6

// Parameter 1 = number of pixels in strip
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_RGB     Pixels are wired for RGB bitstream
//   NEO_GRB     Pixels are wired for GRB bitstream
//   NEO_KHZ400  400 KHz bitstream (e.g. FLORA pixels)
//   NEO_KHZ800  800 KHz bitstream (e.g. High Density LED strip)
// using the arduino shield which is 5x8
const int NUM_PIXELS = 40;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, PIN, NEO_GRB + NEO_KHZ800);


typedef struct
{
  uint32_t activeValues;         // packed 32 bit created by Strip.Color
  uint32_t lastWrittenValues;    //for fade in the future
  byte currentState;             // used for blink
  byte pattern;
  unsigned long lastChangeTime;  // where are we timewise in the pattern
} pixelDefinition;
// should these be 8x5 intead of linear 40 ?
volatile pixelDefinition lightStatus[NUM_PIXELS];
volatile boolean stripShowRequired = false;


///////////////////////////////////////////////////////////////////////////

// time between steps
const int STATE_STEP_INTERVAL = 10; // in milliseconds - all our table times are even multiples of this
const int MAX_PWM_CHANGE_SIZE = 32; // used for fading at some later date

/*================================================================================
 *
 * bell pattern buffer programming pattern lifted from http://arduino.cc/playground/Code/MultiBlink
 *
 *================================================================================*/

typedef struct
{
  boolean  isActive;          // digital value for this state to be active (on off)
  unsigned long activeTime;    // time to stay active in this state stay in milliseconds 
} stateDefinition;

// the number of pattern steps in every blink  pattern 
const int MAX_STATES = 4;
typedef struct
{
  stateDefinition state[MAX_STATES];    // can pick other numbers of slots
} ringerTemplate;

const int NUM_PATTERNS = 10;
const ringerTemplate ringPatterns[] =
{
    //  state0 state1 state2 state3 
    // the length of these times also limit how quickly changes will occure. color changes are only picked up when a true transition happens
  { /* no variable before stateDefinition*/ {{false, 1000}, {false, 1000}, {false, 1000}, {false, 1000}}  /* no variable after stateDefinition*/ },
  { /* no variable before stateDefinition*/ {{true,  1000},  {true, 1000},  {true, 1000},  {true, 1000}}   /* no variable after stateDefinition*/ },
  { /* no variable before stateDefinition*/ {{true , 300}, {false, 300}, {false, 300}, {false, 300}}  /* no variable after stateDefinition*/ },
  { /* no variable before stateDefinition*/ {{false, 300}, {true , 300}, {true , 300}, {true , 300}}  /* no variable after stateDefinition*/ },
  { /* no variable before stateDefinition*/ {{true , 200}, {false, 100}, {true , 200}, {false, 800}}  /* no variable after stateDefinition*/ },
  { /* no variable before stateDefinition*/ {{false, 200}, {true , 100}, {false, 200}, {true , 800}}  /* no variable after stateDefinition*/ },
  { /* no variable before stateDefinition*/ {{true , 300}, {false, 400}, {true , 150}, {false, 400}}  /* no variable after stateDefinition*/ },
  { /* no variable before stateDefinition*/ {{false, 300}, {true , 400}, {false, 150}, {true , 400}}  /* no variable after stateDefinition*/ },
  { /* no variable before stateDefinition*/ {{true , 100}, {false, 100}, {true , 100}, {false, 800}}  /* no variable after stateDefinition*/ },
  { /* no variable before stateDefinition*/ {{false, 100}, {true , 100}, {false, 100}, {true , 800}}  /* no variable after stateDefinition*/ },
};


///////////////////////////////////////////////////////////////////////////


void setup() {
  Serial.begin(9600);
  bluetooth.begin(115200);  // The Bluetooth Mate defaults to 115200bps
  bluetooth.print("$");  // Print three times individually
  bluetooth.print("$");
  bluetooth.print("$");  // Enter command mode
  delay(100);  // Short delay, wait for the Mate to send back CMD
  bluetooth.println("U,9600,N");  // Temporarily Change the baudrate to 9600, no parity
  // 115200 can be too fast at times for NewSoftSerial to relay the data reliably
  bluetooth.begin(9600);  // Start bluetooth serial at 9600
  Serial.println("BT begun");
  
  // 50usec for 40pix @ 1.25usec/pixel : 19200 is .5usec/bit or 5usec/character
  // there is a 50usec quiet period between updates 
  //Serial.begin(19200);
  // don't want to lose characters if interrupt handler too long
  // serial interrupt handler can't run so arduino input buffer length is no help

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'
  stripShowRequired = false;

  // initialize our buffer as all LEDS off
  go_dark();

  //show a quickcolor pattern
  //configureForDemo();
  //delay(3000);
  //go_dark();
  
  //MsTimer2::set(STATE_STEP_INTERVAL, process_blink);
  //MsTimer2::start();
}

void loop(){
  const int READ_BUFFER_SIZE = 4*6; // rgb_lmp_red_grn_blu_rng where ringer is only 1 digit but need place for \0
  char readBuffer[READ_BUFFER_SIZE];
  int readCount = 0;
  char newCharacter = '\0';
  
    if (stripShowRequired) {
      stripShowRequired = false;
      strip.show();
    }
    
  if(COLOR_SHIFT_MODE){
    color_shift_tick();
  }  


  if(bluetooth.available())  // If the bluetooth sent any characters
    {

      String cmd = "";
      cmd = readBT();
      Serial.println("BT cmd: " + cmd);
      char charBuf[cmd.length()+1];
      cmd.toCharArray(charBuf, cmd.length()+1);
      process_command(charBuf,cmd.length()+1);
      
    }
    process_blink();
    
    delay(50);
}


void color_shift_tick(){
  if (redVal < MAX_COLOR_VALUE && greenVal == 0 && blueVal == MAX_COLOR_VALUE){
    redVal += COLOR_SHIFT_VALUE;
  }
  if (redVal == MAX_COLOR_VALUE && blueVal > 0){
    blueVal -= COLOR_SHIFT_VALUE; 
  }
  
  if (redVal == MAX_COLOR_VALUE && blueVal == 0 && greenVal < MAX_COLOR_VALUE){
    greenVal += COLOR_SHIFT_VALUE;
  }
  
  if (redVal > 0 && greenVal == MAX_COLOR_VALUE) {
    redVal -= COLOR_SHIFT_VALUE;
  }
  
  if(redVal == 0 && greenVal == MAX_COLOR_VALUE){
    blueVal += COLOR_SHIFT_VALUE;
  }
  
  if(greenVal > 0 && redVal == 0 && blueVal == MAX_COLOR_VALUE){
    greenVal -= COLOR_SHIFT_VALUE;
  }
  
  String cmd = "rgb -1 " + String(redVal) + " " + String(greenVal) + " " + String(blueVal) + " 1";
      
  char charBuf[cmd.length()+1];
  cmd.toCharArray(charBuf, cmd.length()+1);
  process_command(charBuf,cmd.length()+1);

}

/*
 * blank out the LEDs and buffer
 */
void go_dark(){
  unsigned long ledLastChangeTime = millis();
  for ( int index = 0 ; index < NUM_PIXELS; index++){
    lightStatus[index].currentState = 0;
    lightStatus[index].pattern = 0;
    lightStatus[index].activeValues = strip.Color(0,0,0);
    lightStatus[index].lastChangeTime = ledLastChangeTime;
    strip.setPixelColor(index, lightStatus[index].activeValues);
  }
  // force them all dark immediatly so they go out at the same time
  // could have waited for timer but different blink rates would go dark at slighly different times
  strip.show();
}

//////////////////////////// handler  //////////////////////////////
//
/*
  Interrupt handler that handles all blink operations
 */
void process_blink(){
  boolean didChangeSomething = false;
  unsigned long now = millis();
  for ( int index = 0 ; index < NUM_PIXELS; index++){
    byte currentPattern = lightStatus[index].pattern; 
    if (currentPattern >= 0){ // quick range check for valid pattern?
      if (now >= lightStatus[index].lastChangeTime 
          + ringPatterns[currentPattern].state[lightStatus[index].currentState].activeTime){
        // calculate next state with rollover/repeat
        int currentState = (lightStatus[index].currentState+1) % MAX_STATES;
        lightStatus[index].currentState = currentState;
        lightStatus[index].lastChangeTime = now;

        // will this cause slight flicker if already showing led?
        if (ringPatterns[currentPattern].state[currentState].isActive){
          strip.setPixelColor(index, lightStatus[index].activeValues);
        } else {
          strip.setPixelColor(index,strip.Color(0,0,0));
        }
        didChangeSomething = true;
      }
    }
  }
  // don't show in the interrupt handler because interrupts would be masked
  // for a long time. 
  if (didChangeSomething){
    stripShowRequired = true;
  }
}

// first look for commands without parameters then with parametes 
boolean  process_command(char *readBuffer, int readCount){
  int indexValue;
  byte redValue;
  byte greenValue;
  byte blueValue;
  byte patternValue;
  
  // use string tokenizer to simplify parameters -- could be faster by only running if needed
  char *command;
  char *parsePointer;
  //  First strtok iteration
  command = strtok_r(readBuffer," ",&parsePointer);

  boolean processedCommand = false;
  if (strcmp(command,"h") == 0 || strcmp(command,"?") == 0){
    help();
    processedCommand = true;
  } else if (strcmp(command,"rgb") == 0){
    char * index   = strtok_r(NULL," ",&parsePointer);
    char * red     = strtok_r(NULL," ",&parsePointer);
    char * green   = strtok_r(NULL," ",&parsePointer);
    char * blue    = strtok_r(NULL," ",&parsePointer);
    char * pattern = strtok_r(NULL," ",&parsePointer);
    if (index == NULL || red == NULL || green == NULL || blue == NULL || pattern == NULL){
      help();
    } else {
      // this code shows how lazy I am.
      int numericValue;
      numericValue = atoi(index);
      if (numericValue < 0) { numericValue = -1; }
      else if (numericValue >= NUM_PIXELS) { numericValue = NUM_PIXELS-1; };
      indexValue = numericValue;
      
      numericValue = atoi(red);
      if (numericValue < 0) { numericValue = 0; }
      else if (numericValue > 255) { numericValue = 255; };
      redValue = numericValue;
      numericValue = atoi(green);
      if (numericValue < 0) { numericValue = 0; }
      else if (numericValue > 255) { numericValue = 255; };
      greenValue = numericValue;
      numericValue = atoi(blue);
      if (numericValue < 0) { numericValue = 0; }
      else if (numericValue > 255) { numericValue = 255; };
      blueValue = numericValue;
      numericValue = atoi(pattern);
      if (numericValue < 0) { numericValue = 0; }
      else if (numericValue > NUM_PATTERNS) { numericValue = NUM_PATTERNS-1; };
      patternValue = numericValue;
      
      /*
      Serial.println(indexValue);
      Serial.println(redValue);
      Serial.println(greenValue);
      Serial.println(blueValue);
      Serial.println(patternValue);
      */
      
      if (indexValue >= 0){
        lightStatus[indexValue].activeValues = strip.Color(redValue,greenValue,blueValue);
        lightStatus[indexValue].pattern = patternValue;
      } else {
        for (int i = 0; i < NUM_PIXELS; i++){
          lightStatus[i].activeValues = strip.Color(redValue,greenValue,blueValue);
          lightStatus[i].pattern = patternValue;
          
        }
      }
      processedCommand = true;   
    }
  } else if (strcmp(command,"blank") == 0){
    go_dark();
    processedCommand = true;
  } else if (strcmp(command,"mode") == 0){
    COLOR_SHIFT_MODE = !COLOR_SHIFT_MODE; 
  }else if (strcmp(command,"debug") == 0){
    char * shouldLog   = strtok_r(NULL," ",&parsePointer);
    if (strcmp(shouldLog,"true") == 0){
      logDebug = true;
    } else {
      logDebug = false;
    }
    processedCommand = true;
  } else if (strcmp(command,"demo") == 0){
    configureForDemo();
    processedCommand = true;
  } else {
    // completely unrecognized
  }
  if (!processedCommand){
    Serial.print(command);
    Serial.println(" not recognized ");
  }
  return processedCommand;
}

/*
 * Simple method that displays the help
 */
void help(){
  bluetooth.println("h: help");
  bluetooth.println("?: help");
  bluetooth.println("rgb   <led 0..39> <red 0..255> <green 0..255> <blue 0..255> <pattern 0..9>: set RGB pattern to  pattern <0:off, 1:continuous>");
  bluetooth.println("rgb   <all -1>    <red 0..255> <green 0..255> <blue 0..255> <pattern 0..9>: set RGB pattern to  pattern <0:off, 1:continuous>");
  bluetooth.println("debug <true|false> log all input to serial");
  bluetooth.println("blank clears all");
  bluetooth.println("demo  color and blank wheel");
  //Serial.flush();
}

//////////////////////////// demo  //////////////////////////////

/*
 * show the various blink patterns
 */
void configureForDemo(){
  unsigned long ledLastChangeTime = millis();
  for ( int index = 0 ; index < NUM_PIXELS; index++){
    lightStatus[index].currentState = 0;
    lightStatus[index].pattern = (index%8)+1; // the shield is 8x5 so we do 8 patterns and we know pattern 0 is off
    lightStatus[index].activeValues = Wheel(index*index & 255);
    lightStatus[index].lastChangeTime = ledLastChangeTime;
  }
  uint16_t i;  
  for(i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i,lightStatus[i].activeValues);
  }     
  strip.show();
}

String readBT()
{
  Serial.println("reading BT");
  String content = "";
  char newCharacter;
  while(bluetooth.available() && newCharacter !='\r') 
  {
      
      newCharacter = bluetooth.read();
      content.concat(newCharacter);
  }
  if (content != "") 
  {
    
  }
  return content;
}

//////////////////////////// stuff from the Adafruit NeoPixel sample  //////////////////////////////


// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  if(WheelPos < 85) {
   return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else {
   WheelPos -= 170;
   return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
}

