#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifinodo.h"
#include "mqtt_client.h"
#include <esp_log.h>
#include "driver/gpio.h"
#include <math.h>
#define TAG "[MQTT]"
#define min(a, b) (a>b?b:a)
static bool mqttOk = false;
#define LED_GPIO 2
#define id 3

static esp_mqtt_client_handle_t actClient; //puntero a estructura de cliente MQTT

static void controlarLED(int estado) {
    gpio_set_level(LED_GPIO, estado);
}




static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    char topic[64];
    char data[64];
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        actClient = client;
        msg_id = esp_mqtt_client_subscribe(client, "/ej02/cmd", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/eje02/3/cmd/", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        mqttOk=true;
break;
    case MQTT_EVENT_DISCONNECTED: 
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqttOk = false;
        break;

     case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;

    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        strncpy(topic, event->topic, min(63, event->topic_len));
        strncpy(data, event->data, min(63, event->data_len));
        printf("TOPIC=%s\r\n", topic);
        printf("DATA=%s\r\n", data);

        if (strcmp(topic, "/ej02/cmd") == 0) {
            // Mensaje recibido en /ej02/cmd
            if (strcmp(data, "getid") == 0) {
                // Publicar el valor de id en /ej02/id
        msg_id = esp_mqtt_client_publish(client, "/ej02/id", "3", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            }

        } else if (strncmp(topic, "/ej02/", 7) == 0) {
            // Mensaje recibido en /ej02/{id}/cmd
            int receivedId;
            if (sscanf(topic + 7, "%d/cmd", &receivedId) == 1 && receivedId == id) {
                // Mensaje dirigido a este dispositivo específico
                if (strcmp(data, "ledon") == 0) {
                    // Encender el LED
                    controlarLED(1);
                } else if (strcmp(data, "ledoff") == 0) {
                    // Apagar el LED
                    controlarLED(0);
                }
            }
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
           ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}
void taskPublish(void *param) {
    char toSend[128];
    int cont = 1;
    double frequency = 0.1;  // Frecuencia de la onda senoidal
    double amplitude = 10.0; // Amplitud de la onda senoidal

    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        if (mqttOk) {
            double value = amplitude * sin(2 * M_PI * frequency * cont);
            sprintf(toSend, "valor_senoidal: %.2f", value);

            esp_mqtt_client_publish(actClient, "/ej06/senoidal", toSend, 0, 1, 0);
            cont++;
        }
    }
}

void app_main(void)
{
   
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://192.168.0.141",
        .credentials.username = "esp32",
        .credentials.authentication.password = "esp32esp32",
        //.broker.address.uri = "mqtt://broker.emqx.io",
       // .broker.verification.certificate = (const char *)hivemq_pem_start,
        //.broker.verification.certificate_len  = hivemq_pem_end - hivemq_pem_start,
    };
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      nvs_flash_init();
    }
    wifi_init_sta();

    
    
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    xTaskCreate(taskPublish, "Publish Task", configMINIMAL_STACK_SIZE + 4096, NULL, 1, NULL);

}