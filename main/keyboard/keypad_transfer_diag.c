#include "keypad_transfer_diag.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/tcpip.h"
#include "lwip/priv/tcp_priv.h"
#include <stdatomic.h>
#include <stdbool.h>

static atomic_bool s_pending;
// Caller is the HTTPD task. The callback alone walks the pinned IDF 5.5.3 TCP
// lists, on the tcpip task: traversing these from HTTPD would race with free().
static uint32_t s_last;
static bool s_logged;
static void snapshot(void *arg) {
    (void)arg;
    unsigned active=0,closing=0,timewait=0,queued=0,retransmits=0;
    for(const struct tcp_pcb *p=tcp_active_pcbs;p;p=p->next)if(p->local_port==80) {
        ++active;
        closing+=p->state>=FIN_WAIT_1;
        queued+=TCP_SND_BUF-tcp_sndbuf(p);
        if(p->nrtx>retransmits)retransmits=p->nrtx;
    }
    for(const struct tcp_pcb *p=tcp_tw_pcbs;p;p=p->next)if(p->local_port==80)++timewait;
    ESP_LOGW("transfer","TCP snapshot active=%u closing=%u timewait=%u queued_bytes=%u max_retries=%u",
        active,closing,timewait,queued,retransmits);
    atomic_store(&s_pending,false);
}
void kp_transfer_diag_request(void) {
    uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
    if(s_logged&&(uint32_t)(now-s_last)<5000U)return;
    if(atomic_exchange(&s_pending,true))return;
    s_last=now;s_logged=true;
    if(tcpip_try_callback(snapshot,NULL)!=ERR_OK) {
        atomic_store(&s_pending,false);
        ESP_LOGW("transfer","TCP snapshot unavailable (tcpip queue/allocation pressure)");
    }
}
