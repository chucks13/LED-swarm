#define EFFECTFRAMECOUNT 600

#define EFFECTCOUNT   4

uint32_t get32(uint8_t *data)
{
  uint32_t v;
  v = data[0];
  v <<= 8;
  v += data[1];
  v <<= 8;
  v += data[2];
  v <<= 8;
  v += data[3];
  return v;
}
void put32(uint8_t* data, uint32_t value)
{
  data[3] = (value >> 0) & 255;
  data[2] = (value >> 8) & 255;
  data[1] = (value >> 16) & 255;
  data[0] = (value >> 24) & 255;
}
uint16_t get16(uint8_t *data)
{
  uint32_t v;
  v = data[0];
  v <<= 8;
  v += data[1];
  return v;
}
void put16(uint8_t* data, uint16_t value)
{
  data[1] = (value >> 0) & 255;
  data[0] = (value >> 8) & 255;
}

#define getCounter(data) get16(data+6)
#define getMW(data) get32(data+8)
#define getMZ(data) get32(data+12)
#define getEffect(data) (data[16])
#define getData(data,idx) (data[17+idx])

#define putCounter(data,value) put16(data+6,value)
#define putMW(data,value) put32(data+8,value)
#define putMZ(data,value) put32(data+12,value)
#define putEffect(data,value) data[16]=(value)
#define putData(data,idx,value) if(idx<13) data[17+idx]=(value)   // if statement keeps us honest

// use this random number generator, because its deterministic, and its seed is transmitted in the state variables
// dont make this dependant on the number of LEDs in your strip
uint32_t myRandom(uint8_t *state, int range)
{
  uint32_t m_w = getMW( state);
  uint32_t m_z = getMZ( state);
  m_z = 36969 * (m_z & 65535) + (m_z >> 16);
  m_w = 18000 * (m_w & 65535) + (m_w >> 16);
  putMW( state, m_w);
  putMZ( state, m_z);
  uint64_t value = (m_z << 16) + m_w;
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

void syncedAnimeInit(uint8_t *state)
{
  putMW(state, randomNonZero());
  putMZ(state, randomNonZero());
  putCounter(state , EFFECTFRAMECOUNT);
  putEffect(state, 0);
}

#ifndef STRONGMAN
void drawSolid( uint8_t *state, CRGB *display, int size, int frameNumber)
{
  if (frameNumber == 0)
    putData(state, 0, myRandom(state, 256));
  int color = getData(state, 0);
  for (int i = 0; i < size; i++)
    display[i] = CHSV(color, 255, 255);
}

void drawRainbow( uint8_t *state, CRGB *display, int size, int frameNumber)
{
  if (frameNumber == 0)
  {
    putData(state, 0, myRandom(state, 255));
    putData(state, 1, myRandom(state, 9));
    putData(state, 2, myRandom(state, 9));
  }
  int color = getData(state, 0);
  int pitch = getData(state, 1) + 1;
  int aspeed = getData(state, 2) + 1;
  for (int i = 0; i < size; i++)
  {
    int x = (i * 144) / size;         // was originally scaled for 144
    display[i] = CHSV((color + ( x * pitch) + (frameNumber * aspeed)) & 255, 255, 255);
  }

}

void drawNoise( uint8_t *state, CRGB *display, int size, int frameNumber)
{
  if (frameNumber == 0)
  {
    putData(state, 0, myRandom(state, 4) + 2); // speed
    putData(state, 1, myRandom(state, 255)); // offset
    putData(state, 2, myRandom(state, 3) + 4); // scale
    putData(state, 3, myRandom(state, 255)); // c1
    putData(state, 4, myRandom(state, 255)); // c2
    putData(state, 5, myRandom(state, 2)); // style
  }
  float speed = getData(state, 0);
  float offset = getData(state, 1);
  float scale = getData(state, 2);
  int c1 = getData(state, 3);
  int c2 = getData(state, 4);
  int style = getData(state, 5);
  scale = (scale * 144) / size;     // scale to current display  (was designed for 144
  for (int i = 0; i < size; i++)
  {
    int p = inoise8((i * scale) + offset, frameNumber * speed);
    switch (style)
    {
      case 0:
        display[i] = CHSV(p, 255, 255);
        break;
      case 1:
        if (p > 127)
        {
          display[i] = CHSV(c1, 255, (p - 128) * 6); // times 6 to minimize black
        }
        else
        {
          display[i] = CHSV(c2, 255, (127 - p) * 6);
        }
        break;
    }
  }
}

void fadeall(CRGB *display, int size, int scale) {
  for (int i = 0; i < size; i++) {
    display[i].nscale8(scale);
  }
}

void drawSparkles( uint8_t *state, CRGB *display, int size, int frameNumber)
{
  if (frameNumber == 0)
  {
    putData(state, 0, myRandom(state, 255)); // color
    putData(state, 1, myRandom(state, 4));   // type
    putData(state, 2, ( myRandom(state, 4) * 50) + 100); // fade
  }
  int color = getData(state, 0);
  int type = getData(state, 1);
  int fade = getData(state, 2);
  fadeall(display, size, fade);
  for (int i = 0; i < 50; i++)
  {
    if (random(256) < 20)
    {
      int x = random(300);
      if (x < size)
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
        display[x] = c;
      }
    }
  }
}

#endif

void drawStrongman( uint8_t *state, CRGB *display, int size, int frameNumber)
{
  int level = getData(state, 0);
  int high = getData(state, 1);
  int hightime = getData(state, 2);
  int highest = getData(state, 3);
  if (level > high)
  {
    high = level;
    hightime = 500;
  }
  if (level > highest)
    highest = level;
  if (hightime > 0)
    hightime--;
  if (hightime == 0)
  {
    high--;
    if (high < level)
      high = level;
  }
  for (int i = 0; i < size; i++)
  {
    int x = (i * 144) / size;         // was originally scaled for 144
    if (x < level)
      display[i] = CRGB(64, 0, 0);
    else
      display[i] = CRGB(0, 0, 0);
  }
  display[(high * size) / 144] = CRGB(255, 255, 0);
  display[(highest * size) / 144] = CRGB(0, 255, 0);
  putData(state, 1, high);
  putData(state, 2, hightime);
  putData(state, 3, highest);

}
void drawBlank( uint8_t *state, CRGB *display, int size, int frameNumber)
{
  for (int i = 0; i < size; i++)
  {
    display[i] = CRGB(0, 0, 0);
  }
}

void SyncedAnims( uint8_t *state, CRGB *display, int size) {
  uint32_t frameCounter = getCounter(state);
  int effect = getEffect(state);
  frameCounter++;
  if (frameCounter >= EFFECTFRAMECOUNT)
  {
    effect += myRandom(state, EFFECTCOUNT - 1) + 1;
    effect %= EFFECTCOUNT;
    putEffect(state, effect);
    frameCounter = 0;
  }
  switch (effect)
  {
#ifndef STRONGMAN
    case 0:
      drawSolid(state, display, size, frameCounter);
      break;
    case 1:
      drawRainbow(state, display, size, frameCounter);
      break;
    case 2:
      drawSparkles(state, display, size, frameCounter);
      break;
    case 3:
      drawNoise(state, display, size, frameCounter);
      break;
#endif
    case 10:
      drawStrongman(state, display, size, frameCounter);
      break;
    default:
      drawBlank(state, display, size, frameCounter);
      break;
  }
  putCounter(state, frameCounter);
}






