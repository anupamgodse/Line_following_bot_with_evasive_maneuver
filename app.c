/**
 * This program was created by Anupam Godse and Brayden McDonald for CSC 714
 * It is based on the linetrace code that was provided, but does not use the same algorithm to follow the line
 */

#include "ev3api.h"
#include "app.h"

#define DEBUG
#define abs(x) (x<0?-x:x) 

#ifdef DEBUG
#define _debug(x) (x)
#else
#define _debug(x)
#endif

/**
 * Define the connection ports of the sensors and motors.
 * By default, this application uses the following ports:
 * Touch sensor: Port 2
 * Color sensor: Port 3
 * Sonar sensor: Port 4
 * Left motor:   Port B
 * Right motor:  Port C
 */
const int touch_sensor_port = EV3_PORT_2, color_sensor = EV3_PORT_3, left_motor = EV3_PORT_B, right_motor = EV3_PORT_C, ultrasonic_sensor=EV3_PORT_4;//defines the ports used to access the sensors and motors
int reflected_value, previous_turn=RIGHT, threshold, tracker=0, white=20, black, obstacle_distance=100, evasive_maneuver=0, power=100, evasive_phase=NONE, detection_range=8, angular_counts=550, evasive_direction=LEFT, reverse_count=0, distance_threshold=15, fixed_rotation=150, rotation_counts=0, flag=0, move_away_counts=400, move_parallel_counts = 700, is_pressed=0, rover_active=0, reset_flag=0, correction_flag=0, speed=15, error_count=0, get_line = 0;
int i=0;

int rotate_by = 30;



static void button_clicked_handler(intptr_t button) {
    switch(button) { 
        case BACK_BUTTON:
#if !defined(BUILD_MODULE)
            syslog(LOG_NOTICE, "Back button clicked.");
#endif
            break;
    }
}

/*
 * Dummy function that exists to take up unused spaces in the schedule table, since all rows must have the same number of entries
 * It has no effect on the state of the program
 */
int burn(int temp) {
    return 0;
}

/*
 * This function accesses the touch sensor.
 * It updates the value of the global is_pressed variable; 1 if the sensor is currently being pressed, 0 if it is not
 * It also toggles the value of rover_active whenever is_pressed goes from 1 to 0 
 */
int touch_sensor(int temp) {
    bool_t val = ev3_touch_sensor_is_pressed(touch_sensor_port);
    if(val && is_pressed==0) {//if the button is pressed, set the is_pressed variable
        is_pressed = 1;
    }
    else if(!val && is_pressed==1) {//when the button is released, toggle the rover_active variable
        if(rover_active == 1) {
            rover_active = 0;
        }
        else {
            black = ev3_color_sensor_get_reflect(color_sensor); //calibrate for value of black
            rover_active= 1;
        }
        is_pressed = 0;
    }
    return 0;	
}

/*
 * This function accesses the ultrasonic sensor, and returns the distance to the nearest object.
 * It also updates the value of the global obstacle_distance variable, which is used by the motor function
 */
int sonar_sensor(int temp) {
    if(!rover_active) {//do nothing if the rover is not active
        return 0;
    }
    obstacle_distance = ev3_ultrasonic_sensor_get_distance(ultrasonic_sensor);
    return obstacle_distance;
} 


/*
 * This function accesses the light sensor, and returns the reflected value.
 * It also updates the value of the global reflected_value variable, which is used by the motor function
 * As per the assignment description, it also tracks the number of times that the sensor reads a white value i.e. the robot is not over the black line
 */
int light_sensor(int x) {
    if(!rover_active) { //do nothing if the rover is not active
        return 0;
    }
    reflected_value = ev3_color_sensor_get_reflect(color_sensor);
    if(reflected_value>threshold) {//increment error counter if we are not over the line
        error_count++;
        printf("error_count = %d\n", error_count);
    }
    return reflected_value;
}


/**
 * This is the motor function. It contains most of the logic for the robot's navigation
 * It always returns 0
 */
