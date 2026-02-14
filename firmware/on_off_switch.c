/******************************************************************************
 Group: CMCU LPRF
 Target Device: cc23xx

 ******************************************************************************
 
 Copyright (c) 2024-2025, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************
 
 
 *****************************************************************************/

/***** Trace related defines *****/
#define ZB_TRACE_FILE_ID 40124

/****** Application defines ******/
#define ZB_SWITCH_ENDPOINT          10

#include <ti/log/Log.h>

#include "ti_zigbee_config.h"
#include "zboss_api.h"
#include "zb_led_button.h"

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_fcfg.h)
#include DeviceFamily_constructPath(inc/hw_memmap.h)

/* for button handling */
#include <ti/drivers/GPIO.h>
#include "ti_drivers_config.h"
#include "sensor.h"

#ifdef ZB_CONFIGURABLE_MEM
#include "zb_mem_config_lprf3.h"
#endif

#if !defined ZB_ED_FUNC
#error define ZB_ED_ROLE to compile the tests
#endif

#undef ZB_USE_SLEEP

/****** Application variables declarations ******/
/* IEEE address of the device */
zb_bool_t cmd_in_progress = ZB_FALSE;
zb_bool_t perform_factory_reset = ZB_FALSE;
static zb_bool_t light_is_on = ZB_FALSE;

/****** Application function declarations ******/
zb_uint8_t zcl_specific_cluster_cmd_handler(zb_uint8_t param);
void on_off_read_attr_resp_handler(zb_bufid_t cmd_buf);
void send_on_req(zb_uint8_t param);
void send_off_req(zb_uint8_t param);
void send_cmd_timeout(zb_uint8_t param);
void sensor_poll_handler(zb_uint8_t param);
void occupancy_write_attr_hook(zb_uint8_t endpoint, zb_uint16_t attr_id,
                               zb_uint8_t *new_value, zb_uint16_t manuf_code);

/****** Cluster declarations ******/
/* Switch config cluster attributes */
zb_uint8_t attr_switch_type =
    ZB_ZCL_ON_OFF_SWITCH_CONFIGURATION_SWITCH_TYPE_TOGGLE;
zb_uint8_t attr_switch_actions =
    ZB_ZCL_ON_OFF_SWITCH_CONFIGURATION_SWITCH_ACTIONS_DEFAULT_VALUE;

ZB_ZCL_DECLARE_ON_OFF_SWITCH_CONFIGURATION_ATTRIB_LIST(switch_cfg_attr_list,
                                                       &attr_switch_type,
                                                       &attr_switch_actions);
/* Basic cluster attributes */
zb_uint8_t attr_zcl_version  = ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE;
zb_uint8_t attr_power_source = ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE;
ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST(basic_attr_list, &attr_zcl_version, &attr_power_source);
/* Identify cluster attributes */
zb_uint16_t attr_identify_time = 0;
ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(identify_attr_list, &attr_identify_time);

/* Occupancy Sensing attributes */
zb_uint8_t attr_occupancy = 0;
zb_uint8_t attr_occ_sensor_type = ZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_ULTRASONIC;
zb_uint8_t attr_occ_sensor_type_bitmap = 0x02; /* ultrasonic bit */

/* Custom sensor config attributes (IDs 0xE000-0xE008 on occupancy cluster) */
zb_uint16_t attr_range_min_cm = 30;
zb_uint16_t attr_range_max_cm = 600;
zb_uint16_t attr_trig_range_cm = 300;
zb_uint8_t  attr_trig_sensitivity = 5;
zb_uint8_t  attr_keep_sensitivity = 5;
zb_uint8_t  attr_trig_delay = 50;
zb_uint16_t attr_keep_timeout = 10;
zb_uint8_t  attr_io_polarity = 0;
zb_uint8_t  attr_fretting = 1;

