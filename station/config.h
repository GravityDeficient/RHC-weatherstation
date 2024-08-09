#ifndef CONFIG_H
#define CONFIG_H

// Event name for publishing data
#define EVENT_NAME ("obs")

// Timezone and DST settings for California
#define TIMEZONE (-7)  // Pacific Daylight Time
#define USE_DST (1)    // Use Daylight Saving Time
#define DST_OFFSET (1) // 1 hour offset for DST

// DST dates for USA
// Starts second Sunday in March
#define DST_START_MONTH (3)
#define DST_START_DAY (8)  // This is approximate, we'll use 8 as it's always after this date
// Ends first Sunday in November
#define DST_END_MONTH (11)
#define DST_END_DAY (1)    // This is approximate, we'll use 1 as it's always after this date

// Default conversion factor from counts to mph for wind speed
#define CNTS_TO_MPH (2.25)  // Default for Davis 6410 Anemometer

#endif // CONFIG_H
