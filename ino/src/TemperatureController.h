#ifndef DEBUG
#define DEBUG 0
#endif
#if DEBUG
#define DEBUG_MSG(...) Serial.println(__VA_ARGS__)
#endif
#ifndef DEBUG_MSG
#define DEBUG_MSG(...)
#endif

typedef struct _command{
	char command;
	uint8_t ForceCil;
	uint8_t ForceCal;
	uint8_t Hour2;
	uint8_t Hour1;
	uint8_t BombCil;
	uint8_t BombCal;
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t sec;
	uint8_t threashold;
	uint8_t aHour1_start;
	uint8_t aMin1_start;
	uint8_t aHour1_end;
	uint8_t aMin1_end;
	uint8_t aHour2_start;
	uint8_t aMin2_start;
	uint8_t aHour2_end;
	uint8_t aMin2_end;
	float TempCal;
	float TempCil;
	float TempAmb;
	float TempMin;
}Command;

static int Comm_size = sizeof(Command);
void init_state();
void send_state();
void processMessage(Command msg);
void printAddress(DeviceAddress deviceAddress);
void printResolution(DeviceAddress deviceAddress);
void print_vars(Command vars, char * name);
bool getTime(const char *str);
bool getDate(const char *str);
