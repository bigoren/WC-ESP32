// edit the config.h file and enter your Adafruit IO credentials
// and any additional configuration needed for WiFi, cellular,
// or ethernet clients.
#include "config.h"

#include <TimeLib.h>
#include <ArduinoOTA.h>

#define FASTLED_RMT_MAX_CHANNELS 1

#include "FastLED.h"

FASTLED_USING_NAMESPACE

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

// fastLED configuration
#define DATA_PIN    13
//#define CLK_PIN   4
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define NUM_LEDS    80
#define FRAMES_PER_SECOND  120

CRGB leds[NUM_LEDS];

// WC sign LED positional defines
#define WStart  0
#define WEnd    31
#define CStart  31
#define CEnd    46
#define ManStart 46
#define ManEnd  63
#define WomanStart 63
#define WomanEnd 80
enum WhichLeds {
  Man = 0,
  Woman = 1,
  Both = 2
};

// Animations functions declarations
void quiet(int brightness);
void nextPattern();
void changeHue(int &currHue, int &destHue, int iStart, int iEnd);
void singleColor();
void toggleSegment(int iStart, int iEnd, int hue, int sat);
void toggle();
void rainbow();
void rainbowWithGlitter();
void addGlitter(fract8 chanceOfGlitter);
void confetti();
void sinelon();
void bpm();
void juggle();
void dotted();

int brightness = 96;
bool isPlain = true;

// List of patterns to cycle through.  Each is defined as a separate function below loop code.
typedef void (*SimplePatternList[])();
//SimplePatternList gPatterns = { toggle };
SimplePatternList gPatterns = { toggle, dotted, singleColor, rainbow, confetti };

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

int anaReadVal;

//////// from here starts button code that needs cleaning //////
bool setButtonRead = false;
byte buttonRead = 0;
byte buttonReadPrev = 0;
bool setButtonValue = false;
int buttonValue = 0;
bool resetFlag = false;

unsigned int lastReportTime = 0;
unsigned int lastMonitorTime = 0;

// set up the 'time/seconds' topic
AdafruitIO_Time *seconds = io.time(AIO_TIME_SECONDS);
time_t secTime = 0;

// set up the 'time/milliseconds' topic
//AdafruitIO_Time *msecs = io.time(AIO_TIME_MILLIS);

// set up the 'time/ISO-8601' topic
// AdafruitIO_Time *iso = io.time(AIO_TIME_ISO);
// char *isoTime;

// this int will hold the current count for our sketch
int count = 0;

// set up the 'counter' feed
AdafruitIO_Feed *counter = io.feed("counter");
AdafruitIO_Feed *rssi = io.feed("rssi");
AdafruitIO_Feed *reportButton = io.feed("button");


// message handler for the seconds feed
void handleSecs(char *data, uint16_t len) {
  // Serial.print("Seconds Feed: ");
  // Serial.println(data);
  secTime = atoi (data);
}

// message handler for the milliseconds feed
// void handleMillis(char *data, uint16_t len) {
//   Serial.print("Millis Feed: ");
//   Serial.println(data);
// }

// message handler for the ISO-8601 feed
// void handleISO(char *data, uint16_t len) {
//   Serial.print("ISO Feed: ");
//   Serial.println(data);
//   isoTime = data;
// }

void setButton(AdafruitIO_Data *data) {
  Serial.print("received new button value <- ");
  Serial.println(data->value());
  buttonValue = atoi(data->value());
  setButtonValue = true;
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.println(); 
}

// time sync function
time_t timeSync()
{
  if (secTime == 0) {
    return 0;
  }
  return (secTime + TZ_HOUR_SHIFT * 3600);
}

