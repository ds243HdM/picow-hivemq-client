#include "hardware/structs/rosc.h"

#include <string.h>
#include <time.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "lwip/dns.h"

#include "lwip/altcp_tcp.h"
#include "lwip/altcp_tls.h"
#include "lwip/apps/mqtt.h"

#include "lwip/apps/mqtt_priv.h"

// #include "tusb.h"

#define DEBUG_printf printf

#define MQTT_TLS 1
#include "crypto_consts.h"

#if MQTT_TLS
#ifdef CRYPTO_CERT
const char *cert = CRYPTO_CERT;
#endif
#ifdef CRYPTO_CA
const char *ca = CRYPTO_CA;
#endif
#ifdef CRYPTO_KEY
const char *key = CRYPTO_KEY;
#endif
#endif

typedef struct MQTT_CLIENT_T_ {
    ip_addr_t remote_addr;
    mqtt_client_t *mqtt_client;
    u32_t received;
    u32_t counter;
    u32_t reconnect;
} MQTT_CLIENT_T;
 
err_t mqtt_test_connect(MQTT_CLIENT_T *state);

// Perform initialisation
static MQTT_CLIENT_T* mqtt_client_init(void) {
    MQTT_CLIENT_T *state = calloc(1, sizeof(MQTT_CLIENT_T));
    if (!state) {
        DEBUG_printf("failed to allocate state\n");
        return NULL;
    }
    state->received = 0;
    return state;
}

void dns_found(const char *name, const ip_addr_t *ipaddr, void *callback_arg) {
    MQTT_CLIENT_T *state = (MQTT_CLIENT_T*)callback_arg;
    DEBUG_printf("DNS query finished with resolved addr of %s.\n", ip4addr_ntoa(ipaddr));
    state->remote_addr = *ipaddr;
}

void run_dns_lookup(MQTT_CLIENT_T *state) {
    DEBUG_printf("Starting DNS lookup for MQTT broker: %s\n", MQTT_SERVER_HOST);

    cyw43_arch_lwip_begin();
    err_t err = dns_gethostbyname(MQTT_SERVER_HOST, &(state->remote_addr), dns_found, state);
    cyw43_arch_lwip_end();

    if (err == ERR_ARG) {
        DEBUG_printf("failed to start DNS query\n");
        return;
    }

    if (err == ERR_OK) {
        DEBUG_printf("DNS lookup completed immediately\n");
        return;
    }

    while (state->remote_addr.addr == 0) {
        cyw43_arch_poll();
        sleep_ms(1);
    }
    DEBUG_printf("DNS resolved to: %s\n", ip4addr_ntoa(&(state->remote_addr)));
}

u32_t data_in = 0;

u8_t buffer[1025];
u8_t data_len = 0;

static void mqtt_pub_start_cb(void *arg, const char *topic, u32_t tot_len) {
    DEBUG_printf("mqtt_pub_start_cb: topic %s\n", topic);

    if (tot_len > 1024) {
        DEBUG_printf("Message length exceeds buffer size, discarding");
    } else {
        data_in = tot_len;
        data_len = 0;
    }
}

static void mqtt_pub_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    if (data_in > 0) {
        data_in -= len;
        memcpy(&buffer[data_len], data, len);
        data_len += len;

        if (data_in == 0) {
            buffer[data_len] = 0;
            DEBUG_printf("Message received: %s\n", &buffer);
        }
    }
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    if (status != 0) {
        DEBUG_printf("ERROR: MQTT connection failed with status %d\n", status);
    } else {
        DEBUG_printf("SUCCESS: Connected to MQTT broker\n");
    }
}

void mqtt_pub_request_cb(void *arg, err_t err) {
    MQTT_CLIENT_T *state = (MQTT_CLIENT_T *)arg;
    DEBUG_printf("mqtt_pub_request_cb: err %d\n", err);
    state->received++;
}

void mqtt_sub_request_cb(void *arg, err_t err) {
    DEBUG_printf("mqtt_sub_request_cb: err %d\n", err);
}

err_t mqtt_test_publish(MQTT_CLIENT_T *state)
{
  char buffer[128];

  sprintf(buffer, "{\"message\":\"hello from picow %d / %d\"}", state->received, state->counter);

  err_t err;
  u8_t qos = 0; /* 0 1 or 2, see MQTT specification.  AWS IoT does not support QoS 2 */
  u8_t retain = 0;
  cyw43_arch_lwip_begin();
  err = mqtt_publish(state->mqtt_client, "pico_w/test", buffer, strlen(buffer), qos, retain, mqtt_pub_request_cb, state);
  cyw43_arch_lwip_end();
  if(err != ERR_OK) {
    DEBUG_printf("Publish err: %d\n", err);
  }

  return err; 
}

