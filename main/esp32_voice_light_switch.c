
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

// HOME WI-FI
// Enter your actual credentials locally.
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
// LED GPIO pins
#define RED_LED    GPIO_NUM_18
#define YELLOW_LED GPIO_NUM_19
#define GREEN_LED  GPIO_NUM_21

#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "VOICE_LEDS";
static EventGroupHandle_t wifi_events;

// LED states
static bool red_on = false;
static bool yellow_on = false;
static bool green_on = false;

// ---------- LED CONTROL ----------

static void set_led(gpio_num_t pin, bool on)
{
    gpio_set_level(pin, on ? 1 : 0);

    if (pin == RED_LED) red_on = on;
    if (pin == YELLOW_LED) yellow_on = on;
    if (pin == GREEN_LED) green_on = on;
}

static void all_leds_off(void)
{
    set_led(RED_LED, false);
    set_led(YELLOW_LED, false);
    set_led(GREEN_LED, false);
}

static void leds_init(void)
{
    gpio_config_t config = {
        .pin_bit_mask =
            (1ULL << RED_LED) |
            (1ULL << YELLOW_LED) |
            (1ULL << GREEN_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&config));

    all_leds_off();

    ESP_LOGI(TAG, "LEDs initialized");
}

// ---------- PHONE WEBPAGE ----------

static const char html_page[] =
"<!DOCTYPE html>"
"<html><head>"
"<meta name='viewport' "
"content='width=device-width,initial-scale=1'>"
"<title>ESP32 LED Controller</title>"
"<style>"
"body{font-family:Arial;text-align:center;"
"background:#121826;color:white;padding:24px;}"
"h2{margin-bottom:24px;}"
".item{background:#202c40;padding:14px;"
"margin:12px auto;border-radius:15px;"
"max-width:350px;}"
"button{padding:14px 22px;margin:6px;"
"font-size:17px;border:0;border-radius:10px;"
"color:white;cursor:pointer;}"
".on{background:#159b59;}"
".off{background:#c84343;}"
".all{background:#53657d;}"
"</style></head><body>"

"<h2>ESP32 LED Controller</h2>"

"<div class='item'>"
"<h3>RED LED</h3>"
"<button class='on' onclick=\"cmd('red','on')\">ON</button>"
"<button class='off' onclick=\"cmd('red','off')\">OFF</button>"
"</div>"

"<div class='item'>"
"<h3>YELLOW LED</h3>"
"<button class='on' onclick=\"cmd('yellow','on')\">ON</button>"
"<button class='off' onclick=\"cmd('yellow','off')\">OFF</button>"
"</div>"

"<div class='item'>"
"<h3>GREEN LED</h3>"
"<button class='on' onclick=\"cmd('green','on')\">ON</button>"
"<button class='off' onclick=\"cmd('green','off')\">OFF</button>"
"</div>"

"<button class='all' onclick=\"cmd('all','off')\">"
"TURN ALL OFF</button>"

"<p id='status'>Ready</p>"

"<script>"
"async function cmd(color,state){"
"let s=document.getElementById('status');"
"s.textContent='Sending...';"
"try{"
"let r=await fetch('/api/led?color='+color+'&state='+state,"
"{method:'POST'});"
"s.textContent=await r.text();"
"}catch(e){s.textContent='Connection error';}"
"}"
"</script>"
"</body></html>";

// ---------- HTTP HANDLERS ----------

static esp_err_t home_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, html_page);
}

static esp_err_t led_handler(httpd_req_t *req)
{
    char query[100];
    char color[16];
    char state[8];

    if (httpd_req_get_url_query_str(
            req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(
            query, "color", color,
            sizeof(color)) != ESP_OK ||
        httpd_query_key_value(
            query, "state", state,
            sizeof(state)) != ESP_OK)
    {
        httpd_resp_send_err(
            req, HTTPD_400_BAD_REQUEST,
            "Missing parameters");
        return ESP_OK;
    }

    if (strcmp(color, "all") == 0 &&
        strcmp(state, "off") == 0)
    {
        all_leds_off();
        ESP_LOGI(TAG, "All LEDs OFF");
        return httpd_resp_sendstr(
            req, "All LEDs OFF");
    }

    bool on;

    if (strcmp(state, "on") == 0) {
        on = true;
    } else if (strcmp(state, "off") == 0) {
        on = false;
    } else {
        httpd_resp_send_err(
            req, HTTPD_400_BAD_REQUEST,
            "Invalid state");
        return ESP_OK;
    }

    gpio_num_t pin;

    if (strcmp(color, "red") == 0) {
        pin = RED_LED;
    } else if (strcmp(color, "yellow") == 0) {
        pin = YELLOW_LED;
    } else if (strcmp(color, "green") == 0) {
        pin = GREEN_LED;
    } else {
        httpd_resp_send_err(
            req, HTTPD_400_BAD_REQUEST,
            "Invalid color");
        return ESP_OK;
    }

    set_led(pin, on);

    ESP_LOGI(TAG, "%s LED %s",
             color, on ? "ON" : "OFF");

    return httpd_resp_sendstr(
        req, on ? "LED ON" : "LED OFF");
}

static void start_webserver(void)
{
    httpd_config_t config =
        HTTPD_DEFAULT_CONFIG();

    httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(
        httpd_start(&server, &config));

    const httpd_uri_t home = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = home_handler
    };

    const httpd_uri_t led = {
        .uri = "/api/led",
        .method = HTTP_POST,
        .handler = led_handler
    };

    ESP_ERROR_CHECK(
        httpd_register_uri_handler(server, &home));

    ESP_ERROR_CHECK(
        httpd_register_uri_handler(server, &led));

    ESP_LOGI(TAG, "Web server started");
}

// ---------- WI-FI ----------

static void wifi_event_handler(
    void *arg,
    esp_event_base_t base,
    int32_t event_id,
    void *data)
{
    if (base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *event = data;

        xEventGroupClearBits(
            wifi_events, WIFI_CONNECTED_BIT);

        ESP_LOGW(TAG,
                 "Disconnected, reason=%d",
                 event->reason);

        esp_wifi_connect();
    }
    else if (base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = data;

        ESP_LOGI(TAG,
                 "Connected! Open http://" IPSTR,
                 IP2STR(&event->ip_info.ip));

        xEventGroupSetBits(
            wifi_events, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init(void)
{
    wifi_events = xEventGroupCreate();
    configASSERT(wifi_events != NULL);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(
        esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg =
        WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        wifi_event_handler,
        NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        wifi_event_handler,
        NULL));

    wifi_config_t wifi_config = {0};

    snprintf(
        (char *)wifi_config.sta.ssid,
        sizeof(wifi_config.sta.ssid),
        "%s", WIFI_SSID);

    snprintf(
        (char *)wifi_config.sta.password,
        sizeof(wifi_config.sta.password),
        "%s", WIFI_PASS);

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA, &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());
}

// ---------- MAIN ----------

void app_main(void)
{
    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    ESP_ERROR_CHECK(err);

    leds_init();
    wifi_init();

    ESP_LOGI(TAG, "Waiting for home Wi-Fi...");

    xEventGroupWaitBits(
        wifi_events,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdTRUE,
        portMAX_DELAY);

    start_webserver();

    ESP_LOGI(TAG, "LED controller ready");
}
