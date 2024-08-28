# RHC - Weather Station
An open source self contained weather station for remote applications where wifi and power are not available.

![Soboba Flight Park Weather Station](assets/soboba-weather-station_50.jpeg)
### Overview

The weather station is based around the Particle Cloud platform and the latest design uses the Particle.IO Boron LTE CAT-M1 development board for processing and connectivity. 
Additionaly a small arduino pro-mini is used as a watchdog to monitor system state and induce a restart if the the system is hung. This forces the Particle to reconnect and check in.

Wind observation are taken using a Davis Vantage Pro 6410 anemometer. Temperature, pressure, and humidify can be observed with a BPM280 or BME280 sensor.

Listener software running on a base unit (internet connected Particle Argon) or server application (nodejs) will monitor observations published to particle cloud from the weather station and post the parsed data to Weather Underground and Weather Flow. In the case of the server application we log obervations to a database that serves can be used to generate your own weather graphs without 3rd part services.

- [Recommended BOM](bom.md) 
- [Assembly instructions](assembly.md)
- [RHC-Weatherstation-Listener](https://github.com/GravityDeficient/RHC-Weatherstation-Listener) Reciever applictaion written in NodeJS
- [To-Do](TODO.md)

### Configuration
The weather station uses a configuration file system for easy customization:

1. Default settings are in `config.h`
2. An example configuration file `config.local.h.example` is provided
3. To customize settings:
   - Copy `config.local.h.example` to a new file named `config.local.h`
   - Edit `config.local.h` with your specific settings
   - Do not commit `config.local.h` to the repository (it's in .gitignore)

Example contents of `config.local.h`:

```c
#define EVENT_NAME ("my_custom_event")
#define TIMEZONE (-8)  // Pacific Standard Time
#define USE_DST (1)    // Enable Daylight Saving Time
#define DST_OFFSET (1) // 1 hour offset for DST
#define DST_START_MONTH (3)  // March
#define DST_START_WEEK (2)   // Second week
#define DST_START_DOW (0)    // Sunday
#define DST_END_MONTH (11)   // November
#define DST_END_WEEK (1)     // First week
#define DST_END_DOW (0)      // Sunday
#define CNTS_TO_MPH (2.174)  // Conversion factor for the Mussel Rock Anemometer