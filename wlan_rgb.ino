#include <SparkFunESP8266Server.h>
#include <SparkFunESP8266WiFi.h>


// HSV fade/bounce for Arduino - scruss.com - 2010/09/12
// Note that there's some legacy code left in here which seems to do nothing
// but should do no harm ...

#include <EEPROM.h>

// WiFi
const char mySSID[] = "MYSSID";
const char myPSK[] = "MYPASSWORD";
ESP8266Server server = ESP8266Server(80);

// don't futz with these, illicit sums later
#define RED       3// pin for red LED
#define GREEN     5 // pin for green - never explicitly referenced
#define BLUE      6 // pin for blue - never explicitly referenced
#define SIZE    255
#define DELAY    20
#define HUE_MAX  6.0
#define HUE_DELTA 0.01

String inputString = "";         // a string to hold incoming data
boolean stringComplete = false;  // whether the string is complete
int index;
bool do_loop = true;
int loopindex = 0;
unsigned long time = 0;
unsigned long serverTime = 0;
unsigned char retries = 0;
#define MAX_RETRIES 10

typedef struct HsvColor_
{
    unsigned long h;
    unsigned long s;
    unsigned long v;
} HsvColor;

typedef struct RgbColor_
{
    unsigned long r;
    unsigned long g;
    unsigned long b;
} RgbColor;

//long deltas[3] = { 5, 6, 7 };

/*
chosen LED SparkFun sku: COM-09264
 has Max Luminosity (RGB): (2800, 6500, 1200)mcd
 so we normalize them all to 1200 mcd -
 R  250/600  =  107/256
 G  250/950  =   67/256
 B  250/250  =  256/256
 */
//long bright[3] = { 107, 67, 256};
//long bright[3] = { 256, 256, 256};
#define RED_BRIGHT   0
#define GREEN_BRIGHT 1
#define BLUE_BRIGHT  2
unsigned long bright[3] = { 170, 120, 256 };
unsigned long eSize = 0;

#define RGB_KEY 'r'
#define HSV_KEY 'h'
#define MAGIC 0xdeadbeef

typedef struct MemObject_ 
{
  char key;
  unsigned long delay;
  union {
    unsigned long rgb;
    unsigned long hsv;
  } value;
} MemObject;

MemObject objs[18];
MemObject o;
unsigned long cdelay = 0;
bool serverStarted = false;

void setup () {  
  randomSeed(analogRead(4));
  pinMode(RED, OUTPUT);
  analogWrite(RED, 0);
  pinMode(GREEN, OUTPUT);
  analogWrite(GREEN, 0);
  pinMode(BLUE, OUTPUT);
  analogWrite(BLUE, 0);

  // EEPROM
  read_memory();

  // initialize serial:
  Serial.begin(115200);

  // reserve 200 bytes for the inputString:
  inputString.reserve(200);

  initServer();
}

void loop() {  
  // print the string when a newline arrives:
  if (stringComplete) {
    Serial.println(inputString);
    handle_input();
    // clear the string:
    inputString = "";
    stringComplete = false;
  }

  if (do_loop) {
    if (time < millis()) {
      if (loopindex >= eSize) {
        loopindex = 0;
      }
      cdelay = colour(loopindex++);
      if (cdelay) {
        time = millis() + cdelay;
      } else {
        // If delay is zero; keep this colour indefinitely
        Serial.println("loop: false");
        do_loop = false;
      }
    }
  }

  // Wait TCP client
  if (serverStarted) {
    wait_client();
  } else {
    if (serverTime && (serverTime < millis())) {
      initServer();
    }
    delay(10);
  }
}

void initServer()
{
  // initializeESP8266() verifies communication with the WiFi
  // shield, and sets it up.
  if (!initializeESP8266()) {
    // connectESP8266() connects to the defined WiFi network.
    if (!connectESP8266()) {
      // Setup TCP server
      serverSetup();
      serverTime = 0;
    } else {
      serverTime = 0;
      retries++;
      if (retries < MAX_RETRIES) {
        serverTime = millis() + 10000; // Try next time after 10s
      } else {
        Serial.println(F("Retry count exceeded for WiFi."));
      }
    }
  }  
}

void serverSetup()
{
  // begin initializes a ESP8266Server object. It will
  // start a server on the port specified in the object's
  // constructor (in global area)
  server.begin();
  serverStarted = true;
  Serial.print(F("Server started! Go to "));
  Serial.println(esp8266.localIP());
  Serial.println();
}

