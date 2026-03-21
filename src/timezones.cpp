#include "timezones.h"

// POSIX TZ format: STDoffsetDST[offset],start[/time],end[/time]
// Example: "CET-1CEST,M3.5.0/02:00,M10.5.0/03:00"
//   CET-1    = standard time name + offset (sign inverted vs UTC)
//   CEST     = daylight time name
//   M3.5.0   = March, last (5) Sunday (0)
//   /02:00   = transition at 02:00

static const TimezoneRegion timezoneDatabase[] = {
  // UTC
  {"UTC (no DST)", "UTC0"},

  // Europe
  {"British (UK, Ireland)", "GMT0BST,M3.5.0/01:00,M10.5.0/02:00"},
  {"Western European (Portugal)", "WET0WEST,M3.5.0/01:00,M10.5.0/02:00"},
  {"Central European (Poland, Germany, France, Italy, Spain)", "CET-1CEST,M3.5.0/02:00,M10.5.0/03:00"},
  {"Scandinavian (Sweden, Norway, Denmark)", "CET-1CEST,M3.5.0/02:00,M10.5.0/03:00"},
  {"Central Balkan (Hungary, Slovakia, Czechia)", "CET-1CEST,M3.5.0/02:00,M10.5.0/03:00"},
  {"Eastern European (Finland, Greece, Romania, Bulgaria)", "EET-2EEST,M3.5.0/03:00,M10.5.0/04:00"},
  {"Baltic (Lithuania, Latvia, Estonia)", "EET-2EEST,M3.5.0/03:00,M10.5.0/04:00"},
  {"Ukraine (Kyiv)", "EET-2EEST,M3.5.0/03:00,M10.5.0/04:00"},
  {"Belarus (Minsk - no DST)", "EET-2"},
  {"Russia (Moscow - no DST)", "MSK-3"},
  {"Iceland (Reykjavik - no DST)", "GMT0"},
  {"Turkey (Istanbul - no DST)", "TRT-3"},

  // Americas - North
  {"US Eastern (New York, Washington)", "EST5EDT,M3.2.0/02:00,M11.1.0/02:00"},
  {"US Central (Chicago, Texas)", "CST6CDT,M3.2.0/02:00,M11.1.0/02:00"},
  {"US Mountain (Denver)", "MST7MDT,M3.2.0/02:00,M11.1.0/02:00"},
  {"US Pacific (Los Angeles, San Francisco)", "PST8PDT,M3.2.0/02:00,M11.1.0/02:00"},
  {"Alaska (Anchorage)", "AKST9AKDT,M3.2.0/02:00,M11.1.0/02:00"},
  {"Hawaii (Honolulu - no DST)", "HST10"},
  {"Canada Eastern (Toronto, Montreal)", "EST5EDT,M3.2.0/02:00,M11.1.0/02:00"},
  {"Canada Central (Winnipeg)", "CST6CDT,M3.2.0/02:00,M11.1.0/02:00"},
  {"Canada Mountain (Edmonton)", "MST7MDT,M3.2.0/02:00,M11.1.0/02:00"},
  {"Canada Pacific (Vancouver)", "PST8PDT,M3.2.0/02:00,M11.1.0/02:00"},

  // Americas - Central & South
  {"Mexico Central (Mexico City)", "CST6CDT,M4.1.0/02:00,M10.5.0/02:00"},
  {"Colombia (Bogota - no DST)", "COT5"},
  {"Peru (Lima - no DST)", "PET5"},
  {"Chile (Santiago)", "CLT4CLST,M9.2.0/00:00,M4.2.0/00:00"},
  {"South America (Brazil, Argentina - no DST)", "BRT3"},

  // Asia-Pacific
  {"Japan (Tokyo - no DST)", "JST-9"},
  {"South Korea (Seoul - no DST)", "KST-9"},
  {"China (Shanghai, Beijing - no DST)", "CST-8"},
  {"Hong Kong (no DST)", "HKT-8"},
  {"Taiwan (Taipei - no DST)", "CST-8"},
  {"Singapore (no DST)", "SGT-8"},
  {"Philippines (Manila - no DST)", "PST-8"},
  {"Indonesia (Jakarta - no DST)", "WIB-7"},
  {"Thailand (Bangkok - no DST)", "ICT-7"},
  {"India (Mumbai, Delhi - no DST)", "IST-5:30"},
  {"Pakistan (Karachi - no DST)", "PKT-5"},
  {"Australia Eastern (Sydney, Melbourne)", "AEST-10AEDT,M10.1.0/02:00,M4.1.0/03:00"},
  {"Australia Central (Adelaide)", "ACST-9:30ACDT,M10.1.0/02:00,M4.1.0/03:00"},
  {"Australia Western (Perth - no DST)", "AWST-8"},
  {"New Zealand (Auckland)", "NZST-12NZDT,M9.5.0/02:00,M4.1.0/03:00"},

  // Middle East & Africa
  {"Israel (Jerusalem)", "IST-2IDT,M3.4.0/02:00,M10.2.0/02:00"},
  {"UAE (Dubai - no DST)", "GST-4"},
  {"Saudi Arabia (Riyadh - no DST)", "AST-3"},
  {"Egypt (Cairo - no DST)", "EET-2"},
  {"South Africa (Johannesburg - no DST)", "SAST-2"},
  {"Nigeria (Lagos - no DST)", "WAT-1"},
  {"Kenya (Nairobi - no DST)", "EAT-3"},
};

static const size_t TIMEZONE_COUNT = sizeof(timezoneDatabase) / sizeof(TimezoneRegion);

const TimezoneRegion* getSupportedTimezones(size_t* count) {
  if (count) *count = TIMEZONE_COUNT;
  return timezoneDatabase;
}

const char* getDefaultTimezoneForOffset(int gmtOffsetMinutes) {
  switch (gmtOffsetMinutes) {
    case 0:     return "GMT0BST,M3.5.0/01:00,M10.5.0/02:00";
    case 60:    return "CET-1CEST,M3.5.0/02:00,M10.5.0/03:00";
    case 120:   return "EET-2EEST,M3.5.0/03:00,M10.5.0/04:00";
    case 180:   return "MSK-3";
    case -300:  return "EST5EDT,M3.2.0/02:00,M11.1.0/02:00";
    case -360:  return "CST6CDT,M3.2.0/02:00,M11.1.0/02:00";
    case -420:  return "MST7MDT,M3.2.0/02:00,M11.1.0/02:00";
    case -480:  return "PST8PDT,M3.2.0/02:00,M11.1.0/02:00";
    case 540:   return "JST-9";
    case 480:   return "CST-8";
    case 600:   return "AEST-10AEDT,M10.1.0/02:00,M4.1.0/03:00";
    case 720:   return "NZST-12NZDT,M9.5.0/02:00,M4.1.0/03:00";
    case 330:   return "IST-5:30";
    case 240:   return "GST-4";
    case 300:   return "PKT-5";
    case -600:  return "HST10";
    default:    return nullptr;
  }
}