/* Occupancy attribute list (standard + custom) */
zb_uint16_t occ_cluster_revision = ZB_ZCL_OCCUPANCY_SENSING_CLUSTER_REVISION_DEFAULT;
zb_zcl_attr_t occupancy_attr_list[] = {
  { ZB_ZCL_ATTR_GLOBAL_CLUSTER_REVISION_ID, ZB_ZCL_ATTR_TYPE_U16, ZB_ZCL_ATTR_ACCESS_READ_ONLY, ZB_ZCL_NON_MANUFACTURER_SPECIFIC, &occ_cluster_revision },
  /* Standard occupancy attrs */
  { 0x0000, ZB_ZCL_ATTR_TYPE_8BITMAP, ZB_ZCL_ATTR_ACCESS_READ_ONLY | ZB_ZCL_ATTR_ACCESS_REPORTING, ZB_ZCL_NON_MANUFACTURER_SPECIFIC, &attr_occupancy },
  { 0x0001, ZB_ZCL_ATTR_TYPE_8BIT_ENUM, ZB_ZCL_ATTR_ACCESS_READ_ONLY, ZB_ZCL_NON_MANUFACTURER_SPECIFIC, &attr_occ_sensor_type },
  { 0x0002, ZB_ZCL_ATTR_TYPE_8BITMAP, ZB_ZCL_ATTR_ACCESS_READ_ONLY, ZB_ZCL_NON_MANUFACTURER_SPECIFIC, &attr_occ_sensor_type_bitmap },
  /* Custom config attrs 0xE000-0xE008 (read/write) */
  { 0xE000, ZB_ZCL_ATTR_TYPE_U16, ZB_ZCL_ATTR_ACCESS_READ_WRITE, ZB_ZCL_NON_MANUFACTURER_SPECIFIC, &attr_range_min_cm },
  { 0xE001, ZB_ZCL_ATTR_TYPE_U16, ZB_ZCL_ATTR_ACCESS_READ_WRITE, ZB_ZCL_NON_MANUFACTURER_SPECIFIC, &attr_range_max_cm },
  { 0xE002, ZB_ZCL_ATTR_TYPE_U16, ZB_ZCL_ATTR_ACCESS_READ_WRITE, ZB_ZCL_NON_MANUFACTURER_SPECIFIC, &attr_trig_range_cm },
  { 0xE003, ZB_ZCL_ATTR_TYPE_U8,  ZB_ZCL_ATTR_ACCESS_READ_WRITE, ZB_ZCL_NON_MANUFACTURER_SPECIFIC, &attr_trig_sensitivity },
  { 0xE004, ZB_ZCL_ATTR_TYPE_U8,  ZB_ZCL_ATTR_ACCESS_READ_WRITE, ZB_ZCL_NON_MANUFACTURER_SPECIFIC, &attr_keep_sensitivity },
  { 0xE005, ZB_ZCL_ATTR_TYPE_U8,  ZB_ZCL_ATTR_ACCESS_READ_WRITE, ZB_ZCL_NON_MANUFACTURER_SPECIFIC, &attr_trig_delay },
  { 0xE006, ZB_ZCL_ATTR_TYPE_U16, ZB_ZCL_ATTR_ACCESS_READ_WRITE, ZB_ZCL_NON_MANUFACTURER_SPECIFIC, &attr_keep_timeout },
  { 0xE007, ZB_ZCL_ATTR_TYPE_U8,  ZB_ZCL_ATTR_ACCESS_READ_WRITE, ZB_ZCL_NON_MANUFACTURER_SPECIFIC, &attr_io_polarity },
  { 0xE008, ZB_ZCL_ATTR_TYPE_BOOL,ZB_ZCL_ATTR_ACCESS_READ_WRITE, ZB_ZCL_NON_MANUFACTURER_SPECIFIC, &attr_fretting },
  { ZB_ZCL_NULL_ID, 0, 0, ZB_ZCL_NON_MANUFACTURER_SPECIFIC, NULL } /* terminator */
};

