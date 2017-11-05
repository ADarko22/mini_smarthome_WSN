/*-----------------------------Central Unit-------------------------------
	A Tmote Sky sensor node with Rime Address 3.0 Firmware!
	Placed in Living Room and accessible by the user:
	Output	--> SERIAL MONITOR
	Input	--> Number of CONSECUTIVE ( < 4sec) BUTTON PRESS
	------------------------------------------------------------------
	COMMANDS:
		1) ACTIVATE/DEACTIVATE ALARM.
		2) LOCK/UNLOCK GATE.
		3) OPEN DOOR and GATE.
		4) GET AVERAGE TEMPERATURE by Node1.
		5) GET EXTERNAL LIGHT by Node2.

	EXTENSIONS:
		1.a) ACK Mechanism when BROACASTING the ALARM SWITCH:
				When ACTIVATING the ALARM if Node1 && Node2 Acks
				GREEN LED is TURNED ON otherwise RED LED is TURNED ON.
				When RED LED is ON could be an ERROR on ALARM ACTIVATION!
		3.a) ALARM INPUT BLOCK Mechanism:
				When WAITING GATE and DOOR OPENING-CLOSING is not allowed
				to give ACTIVATE ALARM command.
				BLINKING the BLUE LED every 2s for 16s! 
------------------------------------------------------------------------*/
#include "contiki.h"
#include "stdio.h"
#include "dev/button-sensor.h"
#include "sys/etimer.h"
#include "net/rime/rime.h"
#include "string.h"
#include "leds.h"

//status values
#define	ACTIVE 					1
#define	NOT_ACTIVE				0
#define LOCKED					1
#define UNLOCKED				0
#define AVAILABLE_COMMANDS		5
#define INPUT_INTERVAL			4
#define ALARM_ACK_INTERVAL		5
#define OPEN_CLOSE_INTERVAL		2
#define OPEN_CLOSE_DURATION		16

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
#define GET_TEMP				"GET_TEMP"
#define GET_TEMP_SIZE			9
#define GET_LIGHT				"GET_LIGHT"
#define GET_LIGHT_SIZE			10
#define RECEIVED				1
#define NOT_RECEIVED			0
#define NODE1_RIME_ADDR			1
#define NODE2_RIME_ADDR			2

//status variables
static int alarm_status = NOT_ACTIVE;
static int gate_status = LOCKED;
static int opening_status = NOT_ACTIVE;
static int command = 0;

static int alarm_ACK_Node1 = NOT_RECEIVED;
static int alarm_ACK_Node2 = NOT_RECEIVED;

static process_event_t handle_command_event;

//communication variables
static struct runicast_conn runicast;
static struct broadcast_conn broadcast;

/*----------------------------------------------------------------------*/

/*Reading the command by pressing many times the button*/
PROCESS(input_reader_process, "User-Input Reader Process");

/*Processing the received command*/
PROCESS(command_handler_process, "Command Handler Process");

/*Waiting for the runicast alarm acks & notifying acks not received!*/
PROCESS(wait_alarm_ack_process, "Wait Alarm Ack Process");

/*Waiting for gate and door opening & closing*/
PROCESS(wait_opening_process, "Wait Gate and Door Opening-Closing Process");

AUTOSTART_PROCESSES(&input_reader_process, &command_handler_process);


/*----------------------------------RIME--------------------------------*/

//RUNICAST

static void recv_runicast(struct runicast_conn *c, const linkaddr_t *from, uint8_t seqno){

	char* rcvd_msg = (char *)packetbuf_dataptr();

	if(strcmp(rcvd_msg, ALARM_ACK) == 0){
	/*Receiving Alarm Ack*/

		if(from->u8[0] == NODE1_RIME_ADDR)

			alarm_ACK_Node1 = RECEIVED;

		else if(from->u8[0] == NODE2_RIME_ADDR)

			alarm_ACK_Node2 = RECEIVED;

//printf("UC [%u.%u]: received ALARM ACK from [%d:%d]!\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], from->u8[0], from->u8[1]);
	
	}else if(from->u8[0] == NODE1_RIME_ADDR){
	/*Receiving Temperature Average Reply*/

		printf("Temperature Average: %d\n", *(int*)packetbuf_dataptr());

	}else if(from->u8[0] == NODE2_RIME_ADDR){
	/*Receiving External Light Reply*/

		printf("External Light: %d\n", *(int*)packetbuf_dataptr());
	}
}


