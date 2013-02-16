/* Base Station Receiver
 The circuit:
 IO Allocations
 Digital
 0 (RX)  
 1 (TX)  
 2    
 3 (PWM) - 1-Wire (DS18x20 Pinout 1 - GND, 2 - DATA, 3 - VDD) - Fitted but not currently used.
 4       - 
 5 (PWM) - 
 6 (PWM) - 
 7       - 
 8       - RFM12 nIRQ  --> This means you need to uncomment line 19 and change line 86 to 8 in RF12.cpp
 9 (PWM) - RFM12 nSEL
 10(PWM) - CS, Ethernet
 11(PWM) - SI, Ethernet, RFM12 SDI
 12      - SO, Ethernet, RFM12 SDO
 13      - SCK, Ethernet, RFM12 SCK
 Analogue
 1 
 2
 3
 4       - Data, DS1307 
 5       - Clock, DS1307 
*/
#define VERSION 310 //Software Version
#define DEBUG 1

//includes
#include <EtherCard.h> //Ethernet Shield Library
#include <JeeLib.h> //Various Librarys need for I2C

//Constants
#define MYNODE 15            	// Node ID
#define FREQ RF12_433MHZ     	// Frequency
#define GROUP 212            	// Network group 
#define EMONTX 5				// Current Monitor TX node ID
#define WXTXIS 4
#define WXTX 18					// Weather Sensor node ID
#define BARO 17					// Barometer node ID
#define AQUA 11					// Aquarium node ID
#define MAINSVOLTAGE 240		// Mains voltage
#define POWERUPDATE 200			// Power Update Period (ms)
#define PERSISTENTVARIABLEUPDATE 30000 // Persistent Variable Update PEriod (ms)
#define TWEETPERIOD 1200000 	// Tweet Period
#define NTPPERIOD 300000 		// NTP Period
#define CO2COST 0.521			// 0.521 kg/kWh from http://www.energysavingtrust.org.uk/Energy-Saving-Trust/Our-calculations

//RTC Setup 
#include <Wire.h>
#include <RTClib.h>
RTC_DS1307 RTC;

//Local Network Setup
static byte mymac[] = { 0x54,0x55,0x58,0x10,0x00,0x31 }; // ethernet mac address
#define STATIC 0 				// set to 1 to disable DHCP (adjust myip/gwip values below)
#define CSPIN 10				// Chip Select Pin Ethernet card
#if STATIC
	// ethernet interface ip address
	static byte myip[] = { 192,168,1,15 };
	// gateway ip address
	static byte gwip[] = { 192,168,1,1 };
	//dns ip
	static byte dnsip[] = { 8,8,8,8};
#endif

//Timeserver setup
#define NUM_TIMESERVERS 5
prog_char ntp0[] PROGMEM = "ntp2d.mcc.ac.uk";
prog_char ntp1[] PROGMEM = "ntp2c.mcc.ac.uk";
prog_char ntp2[] PROGMEM = "ntp.exnet.com";
prog_char ntp3[] PROGMEM = "ntp.cis.strath.ac.uk";
prog_char ntp4[] PROGMEM = "clock01.mnuk01.burstnet.eu";
// Now define another array in PROGMEM for the above strings
prog_char *ntpList[] PROGMEM = { ntp0, ntp1, ntp2, ntp3, ntp4 };


// Packet buffer, must be big enough to packet and payload
#define BUFFER_SIZE 500
byte Ethernet::buffer[BUFFER_SIZE];
uint8_t clientPort = 123;
static BufferFiller bfill;

// Converting time received from NTP servers to a value that can be displayed.
#define GETTIMEOFDAY_TO_NTP_OFFSET 2208988800UL //seconds between 1-Jan-1900 (NTP Start) and 1-Jan-1970 (unix starts)
#define	EPOCH_YR	1970
#define	SECS_DAY	86400UL  //(24L * 60L * 60L)
#define	LEAPYEAR(year)	(!((year) % 4) && (((year) % 100) || !((year) % 400)))
#define	YEARSIZE(year)	(LEAPYEAR(year) ? 366 : 365)


