# RHC - Weather Station
An open source self contained weather station for remote applications where wifi and power are not available.

![Soboba Flight Park Weather Station](assets/soboba-weather-station_50.jpeg)
### Overview

The weather station is based around the Particle Cloud platform and the latest design uses the Particle.IO Boron LTE CAT-M1 development board for processing and connectivity. 
Additionally a small arduino pro-mini is used as a watchdog to monitor system state and induce a restart if the system is hung. This forces the Particle to reconnect and check in.

Wind observation are taken using a Davis Vantage Pro 6410 anemometer. Temperature, pressure, and humidify can be observed with a BPM280 or BME280 sensor.

Listener software running on a base unit (internet connected Particle Argon) or server application (nodejs) will monitor observations published to particle cloud from the weather station and post the parsed data to Weather Underground and Weather Flow. In the case of the server application we log obervations to a database that serves can be used to generate your own weather graphs without 3rd part services.

- [Recommended BOM](bom.md) 
- [Assembly instructions](assembly.md)
- [RHC-Weatherstation-Listener](https://github.com/GravityDeficient/RHC-Weatherstation-Listener) Reciever applictaion written in NodeJS
- [To-Do](TODO.md)

### Configuration
The weather station uses a simple configuration system that's easy to customize:

1. All configuration settings are in a single file: `config.h`
2. If you're setting up a new installation, copy `config.h.example` to `config.h`
3. Edit the settings in the clearly marked "USER CONFIGURATION" section
4. Make your changes directly in this file - the comments provide guidance on what each setting does
5. Note that `config.h` is in .gitignore, so your custom settings won't be committed

Key settings you might want to customize:
- EVENT_NAME: The name used for publishing data to the Particle Cloud 
- TIMEZONE: Set to your local timezone
- USE_DST: Enable or disable Daylight Saving Time
- CNTS_TO_MPH: Calibration factor for your specific anemometer

Example of the config.h settings:

```c
// Event name for publishing data
// Change this to your own custom event name
#define EVENT_NAME ("my_custom_event")

// Timezone settings
// Modify for your location
#define TIMEZONE (-8)  // Pacific Standard Time

// Daylight Saving Time settings
#define USE_DST (1)    // Enable Daylight Saving Time
#define DST_OFFSET (1) // 1 hour offset for DST

// DST start rule (second Sunday in March for USA)
#define DST_START_MONTH (3)  // March
#define DST_START_WEEK (2)   // Second week
#define DST_START_DOW (0)    // Sunday (0 = Sunday, 1 = Monday, etc.)

// DST end rule (first Sunday in November for USA)
#define DST_END_MONTH (11)   // November
#define DST_END_WEEK (1)     // First week
#define DST_END_DOW (0)      // Sunday

// Wind speed calibration
// Change this for your specific anemometer
#define CNTS_TO_MPH (2.174)  // For Mussel Rock Anemometer
```

After changing the configuration, compile and flash your device to apply the changes.

### Important Notes for Compilation

- **Pro Mini Watchdog**: Before compiling the Boron firmware, you must remove the `station/pro-mini_watchdog.ino` file. This file is for the Arduino Pro Mini watchdog device and should not be included in the Boron compilation.
