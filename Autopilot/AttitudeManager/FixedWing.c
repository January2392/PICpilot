/**
 * @file   FixedWing.c
 * @author Chris Hajduk
 * @date July 2, 2015
 * @copyright Waterloo Aerial Robotics Group 2017 \n
 *   https://raw.githubusercontent.com/UWARG/PICpilot/master/LICENCE
 */

#include "PWM.h"
#include "AttitudeManager.h"
#include "delay.h"
#include "FixedWing.h"
#include "ProgramStatus.h"

#if VEHICLE_TYPE == FIXED_WING

static int outputSignal[NUM_CHANNELS];
static int control_Roll, control_Pitch, control_Yaw, control_Throttle;

extern int input_RC_Flap;

void initialization(){
    setPWM(THROTTLE_OUT_CHANNEL, MIN_PWM);

    int channel;
    for (channel = 0; channel < NUM_CHANNELS; channel++) {
        outputSignal[channel] = 0;
    }
    setProgramStatus(UNARMED);

//    char str[20];
//    sprintf(str,"AM:%d, PM:%d",sizeof(AMData), sizeof(PMData));
//    debug(str);
    while (getProgramStatus() == UNARMED){
        StateMachine(STATEMACHINE_IDLE);
    }
}

void armVehicle(int delayTime){
    setProgramStatus(ARMING);
    asm("CLRWDT");
    Delay(delayTime);
    asm("CLRWDT");
    setPWM(THROTTLE_OUT_CHANNEL, MIN_PWM);
    setPWM(ROLL_OUT_CHANNEL, 0);
    setPWM(L_TAIL_OUT_CHANNEL, 0);
    setPWM(R_TAIL_OUT_CHANNEL, 0);
    setPWM(FLAP_OUT_CHANNEL, MIN_PWM);
    asm("CLRWDT");
    Delay(delayTime);
    asm("CLRWDT");
}

void dearmVehicle(){
    int i;
    for (i = 1; i <= NUM_CHANNELS; i++){
        setPWM(i, MIN_PWM);
    }
    setProgramStatus(UNARMED);

    while (getProgramStatus() == UNARMED){
        StateMachine(STATEMACHINE_IDLE);
    }
    setProgramStatus(MAIN_EXECUTION);
}

void inputMixing(int* channelIn, int* rollRate, int* pitchRate, int* throttle, int* yawRate){
    if (getControlValue(THROTTLE_CONTROL_SOURCE) == RC_SOURCE) {
        *throttle = channelIn[THROTTLE_IN_CHANNEL - 1];
    }
    if (getControlValue(ROLL_CONTROL_SOURCE) == RC_SOURCE){
        *rollRate = -channelIn[ROLL_IN_CHANNEL - 1];
    }
    if (getControlValue(PITCH_CONTROL_SOURCE) == RC_SOURCE){
        *pitchRate = -channelIn[PITCH_IN_CHANNEL - 1];
    }
    *yawRate = -channelIn[YAW_IN_CHANNEL - 1];

    if (getControlValue(FLAP_CONTROL_SOURCE) == RC_SOURCE) {
        input_RC_Flap = channelIn[FLAP_IN_CHANNEL - 1];
    }

}
/*
 * Reference frames:
 * Uses NED frame (XYZ: North, East, Down)
 * In the plane's space this translates to:
 * +X = Forward
 * +Y = Right
 * +Z = Down
 * Angles are clockwise by those axes. Therefore:
 * +Roll = Right
 * +Pitch = Up
 * +Yaw = Right
 * These
 */

void outputMixing(int* channelOut, int* control_Roll, int* control_Pitch, int* control_Throttle, int* control_Yaw){
    //code for different tail configurations
    #if TAIL_TYPE == STANDARD_TAIL  //is a normal t-tail
    channelOut[PITCH_OUT_CHANNEL] = (*control_Pitch);
    channelOut[YAW_OUT_CHANNEL] = (*control_Yaw);

    #elif TAIL_TYPE == V_TAIL    //V-tail
    // TODO

    #elif TAIL_TYPE == INV_V_TAIL   //Inverse V-Tail
    channelOut[L_TAIL_OUT_CHANNEL - 1] =  (*control_Yaw) * RUDDER_PROPORTION - (*control_Pitch) * ELEVATOR_PROPORTION ; //Tail Output Left
    channelOut[R_TAIL_OUT_CHANNEL - 1] =  (*control_Yaw) * RUDDER_PROPORTION + (*control_Pitch) * ELEVATOR_PROPORTION ; //Tail Output Right


    #endif
    channelOut[ROLL_OUT_CHANNEL - 1] = (*control_Roll);
    channelOut[THROTTLE_OUT_CHANNEL - 1] = (*control_Throttle);
}

