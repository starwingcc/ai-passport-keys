#include "keypad_config.h"
#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char encoded[KP_SETTINGS_JSON_BYTES+1];static size_t size;
static bool write_json(void *user,const char *data,size_t n){(void)user;assert(size+n<=KP_SETTINGS_JSON_BYTES);memcpy(encoded+size,data,n);size+=n;encoded[size]=0;return true;}
static bool decode(cJSON *json,kp_config_t *out){char *s=cJSON_PrintUnformatted(json);assert(s);bool valid=kp_config_decode_string(s,out);free(s);return valid;}
typedef struct {const char *text;size_t pos;} reader_t;
static int one_byte(void *user,char *out,size_t max){(void)max;reader_t *r=user;if(!r->text[r->pos])return 0;*out=r->text[r->pos++];return 1;}
int main(int argc,char **argv){
    if(argc==2&&!strcmp(argv[1],"--settings-stdin")){
        size_t n=fread(encoded,1,sizeof(encoded),stdin);if(n>KP_SETTINGS_JSON_BYTES)return 1;encoded[n]=0;
        kp_settings_t settings;if(!kp_settings_decode_string(encoded,&settings))return 1;
        size=0;assert(kp_settings_encode(&settings,write_json,NULL));printf("{\"settings\":%s}\n",encoded);return 0;
    }
    if(argc==2&&!strcmp(argv[1],"--stdin")){
        size_t n=fread(encoded,1,KP_JSON_BYTES+1,stdin);if(n>KP_JSON_BYTES)return 1;encoded[n]=0;
        kp_config_t d;if(!kp_config_decode_string(encoded,&d))return 1;
        size=0;assert(kp_config_encode(&d,write_json,NULL));printf("{\"bytes\":%zu,\"config\":%s}\n",kp_config_size(&d),encoded);return 0;
    }
    if(argc==2&&!strcmp(argv[1],"--defaults")){kp_config_t d;kp_defaults(&d);assert(kp_config_encode(&d,write_json,NULL));printf("{\"bytes\":%zu,\"config\":%s}\n",kp_config_size(&d),encoded);return 0;}
    // Lock the current packed word layout.
    kp_binding_t descriptor={.title=5735,.steps=2868,.count=64};uint32_t word;
    memcpy(&word,&descriptor,sizeof(word));assert(word==(5735U|(2868U<<13)|(64U<<25)));
    kp_config_t c,decoded;kp_defaults(&c);assert(kp_config_encode(&c,write_json,NULL));
    assert(kp_config_decode_string(encoded,&decoded));
    assert(decoded.profile_count==1&&decoded.active_profile==0&&kp_binding(&decoded,8)->count==1);
    reader_t reader={encoded,0};assert(kp_config_decode(one_byte,&reader,&decoded));
    cJSON *root=cJSON_Parse(encoded);assert(root);
    cJSON *profiles=cJSON_GetObjectItem(root,"profiles"),*profile=cJSON_GetArrayItem(profiles,0);
    cJSON *bindings=cJSON_GetObjectItem(profile,"bindings"),*binding=cJSON_GetArrayItem(bindings,0);
    cJSON *steps=cJSON_GetObjectItem(binding,"steps"),*step=cJSON_GetArrayItem(steps,0);
    cJSON_AddItemToArray(bindings,cJSON_Duplicate(binding,true));assert(!decode(root,&decoded));cJSON_DeleteItemFromArray(bindings,11);
    cJSON_SetNumberValue(cJSON_GetObjectItem(step,"modifiers"),256);assert(!decode(root,&decoded));
    cJSON_SetNumberValue(cJSON_GetObjectItem(step,"modifiers"),1.5);assert(!decode(root,&decoded));
    cJSON_SetNumberValue(cJSON_GetObjectItem(step,"modifiers"),0);
    cJSON *keys=cJSON_GetObjectItem(step,"keys");cJSON_AddItemToArray(keys,cJSON_CreateNumber(41));assert(!decode(root,&decoded));cJSON_DeleteItemFromArray(keys,1);
    cJSON_SetNumberValue(cJSON_GetObjectItem(step,"delay_ms"),1);assert(!decode(root,&decoded));cJSON_SetNumberValue(cJSON_GetObjectItem(step,"delay_ms"),0);
    cJSON_AddNumberToObject(step,"extra",1);assert(!decode(root,&decoded));cJSON_DeleteItemFromObject(step,"extra");
    cJSON_AddNumberToObject(root,"version",4);assert(!decode(root,&decoded));cJSON_DeleteItemFromObject(root,"version");cJSON_DeleteItemFromObject(root,"version");cJSON_AddNumberToObject(root,"version",KP_VERSION);
    cJSON_SetNumberValue(cJSON_GetObjectItem(root,"active_profile"),1);assert(!decode(root,&decoded));cJSON_SetNumberValue(cJSON_GetObjectItem(root,"active_profile"),0);
    cJSON_SetValuestring(cJSON_GetObjectItem(binding,"title"),"😀");assert(!decode(root,&decoded));
    cJSON_SetValuestring(cJSON_GetObjectItem(binding,"title"),"12345678901234567");assert(!decode(root,&decoded));
    cJSON_SetValuestring(cJSON_GetObjectItem(binding,"title"),"编程龘");assert(!decode(root,&decoded));
    cJSON_SetValuestring(cJSON_GetObjectItem(binding,"title"),"编程浏览");assert(decode(root,&decoded));
    cJSON_AddItemToArray(profiles,cJSON_Duplicate(profile,true));assert(!decode(root,&decoded));
    cJSON_SetValuestring(cJSON_GetObjectItem(cJSON_GetArrayItem(profiles,1),"name"),"浏览");assert(decode(root,&decoded));
    cJSON_AddStringToObject(binding,"description","discarded");assert(!decode(root,&decoded));
    cJSON_DeleteItemFromObject(binding,"description");
    cJSON_SetNumberValue(cJSON_GetObjectItem(root,"version"),2);assert(!decode(root,&decoded));
    cJSON_SetNumberValue(cJSON_GetObjectItem(root,"version"),KP_VERSION);
    cJSON_AddNumberToObject(step,"logical_modifiers",3);assert(decode(root,&decoded));
    kp_step_t logical;assert(kp_binding_step(&decoded,kp_binding(&decoded,0),0,&logical)&&logical.action.logical_modifiers==3);
    for(unsigned flags=4;flags<=256;flags+=252){cJSON_SetNumberValue(cJSON_GetObjectItem(step,"logical_modifiers"),flags);assert(!decode(root,&decoded));}
    cJSON_SetNumberValue(cJSON_GetObjectItem(step,"logical_modifiers"),1);
    cJSON_SetNumberValue(cJSON_GetObjectItem(step,"consumer"),1);assert(!decode(root,&decoded));
    cJSON_SetNumberValue(cJSON_GetObjectItem(step,"consumer"),0);
    cJSON_SetNumberValue(cJSON_GetObjectItem(root,"version"),4);assert(!decode(root,&decoded));
    cJSON_DeleteItemFromObject(step,"logical_modifiers");assert(!decode(root,&decoded));
    for(unsigned version=1;version<KP_VERSION;version++){
        cJSON_SetNumberValue(cJSON_GetObjectItem(root,"version"),version);assert(!decode(root,&decoded));
    }
    cJSON_SetNumberValue(cJSON_GetObjectItem(root,"version"),KP_VERSION);assert(decode(root,&decoded));
    cJSON_Delete(root);
    assert(!kp_config_decode_string("{}",&decoded));assert(!kp_config_decode_string("{\"version\":02}",&decoded));
    char *trailing=malloc(strlen(encoded)+4);assert(trailing);sprintf(trailing,"%s {}",encoded);assert(!kp_config_decode_string(trailing,&decoded));free(trailing);
    for(size_t n=0;n<strlen(encoded);n++) {char saved=encoded[n];encoded[n]=0;assert(!kp_config_decode_string(encoded,&decoded));encoded[n]=saved;}
    // Binding-level limits and atomic arena allocation on overflow.
    kp_step_t all[KP_MAX_STEPS];for(unsigned i=0;i<KP_MAX_STEPS;i++)all[i]=(kp_step_t){{.type=KP_KEYBOARD,.keys={4}},i+1==KP_MAX_STEPS?0:5000};
    kp_defaults(&c);assert(kp_binding_set(&c,0,0,"64步",all,KP_MAX_STEPS));
    assert(!kp_binding_set(&c,0,0,"",all,KP_MAX_STEPS+1));
    uint16_t before;do{before=c.used;}while(kp_binding_set(&c,0,1,"",all,KP_MAX_STEPS));assert(c.used==before&&kp_config_valid(&c));
    kp_config_empty(&c);for(unsigned i=0;i<KP_MAX_PROFILES;i++){char name[16];sprintf(name,"情景%u",i);assert(kp_profile_add(&c,name));}
    assert(!kp_profile_add(&c,"第九套"));assert(kp_config_valid(&c));
    assert(!kp_profile_add(&c,"情景0"));
    // Exactly 6 KiB persists; one more string cannot replace a binding.
    kp_config_empty(&c);assert(kp_profile_add(&c,"A"));
    for(unsigned i=0;i<7;i++)assert(kp_binding_set(&c,0,i,i?"":"12345678901",all,64));
    all[29].delay_ms=0;assert(kp_binding_set(&c,0,7,"",all,30));
    assert(kp_config_size(&c)==KP_CONFIG_BYTES&&kp_config_valid(&c));before=c.used;
    assert(!kp_binding_set(&c,0,7,"A",NULL,0)&&c.used==before&&kp_config_valid(&c));
    // The streaming parser admits the exact transport limit and rejects one extra byte.
    size_t base=strlen(encoded);char *limit=malloc(KP_JSON_BYTES+2);assert(limit);memcpy(limit,encoded,base);
    memset(limit+base,' ',KP_JSON_BYTES-base);limit[KP_JSON_BYTES]=0;assert(kp_config_decode_string(limit,&decoded));
    limit[KP_JSON_BYTES]=' ';limit[KP_JSON_BYTES+1]=0;assert(!kp_config_decode_string(limit,&decoded));free(limit);
    assert(!kp_config_decode_string(NULL,&decoded));
    puts("Keypad streaming JSON, strict fields, Unicode and capacity: PASS");
    kp_settings_t settings, restored_settings;kp_settings_defaults(&settings);settings.config=c;
    strcpy(settings.home.name,"星翼");strcpy(settings.wifi.ssid,"test-network");strcpy(settings.wifi.password,"test-pass");
    size=0;assert(kp_settings_encode(&settings,write_json,NULL));
    assert(kp_settings_decode_string(encoded,&restored_settings));
    assert(kp_settings_size(&restored_settings)==offsetof(kp_settings_t,config)+KP_CONFIG_BYTES);
    assert(!memcmp(&settings,&restored_settings,sizeof(settings)));
    reader=(reader_t){encoded,0};assert(kp_settings_decode(one_byte,&reader,&restored_settings));
    root=cJSON_Parse(encoded);assert(root);
    cJSON *home=cJSON_GetObjectItem(root,"home"),*wifi=cJSON_GetObjectItem(root,"wifi");
    for(unsigned fault=0;fault<3;fault++){
        cJSON *bad=cJSON_Duplicate(root,true);
        if(!fault)cJSON_DeleteItemFromObject(bad,"wifi");
        else if(fault==1)cJSON_AddNumberToObject(bad,"version",1);
        else cJSON_SetValuestring(cJSON_GetObjectItem(cJSON_GetObjectItem(bad,"wifi"),"password"),"short");
        char *text=cJSON_PrintUnformatted(bad);assert(text&&!kp_settings_decode_string(text,&restored_settings));free(text);cJSON_Delete(bad);
    }
    assert(home&&wifi);cJSON_Delete(root);
    base=strlen(encoded);limit=malloc(KP_SETTINGS_JSON_BYTES+2);assert(limit);memcpy(limit,encoded,base);
    memset(limit+base,' ',KP_SETTINGS_JSON_BYTES-base);limit[KP_SETTINGS_JSON_BYTES]=0;assert(kp_settings_decode_string(limit,&restored_settings));
    limit[KP_SETTINGS_JSON_BYTES]=' ';limit[KP_SETTINGS_JSON_BYTES+1]=0;assert(!kp_settings_decode_string(limit,&restored_settings));free(limit);
    for(size_t n=0;n<strlen(encoded);n++){char byte=encoded[n];encoded[n]=0;assert(!kp_settings_decode_string(encoded,&restored_settings));encoded[n]=byte;}
    puts("Complete settings JSON round trip, credentials, 6 KiB shortcut arena, strict fields and transport bounds: PASS");
}
