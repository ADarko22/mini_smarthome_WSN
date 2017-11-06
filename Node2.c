/*---------------------------------Node2----------------------------------
	A Tmote Sky sensor node with Rime Address 2.0 Firmware!
	Placed in the Garden, close to the gate.

	NOTE:
	Open Gate Command will trigger the Open Door Command to Node1!

	BEHAVIOUR:
		1) Activating/Deactivating Alarm: BLINKING ALLA LEDS / STOP BLINKING
		1.a) ACK Replay on the Activatin/Deactivating Alarm Request!
		2) Locking/Unlocking Gate: TURN ON RED/GREEN & TURN OFF GREEN/RED LEDS
		3) Opening Gate: BLINKING BLUE LED every 2s for 16s
		5) Replying to Get External Light Request!
------------------------------------------------------------------------*/
#include "contiki.h"
#include "stdio.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "dev/light-sensor.h"
#include "sys/etimer.h"
#include "net/rime/rime.h"
#include "string.h"


//status values
#define	ACTIVE 					1
#define	NOT_ACTIVE				0
#define ON						1
#define OFF						0
#define LOCKED					1
#define UNLOCKED				0

#define ALARM_BLINK_INTERVAL	2
#define OPEN_GATE_INTERVAL		2
#define OPEN_GATE_DURATION		16

//communication values
#define MAX_RETRANSMISSIONS		5
#define ALARM_ON				"ALARM_ON"
#define ALARM_ON_SIZE			9
#define ALARM_OFF				"ALARM_OFF"
#define ALARM_OFF_SIZE			10
#define ALARM_ACK				"ALARM_ACK"
#define ALARM_ACK_SIZE			10
#define LOCK_GATE				"LOCK"
#define LOCK_GATE_SIZE			5
#define UNLOCK_GATE				"UNLOCK"
#define UNLOCK_GATE_SIZE		7
#define OPEN_GATE_DOOR			"OPEN"
#define OPEN_GATE_DOOR_SIZE		5
#define GET_LIGHT				"GET_LIGHT"
#define GET_LIGHT_SIZE			10
#define UC_RIME_ADDR			3
#define NODE1_RIME_ADDR			1
#define NODE4_RIME_ADDR			4


//status variables
static int alarm_status = NOT_ACTIVE;
static int gate_status = LOCKED;
static int red_led = OFF;
static int blue_led = OFF;
static int green_led = OFF;

//communication variables
static struct runicast_conn runicast;
static struct broadcast_conn broadcast;

/*----------------------------------------------------------------------*/

//to handle communication with the CU & receive commands
PROCESS(listening_process, "Listening Process");

//to handle activation/deactivation alarm request
PROCESS(alarm_blink_process, "Alarm Blink Process");

//to handle Open Gate and Door Request
PROCESS(open_gate_process, "Open Gate Process");

AUTOSTART_PROCESSES(&listening_process);

/*---------------------------UTILITY FUNCTIONS--------------------------*/

void send_string(char* msg, int size, int rime_addr){

		linkaddr_t addr;
		packetbuf_copyfrom(msg, size);
	    addr.u8[0] = rime_addr;
	    addr.u8[1] = 0;
	    runicast_send(&runicast, &addr, MAX_RETRANSMISSIONS);
}

void send_int(int msg, int rime_addr){

		linkaddr_t addr;
		packetbuf_copyfrom(&msg, sizeof(int));
	    addr.u8[0] = rime_addr;
	    addr.u8[1] = 0;
	    runicast_send(&runicast, &addr, MAX_RETRANSMISSIONS);
}
/*---------------------------HANDLER FUNCTIONS--------------------------*/

/*Saving LEDS status & start Alarm Blink Process*/
void handle_alarm_request(const char* rcvd_msg){

	if(strcmp(rcvd_msg, ALARM_ON) == 0){

		green_led = (leds_get() & LEDS_GREEN) ? ON : OFF;
		red_led = (leds_get() & LEDS_RED) ? ON : OFF;
		blue_led = (leds_get() & LEDS_BLUE) ? ON : OFF;

		alarm_status = ACTIVE;
		printf("Node2: ACTIVATING ALARM...\n");
		
		leds_on(LEDS_ALL);

		process_start(&alarm_blink_process, NULL);

		send_string(ALARM_ACK, ALARM_ACK_SIZE, UC_RIME_ADDR);
	    
		return;

	}else if(strcmp(rcvd_msg, ALARM_OFF) == 0){

		alarm_status = NOT_ACTIVE;
		printf("Node2: DEACTIVATING ALARM...\n");

		process_exit(&alarm_blink_process);

		send_string(ALARM_ACK, ALARM_ACK_SIZE, UC_RIME_ADDR);

		(green_led == ON)? leds_on(LEDS_GREEN) : leds_off(LEDS_GREEN);
		(blue_led == ON)? leds_on(LEDS_BLUE) : leds_off(LEDS_BLUE);
		(red_led == ON)? leds_on(LEDS_RED) : leds_off(LEDS_RED);

		return;
	}
}