static void sent_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){

}


static void timedout_runicast(struct runicast_conn *c, const linkaddr_t *to, uint8_t retransmissions){

//printf("Timed out sending to %d.%d, retransmit %d\n",to->u8[0], to->u8[1], retransmissions);
}


static const struct runicast_callbacks runicast_calls = {recv_runicast, sent_runicast, timedout_runicast};


//BROADCAST

static void broadcast_recv(struct broadcast_conn *c, const linkaddr_t *from){

}


static void broadcast_sent(struct broadcast_conn *c, int status, int num_tx){

}


static const struct broadcast_callbacks broadcast_call = {broadcast_recv, broadcast_sent};


/*---------------------------UTILITY FUNCTIONS--------------------------*/

void print_avail_commands(){

	printf("#########################\n###COMMANDS AVAILABLE:###\n");

	if(opening_status == NOT_ACTIVE)
		printf("\t1) %s ALARM\n", (alarm_status == ACTIVE)? "DEACTIVATE" : "ACTIVATE");

	if(alarm_status == NOT_ACTIVE){
		
		printf("\t2) %s GATE\n", (gate_status == LOCKED)? "UNLOCK" : "LOCK");
		printf("\t3) OPEN GATE & DOOR\n\t4) GET AVG. TEMP\n\t5) GET EXT. LIGHT\n");
	}
	printf("#########################\n");
}


void send_string(char* msg, int size, int rime_addr){

		linkaddr_t addr;
		packetbuf_copyfrom(msg, size);
	    addr.u8[0] = rime_addr;
	    addr.u8[1] = 0;
	    runicast_send(&runicast, &addr, MAX_RETRANSMISSIONS);

//printf("UC [%u.%u]: sending %s command!\n", linkaddr_node_addr.u8[0], linkaddr_node_addr.u8[1], msg);

}

/*---------------------------HANDLER FUNCTIONS--------------------------*/

/*Broadcasting to Node1 and Node2 ALARM command & starting the WAIT ALARM ACK PROCESS*/
void handle_alarm_command(){

	if(alarm_status == ACTIVE){

		packetbuf_copyfrom(ALARM_OFF, ALARM_OFF_SIZE);
		alarm_status = NOT_ACTIVE;
		/*Resetting Alarm ACKs Leds*/
		leds_off(LEDS_RED);
		leds_off(LEDS_GREEN);

	}else if(alarm_status == NOT_ACTIVE){
			
			packetbuf_copyfrom(ALARM_ON, ALARM_ON_SIZE);
			alarm_status = ACTIVE;
	}

	broadcast_send(&broadcast);

	alarm_ACK_Node1 = NOT_RECEIVED;
	alarm_ACK_Node2 = NOT_RECEIVED;

	process_start(&wait_alarm_ack_process, NULL);
}

/*Sending the Lock/Unlock Gate Request*/
void handle_gate_locking_command(){

	if(gate_status == UNLOCKED){

		gate_status = LOCKED;
		send_string(LOCK_GATE, LOCK_GATE_SIZE, NODE2_RIME_ADDR);

	}else if(gate_status == LOCKED){

		gate_status = UNLOCKED;
		send_string(UNLOCK_GATE, UNLOCK_GATE_SIZE, NODE2_RIME_ADDR);
	}

}

/*Brodcasting Open Gate & Door Request to Node1 and Nod2*/
void handle_gate_door_opening_command(){

	if(opening_status == NOT_ACTIVE){

		printf("OPENING GATE and DOOR ...\n");

		packetbuf_copyfrom(OPEN_GATE_DOOR, OPEN_GATE_DOOR_SIZE);
		broadcast_send(&broadcast);

		opening_status = ACTIVE;

		process_start(&wait_opening_process, NULL);
	}
}

/*Sending to Node1 the Get Temperature Request & handling Reply in recv_runicast()*/
void handle_get_temp_command(){

	send_string(GET_TEMP, GET_TEMP_SIZE, NODE1_RIME_ADDR);
}

