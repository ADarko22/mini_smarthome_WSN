# mini_smarthome_WSN
This is my implementation of the NES 2016-2017 Computer Engineering course from Univerisity of Pisa.

W.r.t. project specification I've added:

1.a) ACK Mechanism when BROACASTING the ALARM SWITCH:

      When ACTIVATING the ALARM if Node1 && Node2 Acks
      
      GREEN LED is TURNED ON otherwise RED LED is TURNED ON.
      
      When RED LED is ON could be an ERROR on ALARM ACTIVATION!
      
3.a) ALARM INPUT BLOCK Mechanism:

      When WAITING GATE and DOOR OPENING-CLOSING is not allowed to give ACTIVATE ALARM command.
      
      BLINKING the BLUE LED every 2s for 16s! 
      
6) ACTIVATE/DEACTIVATE COMFORT BEDROOM

      When ACTIVE GREED LED is ON otherwise OFF:
      
            Sense the temperature every 300s and 
            
            if temp < 19 & avg_temp (of previous 5 sensing) <= 15 START AIR-CONDITIONER:
                  start BLUE LED BLINKING every 2s
                  
            if temp > 19 && avg_temp >= 23 STOP AIR-CONDITIONER:
                  turn off BLUE LED
                  
