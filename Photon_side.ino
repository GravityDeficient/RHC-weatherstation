// ---------------------------------------------------------------------------------
// This app will run on the Particle IO Photon.  It will:
// - Subscribe to the events being published by the Particle IO Electron that
//   reads the wind sensor
// - Parse the data from those events.
// - Form the HTTP GET request from the parsed data for both Weather Underground
//    and WeatherFlow (aka iKitesurf)
// - Send the HTTP GET request to both Weather Underground and/or WeatherFlow
//   (iKitesurf) over wifi (where we don't care about minimizing bandwidth).
//   It will repeat each request up to three times to get a successful response.
//
//  RHC 2017-02-17
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




//------------------------------------------------------------------------------------------------
// NOTE: This function has not yet been implemented.  It will be used if I decide I want to post
//       fractional mph and encode the data with the full set of printable ASCII characters.
//------------------------------------------------------------------------------------------------
int encode_data(char *data, float wf, float gf, float dirf)
{
    char wind_char, gust_char, dir_char_1, dir_char_2;
    float wind_speed, gust_speed, direction;


    wind_char = (char)((wf*2.0)+32);
    gust_char = (char)(gf) + 32;

    dir_char_1 = (int)((dirf*10.0) / 95.0) + 32;
    dir_char_2 = ((int)(dirf*10) % 95) + 32;

    data[0] = wind_char;
    data[1] = gust_char;
    data[2] = dir_char_1;
    data[3] = dir_char_2;
    data[4] = '\0';

    return(0);
}


