//===============================================================================================================
// This code for the Particle Electron publishes an event with wind data on a regular basis.
// There will also be a Particle Photon that will subscribe to the published data.  The Electron uses the
// cell phone network, so it's important to keep the data usage very low.  The Photon will be on a wifi somewhere
// (anywhere really).  It will get the published events, parse the terse data, form the HTTP GET request
// to update Weather Underground and/or WeatherFlow (aka iKitesurf), and submit those HTTP GET requests via wifi.
//
// Wiring to the wind instrument (Davis Instruments Anemometer and weather vane #6410; about $120 from Amazon):
//  - Anemometer GREEN wire to Electron A1 (Green goes to the wiper on the pot that measures wind direction)
//  - Anemometer YELLOW wire to 3.3V  (Red and Yellow are the two ends of the pot that measures direction)
//  - Anemometer RED wire to GND  (Red and black are the cup switch - but polarity is IMPORTANT)
//  - Anemometer BLACK wire to Pin 1 of the Schmitt trigger (74HC14)
//  - Schmitt trigger pin 2 to Electron D2 (interrupt 0)
//  - Schmitt trigger pin 14 to 5V and pin 7 to GND
//  - Schmitt trigger pin 1 is pulled up to Vcc with a 10K resistor
//  - Ceramic "104" cap between Gnd and Schmitt trigger pin 1
//
// Wiring to the MAX6675 temperature sensor:
//  - VCC to Electron 3.3V
//  - GND to Electron GND
//  - CS or SS to Electron A2
//  - SCK to Electron A3
//  - SO or MISO to Electron A4
//
// Wiring the Watchdog/Arduino Pro-Mini
//  - Electron B0 held high (3.3V) with a 20K resistor
//  - Electon B0 to Arduino D2.  This is the line that pings the Arduino to let it know all is well still.
//  - Arduino D3 to Electron Reset.  This resets the Electron if a ping isn't received for 20 minutes.
//
// On this anemometer:
//   Red/Black: cup switch   (Red to GND)
//   Red/Yellow: the two ends of the direction pot (Yellow to Vcc)
//   Green: the wiper on the direction pot (Green to A1)
//   I currently have the wind vane set up so that it reads 175.5 when it's exactly aligned with the arm
//     (i.e. arm pointing to the West and wind coming from the west.)
//   Speed in mph = 2.174 * counts/sec (for )
//
// Solar stuff (looks like this settles into about 43mA at 5V; but frequently goes up to 160mA.
//   Apparently it can run more than 1A during a publish.)
//
// Solar charge controller:
// - Connect the battery, solar panel, then load.  Dicsonnect them in the reverse order
// - On the solar charge controller there's a low-voltage cut-off which defaults to 10.7V.  There's also a "reconnect" threshold
//   which defaults to 12.6V.  I reprogrammed the reconnect to occur at 12.4 volts.
// - The system draws just under 40mA from the 12V battery once it's up and running.
//   So that should run 175 hours (about one week) on a 7AH gel cell.
//
// To-do:
// - Figure out why I get a big jump in wind gust when I have a connection outage.
// - Encode mph with alpha-numerics and do fractions of MPH. Using upper-case letters and numbers we can encode 0-129 mph in 0.1 mph increments (using 2 chars).
//   If I used the printable ASCII characters (32-126) I could encode speed in 1/2 MPH increments with a single character (from 0 to 47 mph).
//   Two characters would allow me to describe direction to about 1/25th of a degree.
// - Maybe I should send previous data with each event.  This could allow me to fill in gaps, or to get greater granularity even if
//   I don't increase the update rate.
// - Figure out whether the failed updates are happening on the Electron or Photon side (by watching the logs).
// - Consider having it sleep at night (lower current draw and potentially some data savings)
//    https://docs.particle.io/reference/firmware/electron/#sleep-sleep-
//    Probably should have sleep from sunset to sun-up.  Figure out what those hours are each month.
// - Check to see if it's stuck trying to connect and maybe do a reset:
//      Look at SYSTEM_MODE(SEMI_AUTOMATIC) and SYSTEM_THREAD(ENABLED) to stay ontop of the background task
//      and handle reconnects myself rather than letting some "obscure" magic happen.
//      User can call reset() to force a full reset.
//      *** With SYSTEM_THREAD(ENABLED) or Software Timers you can have your own code run to check
//          Particle.connected() and do whatever you want when this returns false.
//      Also look at: ApplicationWatchdog()
//      Discussion here: https://community.particle.io/t/using-lots-of-data-to-connect/31292/8
// - Consider reading battery voltage and publishing that variable using Particle.variable()
// - Perhaps I should register a function using Particle.function() that allows me to change the update rate and start/stop times.
// - Figure out something about webhooks, REST, IFTTT
//
// rhc 2017-02-19
// rhc 2018-01-01   Added support for MAX6675 temperature sensor
// rhc 2018-01-10   Added code to have pin B0 ping the external watchdog
//===============================================================================================================
#include <math.h>
#include <stdlib.h>