bool connectESP8266()
{
  // The ESP8266 can be set to one of three modes:
  //  1 - ESP8266_MODE_STA - Station only
  //  2 - ESP8266_MODE_AP - Access point only
  //  3 - ESP8266_MODE_STAAP - Station/AP combo
  // Use esp8266.getMode() to check which mode it's in:
  int retVal = esp8266.getMode();
  if (retVal != ESP8266_MODE_STA)
  { // If it's not in station mode.
    // Use esp8266.setMode([mode]) to set it to a specified
    // mode.
    retVal = esp8266.setMode(ESP8266_MODE_STA);
    if (retVal < 0)
    {
      Serial.println(F("Error setting mode."));
      return true;
    }
  }
  Serial.println(F("Mode set to station"));

  // esp8266.status() indicates the ESP8266's WiFi connect
  // status.
  // A return value of 1 indicates the device is already
  // connected. 0 indicates disconnected. (Negative values
  // equate to communication errors.)
  retVal = esp8266.status();
  if (retVal <= 0)
  {
    Serial.print(F("Connecting to "));
    Serial.println(mySSID);
    // esp8266.connect([ssid], [psk]) connects the ESP8266
    // to a network.
    // On success the connect function returns a value >0
    // On fail, the function will either return:
    //  -1: TIMEOUT - The library has a set 30s timeout
    //  -3: FAIL - Couldn't connect to network.
    retVal = esp8266.connect(mySSID, myPSK);
    if (retVal < 0)
    {
      Serial.println(F("Error connecting"));
      return true;
    }
  }

  return false;
}

bool initializeESP8266()
{
  // esp8266.begin() verifies that the ESP8266 is operational
  // and sets it up for the rest of the sketch.
  // It returns either true or false -- indicating whether
  // communication was successul or not.
  // true
  if (esp8266.begin() != true)
  {
    Serial.println(F("Error talking to ESP8266."));
  }
  Serial.println(F("ESP8266 Shield Present"));

  return false;
}

// +IPD,0,12:0123456789
// 0,CLOSED
void wait_client()
{
  // available() is an ESP8266Server function which will
  // return an ESP8266Client object for printing and reading.
  // available() has one parameter -- a timeout value. This
  // is the number of milliseconds the function waits,
  // checking for a connection.
  ESP8266Client client = server.available(100);

  if (client) 
  {
    inputString = "";
    Serial.println(F("Client Connected!"));
    // an http request ends with a blank line
    while (client.connected()) 
    {
      if (client.available()) 
      {
        char c = client.read();
//        Serial.print(c);
        if (c == 'q') {
          break;
        }
        inputString += c;
        if (c == '\n') {
          int pos = inputString.indexOf(':');
          if (pos >= 0) {
            inputString.remove(0, pos + 1);
          }
          handle_input();
          inputString = "";
        }
      }
    }
   
    // close the connection:
    client.stop();
    Serial.println(F("Client disconnected"));
  }
  
}

int colour(int i)
{
  if (i >= eSize) {
    return 0;
  }

  Serial.print("Colour size: ");
  Serial.println(eSize);
  
  switch(objs[i].key) {
    case RGB_KEY :
      Serial.println(objs[i].value.rgb);
      setColor(objs[i].value.rgb);
      break;
    case HSV_KEY :
      setHSVColor(objs[i].value.hsv);
      break;
    default :
      break;  
  }

  return objs[i].delay;
}

void read_memory()
{
  unsigned long magic;
  unsigned char size;
  eSize = 0;
  EEPROM.get(0, magic);
  if (magic != MAGIC) {
    Serial.print("Magic: ");
    Serial.println(magic);
    Serial.print("Magic: ");
    Serial.println(MAGIC);
    magic = MAGIC;
    EEPROM.put(0, magic);
    size = 0;
    EEPROM.put(sizeof(long), size);
    Serial.println("New created.");
    return;
  }

  EEPROM.get(sizeof(long), size);
  eSize = size;

  Serial.print("Size: ");
  Serial.println(eSize);
  
  if (eSize == 0) {
    return; 
  }

  for (index = 0; index < eSize; index++) {
    EEPROM.get(index * sizeof(MemObject) + sizeof(long) + sizeof(char), o);
    memcpy(&objs[index], &o, sizeof(MemObject));
    Serial.print("Key: ");
    Serial.println(o.key);
    Serial.print("Delay: ");
    Serial.println(o.delay);
  }

  loopindex = 0;
  do_loop = true;
  
  Serial.print("Read: ");
  Serial.println(eSize);
}

void write_memory()
{
  unsigned char size;
  
  if (eSize > 0) {
    for (index = 0; index < eSize; index++) {
      EEPROM.put(index * sizeof(MemObject) + sizeof(long) + sizeof(char), objs[index]);
    }
  }
  
  size = eSize;
  EEPROM.put(sizeof(long), size);

  Serial.print("Written: ");
  Serial.println(eSize);
}

