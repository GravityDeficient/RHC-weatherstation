// ---------------------------------------------------------------------------------
// This app runs on the Particle IO photon.  It will:
// - Subscribe to the events being published by Particle IO Electrons that
//   read the wind sensors.
// - Parse the data from those events.
// - Form the HTTP GET request from the parsed data for both Weather Underground
//    and WeatherFlow (aka iKitesurf)
// - Send the HTTP GET request to both Weather Underground and/or WeatherFlow
//   (iKitesurf) over wifi (where we don't care about minimizing bandwidth).
//   It will repeat each request up to three times to get a successful response.
// - Log all events to a local SD card.
//
//  RHC 2017-02-17   (Last update: 2018-03-18)
// ---------------------------------------------------------------------------------
#include <HttpClient.h>

int wind_dir = 0;
int wind_speed = 0;
int wind_gust = 10;

int loop_counter = 0;

String good_ikitesurf_response = "{\"status_code\":0,\"status_message\":\"Success\"}";
String good_wunderground_response = "success\n";

HttpClient http;

http_header_t headers[] =
        {
                //  { "Content-Type", "application/json" },
                //  { "Accept" , "application/json" },
                { "Accept" , "*/*"},
                { NULL, NULL } // NOTE: Always terminate headers with NULL
        };

http_request_t request;
http_response_t response;





int mySerial(char *str, int linefeed)
{
    if (linefeed)
    {
        Serial.println(str);
        Serial1.println(str);
    }
    else
    {
        Serial.print(str);
        Serial1.print(str);
    }
    return(0);
}




// --------------------------------------------------------------------------------------------------
// This routine is called whenever a "WL3X" event comes through.  The event will have a 9 character
// data string that gives wind speed, gust speed, wind direction, and battery voltage
// Sample data string:  121821376
// First two digits are wind speed: 12 mph
// Second two digits are gust speed: 18 mph
// Next three digits give wind direction: 213 degrees.
// Last two digits give battery voltage: 7.6V (but we add 5V - so this really means 12.6V)
// --------------------------------------------------------------------------------------------------
void myHandler(const char *event, const char *data)
{
    int it;
    char wind[8], gust[8], dir[8], volts[8];
    int w, g, d, v;
    char buf[300];
    int year, month, day, hr, min, sec;  // -3- remember to sync time once/day
    float voltage;

    if (strlen(event) >= 5)  // The electron appends an "X" to the end of the event name for the first event after startup
    {
        mySerial(" ", 1);
        mySerial("--- Rebooted ---", 1);
    }

    mySerial(" ", 1);
    mySerial("Event: ", 0);
    Serial.print(event);
    Serial1.print(event);
    mySerial(", data: ", 0);
    if (data)
    {
        Serial.println(data);
        Serial1.println(data);
        sprintf(wind, "%s", data);
        wind[2] = '\0';
        sprintf(gust, "%s", &(data[2]));
        gust[2] = '\0';
        sprintf(dir, "%s", &(data[4]));
        dir[3] = '\0';
        sprintf(volts, "%s", &(data[7]));
        volts[2] = '\0';
        w = atoi(wind);
        g = atoi(gust);
        d = atoi(dir);
        v = atoi(volts);
        voltage = 5.0 + ((float)v)/10.0;
        // Add 20 degrees to wind direction to account for sensor alignment
        d += 20;
        if (d > 360) d -= 360;

        mySerial("Wind: ", 0);
        Serial.print(w);
        Serial1.print(w);
        mySerial(" Gust: ", 0);
        Serial.print(g);
        Serial1.print(g);
        mySerial(" Dir: ", 0);
        Serial.print(d);
        Serial1.print(d);
        mySerial(" Volts: ", 0);
        Serial.println(voltage);
        Serial1.println(voltage);

        year = Time.year();
        month = Time.month();
        day = Time.day();
        hr = Time.hour();
        min = Time.minute();
        sec = Time.second();

        mySerial("Date: ", 0);
        Serial.print(year);
        Serial1.print(year);
        mySerial("-", 0);
        Serial.print(month);
        Serial1.print(month);
        mySerial("-", 0);
        Serial.print(day);
        Serial1.print(day);
        mySerial("    Time: ", 0);
        Serial.print(hr);
        Serial1.print(hr);
        mySerial(":", 0);
        Serial.print(min);
        Serial1.print(min);
        mySerial(":", 0);
        Serial.println(sec);
        Serial1.println(sec);
        mySerial(" ", 1);

        // Form Weather Underground HTTP GET request for Mussel Rock page
        request.hostname = "weatherstation.wunderground.com";
        request.port = 80;
        sprintf(buf, "/weatherstation/updateweatherstation.php?ID=KCADALYC8&PASSWORD=password&dateutc=now&winddir=%d&windspeedmph=%d&windgustmph=%d&softwaretype=vwsversionxx&action=updateraw HTTP/1.1", d, w, g);
        request.path = buf;
        Serial.println(request.path);
        Serial1.println(request.path);
        for(it=0; it<3; it++) // submit the HTTP GET request up to 3 times if necessary.
        {
            http.get(request, response, headers);
            mySerial("Wunderground Response: ", 0);
            Serial.println(response.body);
            Serial1.println(response.body);
            if (response.body.equals(good_wunderground_response)) break;
        }

        // Form Weatherflow (aka ikitesurf) HTTP GET request
        request.hostname = "pws.weatherflow.com";
        request.port = 80;
        sprintf(buf, "/wxengine/rest/collection/weatherstation?id=V82ZENrXI&timetamp_utc=%d-%02d-%02d%c%c%c%02d:%02d:%02d&wind_dir=%d&wind_gust=%d&wind_avg=%d", year, month, day, 37, 50, 48, hr, min, sec, d, g, w);
        // Note that we replaced the space between the date and the time with a "%20"  This is the "37, 50, 48" in the above sprintf. .  It does not work if we leave that as a space.
        request.path = buf;
        Serial.println(request.path);
        Serial1.println(request.path);
        for(it=0; it<3; it++) // submit the HTTP GET request up to 3 times if necessary.
        {
            http.get(request, response, headers);
            mySerial("ikitesurf Response: ", 0);
            Serial.println(response.body);
            Serial1.println(response.body);
            if (response.body.equals(good_ikitesurf_response)) break;
        }
        mySerial(" ", 1);
    }
    else
    {
        mySerial("NULL", 1);
    }
}





