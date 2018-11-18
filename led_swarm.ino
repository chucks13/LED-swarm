#include <FastLED.h>
#include <SPI.h>
#include <NRFLite.h>
#include <EEPROM.h>

// board arduino nano
/*
  Radio    Arduino
  CE    -> 9
  CSN   -> 10 (Hardware SPI SS)
  MOSI  -> 11 (Hardware SPI MOSI)
  MISO  -> 12 (Hardware SPI MISO)
  SCK   -> 13 (Hardware SPI SCK)
  IRQ   -> No connection
  VCC   -> No more than 3.6 volts
  GND   -> GND

*/

#define NUM_LEDS 60                                // how long our strip is
#define DATA_PIN 7                                  // 2811 data pin
const static uint8_t RADIO_ID = 1;                  // Our radio's id.
const static uint8_t PIN_RADIO_CE = 9;              // hardware pins
const static uint8_t PIN_RADIO_CSN = 10;            // hardware pins
const static uint8_t PIN_BRIGHTNESS_DOWN = 5;            // hardware pins
const static uint8_t PIN_BRIGHTNESS_UP = 6;            // hardware pins


#define SENDPERIOD 2000                             // broadcast period in milliseconds
#define TAGID (('B'<<24)+('l'<<16)+('i'<<8)+'s')    // tag code is 'Blis'
#define EFFECTFRAMECOUNT 600
#define LEDPERIOD (1000 / 60)                       // how often we animate
#define MIN_BRIGHTNESS 20
#define MAX_BRIGHTNESS 255

struct state
{
  uint32_t frameCounter;      // frames since effect started
  uint32_t m_w;               // random seed
  uint32_t m_z;               // random seed
  uint8_t effect;             // current effect being shown
  uint8_t data[11];           // room for 11 more bytes, effect dependent
};

struct RadioPacket // Any packet up to 32 bytes can be sent.
{
  uint32_t tag;                                     // much match to be our protocol
  uint16_t unitId;                                  // for finding who is the master
  state data;                                 // our payload
};

NRFLite _radio;
RadioPacket current;                                // our animation stat must fit in the data element
bool radioAlive;                                    // true if radio booted up
int timeToSend;                                     // how long till next send;
int muteTimer;
int brightness;

// How many leds in your strip?
int timeToDisplay;                                  // how long to update the display
// Define the array of leds
CRGB leds[NUM_LEDS];                                // our display

// use this ramdom number generator, because its deterministic, and its seed is transmitted in the state variables
// dont make this dependant on the number of LEDs in your strip
uint32_t myRandom(int range)
{
  current.data.m_z = 36969 * ( current.data.m_z & 65535) + ( current.data.m_z >> 16);
  current.data.m_w = 18000 * ( current.data.m_w & 65535) + ( current.data.m_w >> 16);
  uint64_t value = ( current.data.m_z << 16) +  current.data.m_w;
  value &= 0xffffffff;
  value = value * range;
  value >>= 32;
  return value;  // 32-bit result
}

// used for initial seed
int randomNonZero()
{
  while (true)
  {
    int v = random(65536);
    if (v != 0)
      return v;
  }
}

void setLevel(int level)
{
  if (level < MIN_BRIGHTNESS)
    level = MIN_BRIGHTNESS;
  if (level > MAX_BRIGHTNESS)
    level = MAX_BRIGHTNESS;
  brightness = level;
  LEDS.setBrightness(brightness);
}

// assumed to be called at 60 fps
void checkButtons()
{
  if (digitalRead(PIN_BRIGHTNESS_DOWN) == 0)
    setLevel(brightness - 1);
  else if (digitalRead(PIN_BRIGHTNESS_UP) == 0)
    setLevel(brightness + 1);
  else
    EEPROM.update(0, brightness);   // eeprom.update only writes if the data is different
}

// initialize the animation
void setupLED()
{
  Serial.println("resetting");
  LEDS.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);
  brightness = EEPROM.read(0);
  setLevel(brightness);
  timeToDisplay = 0;
  current.data.m_z = randomNonZero();
  current.data.m_w = randomNonZero();
  current.data.frameCounter =  EFFECTFRAMECOUNT;
  pinMode(PIN_BRIGHTNESS_DOWN, INPUT_PULLUP);
  pinMode(PIN_BRIGHTNESS_UP, INPUT_PULLUP);

}

// initialize the radio code
void setupRadio()
{
  current.unitId = random(65536);
  current.tag = TAGID;
  radioAlive = true;
  if (!_radio.init(RADIO_ID, PIN_RADIO_CE, PIN_RADIO_CSN))
  {
    Serial.println("Cannot communicate with radio");
    radioAlive = false;
    //        while (1); // Wait here forever.
  }
  timeToSend = 0;
  muteTimer = 0;
}

void setup() {
  randomSeed(analogRead(0));
  Serial.begin(115200);
  setupLED();
  setupRadio();
}

void checkRadioReceive()
{
  // check for incoming data
  while (_radio.hasData())
  {
    RadioPacket incoming;
    _radio.readData(&incoming);
    if (incoming.tag != TAGID)                      // is it our packet type?
    {
      Serial.println("rejected tag");
      continue;
    }
    Serial.print("got data from ");
    Serial.print(incoming.unitId);
    if (incoming.unitId < current.unitId)       // it was lower rank than me slave, ignore it
    {
      Serial.println(" rejected");
      continue;
    }
    else if (incoming.unitId > current.unitId)  // it was higher rank than me, get the data
    {
      memcpy(&current.data, &incoming.data, sizeof(current.data));
      Serial.println(" accepted");
      muteTimer = SENDPERIOD * 3;              // im listening, so I keep quiet
    }
    else                                        // it was the same rank as me, pick a new rank
    {
      current.unitId = random(65536);
      Serial.println(" collision, picking new ID");
    }
  }
}