int update_motor(int value){
    if(!rover_active) {//do nothing if the rover is not active
        return 0;
    }
    if(obstacle_distance > 0 && obstacle_distance <= detection_range) { //switch to evasive mode if the sonar sensor identifies within detection_range
        evasive_maneuver = 1;
    }
    /**
     * In normal movement, the robot will continue moving in a straght line if it sees black
     * Once white is found, the robot will attempt to turn in place in the direction of the previous turn
     * If this does not place the sensor back over black, it will turn in the opposite direction
     * once it finds black, it will record the direction of the previous turn
     */
    if(!evasive_maneuver) {//if we are not evading, move as normal
        if(value > threshold || correction_flag == 1){
            if(reset_flag == 0) {
                ev3_motor_reset_counts(left_motor);
                ev3_motor_reset_counts(right_motor);
                reset_flag=1;
                correction_flag=1;
            }
            if((get_line == 1 && value > black+4) || (abs(ev3_motor_get_counts(left_motor)) < rotate_by)) {// if we do not see black, turn
                if(previous_turn == LEFT){
                    ev3_motor_set_power(right_motor, -power/10);
                    ev3_motor_set_power(left_motor, power/10); 
                }else if(previous_turn == RIGHT){
                    ev3_motor_set_power(right_motor, power/10);
                    ev3_motor_set_power(left_motor, -power/10); 
                }
            }
            else {
                reset_flag = 0;
                correction_flag=0;
                if(value>threshold) {
                    previous_turn = -previous_turn;

                    get_line = 1;
                }
                else {
                    get_line = 0;
                    rotate_by=30;
                }
            }
        }
        else{//go straight
            ev3_motor_steer(left_motor, right_motor, speed, 0);
        }
    }
    else {//if we are evading, execute this code
        /**
         * Evaision involves moving in a trapezoid pattern
         * it is broken up into several phases
         * it will always attempt to dodge inward, i.e. towards the center of the circle
         * if it turns the wrong way, it will back up and turn in the opposite direction		 
         */
        if(evasive_phase == NONE) {//reset motor counters at start
            ev3_motor_reset_counts(left_motor);
            ev3_motor_reset_counts(right_motor);
            evasive_phase = TURN_AWAY_FIXED;
        }
        if(evasive_phase == TURN_AWAY_FIXED) {//begin turning away from the obstacle by a set amount
            if(abs(ev3_motor_get_counts(left_motor)) < fixed_rotation) {	
                if(evasive_direction == RIGHT) {
                    ev3_motor_set_power(right_motor, -power/10);
                    ev3_motor_set_power(left_motor, power/10); 
                }
                else {
                    ev3_motor_set_power(right_motor, power/10);
                    ev3_motor_set_power(left_motor, -power/10); 
                }
            }
            else {//when the turn is complete, switch to the next phase
                evasive_phase=MOVE_AWAY;
                ev3_motor_reset_counts(left_motor);
                ev3_motor_reset_counts(right_motor);
            }
        }
        else if(evasive_phase == MOVE_AWAY) {//move away from the path in a straight line
            reverse_count = abs(ev3_motor_get_counts(left_motor));
            if(reverse_count < move_away_counts) {
                if(reflected_value < threshold && reverse_count > 30) {// if we detect black, then we are moving off the mat and must turn back 
                    evasive_phase = REVERSE;
                    ev3_motor_reset_counts(left_motor);
                    ev3_motor_reset_counts(right_motor);
                }
                else {
                    ev3_motor_steer(left_motor, right_motor, speed, 0);
                }
            }
            else {
                if(evasive_phase != REVERSE) {// if we are not reversing when we are done, proceed to the next phase 
                    evasive_phase=TURN_PARALLEL;
                    ev3_motor_reset_counts(left_motor);
                    ev3_motor_reset_counts(right_motor);
                }
            }
        }
        else if(evasive_phase==REVERSE) {// reverse back to the starting point 
            if(abs(ev3_motor_get_counts(left_motor)) < reverse_count) {//move back to original position
                ev3_motor_steer(left_motor, right_motor, -speed, 0);
            }
            else {//when done, turn back to original position
                if(flag==0) {	
                    ev3_motor_reset_counts(left_motor);
                    ev3_motor_reset_counts(right_motor);
                    reverse_count=0;
                    flag=1;
                }
                if(abs(ev3_motor_get_counts(left_motor)) > fixed_rotation) {
                    evasive_direction*=-1;
                    ev3_motor_reset_counts(left_motor);
                    ev3_motor_reset_counts(right_motor);
                    evasive_phase = TURN_AWAY_FIXED;// we will now attempt to dodge in the opposite direction
                    flag=0;
                }
                if(evasive_direction == LEFT) {
                    ev3_motor_set_power(right_motor, -power/10);
                    ev3_motor_set_power(left_motor, power/10); 
                }
                else {
                    ev3_motor_set_power(right_motor, power/10);
                    ev3_motor_set_power(left_motor, -power/10); 
                }		
            }
        }
        else if(evasive_phase==TURN_PARALLEL) {//turn so that we are parallel to the line
            if(abs(ev3_motor_get_counts(left_motor)) < fixed_rotation) {
                if(evasive_direction == LEFT) {
                    ev3_motor_set_power(right_motor, -power/10);
                    ev3_motor_set_power(left_motor, power/10); 
                }
                else {
                    ev3_motor_set_power(right_motor, power/10);
                    ev3_motor_set_power(left_motor, -power/10); 
                }
            }
            else {//when done, prepare to move
                evasive_phase=MOVE_PARALLEL;
                ev3_motor_reset_counts(left_motor);
                ev3_motor_reset_counts(right_motor);
            }
        }
        else if(evasive_phase == MOVE_PARALLEL) {//move parallel to the line
            if(abs(ev3_motor_get_counts(left_motor)) < move_parallel_counts && reflected_value>black+4) {// continue untill we have gone the set distance or until we see black
                ev3_motor_steer(left_motor, right_motor, speed, 0);
            }
            else {
                if(reflected_value < black+4) {//if we see black, we have found the line and can safely skip some steps
                    evasive_phase = GET_LINE;	
                }
                else {//if not, we prepare to move back to the line ahead of the obstacle
                    evasive_phase=TURN_BACK;
                }
                ev3_motor_reset_counts(left_motor);
                ev3_motor_reset_counts(right_motor);
            }
        }
        else if(evasive_phase==TURN_BACK) {//turn back to the line
            if(abs(ev3_motor_get_counts(left_motor)) < fixed_rotation-70) {
                if(evasive_direction == LEFT) {
                    ev3_motor_set_power(right_motor, -power/10);
                    ev3_motor_set_power(left_motor, power/10); 
                }
                else {
                    ev3_motor_set_power(right_motor, power/10);
                    ev3_motor_set_power(left_motor, -power/10); 
                }
            }
            else {
                evasive_phase=MOVE_BACK;
                ev3_motor_reset_counts(left_motor);
                ev3_motor_reset_counts(right_motor);
            }
        }
        else if(evasive_phase==MOVE_BACK){//move back to the line
            if(reflected_value < black+4) {// if we see black, we have found the line and should recalibrate 
                evasive_phase=GET_LINE;
            }
            else {
                ev3_motor_steer(left_motor, right_motor, speed/1.5, 0);
            }
        }
        else if(evasive_phase == GET_LINE) {//extra phase to make acquiring the line more reliable
            if(reflected_value > white-5) {//keep moving until we find the other end of the line
                evasive_phase=CORRECTION;
            }
            else {
                ev3_motor_steer(left_motor, right_motor, speed, 0);
            }

        }
        else if(evasive_phase==CORRECTION) {////last phase of the evasive maneuver, which turns the robot back in the appropriate direction
            if(reflected_value > black+4) {
                if(evasive_direction==RIGHT) {
                    ev3_motor_set_power(right_motor, -power/10);
                    ev3_motor_set_power(left_motor, power/10); 
                }
                else {
                    ev3_motor_set_power(right_motor, power/10);
                    ev3_motor_set_power(left_motor, -power/10); 
                }
            }
            else {//when done, transition back to normal maneuvers
                evasive_phase=NONE;
                evasive_maneuver=0;
            }
        }
    }
    return(0);
}