/* Declare cluster list (manual, replacing ZB_HA_DECLARE_ON_OFF_SWITCH_CLUSTER_LIST) */
zb_zcl_cluster_desc_t on_off_switch_clusters[] = {
  ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG,
    ZB_ZCL_ARRAY_SIZE(switch_cfg_attr_list, zb_zcl_attr_t), (switch_cfg_attr_list),
    ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID),
  ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_IDENTIFY,
    ZB_ZCL_ARRAY_SIZE(identify_attr_list, zb_zcl_attr_t), (identify_attr_list),
    ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID),
  ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_BASIC,
    ZB_ZCL_ARRAY_SIZE(basic_attr_list, zb_zcl_attr_t), (basic_attr_list),
    ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID),
  ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING,
    ZB_ZCL_ARRAY_SIZE(occupancy_attr_list, zb_zcl_attr_t), (occupancy_attr_list),
    ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID),
  ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_ON_OFF,
    0, NULL, ZB_ZCL_CLUSTER_CLIENT_ROLE, ZB_ZCL_MANUF_CODE_INVALID),
  ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_SCENES,
    0, NULL, ZB_ZCL_CLUSTER_CLIENT_ROLE, ZB_ZCL_MANUF_CODE_INVALID),
  ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_IDENTIFY,
    0, NULL, ZB_ZCL_CLUSTER_CLIENT_ROLE, ZB_ZCL_MANUF_CODE_INVALID),
  ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_GROUPS,
    0, NULL, ZB_ZCL_CLUSTER_CLIENT_ROLE, ZB_ZCL_MANUF_CODE_INVALID),
};

/* Declare endpoint (manual, replacing ZB_HA_DECLARE_ON_OFF_SWITCH_EP) */
ZB_DECLARE_SIMPLE_DESC(4, 4);
ZB_AF_SIMPLE_DESC_TYPE(4, 4) simple_desc_on_off_switch_ep = {
  ZB_SWITCH_ENDPOINT,
  ZB_AF_HA_PROFILE_ID,
  ZB_HA_ON_OFF_SWITCH_DEVICE_ID,
  ZB_HA_DEVICE_VER_ON_OFF_SWITCH,
  0,
  4, /* in_count */
  4, /* out_count */
  {
    ZB_ZCL_CLUSTER_ID_BASIC,
    ZB_ZCL_CLUSTER_ID_IDENTIFY,
    ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG,
    ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING,
    ZB_ZCL_CLUSTER_ID_ON_OFF,
    ZB_ZCL_CLUSTER_ID_SCENES,
    ZB_ZCL_CLUSTER_ID_GROUPS,
    ZB_ZCL_CLUSTER_ID_IDENTIFY,
  }
};
ZBOSS_DEVICE_DECLARE_REPORTING_CTX(reporting_info, 1);
ZB_AF_DECLARE_ENDPOINT_DESC(on_off_switch_ep, ZB_SWITCH_ENDPOINT, ZB_AF_HA_PROFILE_ID,
  0, NULL,
  ZB_ZCL_ARRAY_SIZE(on_off_switch_clusters, zb_zcl_cluster_desc_t),
  on_off_switch_clusters,
  (zb_af_simple_desc_1_1_t*)&simple_desc_on_off_switch_ep,
  1, reporting_info, 0, NULL);

/* Declare application's device context for single-endpoint device */
ZB_HA_DECLARE_ON_OFF_SWITCH_CTX(on_off_switch_ctx, on_off_switch_ep);

static void sync_attrs_from_sensor(void)
{
  sensor_presence_config_t cfg = sensor_refresh_config();
  attr_range_min_cm     = cfg.range_min_cm;
  attr_range_max_cm     = cfg.range_max_cm;
  attr_trig_range_cm    = cfg.trig_range_cm;
  attr_trig_sensitivity = cfg.trig_sensitivity;
  attr_keep_sensitivity = cfg.keep_sensitivity;
  attr_trig_delay       = cfg.trig_delay;
  attr_keep_timeout     = cfg.keep_timeout;
  attr_io_polarity      = cfg.io_polarity;
  attr_fretting         = cfg.fretting ? 1 : 0;
}

