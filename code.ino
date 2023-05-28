#include <Keypad.h>
#include <HCSR04.h>
#include <Servo.h>
#include <TimerOne.h>
#include <util/atomic.h>

/* Sensors - Inputs */
/* Keypad */
constexpr auto KP_ROW_CNT = 4;
constexpr auto KP_COL_CNT = 4;
constexpr byte KP_ROW_PINS[KP_ROW_CNT] = {A7, A6, A5, A4}; 
constexpr byte KP_COL_PINS[KP_COL_CNT] = {A3, A2, A1, A0}; 
constexpr char KP_CHARMAP[KP_ROW_CNT][KP_COL_CNT] = {
	{'1', '2', '3', 'A'},
	{'4', '5', '6', 'B'},
	{'7', '8', '9', 'C'},
	{'*', '0', '#', 'D'}
};
const auto keypad = Keypad(
	makeKeymap(KP_CHARMAP), 
	KP_ROW_PINS, KP_COL_PINS,
	KP_ROW_CNT, KP_COL_CNT
);

/* Ultrasonic */
constexpr auto HCSR04_READ_DELAY_US = 100000;
constexpr auto HCSR04_TRIG_PIN = 3;
constexpr auto HCSR04_ECHO_PIN = 2;
const auto hc = HCSR04(
	HCSR04_TRIG_PIN, 
	HCSR04_ECHO_PIN
);

/* Actuatrs - Outputs */
/* Servo motor */
constexpr auto SERVO_SIG_PIN = 12;
const auto servo = Servo();


/* State logic */

constexpr int FAR_DISTANCE = 100;
enum class State : byte {
	FAR_AWAY, 
	PROMPT_START,
	PROMPTING,
	PROMPT_FAILED,
	PROMPT_SUCCESS,
	DOOR_OPENED,
};

State state = State::FAR_AWAY;
String input = "";
const String true_password = "1234";

void setup() {
	Serial.begin(9600);
	
	// setup HCSR04 updates as an interrupt, to not run it too fast
	Timer1.initialize(HCSR04_READ_DELAY_US);
	Timer1.attachInterrupt(HCSR04_update_distance);
  	Timer1.start();

	// init the servo
	servo.attach(SERVO_SIG_PIN);
	servo.write(0);
}


void loop() {
	const float d = dist();
	
	if (state != State::FAR_AWAY && d >= FAR_DISTANCE) handle_user_leaving();
	else if (state == State::FAR_AWAY) handle_far_away(d);
	else if (state == State::PROMPT_START) handle_prompt_start();
	else if (state == State::PROMPTING) handle_prompting();
	else if (state == State::PROMPT_FAILED) handle_prompt_failed();
	else if (state == State::PROMPT_SUCCESS) handle_prompt_success();
	else if (state == State::DOOR_OPENED) handle_door_opened();
}


// Checks if a user is close enough.
// If there is one at 1m distance, go to prompt them.
void handle_far_away(float d) {
	if (d < FAR_DISTANCE) {
		state = State::PROMPT_START;
		Serial.println("Welcome.");
		Serial.println("Input the password when prompted (* to cancel, # to input)");
	}
	delay(200);
}


// Just print password and actually prompt the user
void handle_prompt_start() {
	Serial.print("Password: ");
	input = "";
	state = State::PROMPTING;
	delay(200);
}


// Case when the user should input the password.
// Just read the currently pressed key and append it to the input
// If the key is #, send the input
// If the key is *, cancel the current attempt
void handle_prompting() {
	const char key = keypad.getKey();
	
	if (key == NO_KEY) {
		// Nothing
	}
	else if (key == '#') {
		Serial.println();
		if (input == true_password) state = State::PROMPT_SUCCESS;
		else state = State::PROMPT_FAILED;
	}
	else if (key == '*') {
		Serial.println();
		Serial.println("Cancelled");
		state = State::PROMPT_START;
	}
	else {
		input += key;
		Serial.print(key);
	}
}


// Case when the user inputs the password incorrectly.
// Just prompt it again
void handle_prompt_failed() {
	Serial.println("Wrong password.");
	state = State::PROMPT_START;
	servo.write(0);
	delay(200);
}


// Case when the user inputs the password correctly.
// Just open the door and go to idle
void handle_prompt_success() {
	Serial.println("Correct password. Opening the door. Have a nice day!");
	servo.write(180);
	state = State::DOOR_OPENED;
	delay(200);
}


// literally do nothing until the user leaves
void handle_door_opened() {
	delay(200);
}


// Case when the user gets far away enough from the device. 
// Just closes the door if it's already opened, and just says goodbye.
void handle_user_leaving() {
	if (state == State::DOOR_OPENED) {
		Serial.println("Closing the door.");
		servo.write(0);
	}
	Serial.println();
	Serial.println("Goodbye!");
	state = State::FAR_AWAY;
	delay(200);
}

// Timed function to update the distance with a poll delay separately from the main loop 
volatile float HCSR04_last_read = 1e9 + 7; // volatile as it may be modified by an interrupt
void HCSR04_update_distance() {
	HCSR04_last_read = hc.dist();
}

// Because the varible is modified in an interrupt handler, and floats have a size > byte, atomicity is required on reads
// This basically just stops interrupts
float dist() {
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { // disable the update interrupt while reading
		return HCSR04_last_read;
	}
}