/*
 * Copyright (c) 2017, RISE SICS.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         NullNet broadcast example
 * \author
*         Simon Duquennoy <simon.duquennoy@ri.se>
 *
 */

#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include <string.h>
#include <stdio.h> /* For printf() */
#include "dev/leds.h"
#include <random.h>

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

/* Configuration */
#define SEND_INTERVAL (2 * CLOCK_SECOND)
#define RESEND_COUNT 3
#define RESEND_INTERVAL (CLOCK_SECOND / 20)

#if MAC_CONF_WITH_TSCH
#include "net/mac/tsch/tsch.h"
#endif /* MAC_CONF_WITH_TSCH */

#define central_node 0x0D
static linkaddr_t coordinator_addr =  {{ central_node, central_node, central_node, 0x00, central_node, 0x74, 0x12, 0x00 }};

#define COMMAND_NONE 0
#define COMMAND_TOGGLE_LED 1

static leds_num_t led_list[] = {LEDS_RED, LEDS_BLUE, LEDS_YELLOW, LEDS_GREEN};
static uint32_t led_list_count = sizeof(led_list)/sizeof(leds_num_t);

leds_num_t get_random_led() {
  return led_list[random_rand() % led_list_count];
}

typedef struct userdata {
  uint8_t command;
  leds_num_t led;
  uint32_t seq;
} userdata_t;

/*---------------------------------------------------------------------------*/
PROCESS(nullnet_example_process, "NullNet broadcast example");
AUTOSTART_PROCESSES(&nullnet_example_process);

/*---------------------------------------------------------------------------*/
void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
  if(linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr)) {
    // we are the central node. do nothing (yet)
    return;
  } else {
    // remember the last received seq in order to prevent broadcasting old messages
    // first message should have seq 1, so broadcasting is performed already
    static uint32_t last_seq = 0;
    if(len == sizeof(userdata_t)) {
      userdata_t recv_data;
      memcpy(&recv_data, data, sizeof(userdata_t));
      switch(recv_data.command) {
      case COMMAND_TOGGLE_LED:
        LOG_INFO("Received seq %lu COMMAND_TOGGLE_LED command %u from ", recv_data.seq, recv_data.led);
        if(recv_data.seq > last_seq) {
          LOG_INFO_LLADDR(src);
          LOG_INFO_("\n");
          leds_toggle(recv_data.led);
          memcpy(nullnet_buf, &recv_data, sizeof(userdata_t));
          NETSTACK_NETWORK.output(NULL);
        }
        break;
      };
      last_seq = recv_data.seq;
    }
  }
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(nullnet_example_process, ev, data)
{
  static struct etimer periodic_timer;
  static userdata_t userdata;
  static uint32_t seq = 0;
  static uint32_t resend_count = 0;
  PROCESS_BEGIN();

#if MAC_CONF_WITH_TSCH
  tsch_set_coordinator(linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr));
#endif /* MAC_CONF_WITH_TSCH */

  /* Initialize NullNet */
  nullnet_buf = (uint8_t *)&userdata;
  nullnet_len = sizeof(userdata_t);
  nullnet_set_input_callback(input_callback);

  if(linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr)) {
    userdata.command = COMMAND_TOGGLE_LED;
    etimer_set(&periodic_timer, SEND_INTERVAL);
  } else {
    userdata.command = COMMAND_NONE;
    etimer_set(&periodic_timer, RESEND_INTERVAL);
  }
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    if(linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr)) {
      // done only once at startup
      // userdata.command = COMMAND_TOGGLE_LED;
      userdata.led = get_random_led();
      seq++;
      userdata.seq = seq;
      LOG_INFO("Sending seq %lu COMMAND_TOGGLE_LED command %u to ", userdata.seq, userdata.led);
      LOG_INFO_LLADDR(NULL);
      LOG_INFO_("\n");
      leds_toggle(userdata.led);
      memcpy(nullnet_buf, &userdata, sizeof(userdata_t));
      NETSTACK_NETWORK.output(NULL);
    } else {
      if(userdata.command != COMMAND_NONE) {
        if(userdata.seq != seq) {
          seq = userdata.seq;
	  resend_count = RESEND_COUNT;
	}
	if(resend_count > 0) {
          NETSTACK_NETWORK.output(NULL);
          resend_count--;
	}
      }
    }
    
    etimer_reset(&periodic_timer);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