//--------------------------------------------------------------------------------
// This function decodes an event string sent by the Electron.  The string
// is 4 characters long.  Each character is a printable ASCII character between
// 32 and 126 (inclusive).
// The first character gives wind speed in mph.  Each ASCII value represents
// 1/2 mph.  So we can encode 0 to 47 mph with this one character.
// The second character gives the gust speed.  But in this case each increment
// represents 1 mph.  So we can encode gusts of 0-94 mph.
// The last 2 characters indicate the wind direction from 0 to 360 degrees.
// Each incremental value represents 0.1 degrees.
//
// NOTE: This function has not yet been implemented.  It will be used if I decide I want to post
//       fractional mph and encode the data with the full set of printable ASCII characters.
//--------------------------------------------------------------------------------
int decode_data(const char *data, float *wf, float *gf, float *dirf)
{
    char wind_char, gust_char, dir_char_1, dir_char_2;
    float wind_speed, gust_speed, direction;

    wind_char = data[0];
    gust_char = data[1];
    dir_char_1 = data[2];
    dir_char_2 = data[3];

    wind_speed = (int)(wind_char) - 32;
    *wf = (float)wind_speed / 2.0;

    gust_speed = (int)(gust_char) - 32;
    *gf = (float)gust_speed;

    direction = ((int)(dir_char_1) - 32) * 95 + ((int)(dir_char_2) - 32);
    *dirf = (float)direction / 10.0;

    return(0);
}



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
// This routine is called whenever a "WL3X" event comes through.  The event will have a 7 character
// data string that gives wind speed, gust speed, and wind direction.
// Sample data string:  1218213
// First two digits are wind speed: 12 mph
// Second two digits are gust speed: 18 mph
// Last three digits give wind direction: 213 degrees.
// --------------------------------------------------------------------------------------------------
void myHandler(const char *event, const char *data)
{
    int it;
    char wind[8], gust[8], dir[8];
    int w, g, d;
    char buf[300];
    int year, month, day, hr, min, sec;  // -3- remember to sync time once/day

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
        w = atoi(wind);
        g = atoi(gust);
        d = atoi(dir);
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
        Serial.println(d);
        Serial1.println(d);

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

        // Form Weather Underground HTTP GET request
        request.hostname = "weatherstation.wunderground.com";
        request.port = 80;
        sprintf(buf, "/weatherstation/updateweatherstation.php?ID=KCADALYC8&PASSWORD=webcampass&dateutc=now&winddir=%d&windspeedmph=%d&windgustmph=%d&softwaretype=vwsversionxx&action=updateraw HTTP/1.1", d, w, g);
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
        // Note that we replaced the space between the date and the time with a "%20".  It does not work if we leave that as a space.
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
// This routine is called whenever a "ZL3X" event comes through.  The event will have a 4 character
// data string that gives wind speed, gust speed, and wind direction.
// Sample data string:  gk
// First digit gives wind speed
// Second digit gives gust speed
// last three digits give wind direction: 213 degrees.
//
// NOTE: This function has not yet been implemented.  It will be used if I decide I want to post
//       fractional mph and encode the data with the full set of printable ASCII characters.
// --------------------------------------------------------------------------------------------------
void altHandler(const char *event, const char *data)
{
    int it;
    float wf, gf, dirf;
    int w, g, d;
    char buf[300];
    int year, month, day, hr, min, sec;  // -3- remember to sync time once/day

    mySerial(" ", 1);
    mySerial("Event: ", 0);
    Serial.print(event);
    Serial1.print(event);
    mySerial(", data: ", 0);
    if (data)
    {
        Serial.println(data);
        Serial1.println(data);
        decode_data(data, &wf, &gf, &dirf);

        mySerial("Wind: ", 0);
        Serial.print(wf);
        Serial1.print(wf);
        mySerial(" Gust: ", 0);
        Serial.print(gf);
        Serial1.print(gf);
        mySerial(" Dir: ", 0);
        Serial.println(dirf);
        Serial1.println(dirf);

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

        // Form Weather Underground HTTP GET request
        request.hostname = "weatherstation.wunderground.com";
        request.port = 80;
        sprintf(buf, "/weatherstation/updateweatherstation.php?ID=KCADALYC8&PASSWORD=password&dateutc=now&winddir=%d&windspeedmph=%04.1f&windgustmph=%04.1f&softwaretype=vwsversionxx&action=updateraw HTTP/1.1", (int)dirf, wf, gf);
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
        sprintf(buf, "/wxengine/rest/collection/weatherstation?id=V82ZENrXI&timetamp_utc=%d-%02d-%02d%c%c%c%02d:%02d:%02d&wind_dir=%d&wind_gust=%04.1f&wind_avg=%04.1f", year, month, day, 37, 50, 48, hr, min, sec, (int)dirf, gf, wf);
        // Note that we replaced the space between the date and the time with a "%20".  It does not work if we leave that as a space.
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






// This function exists for development/debugging only.
void update_weatherflow(int d, int g, int w)
{
    char buf[300];
    int year, month, day, hr, min, sec;

    // Form Weather Underground HTTP GET request
    request.hostname = "weatherstation.wunderground.com";
    request.port = 80;
    sprintf(buf, "/weatherstation/updateweatherstation.php?ID=KCADALYC8&PASSWORD=webcampass&dateutc=now&winddir=%d&windspeedmph=%d&windgustmph=%d&softwaretype=vwsversionxx&action=updateraw HTTP/1.1", d, w, g);
    request.path = buf;
    Serial.println(request.path);
    http.get(request, response, headers);
    Serial.print("Wunderground Response: ");
    Serial.println(response.body);

    // Form Weatherflow (aka ikitesurf) HTTP GET request
    year = Time.year();
    month = Time.month();
    day = Time.day();
    hr = Time.hour();
    min = Time.minute();
    sec = Time.second();

    request.hostname = "pws.weatherflow.com";
    request.port = 80;
    sprintf(buf, "/wxengine/rest/collection/weatherstation?id=V82ZENrXI&timetamp_utc=%d-%02d-%02d%c%c%c%02d:%02d:%02d&wind_dir=%d&wind_gust=%d&wind_avg=%d", year, month, day, 37, 50, 48, hr, min, sec, d, g, w);
    // Note that we replaced the space between the date and the time with a "%20".  It does not work if we leave that as a space.
    request.path = buf;
    Serial.println(request.path);
    http.get(request, response, headers);
    Serial.print("ikitesurf Response: ");
    Serial.println(response.body);
}





void setup()
{
    int i;
    char data[5];
    float wf, gf, dirf;

    Serial.begin(9600);
    Serial1.begin(9600);

    Time.zone(0);  // Weatherflow wants time in UTC

    pinMode(7, OUTPUT);

    /*  -3- For debug only
    delay(5000);

    for(i=0; i<6; i++)
       {
       wf = (float)i/2.0;
       gf = (float)(i);
       dirf = 10.1*(float)i;
       Serial.println(i);
       Serial.println(wf);
       Serial.println(gf);
       Serial.println(dirf);
       encode_data(data, wf, gf, dirf);
              Serial.print("[");
              Serial.print(data);
              Serial.println("]");
       decode_data(data, &wf, &gf, &dirf);
       Serial.println(wf);
       Serial.println(gf);
       Serial.println(dirf);
       Serial.println(" ");
       Serial.println(" ");
       }

    altHandler("ZL3X", (const char *)data);
    */


    // -3- we may need to test and see if we're getting events, and re-subscribe if we're not.
    Particle.subscribe("WL3X", myHandler, "290030000c47343233323032");
    // Particle.subscribe("ZL3X", altHandler, "290030000c47343233323032");   // This one encodes wind speed and direction with printable ascii alpha-numerics
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

    // -3- DEBUG ONLY:  if ((loop_counter%5)==0) myHandler("WL3X", "0310217");

    /*-3- debug only
    wind_dir += 1;
    wind_speed += 1;
    wind_gust += 1;
    update_weatherflow(wind_dir, wind_gust, wind_speed);
    */
}
