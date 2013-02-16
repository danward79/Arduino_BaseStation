// Pinched from Andy Anseldo? I think... Not sure of original source
// isleapyear = 0-1
// month=0-11
// return: how many days a month has
//
// We could do this if ram was no issue:
uint8_t monthlen(uint8_t isleapyear,uint8_t month){
  const uint8_t mlen[2][12] = {
    { 
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
    ,
    { 
      31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
  };
  return(mlen[isleapyear][month]);
}

// Pinched from Andy Anseldo? I think... Not sure of original source
// gmtime -- convert calendar time (sec since 1970) into broken down time
// returns something like Fri 2007-10-19 in day and 01:02:21 in clock
// The return values is the minutes as integer. This way you can update
// the entire display when the minutes have changed and otherwise just
// write current time (clock). That way an LCD display needs complete
// re-write only every minute.
void gmtime(const uint32_t time)//,char *day, char *clock)
{
  uint32_t dayclock;
  uint16_t dayno;
  uint16_t tm_year = EPOCH_YR;
  uint8_t tm_sec,tm_min,tm_hour,tm_wday,tm_mon;

  dayclock = time % SECS_DAY;
  dayno = time / SECS_DAY;

  tm_sec = dayclock % 60UL;
  tm_min = (dayclock % 3600UL) / 60;
  tm_hour = dayclock / 3600UL;
  tm_wday = (dayno + 4) % 7; 	/* day 0 was a thursday */
  while (dayno >= YEARSIZE(tm_year)) {
    dayno -= YEARSIZE(tm_year);
    tm_year++;
  }
  tm_mon = 0;
  while (dayno >= monthlen(LEAPYEAR(tm_year),tm_mon)) {
    dayno -= monthlen(LEAPYEAR(tm_year),tm_mon);
    tm_mon++;
  }

	RTC.adjust(DateTime(tm_year, tm_mon + 1, dayno + 1, tm_hour, tm_min, tm_sec));
}