void checkLimits(int* channelOut){
    //Throttle = 1
    if (channelOut[THROTTLE_OUT_CHANNEL - 1] > MAX_PWM) {
        channelOut[THROTTLE_OUT_CHANNEL - 1] = MAX_PWM;
    } else if (channelOut[THROTTLE_OUT_CHANNEL - 1] < MIN_PWM){
        channelOut[THROTTLE_OUT_CHANNEL - 1] = MIN_PWM;
    }

    if (channelOut[ROLL_OUT_CHANNEL - 1] > MAX_ROLL_PWM) {
        channelOut[ROLL_OUT_CHANNEL - 1] = MAX_ROLL_PWM;
    } else if (channelOut[ROLL_OUT_CHANNEL - 1] < MIN_ROLL_PWM) {
        channelOut[ROLL_OUT_CHANNEL - 1] = MIN_ROLL_PWM;
    }

    if (channelOut[L_TAIL_OUT_CHANNEL - 1] > MAX_L_TAIL_PWM) {
        channelOut[L_TAIL_OUT_CHANNEL - 1] = MAX_L_TAIL_PWM;
    } else if (channelOut[L_TAIL_OUT_CHANNEL - 1] < MIN_L_TAIL_PWM) {
        channelOut[L_TAIL_OUT_CHANNEL - 1] = MIN_L_TAIL_PWM;
    }

    if (channelOut[R_TAIL_OUT_CHANNEL - 1] > MAX_R_TAIL_PWM) {
        channelOut[R_TAIL_OUT_CHANNEL - 1] = MAX_R_TAIL_PWM;
    } else if (channelOut[R_TAIL_OUT_CHANNEL - 1] < MIN_R_TAIL_PWM) {
        channelOut[R_TAIL_OUT_CHANNEL - 1] = MIN_R_TAIL_PWM;
    }

    //Flaps
    if (channelOut[FLAP_OUT_CHANNEL - 1] > MAX_PWM){
        channelOut[FLAP_OUT_CHANNEL - 1] = MAX_PWM;
    }
}

void highLevelControl(){
    /*
    uint8_t rollControlType = getControlValue(ROLL_CONTROL_TYPE);
    uint8_t rollControlSource = getControlValue(ROLL_CONTROL_SOURCE);
    if (rollControlType == ANGLE_CONTROL) {
        setRollAngleSetpoint(getRollAngleInput(rollControlSource));
        setRollRateSetpoint(PIDcontrol(getPID(ROLL_ANGLE), getRollAngleSetpoint() - getRoll(), MAX_ROLL_RATE / MAX_ROLL_ANGLE));
    }
    else if (rollControlType == RATE_CONTROL) {
        setRollRateSetpoint(getRollRateInput(rollControlSource));
    }

    uint8_t pitchControlType = getControlValue(PITCH_CONTROL_TYPE);
    uint8_t pitchControlSource = getControlValue(PITCH_CONTROL_SOURCE);
    if (pitchControlType == ANGLE_CONTROL) {
        setPitchAngleSetpoint(getPitchAngleInput(pitchControlSource));
        setPitchRateSetpoint(PIDcontrol(getPID(PITCH_ANGLE), getPitchAngleSetpoint() - getPitch(), MAX_PITCH_RATE / MAX_PITCH_ANGLE));
    }
    else if (pitchControlType == RATE_CONTROL) {
        setPitchRateSetpoint(getPitchRateInput(pitchControlSource));
    }

    if (getControlValue(HEADING_CONTROL) == CONTROL_ON) { // if heading control is enabled
        setHeadingSetpoint(getHeadingInput(getControlValue(HEADING_CONTROL_SOURCE))); // get heading value (GS or AP)
        setYawRateSetpoint(PIDcontrol(getPID(HEADING), wrap_180(getHeadingSetpoint() - getHeading()), MAX_YAW_RATE / 180));
    }
    else {
        setYawRateSetpoint(getYawRateInput(RC_SOURCE));
    }

    if (getControlValue(ALTITUDE_CONTROL) == CONTROL_ON) { // if altitude control is enabled
        setAltitudeSetpoint(getAltitudeInput(getControlValue(ALTITUDE_CONTROL_SOURCE))); // get altitude value (GS or AP)
        setThrottleSetpoint(PIDcontrol(getPID(ALTITUDE), getAltitudeSetpoint() - getAltitude(), 1) + getThrottleSetpoint());
    }
    else { // if no altitude control, get raw throttle input (RC or GS)
        setThrottleSetpoint(getThrottleInput(getControlValue(THROTTLE_CONTROL_SOURCE)) + getThrottleSetpoint());
    }

    */

    if (getControlValue(ALTITUDE_CONTROL) == CONTROL_ON) {
        setAltitudeSetpoint(getAltitudeInput(getControlValue(ALTITUDE_CONTROL_SOURCE)));
        setPitchAngleSetpoint(PIDcontrol(getPID(ALTITUDE), getAltitudeSetpoint() - getAltitude(), 1));
        setThrottleSetpoint(PIDcontrol(getPID(ALTITUDE), getAltitudeSetpoint() - getAltitude(), HALF_PWM_RANGE / 2) + getThrottleSetpoint());
    } else {
        setPitchAngleSetpoint(getPitchAngleInput(getControlValue(PITCH_CONTROL_SOURCE)));
        setThrottleSetpoint(getThrottleInput(getControlValue(THROTTLE_CONTROL_SOURCE)));
    }

    if (getControlValue(HEADING_CONTROL) == CONTROL_ON) {
        setHeadingSetpoint(getHeadingInput(getControlValue(HEADING_CONTROL_SOURCE)));
        setRollAngleSetpoint(PIDcontrol(getPID(HEADING), wrap_180(getHeadingSetpoint() - getHeading()), 1));
    } else {
        setRollAngleSetpoint(getRollAngleInput(getControlValue(ROLL_CONTROL_SOURCE)));
    }
}

