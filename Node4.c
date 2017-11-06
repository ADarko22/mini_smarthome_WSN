/*---------------------------------Node4----------------------------------

	A Tmote Sky sensor node with Rime Address 4.0 Firmware!
	Placed in the Bed Room, close to the air-conditioner.
	Input	--> BUTTON PRESS
	------------------------------------------------------------------
	COMMANDS:
		1) ACTIVATE/DEACTIVATE COMFORT BEDROOM.
	BEHAVIOUR:
		1) When Active the GREEN LED is ON.  (RED LED OFF)
			Temperature is SENSED every 300 sec and CHEKED:
				if < 15°: Air-Conditionating is Started: BLUE LED BLINKS
				if > 23°: Air-Conditionating is Stopped: BLUE LED OFF
			When Not Active the RED LED is ON. (GREEN LED OFF)
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
#define TEMPERATURE_INTERVAL	60	/*set to 300 for 5 minutes!*/
#define COMFORT_BLINK_INTERVAL	2
#define TEMPERATURE_OPTIMAL		19
#define TEMPERATURE_MIN			15
#define TEMPERATURE_MAX			23

//communication values
#define MAX_RETRANSMISSIONS		5
#define START_COMFORT_BED		"COMFORT"
#define START_COMFORT_BED_SIZE	8
#define STOP_COMFORT_BED		"NO_COMFORT"
#define STOP_COMFORT_BED_SIZE	11
#define RECEIVED				1
#define NOT_RECEIVED			0	
#define UC_RIME_ADDR			3
#define NODE1_RIME_ADDR			1
#define NODE2_RIME_ADDR			2

//status variables
static int comfort_status = NOT_ACTIVE;
static int air_conditioner_status = NOT_ACTIVE;

static int *last_temp_values = NULL;; /*to decide if start/stop*/

//communication variables
static struct runicast_conn runicast;

/*----------------------------------------------------------------------*/

//to handle communication with the CU & receive commands
PROCESS(listening_process, "Listening Process");

//to handle the comfort bedroom input command reading
PROCESS(input_reader_process, "User-Input Reader Process");

//to handle the comfort bedroom command/request
PROCESS(comfort_bedroom_process, "Comfort Bedroom Temperature Process");


AUTOSTART_PROCESSES(&listening_process, &input_reader_process);

/*---------------------------UTILITY FUNCTIONS--------------------------*/

void send_string(char* msg, int size, int rime_addr){

		linkaddr_t addr;
		packetbuf_copyfrom(msg, size);
	    addr.u8[0] = rime_addr;
	    addr.u8[1] = 0;
	    runicast_send(&runicast, &addr, MAX_RETRANSMISSIONS);
}


/*---------------------------HANDLER FUNCTIONS--------------------------*/

void handle_comfort_request(const char* rcvd_msg){

	if(strcmp(rcvd_msg, START_COMFORT_BED) == 0){

		comfort_status = ACTIVE;
		printf("Node4: COMFORT ACTIVATED\n");

		leds_off(LEDS_RED);
		leds_on(LEDS_GREEN);

		process_start(&comfort_bedroom_process, NULL);

	}else{

		comfort_status = NOT_ACTIVE;
		printf("Node4: COMFORT DEACTIVATED\n");

		leds_on(LEDS_RED);
		leds_off(LEDS_GREEN);
		leds_off(LEDS_BLUE);

		process_exit(&comfort_bedroom_process);
	}
}

/*----------------------------------RIME--------------------------------*/

//RUNICAST

static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){

	char* rcvd_msg = (char *)packetbuf_dataptr();

	if(strcmp(rcvd_msg, START_COMFORT_BED) == 0 || strcmp(rcvd_msg, STOP_COMFORT_BED) == 0){
	/*Receiving Activate/Deactivate Comfort Bedroom*/

		handle_comfort_request(rcvd_msg);
	}
}


static void sent_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){

//printf("sent to %d.%d, retransmissions %d\n", to->u8[0], to->u8[1], retransmissions);
}


static void timedout_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){

//printf("Timed out sending to %d.%d, retransmit %d\n",to->u8[0], to->u8[1], retransmissions);
}