#define R2D (57.2958)
#define WIND_DIR_OFFSET (-94.5)
#define PUBLISH_FREQ (48)       // We take wind readings every 10 seconds.  We do PUBLISH_FREQ wind readings for each published event

#define CNTS_TO_MPH (2.174)     // Speed in mph = 2.174 * counts/sec for anemometer used at Mussel Rock (per my own calibration)
// #define CNTS_TO_MPH (2.25)     // Speed in mph = 2.25 * counts/sec for Davis 6410 Anemometer

volatile int counter;
int first_event;
float gust;
float total_x, total_y;
int led;
int wind_cntr;
int last_wind_cntr;
int loop_counter;
int led_on = 0;
int last_hr = 0;
unsigned long time_in_secs;
unsigned long last_time_in_secs;
unsigned long last_gust_time_in_secs;

void publish_wind_event(float dt);
int is_dst(void);
void click(void);


// From Arduino MAX6675 library ==============================
class MAX6675
{
public:
    MAX6675(int8_t SCLK, int8_t CS, int8_t MISOX);

    double readCelsius(void);
    double readFahrenheit(void);
    // For compatibility with older versions:
    double readFarenheit(void) { return readFahrenheit(); }
private:
    int8_t sclk, miso, cs;
    uint8_t spiread(void);
};


// Define SPI communication pins for the MAX6675 temperature sensor
int ktcCS  = A2;  // CS or SS - chip-select or slave-select
int ktcCLK = A3;  // SCK - serial clock
int ktcSO  = A4;  // MISO - master in - slave out


MAX6675 ktc(ktcCLK, ktcCS, ktcSO);  // Temperature sensor

ApplicationWatchdog wd(3600000, System.reset);  // Set the watchdog to reboot the Electron if it doesn't get pinged for 1 hour




void setup()
{
    int dst_status;
    int hr, min, sec;

    Serial.begin(9600);

    led = 0;
    pinMode(B0, OUTPUT);  // This will ping the external watchdog
    pinMode(D7, OUTPUT);  // controls LED
    pinMode(D2, INPUT);   // This reads the input from the wind cups


    digitalWrite(B0, HIGH);

    attachInterrupt(D2, click, RISING);      // Call click() when interrupt 0 (digital pin 2) rises

    // Set time zone (-7.0 during DST; -8.0 otherwise)  // -3- this should be checked daily rather than doing it once at startup.
    dst_status = is_dst();
    if (dst_status) Time.zone(-7);  // Set for Pacific daylight time.
    else            Time.zone(-8);  // Set for Pacific standard time.

    counter = 0;
    gust = 0.0;
    loop_counter = 0;
    last_wind_cntr = 0;
    total_x = 0;
    total_y = 0;

    hr = Time.hour();
    min = Time.minute();
    sec = Time.second();
    last_time_in_secs = hr*3600 + min*60 + sec;
    last_gust_time_in_secs = last_time_in_secs;

    first_event = 1;
    wd.checkin(); // ping the watchdog after the connection is made - in case it took a while.
}




//------------------------------------------------------------------------------------------------------------------------
// We'll loop every 10 seconds and check the wind speed and direction.  We'll do this for 8 minutes, and then publish an
// event with the average wind speed, the wind gust (highest 10 second reading), and the average wind direction.
//------------------------------------------------------------------------------------------------------------------------
void loop()
{
    int dir;
    float speed;
    float x, y;
    float angle, dt;
    int hr, min, sec;

    delay(10000);  // Take wind speed and direction readings every 10 seconds

    hr = Time.hour();
    min = Time.minute();
    sec = Time.second();
    time_in_secs = hr*3600 + min*60 + sec;

    if (hr < last_hr) Particle.syncTime();  // Sync the clock with the cloud every day at midnight.
    last_hr = hr;                           // Note that it automatically syncs when it connects to the cloud.

    Serial.print("Time: ");
    Serial.print(hr);
    Serial.print(":");
    Serial.println(min);

    dir = analogRead(1);

    angle = (345.0/4095.0) * (float)(dir);  // NOTE: we use 345 rather than 360 because there's a roughly 15 degree dead-spot

    x = cos(angle/R2D);
    y = sin(angle/R2D);
    total_x += x;
    total_y += y;

    wind_cntr = counter; // this keeps the total count for the event publish interval (currently 8 minutes)

    //-3- it looks like this is the problem.  If we lose connection the time could end up being much more than 10 seconds.
    dt = (float)(time_in_secs - last_gust_time_in_secs);
    // speed = 2.174 * (float)(wind_cntr - last_wind_cntr) / dt;  // this gives wind speed in mph for the anemometer at Mussel Rock
    speed = CNTS_TO_MPH * (float)(wind_cntr - last_wind_cntr) / dt;  // this gives wind speed in mph for the Davis 6410 anemometer
    if (speed > gust) gust = speed;
    last_wind_cntr = wind_cntr;
    loop_counter += 1;
    last_gust_time_in_secs = time_in_secs;

    Serial.print("wind counter: ");
    Serial.print(wind_cntr);
    Serial.print("  Dir: ");
    Serial.println(angle);

    if (led_on == 1)
    {
        digitalWrite(D7, LOW);
        led_on = 0;
    }
    else
    {
        digitalWrite(D7, HIGH);
        led_on = 1;
    }


    if (loop_counter >= PUBLISH_FREQ)
    {
        wd.checkin();

        digitalWrite(B0, LOW);  // Trigger the external watchdog to let it know we're still alive
        delay(1);
        digitalWrite(B0, HIGH);

        Serial.print("Time in seconds: ");
        dt = (float)(time_in_secs-last_time_in_secs);
        Serial.println(dt);
        last_time_in_secs = time_in_secs;
        if (hr >= 8 && hr < 20) publish_wind_event(dt);
        counter = 0;
        last_wind_cntr = 0;
        loop_counter = 0;
        gust = 0;
        total_x = 0;
        total_y = 0;
        // digitalWrite(4, LOW);  // End the watchdog trigger pulse
    }
}




