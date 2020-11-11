//===============================================================================================================
// This code for the Particle Electron publishes an event with wind data every 5 minutes 24 hrs/day.
// It publishes on each whole 5 minute interval (i.e. 10:00, 10:05; 10:10...).
// There is also be a Particle photon that subscribes to the published data events.  The Electron uses the
// cell phone network, so it's important to keep the data usage very low.  The photon can be on a wifi somewhere
// (anywhere really).  The photon get the published events, parses the terse data, forms the HTTP GET requests
// to update Weather Underground and/or WeatherFlow (aka iKitesurf), and submits those HTTP GET requests via wifi.
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
// Wiring to the BMP280
//   BMP280           Electron
//   ---------        ----------------
//   (1) Vcc            3.3V
//   (2) GND            GND
//   (3) SCL            D1  Don't forget 4.7K pull up resistors on both A4 and A5
//   (4) SDA            D0  Don't forget 4.7K pull up resistors on both A4 and A5
//   (5) CSB            3.3V to select I2C mode
//   (6) SDO            3.3V to select slave address 0x77
//
// Wiring to the MAX6675 temperature sensor (NO LONGER USED.  I now use the BMP280 for temperature and baro pressure)
//  - VCC to Electron 3.3V
//  - GND to Electron GND
//  - CS or SS to Electron A2 (chip-select / slave-select)
//  - SCK to Electron A3           (clock)
//  - SO or MISO to Electron A4    (master-in/slave-out)
//
// Wiring the Watchdog/Arduino Pro-Mini
//  - Electron A0 held high (3.3V) with a 20K resistor
//  - Electon B0 to Arduino D2.  This is the line that pings the Arduino to let it know all is well still.
//  - Arduino D3 to Electron Reset.  This resets the Electron if a ping isn't received for 20 minutes.
//
// Anemometer:
//   Red/Black: cup switch   (Red to GND)
//   Red/Yellow: the two ends of the direction pot (Yellow to Vcc; 3.3V)
//   Green: the wiper on the direction pot (Green to A1)
//   I currently have the Mussel Rock wind vane set up so that it reads 175.5 when it's exactly aligned with the arm
//     (i.e. arm pointing to the West and wind coming from the west.)
//   Mussel Rock anemometer: Speed in mph = 2.174 * counts/sec
//   Davis 6410 Anemometer (Joe's):  Speed in mph = 2.250 * counts/sec
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
// - I measured the current from the panel and found it was only giving me 14mA on an overcast day through the window (with the battery at 12.2V).
//   A bit of research suggests that solar panels produce only 10%-25% of their normal output on overcast days, and Low-E windows can reduce
//   their output by more than 50%.  I then opened the window (still overcast day) and got it up to 33mA.  By holding it outside the window
//   a bit it went up to 40mA (maybe it was in the shadow of the roof overhang).
//   When the sun peeked out for a few minutes I took additional readings: 98mA through the window and 160mA not looking through window.
//   More solar panel results: bright sunny winter day - 305mA through bedroom window; 430mA without window (with battery already at 3.8V).
//
// Battery voltage monitoring:
// - Use 22K and 4.7K resistors to make a voltage divider (12V will map to 2.11V);  22K on top (to +side of gel cell) and 4.7K on bottom (to Gnd).
//   Center of divider connects to input A0 of the Electron.
//   NOTE: the particular charge controller I got for Joe's wind station has positive common between the solar panel input, battery input, and load.
//         This would normally make it impossible to read battery voltage from an analog input on the Electron.  But as luck would have it it the
//         battery and USB output (that I use to power the Electron) share a common ground.  This makes it possible.  DO NOT do this if the
//         battery and Electron don't share a common ground.
//
// To-do:
// - Perhaps I should register a function using Particle.function() that allows me to change the update rate and start/stop times.
// - Figure out why I get a big jump in wind gust when I have a connection outage.
// - Encode mph with alpha-numerics and do fractions of MPH. Using upper-case letters and numbers we can encode 0-129 mph in 0.1 mph increments (using 2 chars).
//   If I used the printable ASCII characters (32-126) I could encode speed in 1/2 MPH increments with a single character (from 0 to 47 mph).
//   Two characters would allow me to describe direction to about 1/25th of a degree.
// - Maybe I should send previous data with each event.  This could allow me to fill in gaps, or to get greater granularity even if
//   I don't increase the update rate.
// - Figure out whether the failed updates are happening on the Electron or photon side (by watching the logs).
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
// - Figure out something about webhooks, REST, IFTTT
//
// rhc 2017-02-19
// rhc 2018-01-01   Added support for MAX6675 temperature sensor
// rhc 2018-01-10   Added code to have pin B0 ping the external watchdog
// rhc 2018-01-27   Got rid of the MAX6675 temperature sensor and replaced it with the BMP280 temperature and pressure sensor.
// see 2019-03-02   - Added variable for event name and first event name
//                  - Defined temperature and pressure to be zero when sensor is not present.
//                  - Moved BMP280 to its own .h library file.
//                  - Change serial.print to serial logging. With logging enabled we can see more details on the connection state.
//                  - Removed the delay 10000 in favor or a timer so we dont need to hold up the entire app. This allows logging to work even between measurements.
//===============================================================================================================
#include <math.h>
#include <stdlib.h>
#include "BMP280.h"