void occupancy_write_attr_hook(zb_uint8_t endpoint, zb_uint16_t attr_id,
                               zb_uint8_t *new_value, zb_uint16_t manuf_code)
{
  ZVUNUSED(endpoint);
  ZVUNUSED(manuf_code);

  Log_printf(LogModule_Zigbee_App, Log_INFO, "write_attr_hook: attr_id=0x%04x", attr_id);

  switch (attr_id) {
    case 0xE000: case 0xE001: case 0xE002:
      Log_printf(LogModule_Zigbee_App, Log_INFO, "set_range: min=%d max=%d trig=%d",
                 attr_range_min_cm, attr_range_max_cm, attr_trig_range_cm);
      sensor_set_range(attr_range_min_cm, attr_range_max_cm, attr_trig_range_cm);
      break;
    case 0xE003: case 0xE004:
      Log_printf(LogModule_Zigbee_App, Log_INFO, "set_sensitivity: trig=%d keep=%d",
                 attr_trig_sensitivity, attr_keep_sensitivity);
      sensor_set_sensitivity(attr_trig_sensitivity, attr_keep_sensitivity);
      break;
    case 0xE005: case 0xE006:
      Log_printf(LogModule_Zigbee_App, Log_INFO, "set_latency: trig_delay=%d keep_timeout=%d",
                 attr_trig_delay, attr_keep_timeout);
      sensor_set_latency(attr_trig_delay, attr_keep_timeout);
      break;
    case 0xE007:
      Log_printf(LogModule_Zigbee_App, Log_INFO, "set_io_polarity: %d", attr_io_polarity);
      sensor_set_io_polarity(attr_io_polarity);
      break;
    case 0xE008:
      Log_printf(LogModule_Zigbee_App, Log_INFO, "set_fretting: %d", attr_fretting);
      sensor_set_fretting(attr_fretting ? ZB_TRUE : ZB_FALSE);
      break;
    default:
      Log_printf(LogModule_Zigbee_App, Log_WARNING, "write_attr_hook: unhandled attr 0x%04x", attr_id);
      return;
  }

  sync_attrs_from_sensor();
}

void my_main_loop()
{
  while (1)
  {
    /* ... User code ... */
    zboss_main_loop_iteration();
    sensor_poll();
    /* ... User code ... */
  }
}

MAIN()
{
  ARGV_UNUSED;

  /* Global ZBOSS initialization */
  ZB_INIT("on_off_switch");

  #ifdef ZB_LONG_ADDR
  // use the address that the customer set in the pre-defined symbols tab
  zb_ieee_addr_t g_long_addr = ZB_LONG_ADDR;
  zb_set_long_address(g_long_addr);
  #else
  /* Set the device's long address to the IEEE address pulling from the FCFG of the device */
  zb_ieee_addr_t ieee_mac_addr;
  ZB_MEMCPY(ieee_mac_addr, fcfg->deviceInfo.macAddr, 8);
  zb_set_long_address(ieee_mac_addr);
  #endif // ZB_LONG_ADDR

#ifdef ZB_COORDINATOR_ROLE
  zb_set_network_coordinator_role(DEFAULT_CHANLIST);

  /* Set keepalive mode to mac data poll so sleepy zeds consume less power */
  zb_set_keepalive_mode(MAC_DATA_POLL_KEEPALIVE);
#ifdef DEFAULT_NWK_KEY
  zb_uint8_t nwk_key[16] = DEFAULT_NWK_KEY;
  zb_secur_setup_nwk_key(nwk_key, 0);
#endif //DEFAULT_NWK_KEY

#ifdef ZBOSS_REV23
  zb_nwk_set_max_ed_capacity(MAX_ED_CAPACITY);
#endif //ZBOSS_REV23

#elif defined ZB_ROUTER_ROLE && !defined ZB_COORDINATOR_ROLE
  zb_set_network_router_role(DEFAULT_CHANLIST);

#ifdef ZBOSS_REV23
  zb_nwk_set_max_ed_capacity(MAX_ED_CAPACITY);
#endif //ZBOSS_REV23

  /* Set keepalive mode to mac data poll so sleepy zeds consume less power */
  zb_set_keepalive_mode(MAC_DATA_POLL_KEEPALIVE);

#elif defined ZB_ED_ROLE
  zb_set_network_ed_role(DEFAULT_CHANLIST);

  /* Set end-device configuration parameters */
  zb_set_ed_timeout(ED_TIMEOUT_VALUE);
  zb_set_rx_on_when_idle(ED_RX_ALWAYS_ON);
#if ( ED_RX_ALWAYS_ON == ZB_FALSE )
  zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(ED_POLL_RATE));
  zb_zdo_pim_set_long_poll_interval(ED_POLL_RATE);
