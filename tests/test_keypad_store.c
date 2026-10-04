#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../main/keyboard/keypad_store.c"
static uint8_t persisted[KP_CONFIG_BYTES],staged[KP_CONFIG_BYTES];static size_t persisted_size,staged_size;
static kp_legacy_config_t legacy;
static bool legacy_valid,fail_open,fail_set,fail_commit;
static int locked,opened,commits;
SemaphoreHandle_t xSemaphoreCreateMutex(void){return &locked;}
int xSemaphoreTake(SemaphoreHandle_t h,int t){(void)t;assert(h==&locked&&!locked);locked=1;return 1;}
void xSemaphoreGive(SemaphoreHandle_t h){assert(h==&locked&&locked);locked=0;}
esp_err_t nvs_open(const char *ns,int mode,nvs_handle_t *h){assert(!strcmp(ns,"passport_keys")&&mode==NVS_READWRITE&&!opened);if(fail_open)return ESP_FAIL;opened=1;*h=1;return ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size){
    assert(h==1&&opened);const void *data;size_t n;
    if(!strcmp(key,"config_v1")){if(!legacy_valid)return ESP_FAIL;data=&legacy;n=sizeof(legacy);}
    else {assert(!strcmp(key,"config_v2"));if(!persisted_size)return ESP_FAIL;data=persisted;n=persisted_size;}
    if(!out){*size=n;return ESP_OK;}if(*size<n)return ESP_FAIL;memcpy(out,data,n);*size=n;return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t n){assert(h==1&&opened&&!strcmp(key,"config_v2")&&n<=sizeof(staged));if(fail_set)return ESP_FAIL;memcpy(staged,data,n);staged_size=n;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){assert(h==1&&opened);if(fail_commit)return ESP_FAIL;memcpy(persisted,staged,staged_size);persisted_size=staged_size;++commits;return ESP_OK;}
void nvs_close(nvs_handle_t h){assert(h==1&&opened);opened=0;}
static bool read_committed(const kp_config_t *config, void *user){
    assert(locked && config == &s_config);
    assert(!memcmp(config,user,sizeof(*config)));
    return true;
}
static bool failed_reader(const kp_config_t *config, void *user){
    assert(locked && config == &s_config && user == NULL);
    return false;
}
int main(void){
    kp_config_t c,actual,original;kp_defaults(&original);assert(kp_store_init()==ESP_OK);kp_store_get(&actual);assert(!memcmp(&actual,&original,sizeof(actual)));
    assert(kp_store_read(read_committed,&original)&&!locked);
    assert(!kp_store_read(NULL,NULL)&&!locked);
    assert(!kp_store_read(failed_reader,NULL)&&!locked);
    c=original;assert(kp_profile_add(&c,"Coding"));c.active_profile=1;
    fail_open=true;assert(kp_store_save(&c)==ESP_FAIL);fail_open=false;
    fail_set=true;assert(kp_store_save(&c)==ESP_FAIL);fail_set=false;
    fail_commit=true;assert(kp_store_save(&c)==ESP_FAIL);fail_commit=false;
    kp_store_get(&actual);assert(!memcmp(&original,&actual,sizeof(actual))&&!opened&&!locked);
    assert(kp_store_save(&c)==ESP_OK&&persisted_size==kp_config_size(&c));assert(kp_store_init()==ESP_OK);
    kp_store_get(&actual);assert(actual.active_profile==1&&!memcmp(&c,&actual,sizeof(c)));
    assert(kp_store_read(read_committed,&c)&&!locked);
    c.version=99;assert(kp_store_save(&c)==ESP_ERR_INVALID_ARG);
    // Valid v1 survives failed migration commits and takes priority over defaults.
    persisted_size=0;legacy=(kp_legacy_config_t){.version=1};legacy.bindings[0]=(kp_action_t){.type=KP_KEYBOARD,.keys={0x52}};legacy_valid=true;
    fail_commit=true;assert(kp_store_init()==ESP_OK);kp_store_get(&actual);kp_step_t s;
    assert(!persisted_size&&kp_binding_step(&actual,kp_binding(&actual,0),0,&s)&&s.action.keys[0]==0x52);
    fail_commit=false;int previous=commits;assert(kp_store_init()==ESP_OK&&commits==previous+1);
    assert(legacy_valid&&legacy.bindings[0].keys[0]==0x52);assert(kp_store_init()==ESP_OK&&commits==previous+1);
    // Invalid/truncated v2 falls back to the retained v1, without an NVS erase.
    ((kp_config_t *)persisted)->version=99;assert(kp_store_init()==ESP_OK);kp_store_get(&actual);assert(kp_binding_step(&actual,kp_binding(&actual,0),0,&s)&&s.action.keys[0]==0x52);
    persisted_size=10;legacy_valid=false;assert(kp_store_init()==ESP_OK);kp_store_get(&actual);assert(!memcmp(&actual,&original,sizeof(actual)));
    puts("Keypad NVS v1 migration, profile persistence and failure recovery: PASS");
}
