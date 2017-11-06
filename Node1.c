/*---------------------------------Node1----------------------------------

	A Tmote Sky sensor node with Rime Address 1.0 Firmware!
	Placed in Entrance Hall of the House, close to the door.
	Input	--> BUTTON PRESS
	------------------------------------------------------------------
	COMMANDS:
		1) SWITCH ON/OFF LIGHTS in the garden.
	BEHAVIOUR:
		1) Activating/Deactivating Alarm: BLINKING ALLA LEDS
		1.a) ACK Replay on the Activatin/Deactivating Alarm Request!
		3) Open Door: wait 14s and BLINK BLUE LED every 2s until 16th s 
		4) Continously sensing temperature every 10 sec &
			Replying to Get Average Temperature Request!
------------------------------------------------------------------------*/
#include "contiki.h"
#include "stdio.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "dev/sht11/sht11-sensor.h"
#include "sys/etimer.h"
#include "net/rime/rime.h"
#include "string.h"
#include "stdlib.h"

//status values
#define	ACTIVE 					1
#define	NOT_ACTIVE				0
#define ON						1
#define OFF						0
#define ALARM_BLINK_INTERVAL	2
#define TEMPERATURE_INTERVAL	10
#define OPEN_DOOR_INTERVAL		2
#define OPEN_DOOR_DURATION		16

//communication values
#define MAX_RETRANSMISSIONS		5
#define ALARM_ON				"ALARM_ON"
#define ALARM_ON_SIZE			9
#define ALARM_OFF				"ALARM_OFF"
#define ALARM_OFF_SIZE			10
#define ALARM_ACK				"ALARM_ACK"
#define ALARM_ACK_SIZE			10
#define OPEN_GATE_DOOR			"OPEN"
#define OPEN_GATE_DOOR_SIZE		5
#define GET_TEMP				"GET_TEMP"
#define GET_TEMP_SIZE			9
#define UC_RIME_ADDR			3
#define NODE2_RIME_ADDR			2
#define NODE4_RIME_ADDR			4


//status variables
static int alarm_status = NOT_ACTIVE;
static int garden_light_status = OFF;
static int red_led = OFF;
static int blue_led = OFF;
static int green_led = OFF;

static int *last_temp_values = NULL;; /*to compute the average*/

//communication variables
static struct runicast_conn runicast;
static struct broadcast_conn broadcast;

/*----------------------------------------------------------------------*/

//to handle communication with the CU & receive commands
PROCESS(listening_process, "Listening Process");

//to handle the garden lights command reading
PROCESS(input_reader_process, "User-Input Reader Process");

//to handle activation/deactivation alarm request
PROCESS(alarm_blink_process, "Alarm Blink Process");

//to handle open gate and door request
PROCESS(open_door_process, "Open Door Process");

//tho handle temperature sensing
PROCESS(temperature_sensing_process, "Temperature Sensing Process");


AUTOSTART_PROCESSES(&listening_process, &input_reader_process, &temperature_sensing_process);

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

void save_led_status(){

	green_led = (leds_get() & LEDS_GREEN) ? ON : OFF;
	red_led = (leds_get() & LEDS_RED) ? ON : OFF;
	blue_led = (leds_get() & LEDS_BLUE) ? ON : OFF;
}

void restore_led_status(){

	(green_led == ON)? leds_on(LEDS_GREEN) : leds_off(LEDS_GREEN);
	(blue_led == ON)? leds_on(LEDS_BLUE) : leds_off(LEDS_BLUE);
	(red_led == ON)? leds_on(LEDS_RED) : leds_off(LEDS_RED);
}

/*---------------------------HANDLER FUNCTIONS--------------------------*/

/*Saving LEDS status & start Alarm Blink Process*/
void handle_alarm_request(const char* rcvd_msg){

	if(strcmp(rcvd_msg, ALARM_ON) == 0){

		save_led_status();

		alarm_status = ACTIVE;
		printf("Node1: ACTIVATING ALARM...\n");

		leds_on(LEDS_ALL);

		process_start(&alarm_blink_process, NULL);

		send_string(ALARM_ACK, ALARM_ACK_SIZE, UC_RIME_ADDR);

		return;

	}else if(strcmp(rcvd_msg, ALARM_OFF) == 0){

		alarm_status = NOT_ACTIVE;
		printf("Node1: DEACTIVATING ALARM...\n");

		process_exit(&alarm_blink_process);

		send_string(ALARM_ACK, ALARM_ACK_SIZE, UC_RIME_ADDR);

		restore_led_status(); 

		return;
	}
}