/*Sending to Node2 the Get Ext. Light Request & handling Reply in recv_runicast()*/
void handle_get_light_command(){

	send_string(GET_LIGHT, GET_LIGHT_SIZE, NODE2_RIME_ADDR);
}

/*######################################################################*/
/*-------------------------INPUT READER PROCESS-------------------------*/

PROCESS_THREAD(input_reader_process, ev, data){

	static struct etimer input_et;
	static int count = 0;

	PROCESS_EXITHANDLER(runicast_close(&runicast));
	PROCESS_EXITHANDLER(broadcast_close(&broadcast));

	PROCESS_BEGIN();

	runicast_open(&runicast, 144, &runicast_calls);
	broadcast_open(&broadcast, 129, &broadcast_call);

	SENSORS_ACTIVATE(button_sensor);

	print_avail_commands();

	while(1){

		PROCESS_WAIT_EVENT();

		if(ev == sensors_event && data == &button_sensor){

			if(count == 0)
				etimer_set(&input_et, INPUT_INTERVAL*CLOCK_SECOND);
			else	
				etimer_restart(&input_et);

			count++;

		}else if(etimer_expired(&input_et) && count != 0){
			/*count != 0 because conflict with alarm_et expiration! ??*/

			if(alarm_status == ACTIVE && count != 1){

				printf("Command not allowed: ALARM IS ACTIVE!\n");

			}else if(opening_status == ACTIVE && count ==1){

				printf("Command not allowed: GATE and DOOR OPEN!\n");
			
			}else{

				if(count > AVAILABLE_COMMANDS || count <= 0)

					printf("Command not valid! (%d)\n", count);

				else{

					printf("Command selected: %d\n", count);

					command = count;

					process_post_synch(&command_handler_process, handle_command_event, NULL);
				}
			}
			print_avail_commands();
			count = 0;
		}
	}

	PROCESS_END();
}


/*----------------------COMMAND HANDLER PROCESS----------------------*/

PROCESS_THREAD(command_handler_process, ev, data){

	PROCESS_BEGIN();

	while(1){

		PROCESS_WAIT_EVENT_UNTIL(ev == handle_command_event);

		switch(command){

			case 1:
				handle_alarm_command();
				break;
			
			case 2:
				handle_gate_locking_command();
				break;
			
			case 3:
				handle_gate_door_opening_command();
				break;
			
			case 4:
				handle_get_temp_command();
				break;
			
			case 5:
				handle_get_light_command();
				break;

			default:
				break;
		}

		command = 0;
	}

	PROCESS_END();
}


/*-----------------------WAIT ALARM ACK PROCESS---------------------------*/

PROCESS_THREAD(wait_alarm_ack_process, ev, data){

	static struct etimer alarm_ack_et;

	PROCESS_BEGIN();

	etimer_set(&alarm_ack_et, ALARM_ACK_INTERVAL*CLOCK_SECOND);
	
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&alarm_ack_et));

	if(alarm_ACK_Node1 == NOT_RECEIVED)
		printf("ALARM ACK from Node1 [%d:0] NOT RECEIVED!\n", NODE1_RIME_ADDR);

	if(alarm_ACK_Node2 == NOT_RECEIVED)
		printf("ALARM ACK from Node2 [%d:0] NOT RECEIVED!\n", NODE2_RIME_ADDR);

	if(alarm_status == ACTIVE){

		if(alarm_ACK_Node1 == NOT_RECEIVED || alarm_ACK_Node2 == NOT_RECEIVED)
			leds_on(LEDS_RED);
		else
			leds_on(LEDS_GREEN);
	}

	PROCESS_END();
}

/*--------------------------WAIT OPENING PROCESS---------------------------*/

PROCESS_THREAD(wait_opening_process, ev, data){

	static struct etimer opening_et;
	static int duration;

	PROCESS_BEGIN();

	etimer_set(&opening_et, OPEN_CLOSE_INTERVAL*CLOCK_SECOND);

	duration = OPEN_CLOSE_DURATION;

	while(duration > 0){
	
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&opening_et));

		leds_toggle(LEDS_BLUE);

		etimer_reset(&opening_et);

		duration -= 2;
	}

	opening_status = NOT_ACTIVE;

	print_avail_commands();

	PROCESS_END();
}
