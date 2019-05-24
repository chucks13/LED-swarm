
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
  static float lax, laz;
  static float laa;
  static int level = 5;
  float ax = mpu6050.getAccX();
  float ay = mpu6050.getAccY();
  float az = mpu6050.getAccZ();
  float tz = (ax * ax)  + (az * az);
  float dx = lax - ax;
  float dz = laz - az;
  lax = ax;
  laz = az;
  float dp = sqrt(dx * dx + dz * dz);
  int delta = 1;
  if (ay > 1.2)
    delta = -1;
  if (ay < -1.2)
    delta = -1;
  if (tz < 0.1)
    delta = -1;
  level = clamp(level + delta, 5, 400);

  int scaled = (level * size) / 100;
  int base = scaled / size;
  int mod = scaled % size;

  CRGB low = pickPass(base);
  CRGB hi = pickPass(base + 1);

  for (int i = 0; i < size; i++)
  {
    if (i > mod)
      display[i] = low;
    else
      display[i] = hi;
  }
  #endif
}




