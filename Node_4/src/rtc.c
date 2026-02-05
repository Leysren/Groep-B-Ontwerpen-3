#include "i2c.h"
#include "rtc.h"

struct rtc_time time;
struct rtc_date date;

void rtc_set_time(TWI_t *twi)
{
  i2c_start(twi, RTC_SLAVE_ADDRESS, I2C_WRITE);
  i2c_write(twi, RTC_SECOND);
  i2c_write(twi, time.second);
  i2c_write(twi, time.minute);
  i2c_write(twi, time.hour);
  i2c_stop(twi);
}

int rtc_get_date(TWI_t *twi)
{
  uint8_t x;

  x = 0;
  if ( (x = i2c_start(twi, RTC_SLAVE_ADDRESS, I2C_WRITE)) > 0 ) {
    return x;
  }
  i2c_write(twi, RTC_DATE);
  if ( (x = i2c_restart(twi, RTC_SLAVE_ADDRESS, I2C_READ)) > 0 ) {
    return x;
  }

  date.day   = i2c_read(twi, I2C_ACK);
  date.month = i2c_read(twi, I2C_ACK);
  date.year  = i2c_read(twi, I2C_NACK);
  i2c_stop(twi);

  return x;
}

int rtc_get_time(TWI_t * twi)
{
  uint8_t x;

  x = 0;
  if ( (x = i2c_start(twi, RTC_SLAVE_ADDRESS, I2C_WRITE)) > 0 ) {
    return x;
  }
  i2c_write(twi, RTC_SECOND);
  if ( (x = i2c_restart(twi, RTC_SLAVE_ADDRESS, I2C_READ)) > 0 ) {
    return x;
  }
  time.second = i2c_read(twi, I2C_ACK);
  time.minute = i2c_read(twi, I2C_ACK);
  time.hour   = i2c_read(twi, I2C_NACK);
  i2c_stop(twi);

  return x;
}

void rtc_set_date(TWI_t *twi)
{
  i2c_start(twi, RTC_SLAVE_ADDRESS, I2C_WRITE);
  i2c_write(twi, RTC_DATE);
  i2c_write(twi, date.day);
  i2c_write(twi, date.month);
  i2c_write(twi, date.year);
  i2c_stop(twi);
}