#define R2D (57.2958)
#define WIND_DIR_OFFSET (-94.5)

// -3- Pick one of these depending on the attached anemometer...
#define CNTS_TO_MPH (2.174)     // Speed in mph = 2.174 * counts/sec for anemometer used at Mussel Rock (per my own calibration)
// #define CNTS_TO_MPH (2.25)     // Speed in mph = 2.25 * counts/sec for Davis 6410 Anemometer

//-3- set this to 1 if this sensor has a BME280 temperature and pressure sensor
#define BME280_PRESENT (0)

//-3- set this to the event name you are listening for and the desired first event name (should be the same with an appended charachter)
#define EVENT_NAME ("WL3X")
#define FIRST_EVENT_NAME ("WL3XX")

//-3- Set the desired timezone
#define TIMEZONE (-7)

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
ApplicationWatchdog wd(3600000, System.reset);  // Set the watchdog to reboot the Electron if it doesn't get pinged for 1 hour

BME280 mySensor;

//@@ setup===============================================================================================================
//=======================================================================================================================
void setup()
{
    int i;
    int hr, min, sec;

    serialConnectBanner();

    //***Driver settings for BMP280 sensor **************//
    //specify chipSelectPin using arduino pin names
    //specify I2C address.  Can be 0x77(default) or 0x76
    mySensor.settings.I2CAddress = 0x77;

    //***Operation settings for BMP280 sensor ***********//
    mySensor.settings.runMode = 3; //  3, Normal mode
    mySensor.settings.tStandby = 0; //  0, 0.5ms
    mySensor.settings.filter = 0; //  0, filter off

    //tempOverSample can be:
    //  0, skipped
    //  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
    mySensor.settings.tempOverSample = 1;

    //pressOverSample can be:
    //  0, skipped
    //  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
    mySensor.settings.pressOverSample = 1;

    delay(10);  //Make sure sensor had enough time to turn on. BME280 requires 2ms to start up.

    //Calling .begin() causes the settings to be loaded
    mySensor.begin();

    led = 0;
    pinMode(A0, INPUT);  // This will monitor the gel-cell battery voltage.
    pinMode(A1, INPUT);  // Reads the wind direction pot.
    pinMode(B0, OUTPUT);  // This will ping the external watchdog
    pinMode(D7, OUTPUT);  // controls LED
    pinMode(D2, INPUT);   // This reads the input from the wind cups

    digitalWrite(B0, LOW);  // Trigger the external watchdog to let it know we're alive
    delay(1);
    digitalWrite(B0, HIGH);

    attachInterrupt(D2, click, RISING);      // Call click() when interrupt 0 (digital pin 2) rises (this is the wind cups spinning)

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
}

//@@ loop================================================================================================================
// We'll loop every 10 seconds and check the wind speed and direction.  We'll do this for 8 minutes, and then publish an
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

        dir = analogRead(A1);

        angle = 15.0 + (345.0/4095.0) * (float)(dir);  // NOTE: we use 345 rather than 360 because there's a roughly 15 degree dead-spot
        // Nominally the arm is supposed to be aimed true North, but there's a dead-spot about 0-15 deg.

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
        Log.info("Wind Counter: %d Dir: %d", wind_cntr, angle);

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

            digitalWrite(B0, LOW);  // Trigger the external watchdog to let it know we're still alive
            delay(1);
            digitalWrite(B0, HIGH);

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
            // digitalWrite(4, LOW);  // End the watchdog trigger pulse
        }
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
    float temperature, pressure;
    int itemp, ipressure, voltage;

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
    temperature = 0;
    ipressure = 0;
    itemp = 0;
    if (BME280_PRESENT)
    {
        temperature = mySensor.readTempF();
        pressure = mySensor.readFloatPressure() / 100;  // Read pressure in Pa and divide by 100 to convert to mBar
        ipressure = (int)(pressure * 10.0 + 0.5);  // resport pressure in tenths of a mBar
        itemp = (int)(temperature + 0.5);
    }
    voltage = get_battery_voltage();

    //-3- pick one of these depending on sensor
    sprintf(buf, "%02d%02d%03d%03d%05d%02d", wind_speed, wind_gust, wind_dir, itemp, ipressure, (int)voltage);  // This one is for Joe's Cayucos sensor
