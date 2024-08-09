#ifndef CONFIG_H
#define CONFIG_H

// Event name for publishing data
#define EVENT_NAME ("obs")

// Timezone and DST settings for California
#define TIMEZONE (-7)  // Pacific Daylight Time
#define USE_DST (1)    // Use Daylight Saving Time
#define DST_OFFSET (1) // 1 hour offset for DST

// DST start rule (second Sunday in March)
#define DST_START_MONTH (3)  // March
#define DST_START_WEEK (2)   // Second week
#define DST_START_DOW (0)    // Sunday (0 = Sunday, 1 = Monday, etc.)

// DST end rule (first Sunday in November)
#define DST_END_MONTH (11)   // November
#define DST_END_WEEK (1)     // First week
#define DST_END_DOW (0)      // Sunday

// Default conversion factor from counts to mph for wind speed
#define CNTS_TO_MPH (2.25)  // Default for Davis 6410 Anemometer

#endif // CONFIG_H