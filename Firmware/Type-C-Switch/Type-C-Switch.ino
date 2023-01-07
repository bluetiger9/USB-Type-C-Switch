/*
 * USB 3.x Type-C Switch
 *
 * Copyright (C) Attila Tőkés
 *
 * Licence: MIT
 *
 */

/* Pin Definitions */
const uint8_t PIN_BUTTON = 0;

const uint8_t PIN_LED_OUT = 3;
const uint8_t PIN_LED_IN1 = 17;
const uint8_t PIN_LED_IN2 = 16;

const uint8_t PIN_VBUS_SNS_OUT = 2;
const uint8_t PIN_VBUS_SNS_IN1 = 8;
const uint8_t PIN_VBUS_SNS_IN2 = 9;

const uint8_t PIN_OE_N_SBU = 4;
const uint8_t PIN_OE_CC_1 = 6;
const uint8_t PIN_OE_CC_2 = 5;

const uint8_t PIN_OE_PWR_1 = 14;
const uint8_t PIN_OE_PWR_2 = 15;

const uint8_t PIN_OE_N_USB3 = 18;
const uint8_t PIN_SEL_USB3 = 19;

const uint8_t PIN_OE_N_USB2 = 22;
const uint8_t PIN_SEL_USB2_SBU = 27;

const uint8_t PIN_USB_N_uC = 24;
const uint8_t PIN_USB_P_uC = 25;

// Power Delivery Trigger circuits
const uint8_t PIN_CC_PD_TRG = 11;
const uint8_t PIN_CC_1_PD_TRG= 8;
const uint8_t PIN_CC_2_PD_TRG = 13;

/* Pin Values */
const int8_t OFF = LOW;
const int8_t ON = HIGH;

const int8_t OFF_N = HIGH;
const int8_t ON_N = LOW;

const int8_t UNCHANGED = -1;

const int8_t SEL_IN1 = LOW;
const int8_t SEL_IN2 = HIGH;

const int8_t SEL_IN1_N = HIGH;
const int8_t SEL_IN2_N = LOW;

/* Voltage Sensing */

/* voltage divider: 47k / 5.6k = 8.39 ~= 67 / 2^3 */
const int V_SNS_MULTIP = 67;
const int V_SNS_SHIFT = 3;

/* threshold voltage: ~ 1.5 V */
const int V_SNS_TH = 4096.0 / 3.3 * 1.5;

/* States */
typedef struct State {
  int8_t oe_n_sbu;
  int8_t oe_cc_1;
  int8_t oe_cc_2;

  int8_t oe_pwr_1;
  int8_t oe_pwr_2;

  int8_t oe_n_usb3;
  int8_t on_n_usb2;

  int8_t sel_usb3;
  int8_t sel_usb2_sbu;

  int8_t led_out;
  int8_t led_in1;
  int8_t led_in2;

  int8_t cc_pd_trg;
  int8_t cc_pd_trg_1;
  int8_t cc_pd_trg_2;

  bool oe_before_select;
};

/* OFF (Idle) */
const State STATE_OFF = {
  oe_n_sbu : OFF_N,
  oe_cc_1 : OFF,
  oe_cc_2 : OFF,
  oe_pwr_1 : OFF,
  oe_pwr_2 : OFF,
  oe_n_usb3 : OFF_N,
  on_n_usb2 : OFF_N,
  sel_usb3 : SEL_IN1_N,
  sel_usb2_sbu : SEL_IN1,
  led_out : ON,
  led_in1 : OFF,
  led_in2 : OFF,
  cc_pd_trg : ON,
  cc_pd_trg_1 : ON,
  cc_pd_trg_2 : ON,
  oe_before_select: true
};

/* IN1 <--> OUT */
const State STATE_ON_IN1 = {
  oe_n_sbu: ON_N,
  oe_cc_1: ON,
  oe_cc_2: OFF,
  oe_pwr_1: ON,
  oe_pwr_2: OFF,
  oe_n_usb3: ON_N,
  on_n_usb2: ON_N,
  sel_usb3: SEL_IN1_N,
  sel_usb2_sbu: SEL_IN1,
  led_out : ON,
  led_in1 : ON,
  led_in2 : OFF,
  cc_pd_trg : OFF,
  cc_pd_trg_1 : OFF,
  cc_pd_trg_2 : ON,
  oe_before_select: false
};

/* IN2 <--> OUT */
const State STATE_ON_IN2 = {
  oe_n_sbu: ON_N,
  oe_cc_1: OFF,
  oe_cc_2: ON,
  oe_pwr_1: OFF,
  oe_pwr_2: ON,
  oe_n_usb3: ON_N,
  on_n_usb2: ON_N,
  sel_usb3: SEL_IN2_N,
  sel_usb2_sbu: SEL_IN2,
  led_out : ON,
  led_in1 : OFF,
  led_in2 : ON,
  cc_pd_trg : OFF,
  cc_pd_trg_1 : ON,
  cc_pd_trg_2 : OFF,
  oe_before_select: false
};

const State *state = &STATE_OFF;

const uint16_t STATE_CHANGE_DELAY = 500;

/* Button (low pass filter) */
bool buttonReleased = true;
const uint16_t BUTTON_CHECK_DELAY = 100;

/* Auto switch ignore / delay */
const uint16_t AUTO_SWITCH_DELAY = 5000;
int64_t autoSwitchNotBefore = -1;

/* Input / output */

void pinModeOut(int pin, int initialValue) {
  digitalWrite(pin, initialValue);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, initialValue);
}