//General Variables
unsigned char currentBuf[7]; 
static uint8_t currentTimeserver = 0;
uint32_t lastTweet = 0;
uint32_t lastNTP = -30000;
uint32_t timeLong;
Stash stash;
uint32_t lastPwrMsg;
float maxTemperature;
float minTemperature;
long lastPowerUpdate = 0;
double todaysKwh = -0.00000000012;				// Total kWh usage since midnight	
long lastPersistentVariableUpdate;

typedef struct { int16_t temperature; int32_t pressure; int battery;} payloadbarotx;
payloadbarotx barotx;

typedef struct { double current; int battery; } PayloadTX;
PayloadTX emontx; 

typedef struct { int light; int humidity; int temperature; int dewpoint; int cloudbase; int battery; } payloadwxtx;
payloadwxtx wxtx;

typedef struct { int temperature; bool light; bool moon;} payloadaquarium;
payloadaquarium aquarium;
// End of config


// Setup Routine
void setup(){
#if DEBUG
  	Serial.begin(9600);
	Serial.println("\n[Base Stn]");
#endif	
	
	//Start RTC
    Wire.begin();
    RTC.begin();
	
	//Ethernet setup
	if (ether.begin(sizeof Ethernet::buffer, mymac, CSPIN) == 0) 
	{
#if DEBUG
		//Serial.println(F("Ethernet init failed"));
#endif
	}
		
#if STATIC
	ether.staticSetup(myip, gwip, dnsip);
#else
	if (!ether.dhcpSetup())
	{
		Serial.println(F("DHCP failed"));
	}
#endif

#if DEBUG
	ether.printIp("IP:", ether.myip);
	//ether.printIp("GW:", ether.gwip);  
	//ether.printIp("DNS:", ether.dnsip);  
#endif
 
	// Reciever setup
	rf12_set_cs(9); 
	rf12_initialize(MYNODE, FREQ, GROUP);
		
	//Used to set min/max temp first time.
	wxtx.dewpoint = barotx.pressure = 9999;
	maxTemperature = -99;
	minTemperature = 99;
	
	//Trigger get stored variable from prior to power cycle.
	readFromRam ();
}

