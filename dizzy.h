
float lastgyro = 0;

CRGB pickPass(int v)
{
  switch (v)
  {
    case 0:
      return CRGB(0, 0, 0);
    case 1:
      return CRGB(255, 0, 0);
    case 2:
      return CRGB(0, 255, 0);
    case 3:
      return CRGB(0, 0, 255);
    default:
      return CRGB(64, 64, 64);
  }
}

int clamp(int a, int b, int c)
{
  if (a < b)
    return b;
  if (a > c)
    return c;
  return a;
}

void DizzyGame( uint8_t *state, CRGB *display, int size) {
#ifdef USEACCEL
  static char *valid = "0001010100111110";
  static int quads[4];
  static float lax, laz;
  static float laa;
  static int level = 5;
  float ax = mpu6050.getAccX();
  float ay = mpu6050.getAccY();
  float az = mpu6050.getAccZ();
  /*
    Serial.print(ax);
    Serial.print(",");
    Serial.print(ay);
    Serial.print(",");
    Serial.print(az);
    Serial.println("");
  */
  float plane = sqrt((ax * ax) + (az * az));
  float total = sqrt((ax * ax) + (ay * ay));

  int quad = 0;
  if (ax > 0)
    quad += 1;
  if (az > 0)
    quad += 2;
  int maxquad = 0;
  quads[quad] = 0;
  int delta = 1;
  int mask = 0;
  for (int i = 0; i < 4; i++)
  {
    quads[i]++;
    mask = mask << 1;
    if (quads[i] > 50)
    {
      quads[i] = 50;
      mask += 1;
    }
  }
  if (ay > 2.0)
    delta = -1;
  if (ay < -2.0)
    delta = -1;
  if (valid[mask] == '0')
    delta = -1;
  if (total < 1.2)
    delta = -1;
  if (plane < 0.2)
    delta = -1;

  bool maxed = (level + delta) > 400;
  level = clamp(level + delta, 5, 400);

  int scaled = (level * size) / 100;
  int base = scaled / size;
  int mod = scaled % size;

  CRGB low = pickPass(base);
  CRGB hi = pickPass(base + 1);
  if (maxed)
  {
    low = CRGB(0, 255, 255);
    hi = CRGB(0, 255, 255);
  }

  for (int i = 0; i < size; i++)
  {
    if (i > mod)
      display[i] = low;
    else
      display[i] = hi;
  }
#endif
}