void lowLevelControl(){
    if (getControlValue(ROLL_CONTROL_TYPE) == ANGLE_CONTROL || getControlValue(HEADING_CONTROL) == CONTROL_ON) {
        setRollRateSetpoint(PIDcontrol(getPID(ROLL_ANGLE), getRollAngleSetpoint() - getRoll(), MAX_ROLL_RATE / MAX_ROLL_ANGLE));
    } else {
        setRollRateSetpoint(getRollRateInput(getControlValue(ROLL_CONTROL_SOURCE)));
    }

    if (getControlValue(PITCH_CONTROL_TYPE) == ANGLE_CONTROL || getControlValue(ALTITUDE_CONTROL) == CONTROL_ON){
        setPitchRateSetpoint(PIDcontrol(getPID(PITCH_ANGLE), getPitchAngleSetpoint() - getPitch(), MAX_PITCH_RATE / MAX_PITCH_ANGLE));
    } else {
        setPitchRateSetpoint(getPitchRateInput(getControlValue(PITCH_CONTROL_SOURCE)));
    }
    setPitchRateSetpoint(coordinatedTurn(getPitchRateSetpoint(), getRoll())); //Apply Coordinated Turn

    setYawRateSetpoint(getYawRateInput(RC_SOURCE));

    control_Roll = PIDcontrol(getPID(ROLL_RATE), getRollRateSetpoint() - getRollRate(), HALF_PWM_RANGE / MAX_ROLL_RATE);
    control_Pitch = PIDcontrol(getPID(PITCH_RATE), getPitchRateSetpoint() - getPitchRate(), HALF_PWM_RANGE / MAX_PITCH_RATE);
    control_Yaw = PIDcontrol(getPID(YAW_RATE), getYawRateSetpoint() - getYawRate(), HALF_PWM_RANGE / MAX_YAW_RATE);
    control_Throttle = getThrottleSetpoint();

    outputSignal[FLAP_OUT_CHANNEL - 1] = getFlapInput(getControlValue(FLAP_CONTROL_SOURCE)); // don't need to mix the flaps

    //Mixing!
    outputMixing(outputSignal, &control_Roll, &control_Pitch, &control_Throttle, &control_Yaw);

    //Error Checking
    checkLimits(outputSignal);
    //Then Output

    if (getProgramStatus() != KILL_MODE) {
        setAllPWM(outputSignal);
    } else{
        setPWM(THROTTLE_OUT_CHANNEL, MIN_PWM);  //Throttle
        setPWM(ROLL_OUT_CHANNEL, MIN_PWM);      //Roll
        setPWM(L_TAIL_OUT_CHANNEL, MIN_PWM);    //Pitch
        setPWM(R_TAIL_OUT_CHANNEL, MIN_PWM);    //Yaw
    }

    //Check for kill mode
#if COMP_MODE
    checkGPS();
    checkHeartbeat();
    checkUHFStatus();
#endif

}

#endif