void handle_input()
{
  unsigned long r = 0;
  unsigned long h = 0;
  
  switch(inputString.charAt(0)) {
    case 'r' :
    case 'R' :
      if (inputString.length() > 6) {
        r = strtol(inputString.c_str() + 1, 0, 16);
        if (!r) {
          Serial.println("Invalid RGB value.");
          break;
        }
      } else {
        Serial.println("Usage: R<RRGGBB> in HEX");
        break;
      }
      Serial.print("R: ");
      Serial.println(r);
      o.key = RGB_KEY;
      o.value.rgb = r;
      setColor(r);
      break;
    case 'h' :
    case 'H' :
      if (inputString.length() > 6) {
        h = strtol(inputString.c_str() + 1, 0, 16);
        if (!h) {
          Serial.println("Invalid HSV value.");
          break;
        }
      } else {
        Serial.println("Usage: H<HHSSVV> in HEX");
        break;
      }
      Serial.print("H: ");
      Serial.println(h);
      o.key = HSV_KEY;
      o.value.hsv = h;
      setHSVColor(h);
      break;
    case 'd' :
    case 'D' :
      if (inputString.length() > 4) {
        cdelay = strtol(inputString.c_str() + 1, 0, 10);
        Serial.print("Delay: ");
        Serial.println(cdelay);
      }
      break;
    case '.' :
      o.delay = cdelay;
      memcpy(&objs[eSize++], &o, sizeof(MemObject));
      Serial.print("Size: ");
      Serial.println(eSize);
      break;
    case 'w' :
    case 'W' :
      write_memory();
      break;
    case 'm' :
      read_memory();
      break;
    case 'c' :
      colour(0);
      break;
    case 'e' :
    case 'E' :
      eSize = 0;
      write_memory();
      break;
    case 's' :
      initServer();
      break;
    default :
      break;
  }
}

void setHSVColor(unsigned long h)
{
  HsvColor hsv;
  RgbColor rgb;
  hsv.h = (h & 0x00FF0000) >> 16 & 0xff; // there must be better ways
  hsv.s = (h & 0x0000FF00) >> 8 & 0xff;
  hsv.v = h & 0xFF;
  unsigned long r = HsvToRgb(hsv.h, hsv.s, hsv.v);
  Serial.print("Result: ");
  Serial.println(r);
  rgb.r = (r & 0x00FF0000) >> 16; // there must be better ways
  rgb.g = (r & 0x0000FF00) >> 8;
  rgb.b = r & 0x000000FF;  
  analogWrite(RED, rgb.r * bright[RED_BRIGHT] / 256);
  analogWrite(GREEN, rgb.g * bright[GREEN_BRIGHT] / 256);
  analogWrite(BLUE, rgb.b * bright[BLUE_BRIGHT] / 256);
}

void setColor(unsigned long r)
{
  RgbColor rgb;
  rgb.r = (r & 0x00FF0000) >> 16 & 0xff; // there must be better ways
  rgb.g = (r & 0x0000FF00) >> 8 & 0xff;
  rgb.b = r & 0xFF;
  analogWrite(RED, rgb.r * bright[RED_BRIGHT] / 256);
  analogWrite(GREEN, rgb.g * bright[GREEN_BRIGHT] / 256);
  analogWrite(BLUE, rgb.b * bright[BLUE_BRIGHT] / 256);
}

/*
  SerialEvent occurs whenever a new data comes in the
 hardware serial RX.  This routine is run between each
 time loop() runs, so using delay inside loop can delay
 response.  Multiple bytes of data may be available.
 */
void serialEvent() {
  while (Serial.available()) {
    // get the new byte:
    char inChar = (char)Serial.read();
    // add it to the inputString:
    inputString += inChar;
    // if the incoming character is a newline, set a flag
    // so the main loop can do something about it:
    if (inChar == '\n') {
      stringComplete = true;
    }
  }
}

unsigned long HsvToRgb(unsigned char h1, unsigned char s1, unsigned char v1)
{
    RgbColor rgb;
    HsvColor hsv;
    unsigned char region, p, q, t;
    unsigned int h, s, v, remainder;

    hsv.h = h1;
    hsv.s = s1;
    hsv.v = v1;
    
    if (hsv.s == 0)
    {
        rgb.r = hsv.v;
        rgb.g = hsv.v;
        rgb.b = hsv.v;
        analogWrite(RED, rgb.r * bright[RED_BRIGHT] / 256);
        analogWrite(GREEN, rgb.g * bright[GREEN_BRIGHT] / 256);
        analogWrite(BLUE, rgb.b * bright[BLUE_BRIGHT] / 256);

        return rgb.r << 16 & 0xff0000 | rgb.g << 8 & 0xff00 | rgb.b;
    }

    // converting to 16 bit to prevent overflow
    h = hsv.h;
    s = hsv.s;
    v = hsv.v;

    region = h / 43;
    remainder = (h - (region * 43)) * 6; 

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
            rgb.r = v;
            rgb.g = t;
            rgb.b = p;
            break;
        case 1:
            rgb.r = q;
            rgb.g = v;
            rgb.b = p;
            break;
        case 2:
            rgb.r = p;
            rgb.g = v;
            rgb.b = t;
            break;
        case 3:
            rgb.r = p;
            rgb.g = q;
            rgb.b = v;
            break;
        case 4:
            rgb.r = t;
            rgb.g = p;
            rgb.b = v;
            break;
        default:
            rgb.r = v;
            rgb.g = p;
            rgb.b = q;
            break;
    }

    Serial.println("RGB: ");
    Serial.println(rgb.r);
    Serial.println(rgb.g);
    Serial.println(rgb.b);
    Serial.println(rgb.r << 16 & 0xff0000 | rgb.g << 8 & 0xff00 | rgb.b);
    
    return rgb.r << 16 & 0xff0000 | rgb.g << 8 & 0xff00 | rgb.b;
}