err_t mqtt_test_connect(MQTT_CLIENT_T *state) {
    struct mqtt_connect_client_info_t ci;
    err_t err;

    memset(&ci, 0, sizeof(ci));

    ci.client_id = "PicoW";
    ci.client_user = MQTT_USERNAME;
    ci.client_pass = MQTT_PASSWORD;
    ci.keep_alive = 0;
    ci.will_topic = NULL;
    ci.will_msg = NULL;
    ci.will_retain = 0;
    ci.will_qos = 0;

    #if MQTT_TLS

    struct altcp_tls_config *tls_config;
  
    #if defined(CRYPTO_CA) && defined(CRYPTO_KEY) && defined(CRYPTO_CERT)
    
    DEBUG_printf("Setting up TLS with 2wayauth (CA + Key + Cert).\n");
    tls_config = altcp_tls_create_config_client_2wayauth(
        (const u8_t *)ca, 1 + strlen((const char *)ca),
        (const u8_t *)key, 1 + strlen((const char *)key),
        (const u8_t *)"", 0,
        (const u8_t *)cert, 1 + strlen((const char *)cert)
    );

    // enable SNI on request
    // see mqtt-sni.patch for changes to support this.
    altcp_tls_set_server_name(tls_config, MQTT_SERVER_HOST);

    #elif defined(CRYPTO_CA)
    DEBUG_printf("Setting up TLS with CA certificate only.\n");
    tls_config = altcp_tls_create_config_client((const u8_t *)ca, 1 + strlen((const char *)ca));

    // enable SNI on request
    // see mqtt-sni.patch for changes to support this.
    altcp_tls_set_server_name(tls_config, MQTT_SERVER_HOST);

    #elif defined(CRYPTO_CERT)
    DEBUG_printf("Setting up TLS with cert only.\n");
    tls_config = altcp_tls_create_config_client((const u8_t *) cert, 1 + strlen((const char *) cert));

    // enable SNI on request
    // see mqtt-sni.patch for changes to support this.
    altcp_tls_set_server_name(tls_config, MQTT_SERVER_HOST);
    #else
        DEBUG_printf("WARNING: No crypto credentials defined. TLS may fail.\n");
        tls_config = altcp_tls_create_config_client(NULL, 0);
        altcp_tls_set_server_name(tls_config, MQTT_SERVER_HOST);
    #endif

    if (tls_config == NULL) {
        DEBUG_printf("Failed to initialize config\n");
        return -1;
    }

    ci.tls_config = tls_config;
    #endif

    const struct mqtt_connect_client_info_t *client_info = &ci;

    DEBUG_printf("Connecting to MQTT broker %s:%d...\n", MQTT_SERVER_HOST, MQTT_SERVER_PORT);
    err = mqtt_client_connect(state->mqtt_client, &(state->remote_addr), MQTT_SERVER_PORT, mqtt_connection_cb, state, client_info);
    
    if (err != ERR_OK) {
        DEBUG_printf("ERROR: mqtt_client_connect failed with code %d\n", err);
    } else {
        DEBUG_printf("mqtt_client_connect initiated (err=ERR_OK)\n");
    }

    return err;
}

void mqtt_run_test(MQTT_CLIENT_T *state) {
    state->mqtt_client = mqtt_client_new();

    state->counter = 0;  

    if (state->mqtt_client == NULL) {
        DEBUG_printf("Failed to create new mqtt client\n");
        return;
    } 
    // psa_crypto_init();
    if (mqtt_test_connect(state) == ERR_OK) {
        absolute_time_t timeout = nil_time;
        bool subscribed = false;
        mqtt_set_inpub_callback(state->mqtt_client, mqtt_pub_start_cb, mqtt_pub_data_cb, 0);

        while (true) {
            cyw43_arch_poll();
            absolute_time_t now = get_absolute_time();
            if (is_nil_time(timeout) || absolute_time_diff_us(now, timeout) <= 0) {
                if (mqtt_client_is_connected(state->mqtt_client)) {
                    cyw43_arch_lwip_begin();

                    if (!subscribed) {
                        mqtt_sub_unsub(state->mqtt_client, "pico_w/recv", 0, mqtt_sub_request_cb, 0, 1);
                        subscribed = true;
                    }

                    if (mqtt_test_publish(state) == ERR_OK) {
                        if (state->counter != 0) {
                            DEBUG_printf("published %d\n", state->counter);
                        }
                        timeout = make_timeout_time_ms(5000);
                        state->counter++;
                    } // else ringbuffer is full and we need to wait for messages to flush.
                    cyw43_arch_lwip_end();
                } else {
                    // DEBUG_printf(".");
                }
            }
        }
    }
}

int main() { 
    stdio_init_all();

    DEBUG_printf("\n=== MQTT IoT Client Starting ===\n");
    DEBUG_printf("MQTT Server: %s:%d (TLS: %s)\n", MQTT_SERVER_HOST, MQTT_SERVER_PORT, MQTT_TLS ? "enabled" : "disabled");
    #ifdef CRYPTO_CA
    DEBUG_printf("Crypto Config: CA present\n");
    #endif
    #ifdef CRYPTO_KEY
    DEBUG_printf("Crypto Config: Key present\n");
    #endif
    #ifdef CRYPTO_CERT
    DEBUG_printf("Crypto Config: Cert present\n");
    #endif

    if (cyw43_arch_init()) {
        DEBUG_printf("ERROR: failed to initialise cyw43\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();

    DEBUG_printf("Connecting to WiFi %s...\n", WIFI_SSID);
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        DEBUG_printf("ERROR: WiFi connection failed\n");
        return 1;
    } else {
        DEBUG_printf("WiFi: Connected successfully\n");
    }

    MQTT_CLIENT_T *state = mqtt_client_init();
    if (!state) {
        DEBUG_printf("ERROR: Failed to initialize MQTT client\n");
        return 1;
    }
     
    run_dns_lookup(state);
    DEBUG_printf("Starting MQTT connection sequence...\n");
    mqtt_run_test(state);

    cyw43_arch_deinit();
    return 0;
}