static const struct runicast_callbacks runicast_calls = {recv_runicast, sent_runicast, timedout_runicast};


/*######################################################################*/
/*--------------------------LISTENING PROCESS---------------------------*/

PROCESS_THREAD(listening_process, ev, data){

	PROCESS_EXITHANDLER(runicast_close(&runicast));

	PROCESS_BEGIN();

	runicast_open(&runicast, 144, &runicast_calls);

	while(1){
	
		PROCESS_WAIT_EVENT();
	}

	PROCESS_END();
}

/*-------------------------INPUT READER PROCESS-------------------------*/

PROCESS_THREAD(input_reader_process, ev, data){

	PROCESS_BEGIN();

	SENSORS_ACTIVATE(button_sensor);

	(comfort_status == NOT_ACTIVE) ? leds_on(LEDS_RED) : leds_on(LEDS_GREEN);

	while(1){
		
		PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event && data == &button_sensor);

		if(comfort_status == NOT_ACTIVE){

			printf("Node4: COMFORT ACTIVATED\n");

			leds_off(LEDS_RED);
			leds_on(LEDS_GREEN);

			send_string(START_COMFORT_BED, START_COMFORT_BED_SIZE, UC_RIME_ADDR);

			process_start(&comfort_bedroom_process, NULL);
		
		}else{

			printf("Node4: COMFORT DEACTIVATED\n");

			leds_on(LEDS_RED);
			leds_off(LEDS_GREEN);


			send_string(STOP_COMFORT_BED, STOP_COMFORT_BED_SIZE, UC_RIME_ADDR);

			process_exit(&comfort_bedroom_process);
		}

		comfort_status = (comfort_status == NOT_ACTIVE) ? ACTIVE : NOT_ACTIVE;
	}

	PROCESS_END();
}

/*-------------------------COMFORT BEDROOM PROCESS--------------------------*/

PROCESS_THREAD(comfort_bedroom_process, ev, data){

	static struct etimer comfort_et;
	static int temperature_interval;
	int i;
	int temperature, avg_temperature;

	PROCESS_BEGIN();

	etimer_set(&comfort_et, COMFORT_BLINK_INTERVAL*CLOCK_SECOND);

	temperature_interval = 0;
	avg_temperature = 0;

	while(1){

		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&comfort_et));

		if(temperature_interval <= 2){

			temperature_interval = TEMPERATURE_INTERVAL;

			SENSORS_ACTIVATE(sht11_sensor);

			temperature = (((sht11_sensor.value(SHT11_SENSOR_TEMP)/10) - 396)/10);

			SENSORS_DEACTIVATE(sht11_sensor);

			printf("Node4: Temperature %d\n", temperature);

			if(last_temp_values == NULL){
				/*initializing last 5 temperature values*/
				
				last_temp_values = malloc(5*sizeof(int));

				for(i=0; i<5; i++)
					last_temp_values[i] = temperature;

				avg_temperature = temperature;
			}
			else{
				/*udating last 5 temperature values & computing the averate temp*/

				avg_temperature = 0;

				for(i=0; i<4; i++){

					avg_temperature += last_temp_values[i];
					last_temp_values[i] = last_temp_values[i+1];
				}

				avg_temperature += last_temp_values[4];
				avg_temperature = avg_temperature/5;

				last_temp_values[4] = temperature;
			}

			/*updating the air conditione status*/
			if(temperature <= TEMPERATURE_MIN || avg_temperature < TEMPERATURE_OPTIMAL)

				air_conditioner_status = ACTIVE;

			else if(temperature >= TEMPERATURE_MAX || avg_temperature > TEMPERATURE_OPTIMAL)

				air_conditioner_status = NOT_ACTIVE;
		}

		/*blink if ari conditioner is active*/
		if(air_conditioner_status == ACTIVE)

			leds_toggle(LEDS_BLUE);
		else
			leds_off(LEDS_BLUE);

		temperature_interval -= 2;

		etimer_reset(&comfort_et);
	}

	PROCESS_END();
}