// Main Loop
void loop()
{
	/* Main loop needs to:
		1. Listen for rf readings.
		2. Update COSM after every new sensor update.
		3. Update weatherunderground every 5 minutes and winew sensor update.
		4. Update Twitter every 20 minutes.
		5. Update time from NTP x minutes.
		6. Process returned ethernet data.
		7. Transmit NTP time stamp after successful update.
	*/
	
	//1 & 2. Listen for incomming RF Data, Update COSM
	if (rf12_recvDone())
	{   
		if (rf12_crc == 0 && (rf12_hdr & RF12_HDR_CTL) == 0)
		{
			int node_id = (rf12_hdr & 0x1F);
			if (node_id == EMONTX)     		
			{
				emontx = *(PayloadTX*) rf12_data;                       // get emontx data 
				
				byte sd = stash.create();
		
				stash.print(F("Current,"));
				stash.println(emontx.current);
				stash.print(F("Power,"));
				stash.println(emontx.current * MAINSVOLTAGE);
				stash.print(F("Battery,"));
				stash.println(emontx.battery);
				stash.print(F("Interval,"));
				stash.println(millis()-lastPwrMsg);
				stash.print(F("kWh,"));
				stash.println(todaysKwh);
				stash.print(F("CO2,"));
				stash.println(todaysKwh * CO2COST); 
							
				stash.save();

				sendToCosm(sd, PSTR("92072"));
				
				//Reset kWh at Midnight
				//Get time 
				DateTime now = RTC.now();
				
				if ((now.hour() == 0) & (now.minute() == 0)) todaysKwh = 0;
				
				lastPwrMsg = millis();
			}
			if (node_id == WXTX or node_id == WXTXIS)
			{
				wxtx = *(payloadwxtx*) rf12_data;   
				float h = wxtx.humidity * 0.1;
				float t = wxtx.temperature * 0.1;
		
				byte sd = stash.create();
				
				if (wxtx.light >= 0 & wxtx.light <= 100)
				{
					stash.print(F("AmbientLightLevel,"));
					stash.println(wxtx.light);
				}
				
				if (t >= -50 & t <= 60)
				{
					//Get time 
					DateTime now = RTC.now();

					//Calculate min/max temp
					if ((now.hour() == 0) & (now.minute() == 0)) maxTemperature = minTemperature = t;
					if (t > maxTemperature) maxTemperature = t;
					if (t < minTemperature) minTemperature = t;
					
					stash.print(F("Temperature,"));
					stash.println(t);
					stash.print(F("MinimumTemperature,"));
					stash.println(minTemperature);
					stash.print(F("MaximumTemperature,"));
					stash.println(maxTemperature);
				}
				
				if (h >= 0 & h <= 100)
				{
					stash.print(F("Humidity,"));
					stash.println(h);
					stash.print(F("Dewpoint,"));
					stash.println(wxtx.dewpoint);
					stash.print(F("Cloudbase,"));
					stash.println(wxtx.cloudbase);
				}
				
				if (wxtx.battery >= 0 & wxtx.battery <= 6000)
				{
					stash.print(F("Battery,"));
					stash.println(wxtx.battery);
				}
				
				stash.save();

				sendToCosm(sd, PSTR("20161"));
			}
			if (node_id == BARO)
			{				
				barotx = *(payloadbarotx*) rf12_data;  
				
				byte sd = stash.create(); 
				
				float p = barotx.pressure * 0.01;
					
				stash.print(F("Pressure,"));
				stash.println(p);
				stash.print(F("BaroBattery,"));
				stash.println(barotx.battery);
								
				stash.save();

				sendToCosm(sd, PSTR("20161"));
			}
        }
    } 
		
	//4. Update twitter every 20 mins
	if ((millis() - lastTweet > TWEETPERIOD))
	{
		DateTime now = RTC.now();
		
		char time[5];
		sprintf(time,"%02d:%02d", now.hour(), now.minute());
		
		byte sd = stash.create();
		
		stash.print(F("status= #Hove "));
		stash.print(time);
		stash.print(F(", Temp: "));
		stash.print(wxtx.temperature/10);
		stash.print(F("C, Humidity: "));
		stash.print(int(wxtx.humidity/10));
		stash.print(F("%25,"));
		
		if (wxtx.dewpoint < 9998)
		{
			stash.print(F(" Dewpoint: "));
			stash.print(wxtx.dewpoint);
			stash.print(F("C, Cloudbase: "));
			stash.print(int(wxtx.cloudbase));
			stash.print(F("Ft,"));
		}
				
		if (barotx.pressure/100 < 9998)
		{
			stash.print(F(" Pres: "));
			stash.print(int(barotx.pressure/100));
			stash.print(F("hPa,"));
		}
		
		stash.print(F(" Today's Min/Max: "));
		stash.print(minTemperature);
		stash.print(F("/"));
		stash.print(maxTemperature);
		stash.print(F("C. #Weather"));
		stash.save();
		
		sendToTwitter(sd);	
	
		lastTweet = millis();
	}
	
	//5. Update NTP
	if ((millis() - lastNTP > NTPPERIOD) | (lastNTP == 0))
	{
		if (!ether.dnsLookup( (char*)pgm_read_word(&(ntpList[currentTimeserver])) )) 
		{
			//Serial.println( F("DNS failed" ));
		} 
		else 
		{    
			ether.ntpRequest(ether.hisip, ++clientPort);
		}
		
		if( ++currentTimeserver >= NUM_TIMESERVERS )
			currentTimeserver = 0; 
    		
		lastNTP += 6000;
	}

	//Faster Update Cycle for kWh Power Calc
	if (millis() - lastPowerUpdate > POWERUPDATE) 
	{
		todaysKwh += (emontx.current * MAINSVOLTAGE) * 0.2 / 3600000;
		lastPowerUpdate = millis();
	}
	
	//Store persistent variables
	if (millis() - lastPersistentVariableUpdate > PERSISTENTVARIABLEUPDATE){
		storeInRam ();
		lastPersistentVariableUpdate = millis();
	}
	
	//6. Process Ethernet Data
	//ether.packetLoop(ether.packetReceive());
	int len = ether.packetReceive();
    int pos=ether.packetLoop(len);
		
		//7. Ethernet Packet
		if (len) 
		{
			if (pos)
			{
				bfill = ether.tcpOffset();
				char* data = (char *) Ethernet::buffer + pos;
			
				//Serve Webpage
				if (strncmp("GET /", data, 5) == 0) 
				{
					homePage(data, bfill);
				}
			}
			timeLong = 0L;
			//NTP Responce
			if (ether.ntpProcessAnswer(&timeLong,clientPort)) 
			{		   
				if (timeLong) 
				{
					timeLong -= GETTIMEOFDAY_TO_NTP_OFFSET;
					
					gmtime(timeLong);
					
					sendTime();
					
					lastNTP = millis();
				}
			}
		}
}