/*Starting Open Door Process*/
void handle_door_opening_request(){

	process_start(&open_door_process, NULL);
}

/*Sending the AVG TEMP VALUES to the Central Unit*/
void handle_temp_request(){

	int i, avg_temp = 0;

	for(i=0; i<5; i++)
		avg_temp += last_temp_values[i];

	avg_temp = avg_temp/5;

	send_int(avg_temp, UC_RIME_ADDR);
}

/*----------------------------------RIME--------------------------------*/

//RUNICAST

static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){

	char* rcvd_msg = (char *)packetbuf_dataptr();

	if(strcmp(rcvd_msg, GET_TEMP) == 0){
	/*Receiving Temperature Average Request*/

		handle_temp_request();
	}
}


static void sent_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){

//printf("sent to %d.%d, retransmissions %d\n", to->u8[0], to->u8[1], retransmissions);
}


static void timedout_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){

//printf("Timed out sending to %d.%d, retransmit %d\n",to->u8[0], to->u8[1], retransmissions);
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

		handle_door_opening_request();
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

	while(1){
	
		PROCESS_WAIT_EVENT();
	}

	PROCESS_END();
}

/*-------------------------INPUT READER PROCESS-------------------------*/

PROCESS_THREAD(input_reader_process, ev, data){

	PROCESS_BEGIN();

	SENSORS_ACTIVATE(button_sensor);

	(garden_light_status == OFF) ? leds_on(LEDS_RED) : leds_on(LEDS_GREEN);

	while(1){

		PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event && data == &button_sensor);

		if(garden_light_status == OFF){

			printf("Node1: TURNING ON GARDEN LIGHTS\n");
			if(alarm_status == NOT_ACTIVE){
			/*Switch the leds*/
				leds_off(LEDS_RED);
				leds_on(LEDS_GREEN);
			}
			else if(alarm_status == ACTIVE){
			/*Saving the leds status to restore when deactivating the alarm*/
				green_led = ON;
				red_led = OFF;
			}
		
		}else{

			printf("Node1: TURNING OFF GARDEN LIGHTS\n");
			if(alarm_status == NOT_ACTIVE){
			/*Switch the leds*/
				leds_on(LEDS_RED);
				leds_off(LEDS_GREEN);
			}
			else if(alarm_status == ACTIVE){
			/*Saving the leds status to restore when deactivating the alarm*/
				green_led = OFF;
				red_led = ON;
			}
		}

		garden_light_status = (garden_light_status == OFF) ? ON : OFF;
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

/*--------------------------OPEN DOOR PROCESS---------------------------*/

PROCESS_THREAD(open_door_process, ev, data){

	static struct etimer open_door_et;
	static int duration;

	PROCESS_BEGIN();

	etimer_set(&open_door_et, OPEN_DOOR_INTERVAL*CLOCK_SECOND);

	duration = OPEN_DOOR_DURATION;

	while(duration > 0){
	
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&open_door_et));

		if(duration <= 4){

			printf("Node1: DOOR %s\n", (duration == 4) ? "OPENING..." : "CLOSED!");
			leds_toggle(LEDS_BLUE);
		}

		etimer_reset(&open_door_et);

		duration -= 2;
	}

	PROCESS_END();
}


/*--------------------TEMPERATURE SENSING PROCESS----------------------*/

PROCESS_THREAD(temperature_sensing_process, ev, data){

	static struct etimer temp_et;
	int i;

	PROCESS_BEGIN();

	SENSORS_ACTIVATE(sht11_sensor);

	etimer_set(&temp_et, TEMPERATURE_INTERVAL*CLOCK_SECOND);

	while(1){

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&temp_et));

		if(last_temp_values == NULL){
			/*initializing last 5 temperature values*/

			last_temp_values = malloc(5*sizeof(int));

			for(i=0; i<5; i++)
				last_temp_values[i] = (((sht11_sensor.value(SHT11_SENSOR_TEMP)/10) - 396)/10);
		}
		else{
			/*udating last 5 temperature values*/

			for(i=0; i<4; i++)
				last_temp_values[i] = last_temp_values[i+1];

			last_temp_values[4] = (((sht11_sensor.value(SHT11_SENSOR_TEMP)/10) - 396)/10);
		}
	
//printf("AVG TEMP: %d\n", (last_temp_values[0]+last_temp_values[1]+last_temp_values[2]+last_temp_values[3]+last_temp_values[4])/5);

		etimer_reset(&temp_et);
	}

	free(last_temp_values);

	last_temp_values = NULL;

	PROCESS_END();
}