/**
 * This table represents the cyclic executive schedule for the robot
 * Frame size is 20ms
 * The hyperperiod is 200ms
 * update_motor is always the last function to run, ensuring that it always has data to start with
 */ 
int (*table[10][4])(int) = 	{
    {touch_sensor, light_sensor, sonar_sensor, update_motor}, 
    {touch_sensor, light_sensor, burn, burn}, 
    {touch_sensor, light_sensor, sonar_sensor, burn}, 
    {touch_sensor, light_sensor, burn, burn}, 
    {touch_sensor, light_sensor, sonar_sensor, burn},
    {touch_sensor, light_sensor, update_motor, burn}, 
    {touch_sensor, light_sensor, sonar_sensor, burn},
    {touch_sensor, light_sensor, burn, burn}, 
    {touch_sensor, light_sensor, sonar_sensor, burn},
    {touch_sensor, light_sensor, burn, burn}, 
};

void main_task(intptr_t unused) {
    // Register button handlers
    ev3_button_set_on_clicked(BACK_BUTTON, button_clicked_handler, BACK_BUTTON);

    // Configure motors
    ev3_motor_config(left_motor, LARGE_MOTOR);
    ev3_motor_config(right_motor, LARGE_MOTOR);

    // Configure sensors
    ev3_sensor_config(touch_sensor_port, TOUCH_SENSOR);
    ev3_sensor_config(color_sensor, COLOR_SENSOR);
    ev3_sensor_config(ultrasonic_sensor, ULTRASONIC_SENSOR);
    ev3_motor_reset_counts(left_motor);
    ev3_motor_reset_counts(right_motor);


    /**
     * Normally we would calibrate the white value, but as per the assignment description, we can't do that
     */

    /**
     * Play tones to indicate program is running
     */
    ev3_speaker_play_tone(NOTE_C4, 100);
    tslp_tsk(100);
    ev3_speaker_play_tone(NOTE_C4, 100);


    int i;


    SYSTIM start_time, current_time;
    SYSTIM t1, t2;

    get_tim(&start_time);
    int current_frame = 0;
    threshold = (white+black)/2;

    while(1) {

        for(i=0; i<4; i++) {//iterate over the row in the table, which contains the jobs to be executed in the current frame
            //get_utm(&t1);
            (*table[current_frame][i])(reflected_value);//execute the appropriate function
            //get_utm(&t2);
        }

        current_frame=(current_frame+1)%10;
        get_tim(&current_time);
        tslp_tsk((start_time-current_time)%20);//set an interupt to go off at the start of the next frame


    } 
}
