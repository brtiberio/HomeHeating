#include <OneWire.h>
#include <DallasTemperature.h>
#include <stdlib.h>
#include <Time.h>
#include <DS1307RTC.h>
#include <Wire.h>
#include <LiquidTWI2.h>
#include <avr/pgmspace.h>
#include "TemperatureController.h"

// include the library code:
// Connect via i2c, default address #0 (A0-A2 not jumpered)
LiquidTWI2 lcd(0);
// Data wire is plugged into port 3 on the rboard
#define ONE_WIRE_BUS 3 //2 sensores connected to nRF24L01+_IRQ pin of the rboard
#define TEMPERATURE_PRECISION 10
//#define DEBUG 0
#define BOMBCAL_OFFSET 3
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
// arrays to hold device addresses
DeviceAddress Cilindro = { 0x28, 0x07, 0x37, 0xE0, 0x05, 0x00, 0x00, 0x32 };
DeviceAddress Caldeira = { 0x28, 0x7F, 0x37, 0xE0, 0x05, 0x00, 0x00, 0x0B };
float tempCal;
float tempCil;
boolean CycleDevices = 0;
uint8_t Relay_Cal = 4;
uint8_t Relay_Cil = 5;
boolean enable_offset1 = false, enable_offset2 = false, isHour1 = false, isHour2 = false;

Command com, state;
char buffer(Comm_size);

const char *monthName[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

tmElements_t tm, tm_hour_start, tm_hour_end;
time_t t_hour_start, t_hour_end;

void init_state(){
    state.threashold = 3;
    state.TempMin = 60;
    state.Hour1 = 1;
    state.aHour1_start = 17;
    state.aMin1_start = 30;
    state.aHour1_end = 23;
    state.aMin1_end = 30;
}

void send_state(){
    // char aux[39];
    // memcpy(&aux, &state, Comm_size);
    uint8_t *bytePointer = (uint8_t *)&state;
    Serial.write('S');
    Serial.write(bytePointer, Comm_size);
    Serial.write('E');
    return;

}
void processMessage(Command msg){
    char order = msg.command;
    tmElements_t new_tm;
    time_t time;
#if defined(DEBUG)
    print_vars(msg, "inside process message");
#endif
    switch (order){
    case 'T': //sync clock
        new_tm.Day = msg.day;
        new_tm.Hour = msg.hour;
        new_tm.Minute = msg.minute;
        new_tm.Month = msg.month;
        new_tm.Second = msg.sec;
        new_tm.Year = CalendarYrToTm(msg.year);
        time = makeTime(new_tm);
        RTC.set(time);
        break;
    case 'f':
        state.ForceCal = msg.ForceCal;
        break;
    case 'F':
        state.ForceCil = msg.ForceCil;
        break;
    case 'h':
        if (msg.Hour1){
            state.Hour1 = 1;
            state.aHour1_start = msg.aHour1_start;
            state.aMin1_start = msg.aMin1_start;
            state.aHour1_end = msg.aHour1_end;
            state.aMin1_end = msg.aMin1_end;
        }
        else
        {
            state.Hour1 = 0;
            isHour1 = false;
            enable_offset1 = false;
            if (state.BombCil){
                digitalWrite(Relay_Cil, LOW);
                state.BombCil = 0;
            }
        }
        break;
    case 'H':
        if (msg.Hour2){
            state.Hour2 = 1;
            state.aHour2_start = msg.aHour2_start;
            state.aMin2_start = msg.aMin2_start;
            state.aHour2_end = msg.aHour2_end;
            state.aMin2_end = msg.aMin2_end;
        }
        else
        {
            state.Hour2 = 0;
            isHour2 = false;
            enable_offset2 = false;
            if (state.BombCil){
                digitalWrite(Relay_Cil, LOW);
                state.BombCil = 0;
            }
        }
        break;
    case 't':
        state.TempMin = msg.TempMin; //tmin to turn heater on
        break;
    case 'o':
        state.threashold = msg.threashold; //offset from tmin to turn off heater;
		break;
    case 's':
        send_state();
        break;
    default:
        Serial.println("Unknown command");
    }
    return;
}


// function to print a device address
void printAddress(DeviceAddress deviceAddress)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        // zero pad the address if necessary
        if (deviceAddress[i] < 16) Serial.print("0");
        Serial.print(deviceAddress[i], HEX);
        Serial.print(" ");
    }
}

