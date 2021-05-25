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
#include <math.h>

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

/* Configuration */
#define SEND_INTERVAL (CLOCK_SECOND/2)
#define MEASUREMENT_INTERVAL 10

#if MAC_CONF_WITH_TSCH
#include "net/mac/tsch/tsch.h"
#endif /* MAC_CONF_WITH_TSCH */

#define COLS_ROWS 3
#define NODES_COUNT (COLS_ROWS*COLS_ROWS)
#define CENTRAL_NODE ((NODES_COUNT+1)/2)

static uint32_t received_messages[NODES_COUNT];
static int16_t centre_distances[NODES_COUNT];

int16_t get_position_x(uint8_t node_nr) {
  return (node_nr-1) % COLS_ROWS;
}

int16_t get_position_y(uint8_t node_nr) {
  return (node_nr-1) / COLS_ROWS;
}

float absf(float a) {
  return a > 0 ? a : -a;
}

int16_t absint16(int16_t i) {
  return i  > 0 ? i : -i;
}

// in the real world, this could be a value flashed into ROM
// or we flash the position as latitude, longitude in a metric system like EPSG 25832 (for ease of distance calculations)
// central node floods its own position -> every node knows its distance to the centre
int16_t get_distance_centre(uint8_t node_nr) {
  // float x = (float)(get_position_x(node_nr) - get_position_x(CENTRAL_NODE));
  // float y = (float)(get_position_y(node_nr) - get_position_y(CENTRAL_NODE));
  // no idea how to make it compile with -lm flag, so we can use sqrt: return sqrt(x*x+y*y);
  // so lets go with manhattan distance:
  int16_t x = get_position_x(node_nr) - get_position_x(CENTRAL_NODE);
  int16_t y = get_position_y(node_nr) - get_position_y(CENTRAL_NODE);
  return absint16(x) + absint16(y);
}

uint8_t get_node_nr(const linkaddr_t* src) {
  return src->u8[0];
}

void get_node_addr(uint8_t node_nr, linkaddr_t* target) {
  linkaddr_t addr = {{ node_nr, node_nr, node_nr, 0x00, node_nr, 0x74, 0x12, 0x00 }};
  memcpy(target, &addr, sizeof(linkaddr_t));
}

static linkaddr_t coordinator_addr =  {{ CENTRAL_NODE, CENTRAL_NODE, CENTRAL_NODE, 0x00, CENTRAL_NODE, 0x74, 0x12, 0x00 }};

typedef struct userdata {
  uint8_t node_id;
  uint32_t round;
  uint8_t measurement; // the random numbers
} userdata_t;

typedef struct beacondata {
  int16_t centre_distance;
} beacondata_t;

/*---------------------------------------------------------------------------*/
PROCESS(nullnet_example_process, "NullNet propagation example");
AUTOSTART_PROCESSES(&nullnet_example_process);

/*---------------------------------------------------------------------------*/

#define QUEUE_SIZE 5
static userdata_t queue[QUEUE_SIZE];
static int front = -1;
static int back = -1;

bool is_queue_empty() {
  return front==-1 && back==-1;
}

bool is_queue_full() {
  return (back-front)==QUEUE_SIZE-1;
}

void push_back(userdata_t* data) {
  if(front == -1 && back == -1) {
    back = 0;
    front = 0;
  } else {
    back++;
  }
  memcpy(&queue[back%QUEUE_SIZE], data, sizeof(userdata_t));
  userdata_t* log_data = &queue[back%QUEUE_SIZE];
  // LOG_INFO("Queue round %lu with measurement %u from node %u\n", log_data->round, log_data->measurement, log_data->node_id);
}

void pop_front(userdata_t* data) {
  userdata_t* log_data = &queue[front%QUEUE_SIZE];
  // LOG_INFO("Dequeue round %lu with measurement %u from node %u\n", log_data->round, log_data->measurement, log_data->node_id);
  memcpy(data, &queue[front%QUEUE_SIZE], sizeof(userdata_t));
  if(front==back) {
    front = -1;
    back = -1;
  } else {
    front++;
  }
}

