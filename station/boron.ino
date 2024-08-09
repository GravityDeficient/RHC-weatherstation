//===============================================================================================================
// This code for the Particle Boron publishes an event with wind data every 5 minutes 24 hrs/day.
// It publishes on each whole 5 minute interval (i.e. 10:00, 10:05; 10:10...).
// The Boron uses the cell phone network, so it's important to keep the data usage very low.
//===============================================================================================================
// Include configuration
#include "config.h"

#ifdef CONFIG_LOCAL_H
#include "config.local.h"
#endif

#include <math.h>
#include <stdlib.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>


#define R2D (57.2958)

volatile int counter;
int publish_min;  // Publish an event the first time we see we're at this minute.
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
unsigned long previousMillis = 0;
const long interval = 10000;

void publish_wind_event(float dt);
int is_dst(void);
void click(void);
float get_battery_voltage(void);

SerialLogHandler logHandler; // Enable Serial Logging
ApplicationWatchdog wd(3600000, System.reset);  // Set the watchdog to reboot the Boron if it doesn't get pinged for 1 hour

//-3- System remote reset variables --We use a particle cloud function to maintain a reset
#define DELAY_BEFORE_RESET 2000
unsigned int resetDelayMillis = DELAY_BEFORE_RESET;
unsigned long resetSync = millis();
bool resetFlag = false;

//-3- BMP/BME280 sensor setup
bool bme280_present;
Adafruit_BME280 bme;
bool bmp280_present;
Adafruit_BMP280 bmp;

//@@ setup===============================================================================================================
//=======================================================================================================================
void setup()
{
    int i;
    int hr, min, sec;

    delay(10);  //Make sure sensor had enough time to turn on. BME280 requires 2ms to start up.

    //Calling .begin() causes the settings to be loaded
    bme280_present = bme.begin();
    if(!bmp280_present) {
        bmp280_present = bmp.begin();
    }

    serialConnectBanner();

    led = 0;
    pinMode(A5, INPUT);  // This will monitor the gel-cell battery voltage.
    pinMode(A4, INPUT);  // Reads the wind direction pot.
    pinMode(D2, OUTPUT);  // This will ping the external watchdog
    pinMode(D7, OUTPUT);  // controls LED
    pinMode(D3, INPUT);   // This reads the input from the wind cups

    digitalWrite(D2, LOW);  // Trigger the external watchdog to let it know we're alive
    delay(1);
    digitalWrite(D2, HIGH);

    attachInterrupt(D3, click, RISING);      // Call click() when interrupt 0 (digital pin 2) rises (this is the wind cups spinning)

    setTimeZone(); // Set the timezone from the provided const TIMEZONE offset

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

    for(i=1; i<=12; i++)
    {
        publish_min = 5*i;
        if (publish_min > min) break;
    }
    publish_min += 5;  // for the first event we'll go up to an extra 5 minutes just in case we're very close to an edge.
    if (publish_min >= 60) publish_min -= 60;

    first_event = 1;

    wd.checkin(); // ping the watchdog after the connection is made - in case it took a while.

    //  Remote Reset Particle Function
    Particle.function("reset", cloudResetFunction);
}