// function to print a device's resolution
void printResolution(DeviceAddress deviceAddress)
{
    Serial.print("Resolution: ");
    Serial.print(sensors.getResolution(deviceAddress));
    Serial.println();
}


bool getTime(const char *str)
{
    int Hour, Min, Sec;
    if (sscanf(str, "%d:%d:%d", &Hour, &Min, &Sec) != 3) return false;
    tm.Hour = Hour;
    tm.Minute = Min;
    tm.Second = Sec;
    return true;
}

bool getDate(const char *str)
{
    char Month[12];
    int Day, Year;
    uint8_t monthIndex;

	if (sscanf(str, "%s %d %d", Month, &Day, &Year) != 3)
		return false;
    for (monthIndex = 0; monthIndex < 12; monthIndex++) {
        if (strcmp(Month, monthName[monthIndex]) == 0) break;
    }
    if (monthIndex >= 12) return false;
    tm.Day = Day;
    tm.Month = monthIndex + 1;
    tm.Year = CalendarYrToTm(Year);
    return true;
}

uint8_t isRunning(){
    tmElements_t aux;
    if (RTC.read(aux)){
        if (RTC.chipPresent()){
            Serial.println("Clock present and running");
            return 1;
        }
    }
    else{
        if (RTC.chipPresent()){
            Serial.println("Clock present but not running");
            return 0;
        }
    }
    Serial.println("Serious Problem DS1307: No clock found!");
    return 2;
}
#if defined (DEBUG)
void print_vars(Command vars, char * name){
    Serial.println(name);
    Serial.println(vars.command);
    Serial.println(vars.ForceCil);
    Serial.println(vars.ForceCal);
    Serial.println(vars.Hour2);
    Serial.println(vars.Hour1);
    Serial.println(vars.BombCil);
    Serial.println(vars.BombCal);
    Serial.println(vars.year);
    Serial.println(vars.month);
    Serial.println(vars.day);
    Serial.println(vars.hour);
    Serial.println(vars.minute);
    Serial.println(vars.sec);
    Serial.println(vars.threashold);
    Serial.println(vars.aHour1_start);
    Serial.println(vars.aMin1_start);
    Serial.println(vars.aHour1_end);
    Serial.println(vars.aMin1_end);
    Serial.println(vars.aHour2_start);
    Serial.println(vars.aMin2_start);
    Serial.println(vars.aHour2_end);
    Serial.println(vars.aMin2_end);
    Serial.println(vars.TempCal);
    Serial.println(vars.TempCil);
    Serial.println(vars.TempAmb);
    Serial.println(vars.TempMin);
}
#endif

void setup(void)
{
    bool parse = false;
    bool config = false;
    Serial.begin(57600);
    // Start up the library
    sensors.begin();
#if defined (DEBUG)
    // locate devices on the bus
	Serial.println(F("Locating devices on bus 3..."));
	Serial.print(F("Found "));
    Serial.print(sensors.getDeviceCount(), DEC);
	Serial.println(F(" devices."));

    // report parasite power requirements
	Serial.print(F("Parasite power on bus 3 is: "));
    if (sensors.isParasitePowerMode()) Serial.println("ON");
    else Serial.println("OFF");
    // show the addresses we found on the bus
	Serial.print(F("Device 0 Address: "));
    printAddress(Caldeira);
    Serial.println();

	Serial.print(F("Device 1 Address: "));
    printAddress(Cilindro);
    Serial.println();
#endif
    // set the resolution to x bit
    sensors.setResolution(Caldeira, TEMPERATURE_PRECISION);
    sensors.setResolution(Cilindro, TEMPERATURE_PRECISION);

	Serial.print(F("Device 0 Resolution: "));
    Serial.print(sensors.getResolution(Caldeira), DEC);
    Serial.println();

	Serial.print(F("Device 1 Resolution: "));
    Serial.print(sensors.getResolution(Cilindro), DEC);
    Serial.println();


    // set the LCD type
    lcd.setMCPType(LTI_TYPE_MCP23008);
    // set up the LCD's number of rows and columns:
    lcd.begin(16, 2);
    lcd.setBacklight(HIGH);
    lcd.clear();
    Serial.flush();
    switch (isRunning()){
    case 0:
        if (getDate(__DATE__) && getTime(__TIME__)) {
            parse = true;
            // and configure the RTC with this info
            if (RTC.write(tm)) {
                config = true;
            }
        }
        break;
    case 1:
        parse = true;
        config = true;
        break;
    default:
        Serial.println("problems with clock chip");
    }

    memset(&com, 0, Comm_size);
    memset(&state, 0, Comm_size);
    memset(&buffer, 0, Comm_size);
    init_state();
    pinMode(Relay_Cal, OUTPUT);
    pinMode(Relay_Cil, OUTPUT);
    if (config && parse){
        Serial.println("RTC running and configured");
    }
    else
    {
        Serial.print("Parse = "), Serial.println(parse);
        Serial.print("Config = "), Serial.println(config);
    }
}