void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
  uint8_t sender = get_node_nr(src);
  // just some sanity check in case we get a weird address
  if(sender>NODES_COUNT) return;
  if(linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr)) {
    // we are the central node. lets do some fancy logging
    if(len == sizeof(userdata_t)) {
      userdata_t recv_data;
      memcpy(&recv_data, data, sizeof(userdata_t));
      if(recv_data.node_id != 0)
        LOG_INFO("Received round %lu with measurement %u from node %u\n", recv_data.round, recv_data.measurement, recv_data.node_id);
    }
  } else if(!linkaddr_cmp(dest, &linkaddr_null) && !linkaddr_cmp(dest, &linkaddr_node_addr)) {
    // its neither broadcast nor is it for us
  } else {
    // make sure userdata_t is not the same size as beacondata_t
    if(len == sizeof(userdata_t)) {
      userdata_t recv_data;
      if(!is_queue_full()) {
	// actually we can save one memcpy:
	push_back((userdata_t*)data);
      }
    } else if(len == sizeof(beacondata_t)) {
      beacondata_t recv_beacondata;
      memcpy(&recv_beacondata, data, sizeof(beacondata_t));
      // LOG_INFO("Updating distance to centre for node %u: %u\n", sender-1, (uint16_t)recv_beacondata.centre_distance);
      centre_distances[sender-1] = recv_beacondata.centre_distance;
      received_messages[sender-1]++; // we count only beacons!
    }
  }
}

void send_to_parent_node() {
  uint8_t node_nr = get_node_nr(&linkaddr_node_addr);
  int best_parent_node=-1;
  int32_t best_parent_heuristic=0;
  int i=0;
  for(i=0; i<NODES_COUNT; ++i) {
    if(i == (node_nr-1)) continue;
    // LOG_INFO("Msgs: %lu, Distance I: %i, Distance N: %i\n", received_messages[i], centre_distances[i], get_distance_centre(node_nr));
    // encourage sending to a node that's closer to the centre and from which we received a lot of messages
    int32_t heuristic = (get_distance_centre(node_nr) - centre_distances[i]) * received_messages[i];
    // if(i == (CENTRAL_NODE-1)) heuristic *= 2;
    // LOG_INFO("Heuristic for node %u: %u\n", (uint8_t)i, (uint16_t)heuristic);
    if(heuristic > best_parent_heuristic) {
      best_parent_heuristic = heuristic;
      best_parent_node = i;
    }
  }
  if(best_parent_node>=0) {
    best_parent_node++; // node nrs start with 1
    // LOG_INFO("Found parent %u\n", (uint8_t)best_parent_node);
    linkaddr_t parent;
    get_node_addr((uint8_t)best_parent_node, &parent);
    NETSTACK_NETWORK.output(&parent);
  }
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(nullnet_example_process, ev, data)
{
  static struct etimer periodic_timer;
  static userdata_t userdata;
  static beacondata_t beacondata;
  uint8_t node_nr = get_node_nr(&linkaddr_node_addr);
  PROCESS_BEGIN();

#if MAC_CONF_WITH_TSCH
  tsch_set_coordinator(linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr));
#endif /* MAC_CONF_WITH_TSCH */

  /* Initialize NullNet */
  nullnet_buf = (uint8_t *)&userdata;
  nullnet_len = sizeof(userdata_t);
  nullnet_set_input_callback(input_callback);

  etimer_set(&periodic_timer, SEND_INTERVAL);
  static uint32_t iterations = 0;
  static uint32_t rounds = 0;
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    if(iterations < MEASUREMENT_INTERVAL) {
      // alternate between sending beacons and forwarding queued userdata
      // this might work badly for large setups when a few nodes have to forward lots of data
      if(iterations % 2 == 0) {
        // in this example, we do not need to calculate the distance everytime again
        // however, calculating it here would be great for non-stationary nodes
        beacondata.centre_distance = get_distance_centre(node_nr);
        nullnet_buf = (uint8_t *)&beacondata;
        nullnet_len = sizeof(beacondata_t);
        NETSTACK_NETWORK.output(NULL); // lets broadcast this, so everyone can update their routing tables
      } else {
        if(!linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr)) {
          if(!is_queue_empty()) {
            pop_front(&userdata);
            nullnet_buf = (uint8_t *)&userdata;
            nullnet_len = sizeof(userdata_t);
            send_to_parent_node();
	  }
        }
      }
      iterations++;
    } else {
      // we send our measurement along the path towards the centre
      if(!linkaddr_cmp(&coordinator_addr, &linkaddr_node_addr)) {
        userdata.node_id = node_nr;
        userdata.round = rounds;
        userdata.measurement = random_rand() % 101;
        LOG_INFO("Sending round %lu with measurement %u from node %u\n", userdata.round, userdata.measurement, userdata.node_id);
        nullnet_buf = (uint8_t *)&userdata;
        nullnet_len = sizeof(userdata_t);
        send_to_parent_node();
      }
      rounds++;
      iterations = 0;
    }
    etimer_reset(&periodic_timer);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