void checkRadioSend(int deltaTime)
{
  if (muteTimer > 0)
    muteTimer -= deltaTime;

  timeToSend -= deltaTime;
  if (timeToSend > 0)
    return;
  timeToSend += SENDPERIOD;

  if (muteTimer > 0)
    return;

  Serial.print(sizeof(current));
  Serial.print(" bytes send from ");
  Serial.print(current.unitId);
  if ( _radio.send(RADIO_ID, &current, sizeof(current), NRFLite::NO_ACK))
  {
    Serial.println(" success");
  }
  else
  {
    Serial.println(" fail");
    timeToSend += current.unitId & 255;            // might have been a collision, wait a little longer, based on my ID
  }
}

void loop()
{
  unsigned long lastTime = millis();
  while (true)
  {
    unsigned long now = millis();
    unsigned long delta = now - lastTime;
    lastTime = now;
    checkAnimation(delta);
    if (radioAlive)
    {
      checkRadioReceive();
      checkRadioSend(delta);
    }
  }
}

void checkAnimation(int deltaTime) {
  timeToDisplay -= deltaTime;
  if (timeToDisplay <= 0)
  {
    checkButtons();
    timeToDisplay += LEDPERIOD;
    if (current.data.frameCounter >= EFFECTFRAMECOUNT)
    {
      current.data.effect = myRandom(4);
      current.data.frameCounter = 0;
    }
    switch (current.data.effect)
    {
      case 0:
        drawSolid(current.data.frameCounter);
        break;
      case 1:
        drawRainbow(current.data.frameCounter);
        break;
      case 2:
        drawSparkles(current.data.frameCounter);
        break;
      case 3:
        drawNoise(current.data.frameCounter);
        break;
    }

    FastLED.show();
    current.data.frameCounter++;
  }
}

void drawSolid(int frameNumber)
{
  if (frameNumber == 0)
    current.data.data[0] = myRandom(256);
  int color = current.data.data[0];
  for (int i = 0; i < NUM_LEDS; i++)
    leds[i] = CHSV(color, 255, 255);
}

void drawRainbow(int frameNumber)
{
  if (frameNumber == 0)
  {
    current.data.data[0] = myRandom(255);
    current.data.data[1] = myRandom(9);
    current.data.data[2] = myRandom(9);
  }
  int color = current.data.data[0];
  int pitch = current.data.data[1] + 1;
  int aspeed = current.data.data[2] + 1;
  for (int i = 0; i < NUM_LEDS; i++)
  {
    int x = (i * NUM_LEDS) / 144;         // was originally scaled for 144
    leds[i] = CHSV((color + ( x * pitch) + (frameNumber * aspeed)) & 255, 255, 255);
  }

}

void drawNoise(int frameNumber)
{
  if (frameNumber == 0)
  {
    current.data.data[0] = myRandom(4) + 2; // speed
    current.data.data[1] = myRandom(255); // offset
    current.data.data[2] = myRandom(3) + 4; // scale
    current.data.data[3] = myRandom(255); // c1
    current.data.data[4] = myRandom(255); // c2
    current.data.data[5] = myRandom(2); // style
  }
  float speed = current.data.data[0];
  float offset = current.data.data[1];
  float scale = current.data.data[2];
  int c1 = current.data.data[3];
  int c2 = current.data.data[4];
  int style = current.data.data[5];
  scale = (scale * NUM_LEDS) / 144;     // scale to current display  (was designed for 144
  for (int i = 0; i < NUM_LEDS; i++)
  {
    int p = inoise8((i * scale) + offset, frameNumber * speed);
    switch (style)
    {
      case 0:
        leds[i] = CHSV(p, 255, 255);
        break;
      case 1:
        if (p > 127)
        {
          leds[i] = CHSV(c1, 255, (p - 128) * 6); // times 6 to minimize black
        }
        else
        {
          leds[i] = CHSV(c2, 255, (127 - p) * 6);
        }
        break;
    }
  }
}

void fadeall(int scale) {
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i].nscale8(scale);
  }
}

void drawSparkles(int frameNumber)
{
  if (frameNumber == 0)
  {
    current.data.data[0] = myRandom(255);   // color
    current.data.data[1] = myRandom(4);     // type
    current.data.data[2] = ( myRandom(4) * 50) + 100; // fade
  }
  int color = current.data.data[0];
  int type = current.data.data[1];
  int fade = current.data.data[2];
  fadeall(fade);
  for (int i = 0; i < 50; i++)
  {
    if (random(256) < 20)
    {
      int x = random(300);
      if (x < NUM_LEDS)
      {
        CRGB c;
        switch (type)
        {
          case 0:
            c = CRGB(255, 255, 255); // white
            break;
          case 1:
            c = CHSV(color, 255, 255); // white            // constant color
            break;
          case 2:
            c = CHSV(random(255), 255, 255); // multi;
            break;
          case 3:
            c = CHSV(frameNumber + x, 255, 255); // rainbow
            break;
        }
        leds[x] = c;
      }
    }
  }
}

