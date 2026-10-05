#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../main/keyboard/keypad_store.c"
static uint8_t persisted[KP_CONFIG_BYTES],staged[KP_CONFIG_BYTES];static size_t persisted_size,staged_size;
static kp_wifi_config_t stored_wifi, staged_wifi; static size_t wifi_size, staged_wifi_size;
static kp_home_config_t stored_home, staged_home; static size_t home_size, staged_home_size;
static kp_legacy_config_t legacy;
static kp_v2_config_t v2;static size_t v2_size;
static kp_v3_config_t v3;static size_t v3_size;
static bool legacy_valid,fail_open,fail_set,fail_commit;
static int locked,opened,commits;
SemaphoreHandle_t xSemaphoreCreateMutex(void){return &locked;}
int xSemaphoreTake(SemaphoreHandle_t h,int t){(void)t;assert(h==&locked&&!locked);locked=1;return 1;}
void xSemaphoreGive(SemaphoreHandle_t h){assert(h==&locked&&locked);locked=0;}
esp_err_t nvs_open(const char *ns,int mode,nvs_handle_t *h){assert(!strcmp(ns,"passport_keys")&&mode==NVS_READWRITE&&!opened);if(fail_open)return ESP_FAIL;opened=1;staged_size=staged_home_size=staged_wifi_size=0;*h=1;return ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size){
    assert(h==1&&opened);const void *data;size_t n;
    if(!strcmp(key,"home_v1")){if(!home_size)return ESP_FAIL;data=&stored_home;n=home_size;}
    else if(!strcmp(key,"wifi_v1")){if(!wifi_size)return ESP_FAIL;data=&stored_wifi;n=wifi_size;}
    else if(!strcmp(key,"config_v1")){if(!legacy_valid)return ESP_FAIL;data=&legacy;n=sizeof(legacy);}
    else if(!strcmp(key,"config_v2")){if(!v2_size)return ESP_FAIL;data=&v2;n=v2_size;}
    else if(!strcmp(key,"config_v3")){if(!v3_size)return ESP_FAIL;data=&v3;n=v3_size;}
    else {assert(!strcmp(key,"config_v4"));if(!persisted_size)return ESP_FAIL;data=persisted;n=persisted_size;}
    if(!out){*size=n;return ESP_OK;}if(*size<n)return ESP_FAIL;memcpy(out,data,n);*size=n;return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t n){assert(h==1&&opened&&n<=sizeof(staged));if(fail_set)return ESP_FAIL;if(!strcmp(key,"home_v1")){assert(n==sizeof(staged_home));memcpy(&staged_home,data,n);staged_home_size=n;}else if(!strcmp(key,"wifi_v1")){assert(n==sizeof(staged_wifi));memcpy(&staged_wifi,data,n);staged_wifi_size=n;}else{assert(!strcmp(key,"config_v4"));memcpy(staged,data,n);staged_size=n;}return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){assert(h==1&&opened);if(fail_commit)return ESP_FAIL;if(staged_size){memcpy(persisted,staged,staged_size);persisted_size=staged_size;}if(staged_home_size){stored_home=staged_home;home_size=staged_home_size;}if(staged_wifi_size){stored_wifi=staged_wifi;wifi_size=staged_wifi_size;}++commits;return ESP_OK;}
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
    // A font shrink must not reset profiles or macros stored by older firmware.
    kp_config_t rare=c;
    memcpy(rare.data+rare.profiles[1].name,"龘字",7);
    memcpy(rare.data+rare.profiles[0].bindings[8].title,"靐字",7);
    assert(kp_config_valid(&rare));memcpy(persisted,&rare,kp_config_size(&rare));
    assert(kp_store_init()==ESP_OK);kp_store_get(&actual);assert(!memcmp(&actual,&rare,sizeof(actual)));
    assert(!kp_profile_name_set(&actual,1,"龘字")&&!kp_binding_set(&actual,0,8,"靐字",NULL,0));
    c.version=99;assert(kp_store_save(&c)==ESP_ERR_INVALID_ARG);
    // Valid v1 survives failed migration commits and takes priority over defaults.
    persisted_size=0;legacy=(kp_legacy_config_t){.version=1};legacy.bindings[0]=(kp_action_t){.type=KP_KEYBOARD,.keys={0x52}};legacy_valid=true;
    fail_commit=true;assert(kp_store_init()==ESP_OK);kp_store_get(&actual);kp_step_t s;
    assert(!persisted_size&&kp_binding_step(&actual,kp_binding(&actual,0),0,&s)&&s.action.keys[0]==0x52);
    fail_commit=false;int previous=commits;assert(kp_store_init()==ESP_OK&&commits==previous+1);
    assert(legacy_valid&&legacy.bindings[0].keys[0]==0x52);assert(kp_store_init()==ESP_OK&&commits==previous+1);
    // Multi-profile v2 uses its exact disk layout and discards only descriptions.
    persisted_size=0;memset(&v2,0,sizeof(v2));v2.version=2;v2.used=1;v2.profile_count=8;v2.active_profile=7;
    for(unsigned p=0;p<8;p++){
        char name[16];sprintf(name,"情景%u",p);v2.profiles[p].name=v2.used;memcpy(v2.data+v2.used,name,strlen(name)+1);v2.used+=strlen(name)+1;
        for(unsigned i=0;i<8;i++){
            kp_v2_binding_t *b=&v2.profiles[p].bindings[i];b->title=v2.used;memcpy(v2.data+v2.used,"标题",7);v2.used+=7;
            b->description=v2.used;memcpy(v2.data+v2.used,"旧描述",10);v2.used+=10;
            b->count=(p+i)%3; if(!b->count)continue;
            b->steps=(v2.used+1)&~1U;
            for(unsigned j=0;j<b->count;j++){kp_step_t step={{.type=KP_KEYBOARD,.keys={4+i}},j+1==b->count?0:137};memcpy(v2.data+b->steps+j*sizeof(step),&step,sizeof(step));}
            v2.used=b->steps+b->count*sizeof(kp_step_t);
        }
    }
    // A v2 blob at the 6 KiB boundary is still valid input; the smaller header preserves even its entire arena.
    v2.used=sizeof(v2.data);v2_size=sizeof(v2);kp_v2_config_t retained=v2;
    fail_set=true;assert(kp_store_init()==ESP_OK&&!persisted_size);fail_set=false;
    fail_commit=true;assert(kp_store_init()==ESP_OK&&!persisted_size);kp_store_get(&actual);
    assert(actual.version==4&&actual.profile_count==8&&actual.active_profile==7&&kp_config_valid(&actual));
    assert(kp_config_size(&actual)<KP_CONFIG_BYTES&&!memcmp(&v2,&retained,sizeof(v2)));
    for(unsigned p=0;p<8;p++){char name[16];sprintf(name,"情景%u",p);assert(!strcmp(kp_text(&actual,actual.profiles[p].name),name));}
    for(unsigned p=0;p<8;p++)for(unsigned i=0;i<8;i++){
        const kp_binding_t *b=&actual.profiles[p].bindings[(i/3)*4+(i%3==2?3:i%3)];assert(!strcmp(kp_text(&actual,b->title),"标题")&&b->count==(p+i)%3);
        for(unsigned j=0;j<b->count;j++){assert(kp_binding_step(&actual,b,j,&s)&&s.action.keys[0]==4+i&&s.delay_ms==(j+1==b->count?0:137));}
    }
    original=actual;assert(kp_store_init()==ESP_OK);kp_store_get(&actual);assert(!memcmp(&actual,&original,sizeof(actual)));
    fail_commit=false;previous=commits;assert(kp_store_init()==ESP_OK&&commits==previous+1&&!memcmp(&v2,&retained,sizeof(v2)));
    assert(kp_store_init()==ESP_OK&&commits==previous+1);kp_store_get(&actual);assert(!memcmp(&actual,&original,sizeof(actual)));
    // Reject malformed legacy metadata/steps before touching the destination.
    kp_config_t sentinel=original;v2.active_profile=8;assert(!kp_migrate_v2(&v2,v2_size,&sentinel)&&!memcmp(&sentinel,&original,sizeof(sentinel)));v2=retained;
    assert(!kp_migrate_v2(&v2,v2_size-1,&sentinel));
    v2.profiles[0].bindings[1].reserved=1;assert(!kp_migrate_v2(&v2,v2_size,&sentinel));v2=retained;
    kp_step_t bad={{.type=KP_KEYBOARD,.keys={0x66}},0};memcpy(v2.data+v2.profiles[0].bindings[1].steps,&bad,sizeof(bad));assert(!kp_migrate_v2(&v2,v2_size,&sentinel));v2=retained;
    // Invalid/truncated v3 falls back to retained v2, then invalid v2 to v1.
    ((kp_config_t *)persisted)->version=99;assert(kp_store_init()==ESP_OK);kp_store_get(&actual);assert(actual.active_profile==7);
    persisted_size=10;v2.profiles[0].bindings[0].title=v2.used;assert(kp_store_init()==ESP_OK);kp_store_get(&actual);
    assert(actual.profile_count==1&&kp_binding_step(&actual,kp_binding(&actual,0),0,&s)&&s.action.keys[0]==0x52);
    v2=retained;persisted_size=0;v2_size=536;legacy_valid=false;assert(kp_store_init()==ESP_OK);kp_store_get(&actual);kp_defaults(&original);assert(!memcmp(&actual,&original,sizeof(actual)));
    // A fully occupied v2 arena, with 465 live steps and a live description, migrates without loss.
    memset(&v2,0,sizeof(v2));v2.version=2;v2.profile_count=1;v2.used=28;v2.profiles[0].name=1;
    memcpy(v2.data+1,"A",2);memcpy(v2.data+3,"123456789012345678901234",25);v2.profiles[0].bindings[0].description=3;
    for(unsigned i=0;i<8;i++){
        kp_v2_binding_t *b=&v2.profiles[0].bindings[i];b->steps=v2.used;b->count=i==7?17:64;
        for(unsigned j=0;j<b->count;j++){kp_step_t step={{.type=KP_KEYBOARD,.keys={4}},j+1==b->count?0:5000};memcpy(v2.data+v2.used,&step,sizeof(step));v2.used+=sizeof(step);}
    }
    assert(offsetof(kp_v2_config_t,data)+v2.used==KP_CONFIG_BYTES&&kp_migrate_v2(&v2,sizeof(v2),&actual));
    assert(kp_config_size(&actual)==6000&&kp_config_valid(&actual));
    for(unsigned i=0;i<8;i++){assert(kp_binding(&actual,(i/3)*4+(i%3==2?3:i%3))->count==(i==7?17:64));assert(kp_binding_step(&actual,kp_binding(&actual,(i/3)*4+(i%3==2?3:i%3)),0,&s)&&s.action.keys[0]==4&&s.delay_ms==5000);}
    // v2 migration also preserves formerly supported names and action data.
    v2=retained;memcpy(v2.data+v2.profiles[0].name,"龘字0",8);
    memcpy(v2.data+v2.profiles[0].bindings[1].title,"靐字",7);
    memcpy(v2.data+v2.profiles[0].bindings[1].description,"龘描述",10);
    assert(kp_migrate_v2(&v2,sizeof(v2),&actual)&&kp_config_valid(&actual));
    assert(actual.profile_count==8&&actual.active_profile==7);
    assert(!strcmp(kp_text(&actual,actual.profiles[0].name),"龘字0"));
    const kp_binding_t *restored=&actual.profiles[0].bindings[1];
    assert(!strcmp(kp_text(&actual,restored->title),"靐字")&&restored->count==1);
    assert(kp_binding_step(&actual,restored,0,&s)&&s.action.keys[0]==5&&s.delay_ms==0);
    original=actual;v2.data[v2.profiles[0].name]=0xc0;
    assert(!kp_migrate_v2(&v2,sizeof(v2),&actual)&&!memcmp(&actual,&original,sizeof(actual)));
    // A fully live 6 KiB v3 blob uses its fixed layout and all eight scenarios.
    persisted_size=0;v2_size=0;memset(&v3,0,sizeof(v3));v3.version=3;v3.profile_count=8;v3.active_profile=7;v3.used=1;
    for(unsigned p=0;p<8;p++){v3.profiles[p].name=v3.used;v3.data[v3.used++]=(uint8_t)('A'+p);v3.data[v3.used++]=0;}
    memcpy(v3.data+17,"12345",6);v3.used=24;
    for(unsigned p=0;p<8;p++){
        kp_v3_binding_t *b=&v3.profiles[p].bindings[p];b->title=17;b->steps=v3.used;b->count=p==7?28:64;
        for(unsigned j=0;j<b->count;j++){kp_step_t step={{.type=KP_KEYBOARD,.modifiers=(uint8_t)p,.keys={4+p}},j+1==b->count?0:180};memcpy(v3.data+v3.used,&step,sizeof(step));v3.used+=sizeof(step);}
    }
    assert(v3.used==sizeof(v3.data));v3_size=sizeof(v3);kp_v3_config_t retained_v3=v3;
    for(unsigned fault=0;fault<2;fault++){
        fail_set=fault==0;fail_commit=fault==1;assert(kp_store_init()==ESP_OK&&!persisted_size);kp_store_get(&actual);
        assert(actual.version==4&&actual.profile_count==8&&actual.active_profile==7&&kp_config_valid(&actual));
        assert(kp_config_size(&actual)==6128&&!memcmp(&v3,&retained_v3,sizeof(v3)));
        for(unsigned p=0;p<8;p++){
            const kp_binding_t *b=&actual.profiles[p].bindings[(p/3)*4+(p%3==2?3:p%3)];
            assert(b->count==(p==7?28:64)&&!strcmp(kp_text(&actual,b->title),"12345"));
            for(unsigned j=0;j<b->count;j++)assert(kp_binding_step(&actual,b,j,&s)&&s.action.keys[0]==4+p&&s.action.modifiers==p&&s.delay_ms==(j+1==b->count?0:180));
            for(unsigned g=2;g<11;g+=4)assert(!actual.profiles[p].bindings[g].count&&!actual.profiles[p].bindings[g].title);
        }
        original=actual;
    }
    fail_set=fail_commit=false;previous=commits;assert(kp_store_init()==ESP_OK&&commits==previous+1);
    kp_store_get(&actual);assert(!memcmp(&actual,&original,sizeof(actual)));
    assert(kp_store_init()==ESP_OK&&commits==previous+1&&!memcmp(&v3,&retained_v3,sizeof(v3)));
    // Every rejected v3 migration leaves its destination untouched.
    for(unsigned fault=0;fault<8;fault++){
        v3=retained_v3;
        if(fault==0)v3.version=4;else if(fault==1)v3.active_profile=8;
        else if(fault==2)v3.profiles[0].name=v3.used;else if(fault==3)v3.profiles[1].name=v3.profiles[0].name;
        else if(fault==4)v3.profiles[0].bindings[0].reserved=1;
        else if(fault==5)v3.profiles[0].bindings[0].steps++;
        else if(fault==6)v3.profiles[0].bindings[0].count=65;else v3.data[v3.profiles[0].name]=0xc0;
        actual=original;assert(!kp_migrate_v3(&v3,sizeof(v3),&actual)&&!memcmp(&actual,&original,sizeof(actual)));
    }
    v3=retained_v3;assert(!kp_migrate_v3(&v3,sizeof(v3)-1,&actual)&&!memcmp(&actual,&original,sizeof(actual)));
    // Shared legacy offsets stay shared: migration does not duplicate live data.
    v3.profiles[1].bindings[0]=v3.profiles[0].bindings[0];assert(kp_migrate_v3(&v3,sizeof(v3),&actual));
    assert(actual.profiles[1].bindings[0].steps==actual.profiles[0].bindings[0].steps&&kp_config_valid(&actual));
    // Home metadata cannot consume or reset even a full shortcut record.
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
    home_size=sizeof(home)-1;assert(kp_store_init()==ESP_OK);kp_store_home_get(&home);kp_home_defaults(&saved_home);
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
    stored_wifi.version=99;assert(kp_store_init()==ESP_OK);kp_store_wifi_get(&next_wifi);assert(next_wifi.version==1&&!next_wifi.ssid[0]);
    stored_wifi.version=1;wifi_size--;assert(kp_store_init()==ESP_OK);kp_store_wifi_get(&next_wifi);assert(!next_wifi.ssid[0]);
    puts("Wi-Fi NVS isolation, cold boot, save failures, clearing and corrupt records: PASS");
    puts("Home NVS defaults, restart, isolated saves, failed commits and clock invalidation: PASS");
    puts("Keypad NVS v1/v2/v3/v4 migration, profile persistence and failure recovery: PASS");
}