void loop(void)
{
    int nbytes = 0, curr_hour = 0, curr_minute = 0;
    sensors.requestTemperatures();

    tempCal = sensors.getTempC(Caldeira);
    tempCil = sensors.getTempC(Cilindro);
    state.TempCal = tempCal;
    state.TempCil = tempCil;

#if defined (DEBUG)
    print_vars(state, "inside loop");
#endif
    if (Serial.available() > 0) {
        nbytes = Serial.readBytes(&buffer, Comm_size);
        if (nbytes == Comm_size){
            memset(&com, 0, Comm_size);
            memcpy(&com, &buffer, Comm_size);
            processMessage(com);

        }
        memset(&buffer, 0, Comm_size);
    }
    else {
        /*======================================================================
                Control of forced circulation pump in the boiler
        ======================================================================*/
        if (state.ForceCal == 1) {
            state.BombCal = 1;
            digitalWrite(Relay_Cal, HIGH);
        }
        else{
            if (tempCal > tempCil + BOMBCAL_OFFSET){
                state.BombCal = 1;
                digitalWrite(Relay_Cal, HIGH);
            }
            else{
                if (tempCal <= tempCil){
                    state.BombCal = 0;
                    digitalWrite(Relay_Cal, LOW);
                }
            }
        }
        /*======================================================================
          get current time and  print to LCD
        ======================================================================*/
        time_t now = RTC.get();
        curr_hour = hour(now);
        lcd.clear();
        lcd.setCursor(0, 1);
        if (curr_hour < 10){
            lcd.print("0");
            lcd.print(curr_hour, DEC);
        }
        else{
            lcd.print(curr_hour, DEC);
        }
        lcd.print(":");
        curr_minute = minute(now);
        if (curr_minute < 10){
            lcd.print("0");
            lcd.print(curr_minute, DEC);
        }
        else{
            lcd.print(curr_minute, DEC);
        }
        state.year = year(now);
        state.month = month(now);
        state.day = day(now);
        state.hour = curr_hour;
        state.minute = curr_minute;
        state.sec = second(now);
        /*======================================================================

                        Heat control

        ======================================================================*/
        // Is forced Heat on?
        if (state.ForceCil == 1){
            digitalWrite(Relay_Cil, HIGH);
            state.BombCil = 1;
            isHour1 = false;
            isHour2 = false;
            enable_offset1 = false;
            enable_offset2 = false;
        }
        else{
            breakTime(now, tm);
            // handle first hour1
            switch (state.Hour1 || state.Hour2){
            case 0:
                // none of timers are activated, turn off heating.
                digitalWrite(Relay_Cil, LOW);
                state.BombCil = 0;
                enable_offset1 = false;
                enable_offset2 = false;
                isHour1 = false;
                isHour2 = false;
                break;
            case 1:
                // at least one timer is on!
                // check first timer control
                // is schedule 1 activated and not inside schedule 2?
                if (state.Hour1 && !isHour2){
                    //have already inside hour1 schedule?
                    if (!isHour1){
                        memcpy(&tm_hour_start, &tm, sizeof(tm));
                        memcpy(&tm_hour_end, &tm, sizeof(tm));
                        tm_hour_start.Hour = state.aHour1_start;
                        tm_hour_start.Minute = state.aMin1_start;
                        tm_hour_start.Second = 0;
                        t_hour_start = makeTime(tm_hour_start);

                        tm_hour_end.Hour = state.aHour1_end;
                        tm_hour_end.Minute = state.aMin1_end;
                        tm_hour_end.Second = 0;

                        t_hour_end = makeTime(tm_hour_end);

                        // this checks if end time passes mid night
                        if (state.aHour1_start > state.aHour1_end)
                            t_hour_end = t_hour_end + SECS_PER_DAY;
                    }
                    // is current time inside defined schedule 1?
                    if ((t_hour_start <= now) && (now <= t_hour_end)){
                        isHour1 = true;
                        // is heating already on?
                        switch (enable_offset1){
                        case 0: //is temperature greate then mimnimum?
                            if (tempCil >= state.TempMin){
                                digitalWrite(Relay_Cil, HIGH);
                                state.BombCil = 1;
                                enable_offset1 = true;
                            }
                            break;
                        case 1: // is temperature below minimum - threashold?
                            if (tempCil <= (state.TempMin - state.threashold)){
                                state.BombCil = 0;
                                digitalWrite(Relay_Cil, LOW);
                                enable_offset1 = false;
                            }
                            break;
                        }
                    }
                    else{
                        isHour1 = false;
                    }
                }
                // check second timer control
                // is schedule 2 activated and not inside schedule 1?
                if (state.Hour2 && !isHour1){
                    if (!isHour2){
                        memcpy(&tm_hour_start, &tm, sizeof(tm));
                        memcpy(&tm_hour_end, &tm, sizeof(tm));
                        tm_hour_start.Hour = state.aHour2_start;
                        tm_hour_start.Minute = state.aMin2_start;

                        t_hour_start = makeTime(tm_hour_start);

                        tm_hour_end.Hour = state.aHour2_end;
                        tm_hour_end.Minute = state.aMin2_end;

                        t_hour_end = makeTime(tm_hour_end);
                        // this checks if end time passes mid night
                        if (state.aHour2_start > state.aHour2_end)
                            t_hour_end = t_hour_end + SECS_PER_DAY;
                    }
                    // is current time inside defined schedule 2?
                    if ((t_hour_start <= now) && (now <= t_hour_end)){
                        isHour2 = true;
                        switch (enable_offset2){
                        case 0:
                            if (tempCil >= state.TempMin){
                                digitalWrite(Relay_Cil, HIGH);
                                state.BombCil = 1;
                                enable_offset2 = true;
                            }
                            break;
                        case 1:
                            if (tempCil <= (state.TempMin - state.threashold)){
                                state.BombCil = 0;
                                digitalWrite(Relay_Cil, LOW);
                                enable_offset2 = false;
                            }
                            break;
                        }
                    }
                    else{
                        isHour2 = false;
                    }
                }
                // am I out  of both schedules? Make sure pump is off!
                if ((!isHour1) && (!isHour2)){
                    digitalWrite(Relay_Cil, LOW);
                    state.BombCil = 0;
                    enable_offset1 = false;
                    enable_offset2 = false;
                }
                break;
            }
        }

        /*====================================================================
            Heating control ended
          ====================================================================*/
        send_state();

        switch (CycleDevices){
        case 0:
            lcd.setCursor(0, 0);
            lcd.print("Caldeira ");
            lcd.print(tempCal);
            CycleDevices = !CycleDevices;
            delay(2000);
            break;
        case 1:
            lcd.setCursor(0, 0);
            lcd.print("Cilindro ");
            lcd.print(tempCil);
            CycleDevices = !CycleDevices;
            delay(2000);
            break;
        default:
            lcd.setCursor(0, 0);
            lcd.print("Unexpected value CycleDevices");
        }
    }
}