//    sprintf(buf, "%02d%02d%03d%02d", wind_speed, wind_gust, wind_dir, (int)voltage);  // this one is for the M.R. sensor (no temp./pressure sensor)

    // Particle.publish(String eventName, String data);
    // We give this event a unique name instead of naming it something like "wind" so we don't
    // end up reading everyone elses "wind" events.
    // -3- Ultimately I may publish these as private events so that only I can subscribe to them.

    // -3- pick a set of these depending on sensor
    // if (first_event) Particle.publish("JL3XX", buf);
    // else             Particle.publish("JL3X", buf);

    // if (first_event) Particle.publish("WL3XX", buf);
    // else             Particle.publish("WL3X", buf);

    // Updated to use new constants for
    if (first_event) {
        Log.info("Event: %s", FIRST_EVENT_NAME);
        Log.info("Publish: %s", buf);
        Particle.publish(FIRST_EVENT_NAME, buf);
    }
    else {
        Log.info("Event: %s", EVENT_NAME);
        Log.info("Publish: %s", buf);
        Particle.publish(EVENT_NAME, buf);
    }

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
// This function reads the battery voltage on pin A0 and maps it to a
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

    voltage = analogRead(A0);

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
    int iday, imonth;

    imonth = Time.month();  // returns 1 to 12
    iday = Time.day();      // returns 1 to 31

    if (imonth > 3   && imonth < 11) return(1);
    if (imonth == 3  && iday >= 12) return(1);
    if (imonth == 11 && iday <=5) return(1);
    return(0);
}





// @@ BMP280 Support functions  =======================================================================================================


//Constructor -- Specifies default configuration
BME280::BME280( void )
{
    //Construct with these default settings if nothing is specified

    //Select address for I2C.
    settings.I2CAddress = 0x77;

    settings.runMode = 0;
    settings.tempOverSample = 0;
    settings.pressOverSample = 0;
    settings.humidOverSample = 0;
}


