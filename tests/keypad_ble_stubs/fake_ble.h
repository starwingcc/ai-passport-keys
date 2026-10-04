#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_NO_MEM 2
#define ESP_ERR_TIMEOUT 3
#define ESP_ERR_NOT_SUPPORTED 4
#define ESP_HID_REPORT_TYPE_INPUT 1
#define ESP_HID_TRANSPORT_BLE 1
#define ESP_HID_PROTOCOL_MODE_BOOT 0
static inline void fake_log(const char *format, ...) {(void)format;}
#define ESP_LOGE(tag, ...) fake_log(__VA_ARGS__)
#define ESP_LOGI(tag, ...) fake_log(__VA_ARGS__)
#define ESP_LOGD(tag, ...) fake_log(__VA_ARGS__)
#define ESP_LOGW(tag, ...) fake_log(__VA_ARGS__)
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define pdMS_TO_TICKS(x) (x)
#define pdTRUE 1
typedef int portMUX_TYPE;
typedef int *SemaphoreHandle_t;
typedef void *esp_hidd_dev_t;
typedef const char *esp_event_base_t;
typedef struct {const uint8_t *data;size_t len;} esp_hid_raw_report_map_t;
typedef struct {const char *device_name,*manufacturer_name,*serial_number;esp_hid_raw_report_map_t *report_maps;size_t report_maps_len;} esp_hid_device_config_t;
typedef struct {uint16_t value;} ble_uuid_t;
typedef struct {ble_uuid_t u;} ble_uuid16_t;
#define BLE_UUID16_INIT(x) {{x}}
#define BLE_HS_CONN_HANDLE_NONE 0xffff
#define BLE_HS_ADV_F_DISC_GEN 1
#define BLE_HS_ADV_F_BREDR_UNSUP 2
#define BLE_GAP_CONN_MODE_UND 1
#define BLE_GAP_DISC_MODE_GEN 1
#define BLE_HS_FOREVER -1
#define BLE_HS_EALREADY 9
#define BLE_HS_EBUSY 15
#define BLE_GATT_CHR_F_NOTIFY 1
#define BLE_GATT_REGISTER_OP_CHR 1
#define BLE_GATT_REGISTER_OP_DSC 2
#define BLE_ERR_REM_USER_CONN_TERM 0x13
#define BLE_SM_IO_CAP_NO_IO 3
#define BLE_SM_PAIR_KEY_DIST_ENC 1
#define BLE_SM_PAIR_KEY_DIST_ID 2
#define BLE_GAP_REPEAT_PAIRING_IGNORE 0
enum {BLE_GAP_EVENT_CONNECT,BLE_GAP_EVENT_DISCONNECT,BLE_GAP_EVENT_ENC_CHANGE,BLE_GAP_EVENT_SUBSCRIBE,BLE_GAP_EVENT_PASSKEY_ACTION,BLE_GAP_EVENT_REPEAT_PAIRING,BLE_GAP_EVENT_ADV_COMPLETE};
typedef struct {uint8_t val[6];} ble_addr_t;
struct ble_gap_conn_desc {ble_addr_t peer_id_addr;struct {bool encrypted,authenticated,bonded;uint8_t key_size;} sec_state;};
struct ble_hs_adv_fields {uint8_t flags;uint16_t appearance;bool appearance_is_present;ble_uuid16_t *uuids16;int num_uuids16;bool uuids16_is_complete;uint8_t *name;int name_len;bool name_is_complete;};
struct ble_gap_adv_params {int conn_mode,disc_mode,itvl_min,itvl_max;};
struct fake_chr {const ble_uuid_t *uuid;int flags;};
struct fake_dsc {const ble_uuid_t *uuid;void *arg;};
struct ble_gatt_register_ctxt {int op;struct {struct fake_chr *chr_def;uint16_t val_handle;} chr;struct {struct fake_dsc *dsc_def;uint16_t handle;} dsc;};
struct ble_gap_event {int type;struct {int status;uint16_t conn_handle;} connect,enc_change;struct {int reason;} disconnect;struct {uint16_t attr_handle;bool cur_notify;} subscribe;struct {uint16_t conn_handle;} passkey;};
struct ble_npl_event {void (*fn)(struct ble_npl_event*);};
struct os_mbuf {uint8_t data[8];size_t len;};
struct {void (*reset_cb)(int);void (*sync_cb)(void);void (*gatts_register_cb)(struct ble_gatt_register_ctxt*,void*);int sm_io_cap,sm_bonding,sm_mitm,sm_sc,sm_our_key_dist,sm_their_key_dist;} ble_hs_cfg;
static int fake_sem, fake_adv, fake_notify_count, fake_terminate_count;
static bool fake_defer, fake_fail_alloc, fake_fail_clear;
static uint8_t fake_protocol=1;
static uint8_t fake_report_ref[2];
static uint16_t fake_descriptor_handle;
static struct ble_npl_event *fake_pending;
static struct ble_gap_conn_desc fake_desc;
static uint8_t fake_last[8];
static uint16_t fake_last_handle;
static int fake_bonds;
static int fake_connections, fake_adv_error;
static ble_addr_t fake_peer;
static inline SemaphoreHandle_t xSemaphoreCreateBinary(void){fake_sem=0;return &fake_sem;}
static inline int xSemaphoreTake(SemaphoreHandle_t s,int t){(void)t;if(!*s)return 0;*s=0;return 1;}
static inline void xSemaphoreGive(SemaphoreHandle_t s){*s=1;}
static inline void vTaskDelay(int t){(void)t;}
static inline int ble_uuid_u16(const ble_uuid_t *u){return u->value;}
static inline bool ble_gap_adv_active(void){return fake_adv;}
static inline int ble_gap_adv_set_fields(const struct ble_hs_adv_fields *f){(void)f;return 0;}
static inline void ble_gap_conn_foreach_handle(int(*cb)(uint16_t,void*),void *arg){if(fake_connections)cb(0,arg);}
static inline int ble_gap_adv_start(uint8_t t,void*p,int d,const struct ble_gap_adv_params*a,int(*cb)(struct ble_gap_event*,void*),void*u){(void)t;(void)p;(void)d;(void)a;(void)cb;(void)u;if(fake_adv_error)return fake_adv_error;fake_adv=1;return 0;}
static inline int ble_gap_adv_stop(void){fake_adv=0;return 0;}
static inline int ble_hs_id_infer_auto(int x,uint8_t *t){(void)x;*t=0;return 0;}
static inline int ble_gap_conn_find(uint16_t h,struct ble_gap_conn_desc*d){(void)h;*d=fake_desc;return 0;}
static inline int ble_gap_terminate(uint16_t h,int reason){(void)h;(void)reason;fake_terminate_count++;return 0;}
static inline int ble_gap_security_initiate(uint16_t h){(void)h;return 0;}
static inline int ble_store_util_bonded_peers(ble_addr_t*p,int*n,int max){(void)max;*n=fake_bonds;*p=fake_peer;return 0;}
static inline int ble_addr_cmp(const ble_addr_t*a,const ble_addr_t*b){return memcmp(a,b,sizeof(*a));}
static inline int ble_store_clear(void){if(fake_fail_clear)return -1;fake_bonds=0;return 0;}
static inline struct os_mbuf *ble_hs_mbuf_from_flat(const void *p,size_t n){if(fake_fail_alloc)return NULL;struct os_mbuf *b=calloc(1,sizeof(*b));if(b){memcpy(b->data,p,n);b->len=n;}return b;}
static inline void os_mbuf_free_chain(struct os_mbuf*b){free(b);}
static inline int ble_gatts_notify_custom(uint16_t conn,uint16_t h,struct os_mbuf*b){(void)conn;fake_notify_count++;fake_last_handle=h;memcpy(fake_last,b->data,b->len);free(b);return 0;}
static inline int ble_att_svr_read_local(uint16_t h,struct os_mbuf**b){*b=h==fake_descriptor_handle?ble_hs_mbuf_from_flat(fake_report_ref,2):ble_hs_mbuf_from_flat(&fake_protocol,1);return *b?0:1;}
static inline int ble_hs_mbuf_to_flat(struct os_mbuf*b,void*p,size_t n,void*out){(void)out;if(b->len<n)return 1;memcpy(p,b->data,n);return 0;}
static inline void ble_npl_event_init(struct ble_npl_event*e,void(*f)(struct ble_npl_event*),void*a){(void)a;e->fn=f;}
static inline void *nimble_port_get_dflt_eventq(void){return NULL;}
static inline void ble_npl_eventq_put(void*q,struct ble_npl_event*e){(void)q;if(fake_defer)fake_pending=e;else e->fn(e);}
static inline int nimble_port_init(void){return ESP_OK;}
static inline void nimble_port_run(void){}
static inline void nimble_port_freertos_deinit(void){}
static inline void nimble_port_freertos_init(void(*f)(void*)){(void)f;}
static inline int ble_svc_gap_device_name_set(const char*n){(void)n;return 0;}
static inline int esp_hidd_dev_init(const esp_hid_device_config_t*c,int t,void(*cb)(void*,esp_event_base_t,int32_t,void*),esp_hidd_dev_t**d){(void)c;(void)t;(void)cb;(void)d;return ESP_OK;}