bool detectVoltage(int snsPin) {
  int volt = (analogRead(snsPin) * V_SNS_MULTIP) >> V_SNS_SHIFT;
  return volt > V_SNS_TH;
}

void initPins() {
  pinMode(PIN_BUTTON, INPUT_PULLDOWN);
  pinMode(PIN_VBUS_SNS_OUT, INPUT);
  pinMode(PIN_VBUS_SNS_IN1, INPUT);
  pinMode(PIN_VBUS_SNS_IN2, INPUT);

  // disable internal USB port
  pinMode(PIN_USB_N_uC, INPUT);
  pinMode(PIN_USB_P_uC, INPUT);

  pinModeOut(PIN_LED_OUT, STATE_OFF.led_out);
  pinModeOut(PIN_LED_IN1, STATE_OFF.led_in1);
  pinModeOut(PIN_LED_IN2, STATE_OFF.led_in2);
  pinModeOut(PIN_OE_N_SBU, STATE_OFF.oe_n_sbu);
  pinModeOut(PIN_OE_CC_1, STATE_OFF.oe_cc_1);
  pinModeOut(PIN_OE_CC_2, STATE_OFF.oe_cc_2);
  pinModeOut(PIN_OE_PWR_1, STATE_OFF.oe_pwr_1);
  pinModeOut(PIN_OE_PWR_2, STATE_OFF.oe_pwr_2);
  pinModeOut(PIN_OE_N_USB3, STATE_OFF.oe_n_usb3);
  pinModeOut(PIN_SEL_USB3, STATE_OFF.sel_usb3);
  pinModeOut(PIN_OE_N_USB2, STATE_OFF.on_n_usb2);
  pinModeOut(PIN_SEL_USB2_SBU, STATE_OFF.sel_usb2_sbu);
  pinModeOut(PIN_CC_PD_TRG, STATE_OFF.cc_pd_trg);
  pinModeOut(PIN_CC_1_PD_TRG, STATE_OFF.cc_pd_trg_1);
  pinModeOut(PIN_CC_2_PD_TRG, STATE_OFF.cc_pd_trg_2);
}

/* State handling */

void changeStateOE(State state) {
  digitalWrite(PIN_OE_N_SBU, state.oe_n_sbu);
  digitalWrite(PIN_OE_CC_1, state.oe_cc_1);
  digitalWrite(PIN_OE_CC_2, state.oe_cc_2);
  digitalWrite(PIN_OE_PWR_1, state.oe_pwr_1);
  digitalWrite(PIN_OE_PWR_2, state.oe_pwr_2);
  digitalWrite(PIN_OE_N_USB3, state.oe_n_usb3);
  digitalWrite(PIN_OE_N_USB2, state.on_n_usb2);
  digitalWrite(PIN_CC_PD_TRG, state.cc_pd_trg);
  digitalWrite(PIN_CC_1_PD_TRG, state.cc_pd_trg_1);
  digitalWrite(PIN_CC_2_PD_TRG, state.cc_pd_trg_2);
}

void changeStateSEL(State state) {
  digitalWrite(PIN_SEL_USB3, state.sel_usb3);
  digitalWrite(PIN_SEL_USB2_SBU, state.sel_usb2_sbu);
  digitalWrite(PIN_LED_OUT, state.led_out);
  digitalWrite(PIN_LED_IN1, state.led_in1);
  digitalWrite(PIN_LED_IN2, state.led_in2);
}

void changeState(const State *newState) {
  state = newState;
  if (state->oe_before_select) {
    changeStateOE(*newState);
    changeStateSEL(*newState);

  } else {
    changeStateOE(*newState);
    changeStateSEL(*newState);
  }
}

void changeStateSafely(const State *newState) {
  changeState(&STATE_OFF);

  autoSwitchNotBefore = millis() + AUTO_SWITCH_DELAY;

  if (newState != &STATE_OFF) {
    delay(STATE_CHANGE_DELAY);
    changeState(newState);
  }
}

const State* nextState() {
  if (state == &STATE_OFF) {
    return &STATE_ON_IN1;

  } if (state == &STATE_ON_IN1) {
    return &STATE_ON_IN2;

  } if (state == &STATE_ON_IN2) {
    return &STATE_OFF;

  } else {
    return &STATE_OFF;
  }
}

/* Setup */

void setup() {
  analogReadResolution(12);

  initPins();
}


/* Loop */
void loop() {
  auto now = millis();

  // Idle and new device is pluged into IN1?
  if (state != &STATE_ON_IN1 && detectVoltage(PIN_VBUS_SNS_IN1) && !detectVoltage(PIN_VBUS_SNS_IN2) && now >= autoSwitchNotBefore) {
    changeStateSafely(&STATE_ON_IN1);
    return;
  }

  // Idle and new device is pluged into IN2?
  if (state != &STATE_ON_IN2 && detectVoltage(PIN_VBUS_SNS_IN2) && !detectVoltage(PIN_VBUS_SNS_IN1) && now >= autoSwitchNotBefore) {
    changeStateSafely(&STATE_ON_IN2);
    return;
  }

  // Was the Button pressed?
  auto button = digitalRead(PIN_BUTTON);
  if (button == HIGH && buttonReleased) {
    delay(BUTTON_CHECK_DELAY);
    if (digitalRead(PIN_BUTTON) == HIGH) {
      changeStateSafely(nextState());
      buttonReleased = false;
    }

  } else if (button == LOW) {
    buttonReleased = true;
  }
}