/*Notify Listening Process to HANDLE LEDS for GATE LOCK/UNLOCK*/
void handle_gate_lock_request(const char* rcvd_msg, const linkaddr_t *from){

	if(strcmp(rcvd_msg, LOCK_GATE) == 0){

			leds_on(LEDS_RED);
			leds_off(LEDS_GREEN);
			printf("Node2: LOCKING GATE...\n");
			gate_status = LOCKED;

	}else if(strcmp(rcvd_msg, UNLOCK_GATE) == 0){

			leds_on(LEDS_GREEN);
			leds_off(LEDS_RED);
			printf("Node2: UNLOCKING GATE...\n");
			gate_status = UNLOCKED;
	}
}

/*Starting Open Gate Process*/
void handle_gate_opening_request(){

	process_start(&open_gate_process, NULL);
}

/*Starting Light Sensing Process to sense and reply the ext. light value*/
void handle_light_request(){

	SENSORS_ACTIVATE(light_sensor);

	int light = (10*light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC))/7;

	send_int(light, UC_RIME_ADDR);

	SENSORS_DEACTIVATE(light_sensor);
}

/*----------------------------------RIME--------------------------------*/

//RUNICAST

static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){

	char* rcvd_msg = (char *)packetbuf_dataptr();

	if((strcmp(rcvd_msg, UNLOCK_GATE) == 0) || (strcmp(rcvd_msg, LOCK_GATE) == 0)){
	/*Receiving Lock/Unlock Gate Request*/
		
		handle_gate_lock_request(rcvd_msg, from);

	}else if(strcmp(rcvd_msg, GET_LIGHT) == 0){
	/*Receiving Temperature Average Request*/

		handle_light_request();
	}
}


static void sent_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){

//printf("sent to %d.%d, retransmissions %d\n", to->u8[0], to->u8[1], retransmissions);
}


static void timedout_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){

//printf("timed out sending to %d.%d, retransmit %d\n",to->u8[0], to->u8[1], retransmissions);
}


static const struct runicast_callbacks runicast_calls = {recv_runicast, sent_runicast, timedout_runicast};


//BROADCAST

static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from){

	char* rcvd_msg = (char *)packetbuf_dataptr();

	if(strcmp(rcvd_msg, ALARM_ON) == 0 || strcmp(rcvd_msg, ALARM_OFF) == 0)
	/*Receiving Activate/Deaactivate Alarm Request*/	

		handle_alarm_request(rcvd_msg);

	else if(strcmp(rcvd_msg, OPEN_GATE_DOOR) == 0)
	/*Receiving Open Gate e Door Request*/

		handle_gate_opening_request();
}


static const struct broadcast_callbacks broadcast_call = {broadcast_recv};


/*######################################################################*/
/*--------------------------LISTENING PROCESS---------------------------*/

PROCESS_THREAD(listening_process, ev, data){

	PROCESS_EXITHANDLER(runicast_close(&runicast));
	PROCESS_EXITHANDLER(broadcast_close(&broadcast));

	PROCESS_BEGIN();

	runicast_open(&runicast, 144, &runicast_calls);
	broadcast_open(&broadcast, 129, &broadcast_call);

	/*Initializing the LOCK GATE LEDS STATUS*/
	(gate_status == LOCKED) ? leds_on(LEDS_RED) : leds_on(LEDS_GREEN);
		
	while(1){
	
		PROCESS_WAIT_EVENT();
	}

	PROCESS_END();
}

/*-------------------------ALARM BLINK PROCESS--------------------------*/

PROCESS_THREAD(alarm_blink_process, ev, data){

	static struct etimer alarm_blink_et;

	PROCESS_BEGIN();

	etimer_set(&alarm_blink_et, ALARM_BLINK_INTERVAL*CLOCK_SECOND);

	while(1){
	
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&alarm_blink_et));

		leds_toggle(LEDS_ALL);

		etimer_reset(&alarm_blink_et);
	}

	PROCESS_END();
}

/*--------------------------OPEN GATE PROCESS--------------------------*/

PROCESS_THREAD(open_gate_process, ev, data){

	static struct etimer open_gate_et;
	static int duration;

	PROCESS_BEGIN();

	etimer_set(&open_gate_et, OPEN_GATE_INTERVAL*CLOCK_SECOND);

	printf("Node2: GATE OPENING ...\n");

	duration = OPEN_GATE_DURATION;

	while(duration > 0){
	
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&open_gate_et));

		leds_toggle(LEDS_BLUE);

		etimer_reset(&open_gate_et);

		duration -= 2;
	}

	printf("Node2: GATE CLOSED!\n");

	PROCESS_END();
}

