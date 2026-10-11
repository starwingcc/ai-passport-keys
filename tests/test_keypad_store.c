#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "keypad_runner.h"
#include "../main/keyboard/keypad_store.c"
static kp_settings_t stored_settings, staged_settings;static size_t settings_size, staged_settings_size;
static kp_screen_config_t stored_screen, staged_screen; static size_t screen_size, staged_screen_size;
static uint8_t stored_system[2], staged_system[2];static size_t system_size, staged_system_size;
static kp_devices_t stored_devices, staged_devices; static size_t devices_size, staged_devices_size;
static bool fail_open,fail_set,fail_commit;
static int locked,opened,commits;
SemaphoreHandle_t xSemaphoreCreateMutex(void){return &locked;}
int xSemaphoreTake(SemaphoreHandle_t h,int t){assert(h==&locked);if(locked){assert(t==0);return 0;}locked=1;return 1;}
void xSemaphoreGive(SemaphoreHandle_t h){assert(h==&locked&&locked);locked=0;}
esp_err_t nvs_open(const char *ns,int mode,nvs_handle_t *h){assert(!strcmp(ns,"passport_keys")&&mode==NVS_READWRITE&&!opened);if(fail_open)return ESP_FAIL;opened=1;staged_settings_size=staged_screen_size=staged_system_size=staged_devices_size=0;*h=1;return ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size){
    assert(h==1&&opened);const void *data;size_t n;
    if(!strcmp(key,"settings_v1")){if(!settings_size)return ESP_FAIL;data=&stored_settings;n=settings_size;}
    else if(!strcmp(key,"devices_v1")){if(!devices_size)return ESP_FAIL;data=&stored_devices;n=devices_size;}
    else if(!strcmp(key,"system_v1")){if(!system_size)return ESP_FAIL;data=stored_system;n=system_size;}
    else {assert(!strcmp(key,"screen_v1"));if(!screen_size)return ESP_FAIL;data=&stored_screen;n=screen_size;}
    if(!out){*size=n;return ESP_OK;}if(*size<n)return ESP_FAIL;memcpy(out,data,n);*size=n;return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t n){
    assert(h==1&&opened&&n<=sizeof(staged_settings));if(fail_set)return ESP_FAIL;
    if(!strcmp(key,"settings_v1")){memset(&staged_settings,0,sizeof(staged_settings));memcpy(&staged_settings,data,n);staged_settings_size=n;}
    else if(!strcmp(key,"devices_v1")){assert(n==sizeof(staged_devices));memcpy(&staged_devices,data,n);staged_devices_size=n;}
    else if(!strcmp(key,"system_v1")){assert(n==sizeof(staged_system));memcpy(staged_system,data,n);staged_system_size=n;}
    else {assert(!strcmp(key,"screen_v1")&&n==sizeof(staged_screen));memcpy(&staged_screen,data,n);staged_screen_size=n;}
    return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h){
    assert(h==1&&opened);if(fail_commit)return ESP_FAIL;
    if(staged_settings_size){stored_settings=staged_settings;settings_size=staged_settings_size;}
    if(staged_screen_size){stored_screen=staged_screen;screen_size=staged_screen_size;}
    if(staged_system_size){memcpy(stored_system,staged_system,staged_system_size);system_size=staged_system_size;}
    if(staged_devices_size){stored_devices=staged_devices;devices_size=staged_devices_size;}
    ++commits;return ESP_OK;
}
void nvs_close(nvs_handle_t h){assert(h==1&&opened);opened=0;}
static bool read_committed(const kp_config_t *config, void *user){
    assert(locked && config == &s_settings.config);
    assert(!memcmp(config,user,sizeof(*config)));
    return true;
}
static bool failed_reader(const kp_config_t *config, void *user){
    assert(locked && config == &s_settings.config && user == NULL);
    return false;
}
static bool copy_settings(const kp_settings_t *settings, void *user){
    assert(locked && settings==&s_settings);*(kp_settings_t *)user=*settings;return true;
}
static void cancel_runner(void *user, kp_effect_t event, const kp_action_t *action) {
    (void)action;
    if (event == KP_CANCEL) { assert(!locked); kp_runner_cancel(user); }
}
static void test_live_settings(void) {
    kp_settings_t next, saved;
    kp_settings_defaults(&next);
    kp_step_t step={.action={.type=KP_KEYBOARD,.keys={4}}};
    assert(kp_binding_set(&next.config,0,0,"A",&step,1));
    assert(kp_store_settings_save(&next)==ESP_OK);
    kp_runner_t runner={0}; kp_model_t model;
    kp_init(&model,&next.config,cancel_runner,&runner);
    kp_home_config_t home=next.home;
    uint32_t revision=0;
    assert(!kp_store_runtime_sync(&revision,&model.config,&home));
    int writes=commits;
    assert(!kp_store_runtime_sync(&revision,&model.config,&home)&&commits==writes);
    assert(kp_runner_start(&runner,&model.config,kp_binding(&model.config,0),true,KP_SYSTEM_MACOS));
    model.executing=true;
    kp_action_t action;assert(kp_runner_request(&runner,100,&action));
    kp_runner_result(&runner,true,100);
    // Home-only saves update the badge without interrupting the held shortcut.
    strcpy(next.home.name,"Live home");assert(kp_store_home_save(&next.home)==ESP_OK);
    assert(kp_store_runtime_sync(&revision,&model.config,&home)==KP_STORE_HOME_CHANGED);
    assert(!strcmp(home.name,"Live home")&&runner.running&&model.executing);
    // A failed complete save never publishes a revision or changes runtime data.
    step.action.keys[0]=5;assert(kp_binding_set(&next.config,0,0,"B",&step,1));
    strcpy(next.home.name,"Live both");uint32_t observed=revision;
    for(unsigned fault=0;fault<3;fault++) {
        fail_open=fault==0;fail_set=fault==1;fail_commit=fault==2;
        assert(kp_store_settings_save(&next)==ESP_FAIL);
        assert(!kp_store_runtime_sync(&revision,&model.config,&home)&&revision==observed);
        assert(runner.running&&model.executing&&!strcmp(home.name,"Live home"));
    }
    fail_open=fail_set=fail_commit=false;
    assert(kp_store_settings_save(&next)==ESP_OK);
    // An HTTP reader holding the mutex cannot block the main task. Retry copies
    // the complete committed pair without another 6 KiB allocation or NVS write.
    locked=1;
    assert(!kp_store_runtime_sync(&revision,&model.config,&home)&&revision==observed);
    assert(!strcmp(home.name,"Live home"));locked=0;writes=commits;
    assert(kp_store_runtime_sync(&revision,&model.config,&home)==(KP_STORE_CONFIG_CHANGED|KP_STORE_HOME_CHANGED));
    kp_config_loaded(&model,200);
    assert(!runner.running&&!model.executing&&commits==writes);
    assert(!strcmp(home.name,"Live both")&&!memcmp(&model.config,&next.config,sizeof(model.config)));
    assert(!kp_runner_request(&runner,300,&action));
    // Several saves before a main tick apply the newest coherent record.
    strcpy(next.home.name,"Intermediate");assert(kp_store_settings_save(&next)==ESP_OK);
    strcpy(next.home.name,"Newest");step.action.keys[0]=6;
    assert(kp_binding_set(&next.config,0,0,"C",&step,1));
    assert(kp_profile_add(&next.config,"Second"));
    assert(kp_store_settings_save(&next)==ESP_OK);
    assert(kp_store_runtime_sync(&revision,&model.config,&home)==(KP_STORE_CONFIG_CHANGED|KP_STORE_HOME_CHANGED));
    assert(!strcmp(home.name,"Newest")&&!memcmp(&model.config,&next.config,sizeof(model.config)));
    // A local profile choice preserves a newer web record, rather than saving
    // the main task's stale copy over freshly committed mappings/home/Wi-Fi.
    strcpy(next.home.badge,"Newest badge");step.action.keys[0]=7;
    assert(kp_binding_set(&next.config,0,0,"D",&step,1));
    assert(kp_store_settings_save(&next)==ESP_OK);
    assert(kp_store_profile_select(1)==ESP_OK);next.config.active_profile=1;
    assert(kp_store_settings_read(copy_settings,&saved)&&!memcmp(&saved,&next,sizeof(saved)));
    assert(kp_store_runtime_sync(&revision,&model.config,&home)==(KP_STORE_CONFIG_CHANGED|KP_STORE_HOME_CHANGED));
    assert(model.config.active_profile==1&&!strcmp(home.badge,"Newest badge"));
    observed=revision;writes=commits;
    assert(kp_store_profile_select(2)==ESP_ERR_INVALID_ARG);
    assert(kp_store_profile_select(UINT32_MAX)==ESP_ERR_INVALID_ARG);
    assert(!kp_store_runtime_sync(&revision,&model.config,&home)&&revision==observed&&commits==writes);
    // Wi-Fi changes and redundant saves do not cancel input or recopy the model.
    strcpy(next.wifi.ssid,"pending-network");strcpy(next.wifi.password,"test-pass");
    assert(kp_store_wifi_save(&next.wifi)==ESP_OK);
    assert(!kp_store_runtime_sync(&revision,&model.config,&home)&&revision!=observed);
    assert(kp_store_settings_save(&next)==ESP_OK);
    assert(!kp_store_runtime_sync(&revision,&model.config,&home));
    puts("Live settings: atomic revisions, failed/busy saves, held macro release, newest record and profile selection: PASS");
}
int main(void){
    kp_devices_t d, old, got; uint8_t peers[2][7]={{0,1,2,3,4,5,6},{1,6,5,4,3,2,1}};
    assert(kp_store_init()==ESP_OK); kp_store_devices_get(&d); assert(!kp_devices_count(&d));
    kp_devices_reconcile(&d,peers,2); d.slot[0].system=KP_SYSTEM_MACOS; d.slot[1].system=KP_SYSTEM_WINDOWS;
    assert(kp_device_name_set(&d.slot[0],"MacBook Air")); d.target=1;
    assert(kp_store_devices_save(&d)==ESP_OK); old=d;
    assert(kp_store_init()==ESP_OK); kp_store_devices_get(&got); assert(!memcmp(&got,&old,sizeof(got)));
    for(unsigned fault=0;fault<3;fault++){
        d=old;d.slot[1].system=KP_SYSTEM_MACOS;
        fail_open=fault==0;fail_set=fault==1;fail_commit=fault==2;
        assert(kp_store_devices_save(&d)==ESP_FAIL);kp_store_devices_get(&got);
        assert(!memcmp(&got,&old,sizeof(got)));
    }
    fail_open=fail_set=fail_commit=false;
    assert(kp_store_init()==ESP_OK);kp_store_devices_get(&got);assert(!memcmp(&got,&old,sizeof(got)));
    d=old;d.slot[1].system=99;assert(kp_store_devices_save(&d)==ESP_ERR_INVALID_ARG);
    stored_devices.slot[1].system=99;assert(kp_store_init()==ESP_OK);kp_store_devices_get(&got);
    assert(!kp_devices_count(&got)); devices_size=0;
    kp_config_t c,actual,original;kp_defaults(&original);assert(kp_store_init()==ESP_OK);kp_store_get(&actual);assert(!memcmp(&actual,&original,sizeof(actual)));
    assert(kp_store_read(read_committed,&original)&&!locked);
    assert(!kp_store_read(NULL,NULL)&&!locked);
    assert(!kp_store_read(failed_reader,NULL)&&!locked);
    c=original;assert(kp_profile_add(&c,"Coding"));c.active_profile=1;
    fail_open=true;assert(kp_store_save(&c)==ESP_FAIL);fail_open=false;
    fail_set=true;assert(kp_store_save(&c)==ESP_FAIL);fail_set=false;
    fail_commit=true;assert(kp_store_save(&c)==ESP_FAIL);fail_commit=false;
    kp_store_get(&actual);assert(!memcmp(&original,&actual,sizeof(actual))&&!opened&&!locked);
    assert(kp_store_save(&c)==ESP_OK&&settings_size==offsetof(kp_settings_t,config)+kp_config_size(&c));assert(kp_store_init()==ESP_OK);
    kp_store_get(&actual);assert(actual.active_profile==1&&!memcmp(&c,&actual,sizeof(c)));
    assert(kp_store_read(read_committed,&c)&&!locked);
    // A font shrink must not reset profiles or macros stored by older firmware.
    kp_config_t rare=c;
    memcpy(rare.data+rare.profiles[1].name,"龘字",7);
    memcpy(rare.data+rare.profiles[0].bindings[8].title,"靐字",7);
    assert(kp_config_valid(&rare));stored_settings.config=rare;
    assert(kp_store_init()==ESP_OK);kp_store_get(&actual);assert(!memcmp(&actual,&rare,sizeof(actual)));
    assert(!kp_profile_name_set(&actual,1,"龘字")&&!kp_binding_set(&actual,0,8,"靐字",NULL,0));
    c.version=99;assert(kp_store_save(&c)==ESP_ERR_INVALID_ARG);
    // Missing or malformed complete records start with defaults, without boot writes on valid records.
    kp_settings_t defaults_all,actual_all;kp_settings_defaults(&defaults_all);
    kp_settings_t retained_settings=stored_settings;size_t retained_size=settings_size;
    int previous=commits;
    for(unsigned fault=0;fault<9;fault++){
        stored_settings=retained_settings;settings_size=retained_size;
        if(fault==0)settings_size=0;
        else if(fault==1)settings_size--;
        else if(fault==2)settings_size=sizeof(stored_settings)+1;
        else if(fault==3)settings_size=1;
        else if(fault==4)stored_settings.version=99;
        else if(fault==5)stored_settings.config.version=4;
        else if(fault==6)stored_settings.home.name[0]=0;
        else if(fault==7)strcpy(stored_settings.wifi.password,"short");
        else stored_settings.config.used=sizeof(stored_settings.config.data)+1;
        assert(kp_store_init()==ESP_OK&&commits==++previous);
        assert(kp_store_settings_read(copy_settings,&actual_all)&&!memcmp(&actual_all,&defaults_all,sizeof(actual_all)));
        assert(settings_size==kp_settings_size(&defaults_all));
        assert(kp_store_init()==ESP_OK&&commits==previous);
    }
    // Defaults remain usable when their first save fails; a later boot retries.
    for(unsigned fault=0;fault<2;fault++){
        settings_size=0;fail_set=fault==0;fail_commit=fault==1;
        assert(kp_store_init()==ESP_OK&&!settings_size&&commits==previous);
        assert(kp_store_settings_read(copy_settings,&actual_all)&&!memcmp(&actual_all,&defaults_all,sizeof(actual_all)));
    }
    fail_set=fail_commit=false;
    assert(kp_store_init()==ESP_OK&&commits==++previous);
    fail_open=true;assert(kp_store_init()==ESP_FAIL&&!opened&&!locked);fail_open=false;
    assert(kp_store_init()==ESP_OK&&commits==previous);
    // Complete storage preserves the full shortcut arena alongside home metadata.
    c=original;c.used=sizeof(c.data);assert(kp_store_save(&c)==ESP_OK);
    kp_home_config_t home, saved_home;kp_store_home_get(&home);kp_home_defaults(&saved_home);
    assert(!memcmp(&home,&saved_home,sizeof(home)));
    strcpy(home.name,"星翼");strcpy(home.badge,"ID / 001");home.avatar=3;home.hour12=1;home.timezone_minutes=-345;
    assert(kp_store_home_save(&home)==ESP_OK);kp_store_get(&original);
    assert(kp_store_time_sync(1791167040,1000)&&kp_store_time_read(3000)==1791167042);
    assert(kp_store_init()==ESP_OK);kp_store_home_get(&saved_home);kp_store_get(&actual);
    assert(!memcmp(&home,&saved_home,sizeof(home))&&!memcmp(&original,&actual,sizeof(actual)));
    assert(!kp_store_time_read(1000)); // Reset starts untrusted, never an old saved wall time.
    for(unsigned fault=0;fault<3;fault++){
        kp_home_config_t next=home;strcpy(next.name,"Changed");
        fail_open=fault==0;fail_set=fault==1;fail_commit=fault==2;
        assert(kp_store_home_save(&next)==ESP_FAIL);kp_store_home_get(&saved_home);
        assert(!memcmp(&home,&saved_home,sizeof(home)));
    }
    fail_open=fail_set=fail_commit=false;
    assert(kp_store_init()==ESP_OK);kp_store_home_get(&saved_home);
    assert(!memcmp(&home,&saved_home,sizeof(home)));kp_store_get(&actual);assert(!memcmp(&original,&actual,sizeof(actual)));
    // Network credentials survive a cold boot without affecting home/macros.
    kp_wifi_config_t wifi, next_wifi;kp_store_wifi_get(&wifi);assert(!wifi.ssid[0]);
    strcpy(wifi.ssid,"test-network");strcpy(wifi.password,"test-pass");
    assert(kp_store_wifi_save(&wifi)==ESP_OK);assert(kp_store_init()==ESP_OK);kp_store_wifi_get(&next_wifi);
    assert(!memcmp(&wifi,&next_wifi,sizeof(wifi)));kp_store_get(&actual);assert(!memcmp(&original,&actual,sizeof(actual)));
    for(unsigned fault=0;fault<3;fault++){
        kp_wifi_config_t next=wifi;strcpy(next.ssid,"replacement");
        fail_open=fault==0;fail_set=fault==1;fail_commit=fault==2;
        assert(kp_store_wifi_save(&next)==ESP_FAIL);kp_store_wifi_get(&next_wifi);
        assert(!memcmp(&wifi,&next_wifi,sizeof(wifi)));
    }
    fail_open=fail_set=fail_commit=false;
    assert(kp_store_init()==ESP_OK);kp_store_wifi_get(&next_wifi);assert(!memcmp(&wifi,&next_wifi,sizeof(wifi)));
    kp_wifi_defaults(&wifi);assert(kp_store_wifi_save(&wifi)==ESP_OK);assert(kp_store_init()==ESP_OK);
    kp_store_wifi_get(&next_wifi);assert(!next_wifi.ssid[0]&&!next_wifi.password[0]);
    // A separate screen record preserves full scenarios and unrelated settings.
    kp_screen_config_t screen, next_screen;kp_store_screen_get(&screen);kp_screen_defaults(&next_screen);
    assert(!memcmp(&screen,&next_screen,sizeof(screen)));
    screen.brightness=60;screen.dim_brightness=15;screen.dim_after_s=30;screen.off_after_s=120;
    kp_store_get(&original);kp_store_home_get(&home);kp_store_wifi_get(&wifi);
    assert(kp_store_screen_save(&screen)==ESP_OK&&screen_size==sizeof(screen));assert(kp_store_init()==ESP_OK);
    kp_store_screen_get(&next_screen);assert(!memcmp(&screen,&next_screen,sizeof(screen)));
    // Persist ten-second timer values across restart.
    screen.dim_after_s=170;screen.off_after_s=180;
    assert(kp_store_screen_save(&screen)==ESP_OK);assert(kp_store_init()==ESP_OK);
    kp_store_screen_get(&next_screen);assert(!memcmp(&screen,&next_screen,sizeof(screen)));
    kp_store_get(&actual);kp_store_home_get(&saved_home);kp_store_wifi_get(&next_wifi);
    assert(!memcmp(&actual,&original,sizeof(actual))&&!memcmp(&home,&saved_home,sizeof(home))&&!memcmp(&wifi,&next_wifi,sizeof(wifi)));
    for(unsigned fault=0;fault<3;fault++){
        kp_screen_config_t next=screen;next.brightness=65;
        fail_open=fault==0;fail_set=fault==1;fail_commit=fault==2;
        assert(kp_store_screen_save(&next)==ESP_FAIL);kp_store_screen_get(&next_screen);
        assert(!memcmp(&screen,&next_screen,sizeof(screen))&&!memcmp(&screen,&stored_screen,sizeof(screen))&&!locked&&!opened);
    }
    fail_open=fail_set=fail_commit=false;next_screen=screen;next_screen.off_after_s=30;
    assert(kp_store_screen_save(&next_screen)==ESP_ERR_INVALID_ARG);
    stored_screen.version=99;assert(kp_store_init()==ESP_OK);kp_store_screen_get(&screen);kp_screen_defaults(&next_screen);
    assert(!memcmp(&screen,&next_screen,sizeof(screen)));
    stored_screen.version=1;screen_size--;assert(kp_store_init()==ESP_OK);kp_store_screen_get(&screen);
    assert(!memcmp(&screen,&next_screen,sizeof(screen)));
    kp_system_t system;kp_store_system_get(&system);assert(system==KP_SYSTEM_MACOS);
    assert(kp_store_system_save(KP_SYSTEM_WINDOWS)==ESP_OK);assert(kp_store_init()==ESP_OK);
    kp_store_system_get(&system);assert(system==KP_SYSTEM_WINDOWS);kp_store_get(&actual);assert(!memcmp(&actual,&original,sizeof(actual)));
    for(unsigned fault=0;fault<3;fault++){
        fail_open=fault==0;fail_set=fault==1;fail_commit=fault==2;
        assert(kp_store_system_save(KP_SYSTEM_MACOS)==ESP_FAIL);kp_store_system_get(&system);
        assert(system==KP_SYSTEM_WINDOWS&&stored_system[1]==KP_SYSTEM_WINDOWS&&!locked&&!opened);
    }
    fail_open=fail_set=fail_commit=false;
    assert(kp_store_system_save(KP_SYSTEM_COUNT)==ESP_ERR_INVALID_ARG);
    assert(kp_store_system_save((kp_system_t)-1)==ESP_ERR_INVALID_ARG);
    // Configuration restores/saves never overwrite the system record.
    assert(kp_store_save(&original)==ESP_OK&&kp_store_init()==ESP_OK);kp_store_system_get(&system);assert(system==KP_SYSTEM_WINDOWS);
    for(unsigned fault=0;fault<3;fault++){
        stored_system[0]=fault==0?99:1;stored_system[1]=fault==1?2:1;system_size=fault==2?1:2;
        assert(kp_store_init()==ESP_OK);kp_store_system_get(&system);assert(system==KP_SYSTEM_MACOS);
    }
    // The v5 NVS layout upgrades system targets and Command migration in place, preserving other settings.
    kp_config_empty(&c);assert(kp_profile_add(&c,"Legacy"));kp_step_t s=(kp_step_t){.action={.type=KP_KEYBOARD,.modifiers=2,.keys={4}}};
    assert(kp_binding_set(&c,0,0,"",&s,1)&&kp_store_save(&c)==ESP_OK);
    stored_settings.config.version=5;
    unsigned offset=kp_binding(&stored_settings.config,0)->steps*2U;
    stored_settings.config.data[offset+8]=3; // Old uint16 logical_modifiers low byte.
    stored_settings.config.data[offset+9]=0;
    assert(kp_store_init()==ESP_OK);kp_store_get(&actual);
    assert(actual.version==KP_VERSION&&kp_binding_step(&actual,kp_binding(&actual,0),0,&s)&&s.action.modifiers==0x8a&&!s.action.consumer&&!s.action.system);
    assert(stored_settings.config.version==KP_VERSION); // Migration is persisted.
    s.action.system=KP_TARGET_WINDOWS;
    assert(kp_binding_set(&actual,0,0,"",&s,1)&&kp_store_save(&actual)==ESP_OK&&kp_store_init()==ESP_OK);
    kp_store_get(&actual);assert(kp_binding_step(&actual,kp_binding(&actual,0),0,&s)&&s.action.system==KP_TARGET_WINDOWS&&s.action.modifiers==0x8a);
    // All web settings occupy one blob, and failed saves replace no active module.
    kp_settings_t before_all,next_all;
    assert(kp_store_settings_read(copy_settings,&before_all)&&!locked);
    assert(!kp_store_settings_read(NULL,NULL)&&!locked);
    next_all=before_all;strcpy(next_all.home.name,"Complete");strcpy(next_all.wifi.ssid,"unified-network");strcpy(next_all.wifi.password,"test-pass");
    assert(kp_profile_add(&next_all.config,"Unified"));next_all.config.active_profile=1;
    kp_settings_t persisted_before=stored_settings;size_t size_before=settings_size;
    for(unsigned fault=0;fault<3;fault++){
        fail_open=fault==0;fail_set=fault==1;fail_commit=fault==2;
        assert(kp_store_settings_save(&next_all)==ESP_FAIL);
        assert(kp_store_settings_read(copy_settings,&actual_all)&&!memcmp(&before_all,&actual_all,sizeof(actual_all)));
        assert(settings_size==size_before&&!memcmp(&stored_settings,&persisted_before,sizeof(stored_settings))&&!locked&&!opened);
    }
    fail_open=fail_set=fail_commit=false;previous=commits;
    assert(kp_store_settings_save(&next_all)==ESP_OK&&commits==previous+1);
    assert(settings_size==kp_settings_size(&next_all)&&kp_store_init()==ESP_OK);
    assert(kp_store_settings_read(copy_settings,&actual_all)&&!memcmp(&next_all,&actual_all,sizeof(actual_all)));
    for(unsigned fault=0;fault<4;fault++){
        kp_settings_t invalid=next_all;
        if(!fault)invalid.version=99;else if(fault==1)invalid.home.name[0]=0;
        else if(fault==2)strcpy(invalid.wifi.password,"short");else invalid.config.active_profile=8;
        assert(kp_store_settings_save(&invalid)==ESP_ERR_INVALID_ARG&&commits==previous+1);
    }
    next_all.config.used=sizeof(next_all.config.data);
    assert(kp_store_settings_save(&next_all)==ESP_OK&&settings_size==offsetof(kp_settings_t,config)+KP_CONFIG_BYTES);
    assert(kp_store_init()==ESP_OK&&kp_store_settings_read(copy_settings,&actual_all)&&!memcmp(&actual_all,&next_all,sizeof(next_all)));
    test_live_settings();
    puts("Complete NVS record, current format, malformed records, atomic saves and full-capacity restart: PASS");
    puts("System/screen NVS isolation, system targets and Command migration, failed saves and clock invalidation: PASS");
    puts("Home/Wi-Fi complete-record persistence, cold boot, save failures and clearing: PASS");
}