//****************************************************************************//
//
//  Configuration section
//
//  This uses the stored SensorSettings to start the IMU
//  Use statements such as "mySensor.settings.commInterface = SPI_MODE;" to
//  configure before calling .begin();
//
//****************************************************************************//
uint8_t BME280::begin()
{
    //Check the settings structure values to determine how to setup the device
    uint8_t dataToWrite = 0;  //Temporary variable

    Wire.begin();

    //Reading all compensation data, range 0x88:A1, 0xE1:E7

    calibration.dig_T1 = ((uint16_t)((readRegister(BME280_DIG_T1_MSB_REG) << 8) + readRegister(BME280_DIG_T1_LSB_REG)));
    calibration.dig_T2 = ((int16_t)((readRegister(BME280_DIG_T2_MSB_REG) << 8) + readRegister(BME280_DIG_T2_LSB_REG)));
    calibration.dig_T3 = ((int16_t)((readRegister(BME280_DIG_T3_MSB_REG) << 8) + readRegister(BME280_DIG_T3_LSB_REG)));

    calibration.dig_P1 = ((uint16_t)((readRegister(BME280_DIG_P1_MSB_REG) << 8) + readRegister(BME280_DIG_P1_LSB_REG)));
    calibration.dig_P2 = ((int16_t)((readRegister(BME280_DIG_P2_MSB_REG) << 8) + readRegister(BME280_DIG_P2_LSB_REG)));
    calibration.dig_P3 = ((int16_t)((readRegister(BME280_DIG_P3_MSB_REG) << 8) + readRegister(BME280_DIG_P3_LSB_REG)));
    calibration.dig_P4 = ((int16_t)((readRegister(BME280_DIG_P4_MSB_REG) << 8) + readRegister(BME280_DIG_P4_LSB_REG)));
    calibration.dig_P5 = ((int16_t)((readRegister(BME280_DIG_P5_MSB_REG) << 8) + readRegister(BME280_DIG_P5_LSB_REG)));
    calibration.dig_P6 = ((int16_t)((readRegister(BME280_DIG_P6_MSB_REG) << 8) + readRegister(BME280_DIG_P6_LSB_REG)));
    calibration.dig_P7 = ((int16_t)((readRegister(BME280_DIG_P7_MSB_REG) << 8) + readRegister(BME280_DIG_P7_LSB_REG)));
    calibration.dig_P8 = ((int16_t)((readRegister(BME280_DIG_P8_MSB_REG) << 8) + readRegister(BME280_DIG_P8_LSB_REG)));
    calibration.dig_P9 = ((int16_t)((readRegister(BME280_DIG_P9_MSB_REG) << 8) + readRegister(BME280_DIG_P9_LSB_REG)));

    calibration.dig_H1 = ((uint8_t)(readRegister(BME280_DIG_H1_REG)));
    calibration.dig_H2 = ((int16_t)((readRegister(BME280_DIG_H2_MSB_REG) << 8) + readRegister(BME280_DIG_H2_LSB_REG)));
    calibration.dig_H3 = ((uint8_t)(readRegister(BME280_DIG_H3_REG)));
    calibration.dig_H4 = ((int16_t)((readRegister(BME280_DIG_H4_MSB_REG) << 4) + (readRegister(BME280_DIG_H4_LSB_REG) & 0x0F)));
    calibration.dig_H5 = ((int16_t)((readRegister(BME280_DIG_H5_MSB_REG) << 4) + ((readRegister(BME280_DIG_H4_LSB_REG) >> 4) & 0x0F)));
    calibration.dig_H6 = ((uint8_t)readRegister(BME280_DIG_H6_REG));

    //Set the oversampling control words.
    //config will only be writeable in sleep mode, so first insure that.
    writeRegister(BME280_CTRL_MEAS_REG, 0x00);

    //Set the config word
    dataToWrite = (settings.tStandby << 0x5) & 0xE0;
    dataToWrite |= (settings.filter << 0x02) & 0x1C;
    writeRegister(BME280_CONFIG_REG, dataToWrite);

    //Set ctrl_hum first, then ctrl_meas to activate ctrl_hum
    dataToWrite = settings.humidOverSample & 0x07; //all other bits can be ignored
    writeRegister(BME280_CTRL_HUMIDITY_REG, dataToWrite);

    //set ctrl_meas
    //First, set temp oversampling
    dataToWrite = (settings.tempOverSample << 0x5) & 0xE0;
    //Next, pressure oversampling
    dataToWrite |= (settings.pressOverSample << 0x02) & 0x1C;
    //Last, set mode
    dataToWrite |= (settings.runMode) & 0x03;
    //Load the byte
    writeRegister(BME280_CTRL_MEAS_REG, dataToWrite);

    return readRegister(0xD0);
}

//*****************************************
//Strictly resets.  Run .begin() afterwards
//*****************************************
void BME280::reset( void )
{
    writeRegister(BME280_RST_REG, 0xB6);
}

//********************************************************************************
// Read barometric pressure in Pa (100 Pa = 1 mBar)
// Output range is 30000 - 110000 Pa (about +9000 to -500 m above/below sea level)
//********************************************************************************
float BME280::readFloatPressure( void )
{
    // Get pressure in Pa as unsigned 32 bit integer in Q24.8 format (24 integer bits and 8 fractional bits).
    // Output value of �24674867� represents 24674867/256 = 96386.2 Pa = 963.862 hPa
    int32_t adc_P = ((uint32_t)readRegister(BME280_PRESSURE_MSB_REG) << 12) | ((uint32_t)readRegister(BME280_PRESSURE_LSB_REG) << 4) | ((readRegister(BME280_PRESSURE_XLSB_REG) >> 4) & 0x0F);

    int64_t var1, var2, p_acc;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calibration.dig_P6;
    var2 = var2 + ((var1 * (int64_t)calibration.dig_P5)<<17);
    var2 = var2 + (((int64_t)calibration.dig_P4)<<35);
    var1 = ((var1 * var1 * (int64_t)calibration.dig_P3)>>8) + ((var1 * (int64_t)calibration.dig_P2)<<12);
    var1 = (((((int64_t)1)<<47)+var1))*((int64_t)calibration.dig_P1)>>33;
    if (var1 == 0)  return 0; // avoid exception caused by division by zero
    p_acc = 1048576 - adc_P;
    p_acc = (((p_acc<<31) - var2)*3125)/var1;
    var1 = (((int64_t)calibration.dig_P9) * (p_acc>>13) * (p_acc>>13)) >> 25;
    var2 = (((int64_t)calibration.dig_P8) * p_acc) >> 19;
    p_acc = ((p_acc + var1 + var2) >> 8) + (((int64_t)calibration.dig_P7)<<4);

    return (float)p_acc / 256.0;
}