void setup() {
  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  
  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while(! Serial);

  Serial.print("Connecting to Adafruit IO");

  // connect to io.adafruit.com
  io.connect();

  // attach message handler for the seconds feed
  seconds->onMessage(handleSecs);

  // attach a message handler for the msecs feed
  //msecs->onMessage(handleMillis);

  // attach a message handler for the ISO feed
  // iso->onMessage(handleISO);

  // attach message handler for the button feed
  reportButton->onMessage(setButton);

  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());

  // Because Adafruit IO doesn't support the MQTT retain flag, we can use the
  // get() function to ask IO to resend the last value for this feed to just
  // this MQTT client after the io client is connected.
  reportButton->get();

  setSyncProvider(timeSync);
  setSyncInterval(60); // sync interval in seconds, consider increasing
  
  //// Over The Air section ////
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname(THING_NAME);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

}

void loop() {
  unsigned int currTime = millis();

  ArduinoOTA.handle();

  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

  // don't do anything before we get a time read and set the clock
  if (timeStatus() == timeNotSet) {
    if (secTime > 0) {
      setTime(timeSync());
      Serial.print("Time set, time is now <- ");
      digitalClockDisplay();
    }
    else {
      return;
    }
  }
  
  if (currTime - lastMonitorTime >= (MONITOR_SECS * 1000)) {
    // save count to the 'counter' feed on Adafruit IO
    Serial.print("sending count and rssi value -> ");
    Serial.println(count);
    counter->save(count);
    // save the wifi signal strength (RSSI) to the 'rssi' feed on Adafruit IO
    rssi->save(WiFi.RSSI());
    
    Serial.print("Time is: ");
    digitalClockDisplay();

    Serial.print("Button value # ");
    Serial.println(buttonValue);

    // increment the count by 1
    count++;
    lastMonitorTime = currTime;
  }
  
// commenting out button section until repurposed for analog status reporting
  // if(((currTime - lastReportTime) >= (DEBOUNCE_SECS * 1000)) || (currTime < (DEBOUNCE_SECS * 1000))) {
    // make debounce for button reads and reports
    // buttonRead = digitalRead(BUTTON_IO);
    // if (buttonRead) {
      // buttonPresses++;
      // Serial.print("sending button pressed! # is now -> ");
      // Serial.println(buttonPresses);
      // digitalClockDisplay();
      // reportButton->save(buttonPresses);
      // lastReportTime = currTime;
    // }
  // }
  if(((currTime - lastReportTime) >= (DEBOUNCE_SECS * 1000)) || (currTime < (DEBOUNCE_SECS * 1000))) {
    if (setButtonRead) {
      Serial.print("Value changed, update web value! # is now -> ");
      Serial.println(buttonRead);
      digitalClockDisplay();
      reportButton->save(buttonRead);
      lastReportTime = currTime;
      setButtonRead = false;
    }
  }

  // reset the count at some hour of the day
  if ((hour() == RESET_HOUR) && (minute() == 0) && (second() == 0) && (!resetFlag)) {
    Serial.print("sending count and presses reset at time -> ");
    digitalClockDisplay();
    count = 0;
    counter->save(count);
    // remember a reset happened so we don't do it again and bombard Adafruit IO with requests
    resetFlag = true;
  }

  // reallow reset to occur after reset time has passed
  if (second() == 1) {
    resetFlag = false;
  }

  // start LED handling code

  // Need to check how to get an analog reading in the ESP32
  // anaReadVal = analogRead(ANALOG_PIN);
  // buttonRead = map(anaReadVal,0,1024,0,255);
  if (buttonReadPrev != buttonRead) {
    Serial.print("new analog mapped value is: ");
    Serial.println(buttonRead);
    buttonReadPrev = buttonRead;
    setButtonRead = true;
    brightness = buttonRead;
  }
  // else take buttonValue from web to be brightness
  else if (setButtonValue && (buttonValue > 0 && buttonValue < 255)) { 
    brightness = buttonValue;
    setButtonValue = false;
  }

  // set master brightness control
  FastLED.setBrightness(brightness);

  if(isPlain && (brightness < 50 || brightness > 128)) {
    isPlain = false;
  }
  if(!isPlain && (brightness > 60 && brightness < 120)) { 
    isPlain = true;
  }

  if(isPlain) {
    quiet(brightness);
  }
  else {
    // Call the current pattern function once, updating the 'leds' array
    gPatterns[gCurrentPatternNumber]();    
  }

  // send the 'leds' array out to the actual LED strip
  FastLED.show();  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FRAMES_PER_SECOND); 

  // do some periodic updates
  EVERY_N_MILLISECONDS( 30 ) { gHue++; } // slowly cycle the "base color" through the rainbow
  EVERY_N_SECONDS( 30 ) { nextPattern(); } // change patterns periodically
}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void quiet(int brightness) {
  byte color = map(brightness,60,120,0,255);
  for(int i=0; i<NUM_LEDS; i++) {
    leds[i] = CHSV(color, 196, 255);
  }
}

