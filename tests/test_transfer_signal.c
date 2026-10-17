#include "keypad_transfer_signal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cJSON.h"
static bool fail_print;
static void *json_malloc(size_t size){return fail_print&&size==256?NULL:malloc(size);}
static bool saw_rtc_false;static unsigned sends;static int last_fd;static char last[KP_TRANSFER_MESSAGE+1];
static bool send_message(void *user,int fd,const char *data,size_t size){(void)user;assert(size<=KP_TRANSFER_MESSAGE);++sends;last_fd=fd;memcpy(last,data,size);last[size]=0;if(strstr(last,"\"rtc\":false"))saw_rtc_false=true;return true;}
static uint64_t mono(uint32_t now){return (uint64_t)UINT32_MAX+1+now;}
static bool message(kp_transfer_signal_t *s,int fd,const char *data,uint32_t now){return kp_transfer_signal_message(s,fd,data,strlen(data),mono(now));}
int main(void){
    kp_transfer_signal_t s;kp_transfer_signal_init(&s,send_message,NULL);
    assert(sizeof(s)<512); // Empty four-page service must not reserve 9 KiB of catalogs.
    for(int fd=10;fd<14;++fd)assert(kp_transfer_signal_open(&s,fd,UINT32_MAX-20));
    assert(!kp_transfer_signal_open(&s,14,0)&&!kp_transfer_signal_open(&s,10,0)&&kp_transfer_signal_count(&s)==0);
    for(unsigned i=0;i<KP_TRANSFER_PEERS;++i)assert(!s.peers[i].catalog);
    assert(!message(&s,10,"{\"type\":\"ping\"}",0));
    assert(message(&s,10,"{\"type\":\"hello\",\"name\":\"手机😀\"}",0));
    assert(message(&s,11,"{\"type\":\"hello\",\"name\":\"电脑\"}",0));assert(kp_transfer_signal_count(&s)==2);
    assert(s.peers[0].rtc&&s.peers[1].rtc);
    assert(message(&s,11,"{\"type\":\"hello\",\"name\":\"电脑\",\"rtc\":false}",0));
    assert(!s.peers[1].rtc&&kp_transfer_signal_count(&s)==2&&saw_rtc_false);
    assert(message(&s,11,"{\"type\":\"ping\"}",0));
    assert(!message(&s,11,"{\"type\":\"hello\",\"name\":\"电脑\",\"rtc\":1}",0));
    assert(message(&s,11,"{\"type\":\"hello\",\"name\":\"电脑\",\"rtc\":true}",0));
    assert(message(&s,10,"{\"type\":\"signal\",\"to\":2,\"request\":true}",1));assert(last_fd==11&&strstr(last,"\"from\":1")&&!strstr(last,"\"to\""));
    assert(message(&s,11,"{\"type\":\"signal\",\"to\":1,\"description\":{\"type\":\"offer\",\"sdp\":\"v=0\\r\\n\"}}",2));assert(last_fd==10&&strstr(last,"offer"));
    assert(message(&s,10,"{\"type\":\"signal\",\"to\":2,\"candidate\":{\"candidate\":\"host.local\",\"sdpMid\":\"0\",\"sdpMLineIndex\":0,\"usernameFragment\":\"abcd\"}}",3));
    assert(message(&s,10,"{\"type\":\"files\",\"files\":[{\"id\":\"binary\",\"name\":\"图片.png\",\"size\":104857600,\"mime\":\"image/png\",\"to\":0}]}",4));assert(strstr(last,"104857600"));
    char *saved=s.peers[0].catalog;assert(saved&&strstr(saved,"binary"));
    cJSON_Hooks hooks={.malloc_fn=json_malloc,.free_fn=free};cJSON_InitHooks(&hooks);
    fail_print=true;assert(!message(&s,10,"{\"type\":\"files\",\"files\":[]}",4));
    assert(s.peers[0].catalog==saved&&strstr(saved,"binary"));fail_print=false;cJSON_InitHooks(NULL);
    // A valid but oversized catalog must also preserve the previous contents.
    char oversized[4096],name[181],mime[81];memset(name,'n',180);name[180]=0;memset(mime,'m',80);mime[80]=0;
    size_t used=(size_t)snprintf(oversized,sizeof(oversized),"{\"type\":\"files\",\"files\":[");
    for(unsigned i=0;i<8;++i)used+=(size_t)snprintf(oversized+used,sizeof(oversized)-used,"%s{\"id\":\"f%u\",\"name\":\"%s\",\"size\":0,\"mime\":\"%s\",\"to\":0}",i?",":"",i,name,mime);
    snprintf(oversized+used,sizeof(oversized)-used,"]}");assert(!message(&s,10,oversized,4)&&s.peers[0].catalog==saved);
    assert(message(&s,11,"{\"type\":\"files\",\"files\":[{\"id\":\"x\",\"name\":\"x\",\"size\":0,\"mime\":\"\",\"to\":0}]}",4)&&s.peers[1].catalog);
    assert(message(&s,11,"{\"type\":\"files\",\"files\":[]}",4)&&!s.peers[1].catalog&&strstr(last,"\"files\":[]"));
    const char *invalid[]={"{\"type\":\"text\",\"text\":\"content\"}","{\"type\":\"signal\",\"to\":2,\"from\":3,\"request\":true}","{\"type\":\"signal\",\"to\":1,\"request\":true}","{\"type\":\"ping\",\"type\":\"hello\"}","{\"type\":\"ping\"}{}","{\"type\":\"files\",\"files\":[{\"id\":\"f\",\"name\":\"x\",\"size\":-1,\"mime\":\"\",\"to\":0}]}","{\"type\":\"files\",\"files\":[],\"body\":\"data\"}","{\"type\":\"signal\",\"to\":2,\"request\":true,\"candidate\":{}}"};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);++i){unsigned before=sends;assert(!message(&s,10,invalid[i],5));assert(sends==before);}
    // A receiver closing while SDP/ICE is in flight must not disconnect its sender.
    assert(message(&s,10,"{\"type\":\"signal\",\"to\":9,\"request\":true}",5));
    assert(last_fd==10&&strstr(last,"\"type\":\"error\"")&&s.peers[0].joined);
    assert(message(&s,10,"{\"type\":\"ping\"}",5));
    const char deep[]="{\"type\":\"files\",\"files\":[[[[[[]]]]]]}";
    assert(!message(&s,10,deep,5));
    char wide[1000];strcpy(wide,"{\"type\":\"files\",\"files\":[");
    for(unsigned i=0;i<200;++i)strcat(wide,i?",0":"0");strcat(wide,"]}");assert(!message(&s,10,wide,5));
    assert(message(&s,10,"{\"type\":\"hello\",\"name\":\"braces {} [] \\\" quote\"}",5));
    char huge[KP_TRANSFER_MESSAGE+2];memset(huge,'x',sizeof(huge));assert(!kp_transfer_signal_message(&s,10,huge,sizeof(huge),mono(5)));
    assert(kp_transfer_signal_expired(&s,mono(KP_TRANSFER_IDLE_MS-22))==-1);assert(kp_transfer_signal_expired(&s,mono(KP_TRANSFER_IDLE_MS-21))==12);
    kp_transfer_signal_close(&s,10);assert(kp_transfer_signal_count(&s)==1&&!s.peers[0].catalog);assert(kp_transfer_signal_open(&s,20,mono(10))&&s.peers[0].id==5);
    kp_transfer_signal_end(&s);assert(!strcmp(last,"{\"type\":\"end\"}"));
    assert(kp_transfer_signal_online(&s,mono(KP_TRANSFER_IDLE_MS+10))==0);
    assert(!message(&s,11,"{\"type\":\"ping\"}",KP_TRANSFER_IDLE_MS+10));
    kp_transfer_signal_clear(&s);kp_transfer_signal_init(&s,send_message,NULL);assert(kp_transfer_signal_open(&s,30,UINT64_MAX-20));
    const char hello[]="{\"type\":\"hello\",\"name\":\"wrap\"}";
    assert(kp_transfer_signal_message(&s,30,hello,strlen(hello),0));
    assert(kp_transfer_signal_online(&s,119999)==1&&kp_transfer_signal_online(&s,120000)==0);
    kp_transfer_signal_clear(&s);
    // Repeated nonempty catalog teardown must free ownership, without sending.
    for(unsigned i=0;i<100;++i){
        kp_transfer_signal_init(&s,send_message,NULL);assert(kp_transfer_signal_open(&s,10,mono(0)));
        assert(message(&s,10,"{\"type\":\"hello\",\"name\":\"test\"}",0));
        assert(message(&s,10,"{\"type\":\"files\",\"files\":[{\"id\":\"x\",\"name\":\"x\",\"size\":0,\"mime\":\"\",\"to\":0}]}",0));
        assert(s.peers[0].catalog);unsigned before=sends;
        if(i%2)kp_transfer_signal_discard(&s,10);else kp_transfer_signal_clear(&s);
        assert(!s.peers[0].catalog&&!s.peers[0].id&&sends==before);
    }
    puts("Transfer signaling: bounded sessions, catalogs, SDP/ICE, sender identity, content rejection, wrap and shutdown: PASS");
}
