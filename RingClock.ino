#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <Adafruit_NeoPixel.h>
#include <Wire.h>
#include <DS3231.h>

/*
 * DEFINES
 */
#define NEO_RING_PIN_NUMBER 6
#define PUSH_BUTTON_PIN_NUMBER 1

/*
 * types
 */
typedef enum states {S0, S1, S2};


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

    unsigned long timeNow;
};

RingClock::RingClock(uint32_t colors[4]) :
    // Adafruit_NeoPixel(uint16_t n, uint16_t pin=6, neoPixelType type=NEO_GRB + NEO_KHZ800);
    Adafruit_NeoPixel(60, NEO_RING_PIN_NUMBER, NEO_GRBW + NEO_KHZ800),
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
  timeNow = millis();
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
  while(millis() < (timeNow + delayBy)) {
    delay(1);
  }
  if (cntr == 0) {
    incrTime();
    cntr = numLEDs;
  } else {
    cntr--;
  }
  tick();
  timeNow = millis();
  return cntr;
}

/*
 * GLOBALS
 */
RingClock *g_rc; // derived from Adafruit_NeoPixel - main class that does all the heavy lifting
RTClib myRTC; // real time clock object
int g_hourOffset; // pushbutton can increment hour by one
states state = S0; // used to find falling edge of button

void updateTime(RingClock * rc)
{
  DateTime now = myRTC.now();
  int hour = now.hour();
  if (hour > 12) {
    hour -= 12;
  }
  int min = now.minute();
  int sec = now.second();

  // when updating the time, set seconds to 0, otherwise the ticker
  // doesn't "hit" the seconds and bump it
  rc->setTime(hour + g_hourOffset, now.minute(), 0); //now.second());
}

// detect a depressed button being released, upon release increment the g_hourOffset
int buttonState(int button) {

  // return value, only true upon release of the button
  int rv = 0;

  // normally in S0, once button pressed move to S1, once button released
  // move to S2 which increments g_hourOffset, returns true, and goes back to S0
  switch (state) {
    case S0 : if (digitalRead(button) == 0) { // The button is pressed
                state = S1;
              }
              break;
              
    case S1 : if (digitalRead(button) == 1) { // The button is released
                state = S2;
              }
              break;

    case S2 : g_hourOffset++;
              if (g_hourOffset > 12) {
                g_hourOffset = 0;
              }
              state = S0;
              rv = 1;
              break;

    default : state = S0;
              break;
  }
  return (rv);
}

void setup() {

  uint32_t colors[4];
  colors[0] = Adafruit_NeoPixel::Color(    0,   0xF,    0, 0);      // hour
  colors[1] = Adafruit_NeoPixel::Color( 0x1F,     0,    0, 0);      // min
  colors[2] = Adafruit_NeoPixel::Color(  0x2,   0x3, 0x12, 0);      // sec
  colors[3] = Adafruit_NeoPixel::Color(  0x4,   0x1,  0x6, 0);      // tick
  g_rc = new RingClock(colors);

  Wire.begin();
  updateTime(g_rc);

  // used to advance the hour
  pinMode(PUSH_BUTTON_PIN_NUMBER, INPUT);

}

int g_loopCounter = 0;
int g_secCounter = 0;

void loop() {
  if (g_loopCounter < 30) {
    g_loopCounter = g_rc->tickDelay(14, g_loopCounter);
  } else {
    g_loopCounter = g_rc->tickDelay(13, g_loopCounter);
  }

  if (!g_loopCounter) {
    g_secCounter++;
  }

  // every 5 minutes query the RTC and update the time
  if (g_secCounter >= 300) {
    updateTime(g_rc);
    g_secCounter = 0;
  }

  // check the button, if pressed update the time
  if (buttonState(PUSH_BUTTON_PIN_NUMBER)) {
    updateTime(g_rc);
  }
}
