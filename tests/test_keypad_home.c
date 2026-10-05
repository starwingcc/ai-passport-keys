#include "keypad_home.h"
#include "keypad_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { const char *text; size_t offset; } reader_t;
static int read_json(void *user, char *out, size_t capacity) {
    reader_t *r = user; size_t n = strlen(r->text + r->offset);
    if (n > capacity) n = capacity;
    if (n > 3) n = 3; // UTF-8 and escaped strings cross transport fragments.
    memcpy(out, r->text + r->offset, n); r->offset += n; return (int)n;
}
static bool decode(const char *text, kp_home_config_t *home) {
    reader_t r = {text, 0}; return kp_home_decode(read_json, &r, home);
}
static char encoded[1024]; static size_t used;
static bool write_json(void *user, const char *text, size_t size) {
    (void)user; assert(used + size < sizeof(encoded));
    memcpy(encoded + used, text, size); used += size; encoded[used] = 0; return true;
}
static void expect(const kp_home_config_t *home, uint64_t epoch, const char *time, const char *date, const char *period) {
    char t[6], d[15], p[3]; kp_home_clock_text(home, epoch, t, d, p);
    assert(!strcmp(t,time) && !strcmp(d,date) && !strcmp(p,period));
}
int main(void) {
    kp_home_config_t home, copy; kp_home_defaults(&home);
    assert(kp_home_valid(&home,false) && home.timezone_minutes == 480 && !home.hour12 && !home.avatar);
    expect(&home,0,"--:--","----/--/-- ---","");
    assert(decode("{\"version\":1,\"name\":\"星翼\",\"badge\":\"ID / 001\",\"avatar\":2,\"timezone_minutes\":-345,\"hour12\":1}",&home));
    used = 0; assert(kp_home_encode(&home,write_json,NULL) && decode(encoded,&copy) && !memcmp(&home,&copy,sizeof(home)));
    strcpy(home.name,"A\"B\\C"); strcpy(home.badge,""); used = 0;
    assert(kp_home_encode(&home,write_json,NULL) && decode(encoded,&copy) && !strcmp(home.name,copy.name));
    const char *bad[] = {"{}","null","{\"version\":1,\"name\":\"A\",\"badge\":\"\",\"avatar\":4,\"timezone_minutes\":480,\"hour12\":0}",
        "{\"version\":1,\"name\":\"\",\"badge\":\"\",\"avatar\":0,\"timezone_minutes\":480,\"hour12\":0}",
        "{\"version\":1,\"name\":\"😀\",\"badge\":\"\",\"avatar\":0,\"timezone_minutes\":480,\"hour12\":0}",
        "{\"version\":1,\"name\":\"A\",\"badge\":\"\",\"avatar\":0,\"timezone_minutes\":-735,\"hour12\":0}",
        "{\"version\":1,\"name\":\"A\",\"badge\":\"\",\"avatar\":0,\"timezone_minutes\":481,\"hour12\":0}",
        "{\"version\":1,\"name\":\"A\",\"badge\":\"\",\"avatar\":0,\"timezone_minutes\":480,\"hour12\":2}",
        "{\"version\":1,\"name\":\"A\",\"name\":\"B\",\"badge\":\"\",\"avatar\":0,\"timezone_minutes\":480,\"hour12\":0}"};
    for (unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) assert(!decode(bad[i],&copy));
    kp_home_defaults(&home); strcpy(home.name,"龘"); assert(kp_home_valid(&home,true) && !kp_home_valid(&home,false));
    memset(home.name,'A',sizeof(home.name)); assert(!kp_home_valid(&home,true));
    kp_home_defaults(&home); strcpy(home.name,"浏览情景长名称测试全选后复制窗口"); assert(kp_home_valid(&home,false));
    kp_clock_t clock = {0}; assert(!kp_clock_read(&clock,100));
    uint64_t start = 1791167040; // 2026/10/05 10:24 UTC+8, Monday.
    assert(kp_clock_sync(&clock,start,UINT64_C(4294967200)));
    assert(kp_clock_read(&clock,UINT64_C(4294968199))==start);
    assert(kp_clock_read(&clock,UINT64_C(4294968200))==start+1);
    assert(!kp_clock_read(&clock,1));
    assert(!kp_clock_sync(&clock,KP_TIME_MIN-1,1) && clock.epoch==start);
    assert(!kp_clock_sync(&clock,KP_TIME_MAX+1,1) && clock.epoch==start);
    expect(&home,start,"10:24","2026/10/05 MON","");
    const char *week[]={"MON","TUE","WED","THU","FRI","SAT","SUN"};
    for(unsigned i=0;i<7;i++){char date[15];snprintf(date,sizeof(date),"2026/10/%02u %s",i+5,week[i]);expect(&home,start+i*86400,"10:24",date,"");}
    home.timezone_minutes=-720; expect(&home,start,"14:24","2026/10/04 SUN","");
    home.timezone_minutes=345; expect(&home,start,"08:09","2026/10/05 MON","");
    home.timezone_minutes=0;
    expect(&home,1709164800,"00:00","2024/02/29 THU","");
    expect(&home,1709251200,"00:00","2024/03/01 FRI","");
    expect(&home,1735689600,"00:00","2025/01/01 WED","");
    home.hour12=1;
    expect(&home,1735689600,"12:00","2025/01/01 WED","AM");
    expect(&home,1735732800,"12:00","2025/01/01 WED","PM");
    expect(&home,1735775940,"11:59","2025/01/01 WED","PM");
    expect(&home,0,"--:--","----/--/-- ---","");
    assert(kp_clock_sync(&clock,KP_TIME_MAX,0) && !kp_clock_read(&clock,1000));
    uint64_t epoch=0; reader_t r={"{\"epoch\":1791167040}",0};assert(kp_time_decode(read_json,&r,&epoch)&&epoch==start);
    const char *bad_time[]={"{\"epoch\":0}","{\"epoch\":4102444800}","{\"epoch\":1791167040.5}","{\"epoch\":-1}","{\"epoch\":1791167040,\"extra\":1}","{\"epoch\":1791167040} garbage"};
    for(unsigned i=0;i<sizeof(bad_time)/sizeof(bad_time[0]);i++){r=(reader_t){bad_time[i],0};assert(!kp_time_decode(read_json,&r,&epoch));}
    puts("Home identity, fragmented JSON, UTC offsets, seven weekdays, leap dates, 12/24h and monotonic clock: PASS");
}