#ifdef DISABLE_TURBO_POLL
  // Disable turbo poll feature
  zb_zdo_pim_permit_turbo_poll(ZB_FALSE);
#endif // DISABLE_TURBO_POLL
#endif // ED_RX_ALWAYS_ON
#endif //ZB_ED_ROLE

  zb_set_nvram_erase_at_start(ZB_FALSE);

  /* Register device ZCL context */
  ZB_AF_REGISTER_DEVICE_CTX(&on_off_switch_ctx);
  /* Register write-attribute hook for occupancy cluster custom attrs */
  zb_zcl_add_cluster_handlers(ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING,
    ZB_ZCL_CLUSTER_SERVER_ROLE, NULL, occupancy_write_attr_hook, NULL);
  /* Register cluster commands handler for a specific endpoint */
  ZB_AF_SET_ENDPOINT_HANDLER(ZB_SWITCH_ENDPOINT, zcl_specific_cluster_cmd_handler);

  /* Initiate the stack start without starting the commissioning */
  if (zboss_start_no_autostart() != RET_OK)
  {
    Log_printf(LogModule_Zigbee_App, Log_ERROR, "zboss_start failed");
  }
  else
  {
    GPIO_setConfig(CONFIG_GPIO_BTN1, GPIO_CFG_IN_PU);
    // if either button 1 or button 2 gets pressed
    zb_bool_t sideButtonPressed = GPIO_read((zb_uint8_t)CONFIG_GPIO_BTN1) == 0U;
    // then perform a factory reset
    if (sideButtonPressed)
    {
      perform_factory_reset = ZB_TRUE;
      Log_printf(LogModule_Zigbee_App, Log_INFO, "perform factory reset");
    }

    sensor_init();
    sync_attrs_from_sensor();

    /* Call the application-specific main loop */
    my_main_loop();
  }

  MAIN_RETURN(0);
}

static zb_bool_t finding_binding_cb(zb_int16_t status,
                                    zb_ieee_addr_t addr,
                                    zb_uint8_t ep,
                                    zb_uint16_t cluster)
{
  /* Unused without trace. */
  ZVUNUSED(status);
  ZVUNUSED(addr);
  ZVUNUSED(ep);
  ZVUNUSED(cluster);

  Log_printf(LogModule_Zigbee_App, Log_INFO, "finding_binding_cb status %d addr %x ep %d cluster %d",
             status, ((zb_uint32_t *)addr)[0], ep, cluster);
  return ZB_TRUE;
}

zb_uint8_t zcl_specific_cluster_cmd_handler(zb_uint8_t param)
{
  zb_zcl_parsed_hdr_t *cmd_info = ZB_BUF_GET_PARAM(param, zb_zcl_parsed_hdr_t);
  zb_bool_t unknown_cmd_received = ZB_TRUE;

  Log_printf(LogModule_Zigbee_App, Log_INFO, "> zcl_specific_cluster_cmd_handler %i", param);
  Log_printf(LogModule_Zigbee_App, Log_INFO, "payload size: %i", zb_buf_len(param));

  if (cmd_info->cmd_direction == ZB_ZCL_FRAME_DIRECTION_TO_CLI)
  {
    if (cmd_info->cmd_id == ZB_ZCL_CMD_DEFAULT_RESP)
    {
      unknown_cmd_received = ZB_FALSE;

      cmd_in_progress = ZB_FALSE;
      ZB_SCHEDULE_APP_ALARM_CANCEL(send_cmd_timeout, ZB_ALARM_ANY_PARAM);

      zb_buf_free(param);
    }
  }

  Log_printf(LogModule_Zigbee_App, Log_INFO, "< zcl_specific_cluster_cmd_handler %i", param);
  return ! unknown_cmd_received;
}

void send_cmd_timeout(zb_uint8_t param)
{
  ZVUNUSED(param);
  Log_printf(LogModule_Zigbee_App, Log_WARNING, "send command timed out, clearing cmd_in_progress");
  cmd_in_progress = ZB_FALSE;
  light_is_on = !light_is_on;
}

