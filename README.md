Single Author info:

angodse Anupam N Godse

Group info:

username FirstName MiddleInitial LastName

# Line_following_bot_with_evasive_maneuver
EV3 Mindstorms rover programmed on ev3rt

Description:
We bulit a rover with 2 wheels, 2 motors, 1 color sensor, 1 sonar sensor, and other components supporting the rover framework and developed a program to make rover follow the black line and also can circumvent obstacles in the path and get back on line.

This program was implemented using a cyclic executive schedule of following tasks:

Task 1: light sensor - read the reading of color sensor every 20ms to detect color.
Task 2: motors - Update motor every 100ms for navigation based on sonar sensor reading and color sensor reading.
Task 3: sonar sensor - Get sonar sensor reading every 40 ms to detect the obstacle in path.
Task 4: touch sensor - Get touch sensor reading every 20ms to start/stop the rover if pressed.


Cyclic executive approach:
We have 4 tasks to schedule with their respective periods.

ti = (pi, ei) : ith task having pi period (also deadline) and ei execution time in ms 
Execution period of every task was measured using get_utm() and the execution time was only of the order of hundreds of microseconds, so assuming 1ms execution time of each task is safe.

t1 = (20, 1)
t2 = (100, 1)
t3 = (40, 1)
t4 = (20, 1)

Deciding the minor cycle i.e the frame size f

1st constraint: f >= execution time(ei) for each task  i.e f can be any value greater than equal to 1

2nd constraint: pi (for some i) should be evenly divisible by f i.e floor(pi/f) - pi/f = 0
We have many candidate values of f here like 1, 2, 4, 20, 40 etc.

3rd constraint: 2f - gcd(pi, f) >= Di (deadline)
Minimum value of deadline Di is 20

so 2f - gcd(pi, f) >= 20 (at least) so the values of f =1, 2, 4 (less than 20) can be eliminated

if f = 20, gcd(pi, f) = f for every task i

so 2f - f >= f i.e f >= f is satisfied

So we choose f = 20ms

Hyperperiod(major cycle): LCM of all the periods is 200ms, therefore hyperperiod H = 200ms

Number of frames(minor cycles) in a hyperperiod(major cycle) F = H/f = 200/20 = 10

Below is the schedule built for the 10 minor cyles i.e one hyperperiod which is repeated.

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
          
          
burn is a task which does nothing and just returns, used to fill the gaps in schedule if any.


Description of each task:

task 1: light sensor task
Read color sensor reading every 20ms and store in global variable

task 3: sonar sensor task
Read ultrasonic sensor reading every 20ms and store in a gloabl variable

task 2: update motor task
Update motors every 100ms so that they decide how to navigate based on the reading of color senor and the sonar sensor. 

Navigation strategy used:
When on black line go straight.
If off the line, try to search for line towards left for some degrees (we used 30)
