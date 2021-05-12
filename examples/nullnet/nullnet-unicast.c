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
 *         NullNet unicast example
 * \author
*         Simon Duquennoy <simon.duquennoy@ri.se>
 *
 */

#include "contiki.h"

#include "dev/leds.h"

#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "sys/clock.h"

#include <random.h>
#include <string.h>
#include <stdio.h> /* For printf() */

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

/* Configuration */
#define SEND_INTERVAL (8 * CLOCK_SECOND)
// static linkaddr_t dest_addr =         {{ 0x01, 0x01, 0x01, 0x00, 0x01, 0x74, 0x12, 0x00 }};
static linkaddr_t dest_addr =         {{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};

#if MAC_CONF_WITH_TSCH
#include "net/mac/tsch/tsch.h"
static linkaddr_t coordinator_addr =  {{ 0x01, 0x01, 0x01, 0x00, 0x01, 0x74, 0x12, 0x00 }};
#endif /* MAC_CONF_WITH_TSCH */

typedef struct userdata {
	unsigned number;
	unsigned count;
	clock_time_t timestamp;
} userdata_t;


/*---------------------------------------------------------------------------*/
PROCESS(nullnet_example_process, "NullNet unicast example");
AUTOSTART_PROCESSES(&nullnet_example_process);

/*---------------------------------------------------------------------------*/
void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
  static uint32_t recv_messages[6] = {0,0,0,0,0,0 };
  static uint64_t delays[6] = {0,0,0,0,0,0 };
  if(len == sizeof(userdata_t)) {
    userdata_t rx_data;
    uint8_t sender_id = src->u8[0];
    memcpy(&rx_data, data, sizeof(rx_data));
    if (sender_id < sizeof(recv_messages)) {
      recv_messages[sender_id]++;
      delays[sender_id] += (clock_time() - rx_data.timestamp);
      LOG_INFO("Messages received from %u: %u, Messages sent by %u: %u\n", sender_id, recv_messages[sender_id], sender_id, rx_data.count);
      LOG_INFO("Average delay from %u: %u ms\n", sender_id, 1000 * delays[sender_id] / recv_messages[sender_id] / CLOCK_SECOND);
    }
    LOG_INFO("Received %u from %u\n", rx_data.number, sender_id);
    leds_toggle(LEDS_RED);
  }
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(nullnet_example_process, ev, data)
{
  static struct etimer periodic_timer;
  static userdata_t tx_data;

  PROCESS_BEGIN();


#if MAC_CONF_WITH_TSCH
  tsch_set_coordinator(linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr));
#endif /* MAC_CONF_WITH_TSCH */

  /* Initialize NullNet */
  nullnet_buf = (uint8_t *)&tx_data;
  nullnet_len = sizeof(tx_data);
  nullnet_set_input_callback(input_callback);

  if(!linkaddr_cmp(&dest_addr, &linkaddr_node_addr)) {
    etimer_set(&periodic_timer, SEND_INTERVAL);
    while(1) {
      tx_data.count++;
      tx_data.number = random_rand() % 101;
      tx_data.timestamp = clock_time();
      PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
      LOG_INFO("Sending msg #%u with random number %u to ", tx_data.count, tx_data.number);
      LOG_INFO_LLADDR(&dest_addr);
      LOG_INFO_("\n");

      NETSTACK_NETWORK.output(&dest_addr);
      etimer_reset_with_new_interval(&periodic_timer, CLOCK_SECOND * (random_rand()&0xFFFF)/65535);
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
