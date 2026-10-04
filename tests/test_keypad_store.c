#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../main/keyboard/keypad_store.c"
static kp_config_t persisted, staged;
static bool valid, fail_open, fail_set, fail_commit;
static int locked, opened;
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &locked; }
int xSemaphoreTake(SemaphoreHandle_t h,int t) { (void)t;assert(h==&locked&&!locked);locked=1;return 1; }
void xSemaphoreGive(SemaphoreHandle_t h) { assert(h==&locked&&locked);locked=0; }
esp_err_t nvs_open(const char *ns,int mode,nvs_handle_t *h) {
    assert(!strcmp(ns,"passport_keys")&&mode==NVS_READWRITE&&!opened);
    if(fail_open)return ESP_FAIL;
    opened=1;*h=1;return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size) {
    assert(h==1&&opened&&!strcmp(key,"config_v1")&&*size==sizeof(persisted));
    if(!valid)return ESP_FAIL;
    memcpy(out,&persisted,*size);return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size) {
    assert(h==1&&opened&&locked&&!strcmp(key,"config_v1")&&size==sizeof(staged));
    if(fail_set)return ESP_FAIL;
    memcpy(&staged,data,size);return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) { assert(h==1&&opened&&locked);if(fail_commit)return ESP_FAIL;persisted=staged;valid=true;return ESP_OK; }
void nvs_close(nvs_handle_t h) { assert(h==1&&opened);opened=0; }
int main(void) {
    kp_config_t c,actual,original;kp_defaults(&original);
    assert(kp_store_init()==ESP_OK);kp_store_get(&actual);assert(!memcmp(&original,&actual,sizeof(actual)));
    c=original;c.bindings[0].keys[0]=40;
    fail_open=true;assert(kp_store_save(&c)==ESP_FAIL);fail_open=false;
    fail_set=true;assert(kp_store_save(&c)==ESP_FAIL);fail_set=false;
    fail_commit=true;assert(kp_store_save(&c)==ESP_FAIL);fail_commit=false;
    kp_store_get(&actual);assert(!memcmp(&original,&actual,sizeof(actual))&&!opened&&!locked);
    assert(kp_store_save(&c)==ESP_OK);assert(kp_store_init()==ESP_OK);
    kp_store_get(&actual);assert(!memcmp(&c,&actual,sizeof(actual)));
    c.version=99;assert(kp_store_save(&c)==ESP_ERR_INVALID_ARG);
    persisted.version=99;assert(kp_store_init()==ESP_OK);kp_store_get(&actual);
    assert(!memcmp(&actual,&original,sizeof(actual)));
    puts("Keypad NVS persistence and failure recovery: PASS");
}
