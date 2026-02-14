#include "sensor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ti/drivers/UART2.h>
#include <ti/drivers/dpl/ClockP.h>
#include "ti/log/Log.h"
#include "ti_drivers_config.h"

static UART2_Handle uartHandle;
static zb_bool_t sensor_presence = ZB_FALSE;
static uint8_t uart_rx_buf[32];
static size_t uart_rx_idx = 0;
static sensor_presence_config_t cached_config;

static void sensor_send_cmd(const char *cmd)
{
    if (uartHandle == NULL) return;
    size_t bytesWritten;
    UART2_write(uartHandle, cmd, strlen(cmd), &bytesWritten);
    UART2_write(uartHandle, "\r\n", 2, &bytesWritten);
    ClockP_usleep(200000);
}

static void sensor_drain(void)
{
    uint8_t discard;
    size_t bytesRead;
    while (UART2_read(uartHandle, &discard, 1, &bytesRead) == UART2_STATUS_SUCCESS && bytesRead > 0)
        ;
}

typedef struct {
    zb_bool_t ok;
    float val1;
    float val2;
} sensor_response_t;

static sensor_response_t sensor_query(const char *cmd)
{
    sensor_response_t resp = { .ok = ZB_FALSE, .val1 = 0, .val2 = 0 };
    char buf[64];
    size_t total = 0;
    size_t bytesWritten, bytesRead;

    if (uartHandle == NULL) return resp;

    sensor_drain();
    UART2_write(uartHandle, cmd, strlen(cmd), &bytesWritten);
    UART2_write(uartHandle, "\r\n", 2, &bytesWritten);
    ClockP_usleep(100000);

    while (total < sizeof(buf) - 1 &&
           UART2_read(uartHandle, &buf[total], 1, &bytesRead) == UART2_STATUS_SUCCESS &&
           bytesRead > 0)
    {
        total++;
    }
    buf[total] = '\0';

    char *res = strstr(buf, "Response");
    if (res != NULL)
    {
        char *tok;
        resp.ok = ZB_TRUE;
        strtok(res, " \r\n");  /* skip "Response" */
        tok = strtok(NULL, " \r\n");
        if (tok != NULL) resp.val1 = strtof(tok, NULL);
        tok = strtok(NULL, " \r\n");
        if (tok != NULL) resp.val2 = strtof(tok, NULL);
    }
    return resp;
}

static sensor_presence_config_t sensor_read_config(void)
{
    sensor_presence_config_t config = {0};
    sensor_response_t resp;

    sensor_send_cmd("sensorStop");

    resp = sensor_query("getRange");
    if (resp.ok)
    {
        config.range_min_cm = (uint16_t)(resp.val1 * 100 + 0.5f);
        config.range_max_cm = (uint16_t)(resp.val2 * 100 + 0.5f);
    }

    resp = sensor_query("getTrigRange");
    if (resp.ok)
    {
        config.trig_range_cm = (uint16_t)(resp.val1 * 100 + 0.5f);
    }

    resp = sensor_query("getSensitivity");
    if (resp.ok)
    {
        config.keep_sensitivity = (uint8_t)resp.val1;
        config.trig_sensitivity = (uint8_t)resp.val2;
    }

    resp = sensor_query("getLatency");
    if (resp.ok)
    {
        config.trig_delay = (uint8_t)(resp.val1 * 100 + 0.5f);
        config.keep_timeout = (uint16_t)(resp.val2 * 2 + 0.5f);
    }

    resp = sensor_query("getGpioMode 1");
    if (resp.ok)
    {
        config.io_polarity = (uint8_t)resp.val2;
    }

    resp = sensor_query("getMicroMotion");
    if (resp.ok)
    {
        config.fretting = resp.val1 != 0 ? ZB_TRUE : ZB_FALSE;
    }

    sensor_send_cmd("sensorStart");
    return config;
}

void sensor_init(void)
{
    UART2_Params params;
    UART2_Params_init(&params);
    params.readMode = UART2_Mode_NONBLOCKING;
    params.baudRate = 9600;

    uartHandle = UART2_open(CONFIG_UART2_0, &params);
    if (uartHandle != NULL)
    {
        UART2_rxEnable(uartHandle);

        sensor_presence_config_t default_config = {
            .range_min_cm   = 30,
            .range_max_cm   = 600,
            .trig_range_cm  = 300,
            .trig_sensitivity = 5,
            .keep_sensitivity = 5,
            .trig_delay     = 50,
            .keep_timeout   = 10,
            .io_polarity    = 0,
            .fretting       = ZB_TRUE,
        };
        sensor_configure_presence(&default_config);
        cached_config = sensor_read_config();
    }
}

sensor_presence_config_t sensor_get_config(void)
{
    return cached_config;
}

sensor_presence_config_t sensor_refresh_config(void)
{
    cached_config = sensor_read_config();
    return cached_config;
}

