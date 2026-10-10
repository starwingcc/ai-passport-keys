#include "keypad_transfer_signal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool saw_rtc_false;static unsigned sends;static int last_fd;static char last[KP_TRANSFER_MESSAGE+1];
static bool send_message(void *user,int fd,const char *data,size_t size){(void)user;assert(size<=KP_TRANSFER_MESSAGE);++sends;last_fd=fd;memcpy(last,data,size);last[size]=0;if(strstr(last,"\"rtc\":false"))saw_rtc_false=true;return true;}
static uint64_t mono(uint32_t now){return (uint64_t)UINT32_MAX+1+now;}
static bool message(kp_transfer_signal_t *s,int fd,const char *data,uint32_t now){return kp_transfer_signal_message(s,fd,data,strlen(data),mono(now));}
int main(void){
    kp_transfer_signal_t s;kp_transfer_signal_init(&s,send_message,NULL);
    for(int fd=10;fd<14;++fd)assert(kp_transfer_signal_open(&s,fd,UINT32_MAX-20));
    assert(!kp_transfer_signal_open(&s,14,0)&&!kp_transfer_signal_open(&s,10,0)&&kp_transfer_signal_count(&s)==0);
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
    kp_transfer_signal_close(&s,10);assert(kp_transfer_signal_count(&s)==1&&!s.peers[0].catalog[0]);assert(kp_transfer_signal_open(&s,20,mono(10))&&s.peers[0].id==5);
    kp_transfer_signal_end(&s);assert(!strcmp(last,"{\"type\":\"end\"}"));
    assert(kp_transfer_signal_online(&s,mono(KP_TRANSFER_IDLE_MS+10))==0);
    assert(!message(&s,11,"{\"type\":\"ping\"}",KP_TRANSFER_IDLE_MS+10));
    kp_transfer_signal_init(&s,send_message,NULL);assert(kp_transfer_signal_open(&s,30,UINT64_MAX-20));
    const char hello[]="{\"type\":\"hello\",\"name\":\"wrap\"}";
    assert(kp_transfer_signal_message(&s,30,hello,strlen(hello),0));
    assert(kp_transfer_signal_online(&s,119999)==1&&kp_transfer_signal_online(&s,120000)==0);
    puts("Transfer signaling: bounded sessions, catalogs, SDP/ICE, sender identity, content rejection, wrap and shutdown: PASS");
}
