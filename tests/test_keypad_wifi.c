#include "keypad_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { const char *text; size_t offset; } input_t;
static int read_json(void *user, char *out, size_t size) {
    input_t *in = user; size_t n = strlen(in->text + in->offset);
    if (n > size) n = size;
    if (n > 1) n = 1;
    memcpy(out, in->text + in->offset, n); in->offset += n; return (int)n;
}
static bool decode(const char *json, const kp_wifi_config_t *saved, kp_wifi_config_t *out) {
    input_t in = {json, 0}; return kp_wifi_decode(read_json, &in, saved, out);
}
static char response[4096]; static size_t used;
static bool write_json(void *user, const char *text, size_t n) {
    (void)user; assert(used + n < sizeof(response)); memcpy(response + used, text, n);
    used += n; response[used] = 0; return true;
}
int main(void) {
    kp_wifi_config_t wifi, next; kp_wifi_defaults(&wifi); assert(kp_wifi_valid(&wifi));
    assert(decode("{\"ssid\":\"家庭网络\",\"password\":\"test-pass\",\"keep_password\":false}", &wifi, &next));
    wifi = next;
    assert(decode("{\"ssid\":\"家庭网络\",\"password\":\"\",\"keep_password\":true}", &wifi, &next));
    assert(!strcmp(next.password, "test-pass"));
    assert(!decode("{\"ssid\":\"other\",\"password\":\"\",\"keep_password\":true}", &wifi, &next));
    const char *bad[] = {
        "{}", "null", "{\"ssid\":\"test\",\"password\":\"short\",\"keep_password\":false}",
        "{\"ssid\":\"test\",\"password\":\"test-pass\",\"keep_password\":true}",
        "{\"ssid\":\"\",\"password\":\"test-pass\",\"keep_password\":false}",
        "{\"ssid\":\"\",\"password\":\"\",\"keep_password\":true}",
        "{\"ssid\":\"test\",\"password\":\"密码密码\",\"keep_password\":false}",
        "{\"ssid\":\"test\\n\",\"password\":\"\",\"keep_password\":false}",
        "{\"ssid\":\"test\",\"password\":\"\",\"keep_password\":1}",
        "{\"ssid\":\"test\",\"password\":\"\",\"keep_password\":false,\"extra\":0}",
        "{\"ssid\":\"test\",\"ssid\":\"other\",\"password\":\"\",\"keep_password\":false}"
    };
    for (unsigned i = 0; i < sizeof(bad)/sizeof(bad[0]); ++i) assert(!decode(bad[i], &wifi, &next));
    used = 0; assert(kp_wifi_encode(&wifi, 5, false, write_json, NULL));
    assert(strstr(response, "家庭网络") && strstr(response, "\"has_password\":true") && !strstr(response, "test-pass"));
    strcpy(wifi.ssid, "A\"B\\C"); used = 0; assert(kp_wifi_encode(&wifi, 4, true, write_json, NULL));
    assert(strstr(response, "A\\\"B\\\\C") && strstr(response, "\"clock_valid\":true"));
    memset(wifi.ssid, 'A', 32); wifi.ssid[32] = 0; memset(wifi.password, 'p', 63); wifi.password[63] = 0;
    assert(kp_wifi_valid(&wifi)); wifi.ssid[32] = 'A'; assert(!kp_wifi_valid(&wifi));
    kp_wifi_defaults(&wifi); strcpy(wifi.ssid, "中中中中中中中中中中"); assert(kp_wifi_valid(&wifi));
    strcpy(wifi.ssid, "\xc0\xaf"); assert(!kp_wifi_valid(&wifi));
    assert(decode("{\"ssid\":\"\",\"password\":\"\",\"keep_password\":false}", &wifi, &next));
    assert(!next.ssid[0] && !next.password[0]);
    kp_wifi_job_t job={.id=7,.kind=KP_WIFI_SCAN,.state=KP_WIFI_SCAN_DONE};
    for(unsigned i=0;i<32;i++){char name[33];snprintf(name,sizeof(name),"network-%u",i);kp_wifi_scan_add(&job,name,-90+(int)i,true);}
    assert(job.count==16&&!strcmp(job.networks[0].ssid,"network-31")&&job.networks[15].rssi==-74);
    kp_wifi_scan_add(&job,"network-30",-10,false);assert(job.count==16&&!strcmp(job.networks[0].ssid,"network-30")&&!job.networks[0].secured);
    kp_wifi_scan_add(&job,"",-1,false);kp_wifi_scan_add(&job,"bad\n",-1,false);kp_wifi_scan_add(&job,"\xc0\xaf",-1,false);assert(job.count==16);
    char oversized[34];memset(oversized,'a',33);oversized[33]=0;kp_wifi_scan_add(&job,oversized,-1,false);assert(job.count==16);
    used=0;assert(kp_wifi_job_encode(&job,write_json,NULL)&&strstr(response,"network-31")&&strstr(response,"\"state\":5")&&!strstr(response,"password"));
    assert(kp_wifi_failure(202)==KP_WIFI_AUTH_FAILED&&kp_wifi_failure(201)==KP_WIFI_NOT_FOUND&&kp_wifi_failure(211)==KP_WIFI_UNSUPPORTED&&kp_wifi_failure(203)==KP_WIFI_ERROR);
    assert(!kp_wifi_job_expired(KP_WIFI_CONNECTING,29999)&&kp_wifi_job_expired(KP_WIFI_CONNECTING,30000));
    assert(!kp_wifi_job_expired(KP_WIFI_SYNCING,19999)&&kp_wifi_job_expired(KP_WIFI_SYNCING,20000));
    assert(!kp_wifi_job_expired(KP_WIFI_SCANNING,14999)&&kp_wifi_job_expired(KP_WIFI_SCANNING,15000));
    puts("Wi-Fi fragmented codec, strict fields, byte limits, password secrecy and clear: PASS");
}