//*************************
// Read Temperature
//*************************
float BME280::readTempF( void )
{
    float outputF, outputC;

    // Returns temperature in DegF, resolution is 0.01 DegC. OutputC value of �5123� equals 51.23 DegC.
    // t_fine carries fine temperature as global value

    //get the reading (adc_T);
    int32_t adc_T = ((uint32_t)readRegister(BME280_TEMPERATURE_MSB_REG) << 12) | ((uint32_t)readRegister(BME280_TEMPERATURE_LSB_REG) << 4) | ((readRegister(BME280_TEMPERATURE_XLSB_REG) >> 4) & 0x0F);

    //By datasheet, calibrate
    int64_t var1, var2;

    var1 = ((((adc_T>>3) - ((int32_t)calibration.dig_T1<<1))) * ((int32_t)calibration.dig_T2)) >> 11;
    var2 = (((((adc_T>>4) - ((int32_t)calibration.dig_T1)) * ((adc_T>>4) - ((int32_t)calibration.dig_T1))) >> 12) *
            ((int32_t)calibration.dig_T3)) >> 14;
    t_fine = var1 + var2;
    outputC = (t_fine * 5 + 128) >> 8;

    outputC = outputC / 100;

    outputF = (outputC * 9) / 5 + 32;  // Convert from deg-C to deg-F

    return outputF;
}

//****************************************************************************//
//
//  Utility functions
//
//****************************************************************************//

void BME280::readRegisterRegion(uint8_t *outputPointer , uint8_t offset, uint8_t length)
{
    //define pointer that will point to the external space
    uint8_t i = 0;
    char c = 0;

    Wire.beginTransmission(settings.I2CAddress);
    Wire.write(offset);
    Wire.endTransmission();

    // request bytes from slave device
    Wire.requestFrom(settings.I2CAddress, length);
    while ( (Wire.available()) && (i < length))  // slave may send less than requested
    {
        c = Wire.read(); // receive a byte as character
        *outputPointer = c;
        outputPointer++;
        i++;
    }
}

uint8_t BME280::readRegister(uint8_t offset)
{
    //Return value
    uint8_t result;
    uint8_t numBytes = 1;

    Wire.beginTransmission(settings.I2CAddress);
    Wire.write(offset);
    Wire.endTransmission();

    Wire.requestFrom(settings.I2CAddress, numBytes);
    while ( Wire.available() ) // slave may send less than requested
    {
        result = Wire.read(); // receive a byte as a proper uint8_t
    }

    return result;
}

int16_t BME280::readRegisterInt16( uint8_t offset )
{
    uint8_t myBuffer[2];
    readRegisterRegion(myBuffer, offset, 2);  //Does memory transfer
    int16_t output = (int16_t)myBuffer[0] | int16_t(myBuffer[1] << 8);

    return output;
}


void BME280::writeRegister(uint8_t offset, uint8_t dataToWrite)
{
    //Write the byte
    Wire.beginTransmission(settings.I2CAddress);
    Wire.write(offset);
    Wire.write(dataToWrite);
    Wire.endTransmission();
}

void setTimeZone() {
    int dst_status;
    // Set time zone (-7.0 during DST; -8.0 otherwise)
    dst_status = is_dst();
    if (dst_status) Time.zone(TIMEZONE);  // Set for Pacific daylight time.
    else            Time.zone(TIMEZONE - 1);  // Set for Pacific standard time.
}

//****************************************************************************//
//  Serial connection banner -
//      Logs information about currently the running firmware on serial connect
//****************************************************************************//
void serialConnectBanner() {
    static int serial_state;

    // on serial connect we output the data on the running firmware
    if(Serial.isConnected() && serial_state == 0) {

        Log.info("DeviceID: %s", (const char*)System.deviceID());
        Log.info("System Version: %s", System.version().c_str());
        Log.info("BME280_PRESENT: %s", BME280_PRESENT);
        Log.info("EVENT_NAME: %s", EVENT_NAME);
        Log.info("FIRST_EVENT_NAME: %s", FIRST_EVENT_NAME);
        Log.info("UpTime: %d", System.uptime() / 60);

        serial_state = 1;
    }

    if(!Serial.isConnected()) {
        serial_state = 0;
    }
}