void send_on_req(zb_uint8_t param)
{
  zb_uint16_t addr = 0;

  ZB_ASSERT(param);

  if (ZB_JOINED() && !cmd_in_progress)
  {
    cmd_in_progress = ZB_TRUE;
    Log_printf(LogModule_Zigbee_App, Log_INFO, "send_on_req %d", param);

    ZB_SCHEDULE_APP_ALARM(send_cmd_timeout, 0, 5 * ZB_TIME_ONE_SECOND);

    ZB_ZCL_ON_OFF_SEND_ON_REQ(
      param,
      addr,
      ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT,
      0,
      ZB_SWITCH_ENDPOINT,
      ZB_AF_HA_PROFILE_ID,
      ZB_FALSE, NULL);

    light_is_on = ZB_TRUE;
  }
  else
  {
    Log_printf(LogModule_Zigbee_App, Log_INFO, "send_on_req %d - not joined or busy", param);
    zb_buf_free(param);
  }
}

void send_off_req(zb_uint8_t param)
{
  zb_uint16_t addr = 0;

  ZB_ASSERT(param);

  if (ZB_JOINED() && !cmd_in_progress)
  {
    cmd_in_progress = ZB_TRUE;
    Log_printf(LogModule_Zigbee_App, Log_INFO, "send_off_req %d", param);

    ZB_SCHEDULE_APP_ALARM(send_cmd_timeout, 0, 5 * ZB_TIME_ONE_SECOND);

    ZB_ZCL_ON_OFF_SEND_OFF_REQ(
      param,
      addr,
      ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT,
      0,
      ZB_SWITCH_ENDPOINT,
      ZB_AF_HA_PROFILE_ID,
      ZB_FALSE, NULL);

    light_is_on = ZB_FALSE;
  }
  else
  {
    Log_printf(LogModule_Zigbee_App, Log_INFO, "send_off_req %d - not joined or busy", param);
    zb_buf_free(param);
  }
}

void restart_commissioning(zb_uint8_t param)
{
  ZVUNUSED(param);
  bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);
}

void sensor_poll_handler(zb_uint8_t param)
{
  if (!param)
  {
    if(zb_buf_is_oom_state())
    {
      Log_printf(LogModule_Zigbee_App, Log_WARNING, "OOM state in sensor_poll_handler");
      while (1) {};
    }
    /* Button is pressed, gets buffer for outgoing command */
    zb_buf_get_out_delayed(sensor_poll_handler);
  }
  else
  {
    /* Update occupancy attribute */
    zb_uint8_t new_occ = sensor_get_presence() ? 1 : 0;
    if (new_occ != attr_occupancy) {
      attr_occupancy = new_occ;
      ZB_ZCL_SET_ATTRIBUTE(ZB_SWITCH_ENDPOINT, ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING,
        ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID,
        &attr_occupancy, ZB_FALSE);
    }

    if (sensor_get_presence() && !light_is_on && !cmd_in_progress)
    {
      send_on_req(param);
    }
    else if (!sensor_get_presence() && light_is_on && !cmd_in_progress)
    {
      send_off_req(param);
    }
    else
    {
      zb_buf_free(param);
    }

    ZB_SCHEDULE_APP_ALARM(sensor_poll_handler, 0, 1 * ZB_TIME_ONE_SECOND);
  }
}

void permit_joining_cb(zb_uint8_t param)
{
  Log_printf(LogModule_Zigbee_App, Log_INFO, "permit joining done");
  zb_buf_free(param);
}

void start_finding_binding(zb_uint8_t param)
{
  ZVUNUSED(param);

  Log_printf(LogModule_Zigbee_App, Log_INFO, "Successful steering, start f&b initiator");
  zb_bdb_finding_binding_initiator(ZB_SWITCH_ENDPOINT, finding_binding_cb);
}