void publish_wind_event(float dt)
{
    // dt is the number of seconds we've been accumulating wind data for since the last publish.
    char buf[300];
    float speed;
    int wind_speed;
    int wind_gust;
    int wind_dir;
    float dir;
    double temperature;
    int itemp;

    // Use global "wind_cntr" as the total cup turns for the publish interval
    // Use global "gust" as the max 10 second cup speed (in rotations/second)
    // Use globals "total_x" and "total_y" to compute avg wind direction.

    speed = CNTS_TO_MPH * (float)wind_cntr / dt;
    wind_speed = (int)(speed + 0.5);
    wind_gust = (int)(gust + 0.5);
    dir = R2D * atan2(total_y, total_x);
    wind_dir = (int)(dir + 0.5);
    if (wind_dir >= 360) wind_dir -= 360;
    if (wind_dir < 0) wind_dir += 360;
    /*
    temperature = ktc.readFahrenheit();  // Mussel Rock sensor doesn't have a temperature sensor
    itemp = (int)(temperature + 0.5);
    sprintf(buf, "%02d%02d%03d%03d", wind_speed, wind_gust, wind_dir, itemp);
    */
    sprintf(buf, "%02d%02d%03d", wind_speed, wind_gust, wind_dir);
    Serial.print("publish: ");
    Serial.println(buf);

    // Particle.publish(String eventName, String data);
    // We give this event a unique name instead of naming it something like "wind" so we don't
    // end up reading everyone elses "wind" events.
    // -3- Ultimately I may publish these as private events so that only I can subscribe to them.
    /*  These are for Joe's sensor
    if (first_event) Particle.publish("JL3XX", buf);
    else             Particle.publish("JL3X", buf);
    */
    if (first_event) Particle.publish("WL3XX", buf);
    else             Particle.publish("WL3X", buf);
    first_event = 0;
}



// ============================================================================
// Interupt routine.  This gets called once for each rotation of the wind cups.
// ============================================================================
void click()
{
    counter++;
}




// ============================================================================
// Return 1 if we're currently observing Daylight Savings Time.
// I begin DST on March 12 and end it on November 5 (I think it always happens
// on a Sunday in real life, but this will have to do)
// ============================================================================
int is_dst()
{
    int iday, imonth;

    imonth = Time.month();  // returns 1 to 12
    iday = Time.day();      // returns 1 to 31

    if (imonth > 3   && imonth < 11) return(1);
    if (imonth == 3  && iday >= 12) return(1);
    if (imonth == 11 && iday <=5) return(1);
    return(0);
}




// =========================================================================================
// From Arduino MAX6675 library    www.ladyada.net/learn/sensors/thermocouple

MAX6675::MAX6675(int8_t SCLK, int8_t CS, int8_t MISOX)
{
    sclk = SCLK;
    cs = CS;
    miso = MISOX;

    //define pin modes
    pinMode(cs, OUTPUT);
    pinMode(sclk, OUTPUT);
    pinMode(miso, INPUT);

    digitalWrite(cs, HIGH);
}



double MAX6675::readCelsius(void)
{
    uint16_t v;

    digitalWrite(cs, LOW);
    delay(1);

    v = spiread();
    v <<= 8;
    v |= spiread();

    digitalWrite(cs, HIGH);

    if (v & 0x4)
    {
        // uh oh, no thermocouple attached!
        return NAN;
        //return -100;
    }

    v >>= 3;

    return v*0.25;
}




double MAX6675::readFahrenheit(void)
{
    return readCelsius() * 9.0/5.0 + 32;
}




byte MAX6675::spiread(void)
{
    int i;
    byte d = 0;

    for (i=7; i>=0; i--)
    {
        digitalWrite(sclk, LOW);
        delay(1);
        if (digitalRead(miso))
        {
            //set the bit to 0 no matter what
            d |= (1 << i);
        }
        digitalWrite(sclk, HIGH);
        delay(1);
    }

    return d;
}

