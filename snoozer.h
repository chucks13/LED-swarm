#include <Snooze.h>
SnoozeDigital digital;
SnoozeBlock config(digital);


//------------------------------------------------------------------------
// start up snooze mode
// the power module has a tendency to cycle in snooze mode, tso this might exit early
// so we keep track of what state we atre in in NV ram so we can resume snoozing

void startSnooze(int address)
{
  EEPROM.write(address, 0);  // we are sleeping
  Snooze.sleep( config );         // go to sleep
  EEPROM.write(address, 1);  // we woke up normally
}

// if we power up in snooze modce, go back to sleep
void waitSnooze(int address,int pin)
{
  digital.pinMode(pin, INPUT_PULLUP, RISING);//pin, mode, type
  byte sleepstate = EEPROM.read(address);
  if (sleepstate == 0)
  {
    Snooze.sleep( config );         // go back to sleep
    EEPROM.write(address, 1);  // we woke up from a power snooze
  }
}