void set_tx_power(zb_int8_t power)
{
  zb_uint32_t chanlist = DEFAULT_CHANLIST;
  for (zb_uint8_t i = 0; i < 32; i++) {
    if (chanlist & (1U << i)) {
      zb_bufid_t buf = zb_buf_get_out();
      if (!buf)
      {
        Log_printf(LogModule_Zigbee_App, Log_WARNING, "no buffer available");
        return;
      }

      zb_tx_power_params_t *power_params = (zb_tx_power_params_t *)zb_buf_begin(buf);
      power_params->status = RET_OK;
      power_params->page = 0;
      power_params->channel = i;
      power_params->tx_power = power;
      power_params->cb = NULL;

      zb_set_tx_power_async(buf);
    }
  }
}

void zboss_signal_handler(zb_uint8_t param)
{
  zb_zdo_app_signal_hdr_t *sg_p = NULL;
  zb_zdo_app_signal_type_t sig = zb_get_app_signal(param, &sg_p);
  zb_bufid_t buf;
  zb_bufid_t req_buf = 0;
  zb_zdo_mgmt_permit_joining_req_param_t *req_param;

  if (ZB_GET_APP_SIGNAL_STATUS(param) == 0)
  {
    switch(sig)
    {
      case ZB_ZDO_SIGNAL_SKIP_STARTUP:
#ifndef ZB_MACSPLIT_HOST
        Log_printf(LogModule_Zigbee_App, Log_INFO, "ZB_ZDO_SIGNAL_SKIP_STARTUP: boot, not started yet");
        set_tx_power(DEFAULT_TX_PWR);
        zboss_start_continue();
#endif /* ZB_MACSPLIT_HOST */
        break;

#ifdef ZB_MACSPLIT_HOST
      case ZB_MACSPLIT_DEVICE_BOOT:
        Log_printf(LogModule_Zigbee_App, Log_INFO, "ZB_MACSPLIT_DEVICE_BOOT: boot, not started yet");
        set_tx_power(DEFAULT_TX_PWR);
        zboss_start_continue();
        break;
#endif /* ZB_MACSPLIT_HOST */
      case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        Log_printf(LogModule_Zigbee_App, Log_INFO, "FIRST_START: start steering");
        if (perform_factory_reset)
        {
          // passing in 0 as the parameter means that a buffer will be allocated automatically for the reset
          zb_bdb_reset_via_local_action(0);
          perform_factory_reset = ZB_FALSE;
        }
        set_tx_power(DEFAULT_TX_PWR);
        bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);

        buf = zb_buf_get_out();
        if (!buf)
        {
          Log_printf(LogModule_Zigbee_App, Log_WARNING, "no buffer available");
          break;
        }

        req_param = ZB_BUF_GET_PARAM(buf, zb_zdo_mgmt_permit_joining_req_param_t);

        req_param->dest_addr = 0xfffc;
        req_param->permit_duration = 0;
        req_param->tc_significance = 1;

        zb_zdo_mgmt_permit_joining_req(buf, permit_joining_cb);

        break;
      case ZB_BDB_SIGNAL_DEVICE_REBOOT:
        Log_printf(LogModule_Zigbee_App, Log_INFO, "Device RESTARTED OK");
        if (perform_factory_reset)
        {
          Log_printf(LogModule_Zigbee_App, Log_INFO, "Performing a factory reset.");
          zb_bdb_reset_via_local_action(0);
          perform_factory_reset = ZB_FALSE;
        }
        else
        {
          ZB_SCHEDULE_APP_ALARM_CANCEL(sensor_poll_handler, ZB_ALARM_ANY_PARAM);
          ZB_SCHEDULE_APP_ALARM(sensor_poll_handler, 0, 1 * ZB_TIME_ONE_SECOND);
        }
        break;
#ifdef ZB_COORDINATOR_ROLE
      case ZB_ZDO_SIGNAL_DEVICE_ANNCE:
#else
      case ZB_BDB_SIGNAL_TC_REJOIN_DONE:
        Log_printf(LogModule_Zigbee_App, Log_INFO, "TC rejoin is completed successfully");
      case ZB_BDB_SIGNAL_STEERING:
#endif
      {
        zb_nwk_device_type_t device_type = ZB_NWK_DEVICE_TYPE_NONE;
        device_type = zb_get_device_type();
        Log_printf(LogModule_Zigbee_App, Log_INFO, "Device (%d) STARTED OK", device_type);
        ZB_SCHEDULE_APP_ALARM(start_finding_binding, 0, 3 * ZB_TIME_ONE_SECOND);
        break;
      }

      case ZB_BDB_SIGNAL_FINDING_AND_BINDING_INITIATOR_FINISHED:
      {
        Log_printf(LogModule_Zigbee_App, Log_INFO, "Finding&binding done");
        cmd_in_progress = ZB_FALSE;
        ZB_SCHEDULE_APP_ALARM_CANCEL(sensor_poll_handler, ZB_ALARM_ANY_PARAM);
        ZB_SCHEDULE_APP_ALARM(sensor_poll_handler, 0, 1 * ZB_TIME_ONE_SECOND);
      }
      break;

      case ZB_ZDO_SIGNAL_LEAVE:
      break;
      case ZB_COMMON_SIGNAL_CAN_SLEEP:
      {
#ifdef ZB_USE_SLEEP
        zb_sleep_now();
#endif
        break;
      }
      case ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
      {
        Log_printf(LogModule_Zigbee_App, Log_INFO, "Production config is ready");
        break;
      }

      default:
        Log_printf(LogModule_Zigbee_App, Log_WARNING, "Unknown signal %d, do nothing", sig);
    }
  }
  else
  {
    switch (sig)
    {
      case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
        Log_printf(LogModule_Zigbee_App, Log_WARNING, "Device can not find any network on start, so try to perform network steering");
        ZB_SCHEDULE_APP_ALARM(restart_commissioning, 0, 10 * ZB_TIME_ONE_SECOND);
        break; /* ZB_BDB_SIGNAL_DEVICE_FIRST_START */

      case ZB_BDB_SIGNAL_DEVICE_REBOOT:
        Log_printf(LogModule_Zigbee_App, Log_WARNING, "Device can not find any network on restart");

        if (zb_bdb_is_factory_new())
        {
          /* Device tried to perform TC rejoin after reboot and lost its authentication flag.
           * Do nothing here and wait for ZB_BDB_SIGNAL_TC_REJOIN_DONE to handle TC rejoin error */
          Log_printf(LogModule_Zigbee_App, Log_WARNING, "Device lost authentication flag");
        }
        else
        {
          /* Device tried to perform secure rejoin, but didn't found any networks or can't decrypt Rejoin Response
           * (it is possible when Trust Center changes network key when ZED is powered off) */
          Log_printf(LogModule_Zigbee_App, Log_WARNING, "Device is still authenticated, try to perform TC rejoin");
          ZB_SCHEDULE_APP_ALARM(zb_bdb_initiate_tc_rejoin, 0, ZB_TIME_ONE_SECOND);
        }
        break; /* ZB_BDB_SIGNAL_DEVICE_REBOOT */

      case ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY:
        Log_printf(LogModule_Zigbee_App, Log_INFO, "Production config is not present or invalid");
        break; /* ZB_ZDO_SIGNAL_PRODUCTION_CONFIG_READY */

      case ZB_BDB_SIGNAL_TC_REJOIN_DONE:
        Log_printf(LogModule_Zigbee_App, Log_WARNING, "TC rejoin failed, so try it again with interval");

        ZB_SCHEDULE_APP_ALARM(zb_bdb_initiate_tc_rejoin, 0, 3 * ZB_TIME_ONE_SECOND);
        break; /* ZB_BDB_SIGNAL_TC_REJOIN_DONE */

      case ZB_BDB_SIGNAL_STEERING:
        Log_printf(LogModule_Zigbee_App, Log_WARNING, "Steering failed, retrying again in 10 seconds");
        ZB_SCHEDULE_APP_ALARM(restart_commissioning, 0, 10 * ZB_TIME_ONE_SECOND);
        break; /* ZB_BDB_SIGNAL_STEERING */

      default:
        Log_printf(LogModule_Zigbee_App, Log_WARNING, "Unknown signal %hd with error status, do nothing", sig);
        break;
    }
  }

  if (param)
  {
    zb_buf_free(param);
  }
}
