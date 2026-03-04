#define RTC_SLAVE_ADDRESS 0x51  // 1010001

#define RTC_SECOND        0x02
#define RTC_MINUTE        0x03
#define RTC_HOUR          0x04
#define RTC_DAY           0x06
#define RTC_DATE          0x05
#define RTC_MONTH         0x07
#define RTC_YEAR          0x08

#define BAUD_100K         100000UL
#define BAUD_400K         400000UL

void  rtc_set_time(TWI_t *twi);
void  rtc_set_date(TWI_t *twi);
int   rtc_get_time(TWI_t *twi);
int   rtc_get_date(TWI_t *twi);
char *rtc_time_to_string(char *s);
void  string_to_rtc_time(char *s);

struct rtc_time {
  uint8_t second;
  uint8_t minute;
  uint8_t hour;
};

struct rtc_date {
  uint8_t day;
  uint8_t month;
  uint8_t year;
};

char *rtc_date_to_string(char *s);
void  string_to_rtc_date(char *s);