void sensor_configure_presence(const sensor_presence_config_t *config)
{
    char cmd[48];

    sensor_send_cmd("sensorStop");

    sensor_send_cmd("setRunApp 0");

    snprintf(cmd, sizeof(cmd), "setRange %d.%d %d.%d",
             config->range_min_cm / 100, (config->range_min_cm / 10) % 10,
             config->range_max_cm / 100, (config->range_max_cm / 10) % 10);
    sensor_send_cmd(cmd);

    snprintf(cmd, sizeof(cmd), "setTrigRange %d.%d",
             config->trig_range_cm / 100, (config->trig_range_cm / 10) % 10);
    sensor_send_cmd(cmd);

    snprintf(cmd, sizeof(cmd), "setSensitivity %d %d",
             config->keep_sensitivity, config->trig_sensitivity);
    sensor_send_cmd(cmd);

    snprintf(cmd, sizeof(cmd), "setLatency %d.%d %d.%d",
             config->trig_delay / 100, (config->trig_delay / 10) % 10,
             config->keep_timeout / 2, (config->keep_timeout % 2) * 5);
    sensor_send_cmd(cmd);

    snprintf(cmd, sizeof(cmd), "setGpioLevel %d", config->io_polarity);
    sensor_send_cmd(cmd);

    snprintf(cmd, sizeof(cmd), "setMicroMotion %d", config->fretting ? 1 : 0);
    sensor_send_cmd(cmd);

    sensor_send_cmd("saveConfig");
    sensor_send_cmd("sensorStart");

    cached_config = *config;
}

static void sensor_write_setting(const char *cmd)
{
    sensor_send_cmd("sensorStop");
    sensor_send_cmd(cmd);
    sensor_send_cmd("saveConfig");
    sensor_send_cmd("sensorStart");
}

void sensor_set_range(uint16_t min_cm, uint16_t max_cm, uint16_t trig_cm)
{
    char cmd[48];

    sensor_send_cmd("sensorStop");

    snprintf(cmd, sizeof(cmd), "setRange %d.%d %d.%d",
             min_cm / 100, (min_cm / 10) % 10,
             max_cm / 100, (max_cm / 10) % 10);
    sensor_send_cmd(cmd);

    snprintf(cmd, sizeof(cmd), "setTrigRange %d.%d",
             trig_cm / 100, (trig_cm / 10) % 10);
    sensor_send_cmd(cmd);

    sensor_send_cmd("saveConfig");
    sensor_send_cmd("sensorStart");

    cached_config.range_min_cm = min_cm;
    cached_config.range_max_cm = max_cm;
    cached_config.trig_range_cm = trig_cm;
}

void sensor_set_sensitivity(uint8_t trig, uint8_t keep)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "setSensitivity %d %d", keep, trig);
    sensor_write_setting(cmd);

    cached_config.trig_sensitivity = trig;
    cached_config.keep_sensitivity = keep;
}

void sensor_set_latency(uint8_t trig_delay, uint16_t keep_timeout)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "setLatency %d.%d %d.%d",
             trig_delay / 100, (trig_delay / 10) % 10,
             keep_timeout / 2, (keep_timeout % 2) * 5);
    sensor_write_setting(cmd);

    cached_config.trig_delay = trig_delay;
    cached_config.keep_timeout = keep_timeout;
}

void sensor_set_io_polarity(uint8_t polarity)
{
    char cmd[16];
    snprintf(cmd, sizeof(cmd), "setGpioLevel %d", polarity);
    sensor_write_setting(cmd);

    cached_config.io_polarity = polarity;
}

void sensor_set_fretting(zb_bool_t enabled)
{
    char cmd[20];
    snprintf(cmd, sizeof(cmd), "setMicroMotion %d", enabled ? 1 : 0);
    sensor_write_setting(cmd);

    cached_config.fretting = enabled;
}

void sensor_poll(void)
{
    char line;
    size_t bytesRead;

    if (uartHandle == NULL)
    {
        return;
    }

    while (UART2_read(uartHandle, &line, 1, &bytesRead) == UART2_STATUS_SUCCESS && bytesRead > 0)
    {
        if (line == '\r' || line == '\n' || line == '\0')
        {
            uart_rx_buf[uart_rx_idx] = '\0';
            if (strcmp((char *)uart_rx_buf, "$DFHPD,1, , , *") == 0)
            {
                sensor_presence = ZB_TRUE;
            }
            else if (strcmp((char *)uart_rx_buf, "$DFHPD,0, , , *") == 0)
            {
                sensor_presence = ZB_FALSE;
            }
            uart_rx_idx = 0;
        }
        else
        {
            if (uart_rx_idx < sizeof(uart_rx_buf))
            {
                uart_rx_buf[uart_rx_idx++] = line;
            }
            else
            {
                uart_rx_idx = 0;
            }
        }
    }
}

zb_bool_t sensor_get_presence(void)
{
    return sensor_presence;
}