// --------------------------------------------------------------------------------------------------
// This routine is called whenever a "JL3X" event comes through.  The event will have a 7 character
// data string (-3- plus temperature) that gives wind speed, gust speed, and wind direction.
// Sample data string:  1218213
// First two digits are wind speed: 12 mph
// Second two digits are gust speed: 18 mph
// Last three digits give wind direction: 213 degrees.
//
// The data packet published by the Electon looks like this...
//
//  %02d  wind_speed
//  %02d  wind_gust
//  %03d  wind_dir,
//  %03d  temperature deg-F
//  %05d  pressure in mbar * 10
//  %02d  voltage (in tenths of a volt over 5 volts)
//
//  Example:  00 00 114 077 10274 74 (I added the spaces.  They do not exist in the event)
//
// --------------------------------------------------------------------------------------------------
void JoesHandler(const char *event, const char *data)
{
    int it;
    char wind[20], gust[20], dir[20], temperature[20], pressure[20], voltage[20];
    int w, g, d, t;
    char buf[300];
    int year, month, day, hr, min, sec;  // -3- remember to sync time once/day
    float p; // pressure in mbar
    float press_in; // pressure in inches of mercury
    float volts;

    if (strlen(event) >= 5)  // The electron appends an "X" to the end of the event name for the first event after startup
    {
        mySerial(" ", 1);
        mySerial("--- Rebooted ---", 1);
    }

    mySerial(" ", 1);
    mySerial("Event: ", 0);
    Serial.print(event);
    Serial1.print(event);
    mySerial(", data: ", 0);
    if (data)
    {
        Serial.println(data);
        Serial1.println(data);
        sprintf(wind, "%s", data);
        wind[2] = '\0';
        sprintf(gust, "%s", &(data[2]));
        gust[2] = '\0';
        sprintf(dir, "%s", &(data[4]));
        dir[3] = '\0';
        sprintf(temperature, "%s", &(data[7]));
        temperature[3] = '\0';
        sprintf(pressure, "%s", &(data[10]));
        pressure[5] = '\0';
        w = atoi(wind);
        g = atoi(gust);
        d = atoi(dir);
        t = atoi(temperature);
        p = atof(pressure) / 10.0;  // Pressure is reported in tenths of a mBar.  So we convert to mBar here.
        press_in = p * 0.02952999;  // convert mBar to inches of mercury
        sprintf(voltage, "%s", &(data[15]));
        voltage[2] = '\0';
        volts = 5.0 + ((float)atoi(voltage) ) / 10.0;

        // -3-  Add 20 degrees to wind direction to account for sensor alignment
        d += 20;
        if (d > 360) d -= 360;

        mySerial("Wind: ", 0);
        Serial.print(w);
        Serial1.print(w);
        mySerial(" Gust: ", 0);
        Serial.print(g);
        Serial1.print(g);
        mySerial(" Dir: ", 0);
        Serial.print(d);
        Serial1.print(d);
        mySerial(" Temperature: ", 0);
        Serial.print(t);
        Serial1.print(t);
        mySerial(" Pressure: ", 0);
        Serial.print(p);
        Serial1.print(p);
        mySerial(" Battery: ", 0);
        Serial.println(volts);
        Serial1.println(volts);

        year = Time.year();
        month = Time.month();
        day = Time.day();
        hr = Time.hour();
        min = Time.minute();
        sec = Time.second();

        mySerial("Date: ", 0);
        Serial.print(year);
        Serial1.print(year);
        mySerial("-", 0);
        Serial.print(month);
        Serial1.print(month);
        mySerial("-", 0);
        Serial.print(day);
        Serial1.print(day);
        mySerial("    Time: ", 0);
        Serial.print(hr);
        Serial1.print(hr);
        mySerial(":", 0);
        Serial.print(min);
        Serial1.print(min);
        mySerial(":", 0);
        Serial.println(sec);
        Serial1.println(sec);
        mySerial(" ", 1);

        // Form Weather Underground HTTP GET request for Joe's page (this is just for testing purposes)
        request.hostname = "weatherstation.wunderground.com";
        request.port = 80;
        sprintf(buf, "/weatherstation/updateweatherstation.php?ID=KCACAYUC16&PASSWORD=7gbu4tfb&dateutc=now&winddir=%d&windspeedmph=%d&windgustmph=%d&tempf=%d&baromin=%f&softwaretype=vwsversionxx&action=updateraw HTTP/1.1", d, w, g, t, press_in);
        request.path = buf;
        Serial.println(request.path);
        Serial1.println(request.path);
        for(it=0; it<3; it++) // submit the HTTP GET request up to 3 times if necessary.
        {
            http.get(request, response, headers);
            mySerial("Wunderground Response: ", 0);
            Serial.println(response.body);
            Serial1.println(response.body);
            if (response.body.equals(good_wunderground_response)) break;
        }

        // Form Weatherflow (aka ikitesurf) HTTP GET request
        request.hostname = "pws.weatherflow.com";
        request.port = 80;

        sprintf(buf, "/wxengine/rest/collection/weatherstation?id=Ig7SxBXib&timetamp_utc=%d-%02d-%02d%c%c%c%02d:%02d:%02d&pressure=%f&air_temp=%d&wind_dir=%d&wind_gust=%d&wind_avg=%d", year, month, day, 37, 50, 48, hr, min, sec, p, t, d, g, w);
        // Note that we replaced the space between the date and the time with a "%20"  This is the "37, 50, 48" in the above sprintf. .  It does not work if we leave that as a space.
        request.path = buf;
        Serial.println(request.path);
        Serial1.println(request.path);
        for(it=0; it<3; it++) // submit the HTTP GET request up to 3 times if necessary.
        {
            http.get(request, response, headers);
            mySerial("ikitesurf Response: ", 0);
            Serial.println(response.body);
            Serial1.println(response.body);
            if (response.body.equals(good_ikitesurf_response)) break;
        }
        mySerial(" ", 1);
    }
    else
    {
        mySerial("NULL", 1);
    }
}







void setup()
{
    int i;
    char data[5];
    float wf, gf, dirf;

    Serial.begin(9600);    // This channel communicates through the USB port and when connected to a computer, will show up as a virtual COM port.
    Serial1.begin(9600);   // This channel is available via the device's TX and RX pins.

    Time.zone(0);  // Weatherflow wants time in UTC

    pinMode(7, OUTPUT);

    // -3- we may need to test and see if we're getting events, and re-subscribe if we're not.
    Particle.subscribe("WL3X", myHandler, "290030000c47343233323032");   // Mussel Rock wind station
    Particle.subscribe("JL3X", JoesHandler, "290030000c47343233323032");   // Joe Seitz' wind station
}




void loop()
{
    char buf[300];
    char unparsed_data[20];

    delay(60000);

    loop_counter++;
    mySerial("Still alive: ", 0);
    Serial.println(loop_counter);
    Serial1.println(loop_counter);

    if ((loop_counter%2)) digitalWrite(7, HIGH);
    else                  digitalWrite(7, LOW);
}