//@@ loop================================================================================================================
// We'll loop every 10 seconds and check the wind speed and direction.  We'll do this for 5 minutes, and then publish an
// event with the average wind speed, the wind gust (highest 10 second reading), and the average wind direction.
//=======================================================================================================================
void loop()
{
    int dir;
    float speed;
    float x, y;
    float angle, dt;
    int hr, min, sec;
    unsigned long currentMillis = millis();

    serialConnectBanner();

    if (currentMillis - previousMillis >= interval) { // Take wind speed and direction readings every 10 seconds
        previousMillis = currentMillis;
        hr = Time.hour();
        min = Time.minute();
        sec = Time.second();
        time_in_secs = hr*3600 + min*60 + sec;

        if (hr < last_hr) {
            Particle.syncTime();  // Sync the clock with the cloud every day at midnight.
            setTimeZone(); // Reset the timezone every day to allow DST adjustments
        }
        last_hr = hr; // Note that it automatically syncs when it connects to the cloud.

        Log.info("Time: %d:%d", hr, min);

        dir = analogRead(A4);

        angle = 15.0 + (345.0/4095.0) * (float)(dir);  // NOTE: we use 345 rather than 360 because there's a roughly 15 degree dead-spot
        // Nominally the arm is supposed to be aimed true North, but there's a dead-spot about 0-15 deg.

        x = cos(angle/R2D);
        y = sin(angle/R2D);
        total_x += x;
        total_y += y;

        wind_cntr = counter; // this keeps the total count for the event publish interval (currently 8 minutes)

        //-3- it looks like this is the problem.  If we lose connection the time could end up being much more than 10 seconds.
        dt = (float)(time_in_secs - last_gust_time_in_secs);
        speed = CNTS_TO_MPH * (float)(wind_cntr - last_wind_cntr) / dt;  // this gives wind speed in mph for the Davis 6410 anemometer
        if (speed > gust) gust = speed;
        last_wind_cntr = wind_cntr;
        loop_counter += 1;
        last_gust_time_in_secs = time_in_secs;
        Log.info("Wind Counter: %d Dir: %d", (int)wind_cntr, (int)angle);

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

        if (min == publish_min)
        {
            publish_min += 5;
            if (publish_min >= 60) publish_min -= 60;
            wd.checkin();

            digitalWrite(D2, LOW);  // Trigger the external watchdog to let it know we're still alive
            delay(1);
            digitalWrite(D2, HIGH);

            dt = (float)(time_in_secs-last_time_in_secs);
            Log.info("Time in seconds: %d", dt);
            last_time_in_secs = time_in_secs;
            /* if (hr >= 8 && hr < 20)  We publish wind events 24 hrs/day now since Particle up'd the monthly data from 1MB to 3MB */
            publish_wind_event(dt);
            counter = 0;
            last_wind_cntr = 0;
            loop_counter = 0;
            gust = 0;
            total_x = 0;
            total_y = 0;
        }
    }

    //  Remote Reset Function
    if ((resetFlag) && (millis() - resetSync >=  resetDelayMillis)) {
        Log.info("Remote Reset Initiated");
        Particle.publish("Debug", "Remote Reset Initiated", 300, PRIVATE);
        System.reset();
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
    float temp, pressure;
    int itemp, ipressure, humidity, voltage;

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

    itemp = 0;
    ipressure = 0;
    humidity = 0;
    if (bme280_present) {

        temp = (bme.readTemperature() * 9/5) + 32;    // Read Temperature in c convert to f
        itemp = (int)(temp + 0.5);              // report in tenths of
        pressure =  bme.readPressure() / 100;     // Read Pressure in Pa and divide by 100 to convert to mBar
        ipressure = (int)(pressure * 10.0 + 0.5);      // report pressure in tenths of a mBar
        humidity = bme.readHumidity();

        Log.info("Temperature: %i F", temp);
        Log.info("iTemperature: %i F", itemp);
        Log.info("Pressure: %i mBar", pressure);
        Log.info("iPressure: %i mBar", ipressure);
        Log.info("Humidity: %i %", humidity);

    } else if (bmp280_present) {

        temp = (bmp.readTemperature() * 9/5) + 32;    // Read Temperature in c convert to f
        itemp = (int)(temp + 0.5);              // report in tenths of
        pressure =  bmp.readPressure() / 100;     // Read Pressure in Pa and divide by 100 to convert to mBar
        ipressure = (int)(pressure * 10.0 + 0.5);      // report pressure in tenths of a mBar

        Log.info("Temperature: %i F", temp);
        Log.info("iTemperature: %i F", itemp);
        Log.info("Pressure: %i mBar", pressure);
        Log.info("iPressure: %i mBar", ipressure);

    }

    voltage = get_battery_voltage();

    if (bme280_present) {
        sprintf(buf, "%02d%02d%03d%03d%05d%03d%02d%1d", wind_speed, wind_gust, wind_dir, itemp, ipressure, humidity, (int)voltage, first_event);
    } else if (bmp280_present) {
        sprintf(buf, "%02d%02d%03d%03d%05d%02d%1d", wind_speed, wind_gust, wind_dir, itemp, ipressure, (int)voltage, first_event);
    } else {
        sprintf(buf, "%02d%02d%03d%02d%1d", wind_speed, wind_gust, wind_dir, (int)voltage, first_event);
    }


    Log.info("Event: %s", EVENT_NAME);
    Log.info("Publish: %s", buf);
    Particle.publish(EVENT_NAME, buf, PRIVATE);

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
// This function reads the battery voltage on pin A5 and maps it to a
// scale of 0-99 (which maps to 5.0-14.9 volts).  That way we can post it with
// the rest of the event data as 2 digits.
// Note: The pin delivers a value of 0-4095 which represents 0 - 3.3V
//       But the input battery voltage goes through a voltage divider that
//       uses a 22K resistor over a 4.7K resistor.
// ============================================================================
float get_battery_voltage()
{
    int voltage;
    float batt_voltage;

    voltage = analogRead(A5);

    // divide by 4096 and multiply by 3.3 to get the voltage on the pin.
    // Then multiply by 5.68 to get the battery voltage rather than the
    // voltage coming out of the voltage divider.

    batt_voltage = (float)voltage / 218.52;
    Log.info("Batt: %d", batt_voltage);

    // We want to express the voltage as two digits so we'll subtract 5 volts and
    // then divide by 10.  This way 5-14.9 Volts becomes 0-99

    batt_voltage -= 5.0;
    if (batt_voltage < 0.0) batt_voltage = 0.0;

    batt_voltage *= 10;
    if (batt_voltage > 99) batt_voltage = 99;

    return(batt_voltage);
}

// ============================================================================
// Return 1 if we're currently observing Daylight Savings Time.
// I begin DST on March 12 and end it on November 5 (I think it always happens
// on a Sunday in real life, but this will have to do)
// ============================================================================
int is_dst()
{
    #if USE_DST
    int year = Time.year();
    int month = Time.month();
    int day = Time.day();

    // DST starts at 2:00 AM on the second Sunday in March
    int dstStart = (14 - (1 + year * 5 / 4) % 7);
    
    // DST ends at 2:00 AM on the first Sunday in November
    int dstEnd = (7 - (1 + year * 5 / 4) % 7);

    if (month > DST_START_MONTH && month < DST_END_MONTH) return 1;
    if (month == DST_START_MONTH && day >= dstStart) return 1;
    if (month == DST_END_MONTH && day < dstEnd) return 1;
    #endif

    return 0;
}

//****************************************************************************//
//
//  Utility functions
//
//****************************************************************************//

void setTimeZone() {
    int dst_status = is_dst();
    if (dst_status) Time.zone(TIMEZONE);
    else Time.zone(TIMEZONE - DST_OFFSET);
}

//  Remote Reset Function
int cloudResetFunction(String command) {
    resetFlag = true;
    resetSync = millis();
    return 0;
}

//****************************************************************************//
//  Serial connection banner -
//      Logs information about currently the running firmware on serial connect
//****************************************************************************//
void serialConnectBanner() {
    static int serial_state;

    // on serial connect we output the data on the running firmware
    if(Serial.isConnected() && serial_state == 0) {

        Log.info("Device ID: %s", (const char*)System.deviceID());
        Log.info("System Version: %s", System.version().c_str());
        if(bme280_present) Log.info("BME280_PRESENT: TRUE");
            else Log.info("BME280_PRESENT: FALSE");
        if(bmp280_present) Log.info("BMP280_PRESENT: TRUE");
            else Log.info("BMP280_PRESENT: FALSE");
        Log.info("EVENT_NAME: %s", EVENT_NAME);
        Log.info("UpTime: %d", System.uptime() / 60);

        serial_state = 1;
    }

    if(!Serial.isConnected()) {
        serial_state = 0;
    }
}