#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <Adafruit_NeoPixel.h>

//////////////////////////////////////////////////////////////////////
// The LED class simply holds the state of the LED, on or off
class LED {
  public:
    LED(void)           {clear();}
    void set(void)      {state = 1;};
    void clear(void)    {state = 0;};
    int getState(void)  {return state;};

  private:
    int state;
};

//////////////////////////////////////////////////////////////////////
// The Ring class holds an array of LEDs and their color,
// and can change the state of the LEDs
class Ring {
  public:
    Ring(int length_in, int32_t color_in);

    LED &operator[](int index);

    void    increment(void);
    void    setOnly(int index);
    int     getOnly(void);
    int32_t getColor(void) {return color;};

  private:
    int length;
    LED * leds;
    int32_t color;
};

Ring::Ring(int length_in, int32_t color_in):length(length_in),color(color_in)
{
  leds = new LED[length];
  setOnly(0);
}

LED &Ring::operator[](int index) {
  return leds[index];
}

// Finds currently set LED and clears it and sets the next one
void Ring::increment(void) {

  // Find the LED that's on
  int j = 0;
  while ((j < length) && !(leds[j].getState())) {
    j++;
  }

  // Clear it
  leds[j].clear();

  // Turn on the next one,
  // unless we just cleared the last one,
  // then turn on the first one
  if ((j + 1) == length)
    leds[0].set();
  else
    leds[j + 1].set();

}

void Ring::setOnly(int index) {
  for (int j = 0; j < length; j++) {
    leds[j].clear();
  }
  leds[index].set();
}

int Ring::getOnly(void) {
  int j = 0;
  while ((j < length) && (!leds[j].getState())) {
    j++;
  }
  return j;
}

// The HourRing class derives from the Ring class,
// it increments by 5 instead of just 1
class HourRing : public Ring {
  public:
    HourRing(int32_t color_in):Ring(60, color_in){};
    void increment(void);
};

void HourRing::increment(void) {
  for (int j = 0; j < 5; j++) {
    Ring::increment();
  }
}

// The RingClock class derives from the Adafruit_NeoPixel class,
// it sets the LEDs and updates the time
class RingClock : public Adafruit_NeoPixel {
  public:
    RingClock(uint32_t colors[4]);
    void setTime(int hour, int min, int sec);
    void incrTime();
    void tick(void);
    int  tickDelay(int delayBy, int cntr);

  private:
    HourRing  hourRing;
    Ring      minRing;
    Ring      secRing;
    Ring      tickRing;
};

RingClock::RingClock(uint32_t colors[4]) :
    Adafruit_NeoPixel(60, 6, NEO_GRBW + NEO_KHZ800),
    hourRing(colors[0]),
    minRing(numLEDs, colors[1]),
    secRing(numLEDs, colors[2]),
    tickRing(numLEDs, colors[3]) {
  tickRing.setOnly(0);
  begin();
  clear();
  show();
}

void RingClock::setTime(int hour, int min, int sec) {
  hourRing.setOnly((hour % 12) * 5);
  minRing.setOnly(min);
  secRing.setOnly(sec);
}

void RingClock::incrTime() {
  secRing.increment();
  if (secRing[0].getState()) {
    minRing.increment();
    if (minRing[0].getState()) {
      hourRing.increment();
    }
  }
}

void RingClock::tick() {
  clear();
  tickRing.increment();
  setPixelColor(tickRing.getOnly(), tickRing.getColor());
  setPixelColor( secRing.getOnly(),  secRing.getColor());
  setPixelColor( minRing.getOnly(),  minRing.getColor());
  setPixelColor(hourRing.getOnly(), hourRing.getColor());
  show();
}

// after a delay, move the tick LED and increment a counter.
// once the counter has reached the number of LEDs in the ring,
// increment time
int RingClock::tickDelay(int delayBy, int cntr) {
  delay(delayBy);
  if (cntr == 0) {
    incrTime();
    cntr = numLEDs;
  } else {
    cntr--;
  }
  tick();
  return cntr;
}

RingClock *rc;

void setup() {
  uint32_t colors[4];
  colors[0] = Adafruit_NeoPixel::Color(150,   0,   0);      // hour
  colors[1] = Adafruit_NeoPixel::Color(  0, 150,   0);      // min
  colors[2] = Adafruit_NeoPixel::Color(153,   0, 153);      // sec
  colors[3] = Adafruit_NeoPixel::Color(  0,   0,   0,  50); // tick
  rc = new RingClock(colors);
  rc->setTime(9, 44, 0);
}

int loopCounter = 0;
void loop() {
  loopCounter = rc->tickDelay(15, loopCounter);
}

#if 0
time_t now()
{
  return (time(NULL));
}

void updateTime(RingClock * rc)
{
  time_t current_time;
  struct tm * time_ptr;
  current_time = now();
  time_ptr = localtime(&current_time);
  rc->setTime(time_ptr->tm_hour, time_ptr->tm_min, time_ptr->tm_sec);
}
#endif