void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}

void changeHue(int &currHue, int &destHue, int iStart, int iEnd) {
  
  if(currHue == destHue) {
    destHue = random(255);
  }

  if(((currHue - destHue) % 256) < 128) {
    currHue--;
  }
  else {
    currHue++;
  }

  for(int i=iStart; i<iEnd; i++) {
    leds[i] = CHSV(currHue, 255, 255);
  }

}

void singleColor() {

  static int wHue = 0;
  static int wDestHue = 0;
  static int cHue = 30;
  static int cDestHue = 0;
  static int manHue = 60;
  static int manDestHue = 0;
  static int womanHue = 90;
  static int womanDestHue = 0;
  
  changeHue(wHue, wDestHue, WStart, WEnd);
  changeHue(cHue, cDestHue, CStart, CEnd);
  changeHue(manHue, manDestHue, ManStart, ManEnd);
  changeHue(womanHue, womanDestHue, WomanStart, WomanEnd);
  
}

void toggleSegment(int iStart, int iEnd, int hue, int sat) {
  int pos = iStart + random16(iEnd - iStart);
  leds[pos] += CHSV( hue + random8(16), sat, 255);    
}

void toggle() {

  
  static WhichLeds whichSegment = Man;
  static unsigned long lastChange = 0;
  if(millis() - lastChange > 1000 * 2) {
    switch(whichSegment) {
      case Man:
        whichSegment = Woman;
        break;
      case Woman:
        whichSegment = Both;
        break;
      case Both:
        whichSegment = Man;
        break;
    }
    lastChange = millis();
  }
  fadeToBlackBy( leds, NUM_LEDS, 10);

  for(int i=WStart; i < CEnd; i++) {
    leds[i] = CHSV((1.0/6.0) * 255, 196, 255);
  }

  if(whichSegment == Man || whichSegment == Both) {
    for(int i=ManStart; i < ManEnd; i++) {
      leds[i] = CHSV(gHue, 196, 255);
    }
    //toggleSegment(ManStart, ManEnd, (255 * 2) / 3, 255);
  }
  
  if(whichSegment == Woman || whichSegment == Both) {
    for(int i=WomanStart; i < WomanEnd; i++) {
      leds[i] = CHSV(gHue + 128, 196, 255);
    }
    //toggleSegment(WomanStart, WomanEnd, 0 / 3, 255);    
  }
}

void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
  for(int i=0; i<NUM_LEDS; i++) {
    uint8_t hue = (gHue + (uint8_t)((double)i / (double)NUM_LEDS) * 256) % 256;
    leds[i] = CHSV(hue, 255, 255);
  }
}

void rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  for(int i=0; i<2; i++) {
    int pos = random16(NUM_LEDS);
    leds[pos] += CHSV( gHue + random8(64), 200, 255);
  }
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13,0,NUM_LEDS);
  leds[pos] += CHSV( gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 30;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16(i+7,0,NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void dotted() {
  for(int i=0; i<NUM_LEDS; i++) {
    uint8_t hue = gHue + i%6 * (255 / 6);
    leds[i] =   CHSV(hue, 255, 255);
  }
}