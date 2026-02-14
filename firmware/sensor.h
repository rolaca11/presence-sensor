#ifndef SENSOR_H
#define SENSOR_H

#include "zboss_api.h"
#include <stdint.h>

typedef struct {
    uint16_t range_min_cm;      /* Min detection range: 30-2000 cm */
    uint16_t range_max_cm;      /* Max detection range: 240-2000 cm */
    uint16_t trig_range_cm;     /* Trigger range: 30-2000 cm */
    uint8_t trig_sensitivity;   /* Trigger sensitivity: 0-9 */
    uint8_t keep_sensitivity;   /* Keep sensitivity: 0-9 */
    uint8_t trig_delay;         /* Trigger delay: 0-200 (x10ms, so 0-2s) */
    uint16_t keep_timeout;      /* Keep timeout: 4-3000 (x500ms, so 2-1500s) */
    uint8_t io_polarity;        /* Output pin polarity: 0 or 1 */
    zb_bool_t fretting;         /* Micromotion detection: ZB_TRUE/ZB_FALSE */
} sensor_presence_config_t;

void sensor_init(void);
void sensor_configure_presence(const sensor_presence_config_t *config);
sensor_presence_config_t sensor_get_config(void);
sensor_presence_config_t sensor_refresh_config(void);
void sensor_set_range(uint16_t min_cm, uint16_t max_cm, uint16_t trig_cm);
void sensor_set_sensitivity(uint8_t trig, uint8_t keep);
void sensor_set_latency(uint8_t trig_delay, uint16_t keep_timeout);
void sensor_set_io_polarity(uint8_t polarity);
void sensor_set_fretting(zb_bool_t enabled);
void sensor_poll(void);
zb_bool_t sensor_get_presence(void);

#endif /* SENSOR_H */