void storeInRam (){
	DateTime now = RTC.now();
	
	RTC.writeByteInRam (8, now.day());
	RTC.writeByteInRam (9, now.month());
	RTC.writeByteInRam (10, now.year());
	RTC.writeByteInRam (11, now.hour());
	RTC.writeByteInRam (12, now.minute());
	RTC.writeByteInRam (13, now.second());
	
	RTC.writeByteInRam (14, int (todaysKwh));	
	int todaysKwhDecimal = (todaysKwh - int(todaysKwh)) * 255;
	RTC.writeByteInRam (15, todaysKwhDecimal);
	
	RTC.writeByteInRam (16, int (maxTemperature));
	int maxTemperatureDecimal = (maxTemperature - int(maxTemperature)) * 255;
	RTC.writeByteInRam (17, maxTemperatureDecimal);
	
	RTC.writeByteInRam (18, int (minTemperature));	
	int minTemperatureDecimal = (minTemperature - int(minTemperature)) * 255;
	RTC.writeByteInRam (19, minTemperatureDecimal);	
}

void readFromRam (){
	DateTime now = RTC.now();
	
	if (now.day() == RTC.readByteInRam (8))
	{
		if (now.hour() >= RTC.readByteInRam (11))
		{
			if (now.minute() >= RTC.readByteInRam (12))
			{
				todaysKwh = RTC.readByteInRam (14) + (float (RTC.readByteInRam (15)) / 255);
				maxTemperature = RTC.readByteInRam (16) + (float (RTC.readByteInRam (17)) / 255);
				minTemperature = RTC.readByteInRam (18) + (float (RTC.readByteInRam (19)) / 255);
			}
		}
	}
}

char okHeader[] PROGMEM = 
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Pragma: no-cache\r\n"
;

int getIntArg(const char* data, const char* key, int value =-1) {
    char temp[10];
    if (ether.findKeyVal(data + 6, temp, sizeof temp, key) > 0)
        value = atoi(temp);
    return value;
}

void sendLight(bool s)
{
	char data[] = {'l', s};
	int i = 0; while (!rf12_canSend() && i<10) {rf12_recvDone(); i++;}
	rf12_sendStart(0, data, sizeof data);
	rf12_sendWait(0);
}

void homePage(const char* data, BufferFiller buf)
{
	if (data[5] == '?')
	{
		int a = getIntArg(data, "a");
		if (a != -1) sendLight(a);
				
		int t = getIntArg(data, "t");
		if (t == 1) sendTime();
	}
	
	DateTime now = RTC.now();
				
	buf.emit_p(PSTR("$F\r\n"
		"<meta http-equiv='refresh' content='10; url=?'/>"
        "<title>Base Station</title>" 
        "<h2>Base Station Interface</h2>"
		"<form>"
			"<h3>Aquarium</h3>"
				"Light: <input type=radio name=a value='1' $S>On<input type=radio name=a value='0' $S>Off<br>"
				"Temperature: $D C<br>"
			"<input type=submit value=Set>"
		"</form>"
		"<form>"
			"<h3>Time</h3>"
				"Time: $D$D:$D$D:$D$D<br>"
			"<input type=hidden name=t value=1>"	
			"<input type=submit value=Send>"
		"</form>"), okHeader, 
			aquarium.light ? "CHECKED" : "", aquarium.light ? "" : "CHECKED",
			aquarium.temperature, now.hour()/10, now.hour()%10, now.minute()/10, now.minute()%10, now.second()/10, now.second()%10);

			ether.httpServerReply(buf.position()); // send web page data
}

void sendTime()
{
	DateTime now = RTC.now();
	char data[] = {'t',now.hour(), now.minute(), now.second(), now.year(), now.month(), now.day()};
	int i = 0; while (!rf12_canSend() && i<10) {rf12_recvDone(); i++;}
	rf12_sendStart(0, data, sizeof data);
	rf12_sendWait(0);
}