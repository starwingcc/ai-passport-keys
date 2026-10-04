#include "keypad_config.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    kp_config_t c, decoded; kp_defaults(&c);
    cJSON *root=kp_config_json(&c); assert(root);
    assert(kp_config_parse(root,&decoded)); assert(!memcmp(&c,&decoded,sizeof(c)));
    cJSON *list=cJSON_GetObjectItem(root,"bindings");
    cJSON_AddItemToArray(list,cJSON_Duplicate(cJSON_GetArrayItem(list,0),true));
    assert(!kp_config_parse(root,&decoded)); // Ninth binding could overwrite reserved OK hold.
    cJSON_DeleteItemFromArray(list,8);
    cJSON *a=cJSON_GetArrayItem(list,0);
    cJSON_SetNumberValue(cJSON_GetObjectItem(a,"modifiers"),256); assert(!kp_config_parse(root,&decoded));
    cJSON_SetNumberValue(cJSON_GetObjectItem(a,"modifiers"),1.5); assert(!kp_config_parse(root,&decoded));
    cJSON_SetNumberValue(cJSON_GetObjectItem(a,"modifiers"),0);
    cJSON *keys=cJSON_GetObjectItem(a,"keys");
    cJSON_AddItemToArray(keys,cJSON_CreateNumber(c.bindings[0].keys[0])); assert(!kp_config_parse(root,&decoded));
    cJSON_DeleteItemFromArray(keys,1);
    cJSON_AddNumberToObject(a,"ok_long",1); assert(!kp_config_parse(root,&decoded));
    cJSON_DeleteItemFromObject(a,"ok_long");
    cJSON_AddNumberToObject(root,"version",1); assert(!kp_config_parse(root,&decoded));
    cJSON_Delete(root);
    assert(!kp_config_parse(NULL,&decoded));
    puts("Keypad JSON validation: PASS");
}
