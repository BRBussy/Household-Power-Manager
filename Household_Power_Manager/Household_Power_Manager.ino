/*
 Name:		Household_Power_Manager.ino
 Created:	3/23/2016 3:01:35 PM
 Author:	Bernard
*/
#include <Arduino.h> 
#include <Wire.h>
#include <RtcUtility.h>
#include <RtcDS3231.h>
#include <RtcDateTime.h>
#include "Ticker.h"
#include "WiFiServer.h"
#include "WiFiClientSecure.h"
#include "WiFiClient.h"
#include "ESP8266WiFiType.h"
#include "ESP8266WiFiSTA.h"
#include "ESP8266WiFiScan.h"
#include "ESP8266WiFiMulti.h"
#include "ESP8266WiFiGeneric.h"
#include "ESP8266WiFiAP.h"
#include "ESP8266WiFi.h"
#include "FS.h"
#include "rebuild_received_anything.h"

//Special Data Types
//Scheduling Information Structure
struct scheduling_information { //Declare scheduling_information struct type
	int ID; //Device ID
	bool hours_on_off[7][24][60]; //hours_on_off[Days, Hours, Minutes]
};
//Time and Date Structure
struct time_and_date { //Declare time_and_date struct type
	uint16_t year;
	uint8_t month;
	uint8_t dayOfMonth;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
};
//Power Measurement Structure
struct power_measurement { //Declare power_measurement struct type
	float measurement; //Actual Power Measurement
	time_and_date when_made; //when this reading was taken
	int ID; //The Major Appliance ID
};
//Major Appliance Status Structure
struct major_appliance_status { //Declare major_appliance_status struct type
	bool on_off; //Current on/off Status
	power_measurement latest_measurement; //Most recent power reading
};

//Definitions
	#define BAUD 115200
	//Device IDs
	#define Total_Household 0
	#define Major_Appliance 1
	//Modes
	#define Setup_Mode 0
	#define Normal_Mode 1
	//Pin Definitions
	#define SetupModeButton 0
	#define SDA_pin 2
	#define SCL_pin 14
	//Statuses
	#define WifiConnected 0
	#define ConnectedtoServer 1
	//WiFi Data Recieve Flags
	#define New_Data_for_Client 0
	#define Send_Data_to_Server 1
	#define No_Command_in_data_from_Server 2
	#define No_Data_Received 3
	#define Wait_Until_Recv true
	//Commands_Received
	#define No_Command 0 
	#define Send_Data_To_Server 1 
	#define Receive_Data_From_Server 2
	//RTC Definitions
	#define countof(a) (sizeof(a) / sizeof(a[0]))

//Global Variable Declarations

	//Network Details --SAVE to EEPROM
	String NetworkName = "";
	String NetworkPassword = "";
	
	//Client Server Details
	String Host = "10.0.0.9";
	int Port = 6950;

	//Major Appliance Variables
	scheduling_information device_schedule;

	//System Variables
	WiFiClient client;
	RtcDS3231 Rtc;
	FSInfo fs_info;

	//Other Uncategorized Variables
	int Take_Measurement_counter = 5;
	int Operating_Mode;
	bool Statuses[2] = {};
	bool Setup_mode_print_waiting_message = true;

// the setup function runs once when you press reset or power the board
void setup() {
	//Hardware Setup, pin modes etc.
	Serial.begin(BAUD);
	pinMode(SetupModeButton, INPUT);
	RTC_SETUP();
	File_System_Setup();

	//Retrieve old device schedule from memory;
	Retrieve_Schedule_information();
	//Retrieve network details from memory;
	
	//Software Setup
	Operating_Mode = Setup_Mode;
}//End setup();

// the loop function runs over and over again until power down or reset
void loop() {
	//Serial.begin(BAUD);
	delay(10);

	//Check if User is Requesting SetupMode
	if (!digitalRead(SetupModeButton)) {
		Operating_Mode = Setup_Mode;
	}

	switch (Operating_Mode) {
	case Setup_Mode:
	{
		int no_of_Bytes_Received = 0;
		byte *Bytes_of_Data_In = NULL;

		Bytes_of_Data_In = setup_mode_receive_Data_via_Serial(no_of_Bytes_Received);
		if (Bytes_of_Data_In != NULL)
		{
			Data_Identification_Protocol(Bytes_of_Data_In, no_of_Bytes_Received);
			Serial.println("");
			Setup_mode_print_waiting_message = true;
		}
		else {
			if (Setup_mode_print_waiting_message) {
				Serial.println("Waiting for Serial Data");
				Setup_mode_print_waiting_message = false;
			}
			else
				Serial.print(".");
		}
		delay(1000);

		/*//Setup Mode Variables
		String inputString = "";
		String SSID = "";
		String Password = "";

		//Setup Mode Code
		Serial.println("Waiting For Input from Serial:");

		/*while (Operating_Mode == Setup_Mode)
		{
			//Read Input From Serial
			while (Serial.available() > 0)
			{
				inputString += (char)Serial.read();
			}
			if (inputString == "Scan") //Display Available WiFi Networks to User
			{
				int n = WiFi.scanNetworks(); //No of Networks found
				if (n == 0)
					Serial.println("No Networks Found");
				else
				{
					Serial.print(n);
					Serial.println("Networks Found:");
					for (int i = 0; i < n; ++i)
					{
						// Print SSID and RSSI for each network found
						Serial.print(i + 1);
						Serial.print(": ");
						Serial.print(WiFi.SSID(i));
						Serial.print(" (");
						Serial.print(WiFi.RSSI(i));
						Serial.print(")");
						Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
						delay(10);
					}
				}
			} //End case "Scan"
			else if (inputString == "Connect") //Connect to WiFi Network
			{
				Connect_to_WiFi();
			} //End case "Connect"
			else if (inputString == "Status") //Display Status of Connection
			{
				Serial.print("Network Name: ");
				Serial.println(NetworkName);
				Serial.print("Network Password: ");
				Serial.println(NetworkPassword);
			} //End case Status
			else if (inputString.startsWith("NetDet:") && inputString.endsWith("|"))
			{
				String SSID = "";
				String Password = "";
				Serial.println("Network Details Received:");
				int charnum = 7;
				//Extract NetworkName
				while (inputString.charAt(charnum) != '|')
				{
					SSID += inputString.charAt(charnum);
					charnum++;
				}
				charnum++;
				//Extract NetworkPassword
				while (inputString.charAt(charnum) != '|')
				{
					Password += inputString.charAt(charnum);
					charnum++;
				}
				NetworkName = SSID;
				NetworkPassword = Password;
				Serial.print("Network Name: ");
				Serial.println(NetworkName);
				Serial.print("Network Password: ");
				Serial.println(NetworkPassword);
			}
			else if (inputString == "Done")
			{
				Serial.println("Starting Normal Mode...");
				Operating_Mode = Normal_Mode;
			}
			else
			{
				Serial.print(".");
			}

		inputString = "";
		delay(1000);
		}*/
		
		free(Bytes_of_Data_In);
		break;
	}//End Setup Mode

	case Normal_Mode:
	{
		float measurement = 22.3;
		int ID = Major_Appliance;
		
		Send_Receive_Protocol();
		time_test();
		if (Take_Measurement_counter <= 5) { 
			Take_Measurement_counter++; 
		}
		else {
			Store_power_measurement(measurement, ID);
			Take_Measurement_counter = 0;
		}
		if (Take_Measurement_counter == 0) {
			Display_Measurements();
		}
		

		//delay(500);
		break;
	}//End Normal Mode
	}//End Switch(Operating_Mode)
}//End loop()


//Subroutines
void Send_Receive_Protocol(void)
{	
	//Send_Receive_Protocol Variables
	bool Wait_for_Data = false;
	bool Data_Received = false;
	bool Timeout_Expired = false;

	int no_of_Bytes_Received = 0;
	int Timeout_Counter = 0;
	
	byte *Bytes_of_Data_In = NULL; //Will point to array of bytes in
	String temp = "nothing";
	
	do {
		//WiFi Connected?
		if (WiFi.status() == WL_CONNECTED)
		{//WiFi Connected? -->YES
			Statuses[WifiConnected] = true;

			//Connected to Server?
			if (client.connected())
			{//Connected to Server? -->YES
				Statuses[ConnectedtoServer] = true;
				Bytes_of_Data_In = receive_Data_From_Server(no_of_Bytes_Received);
				if (Bytes_of_Data_In != NULL)
				{
					Data_Received = true;
				}
				//Data From Server Received?
				if (Data_Received) 
				{//Data From Server Received? --> YES
					//Pass to Data_Identification_Protocol
					int Command_Received = Data_Identification_Protocol(Bytes_of_Data_In, no_of_Bytes_Received);
					
					//Received What from Server?
					switch (Command_Received) {
					case Send_Data_To_Server: { //Received What from Server? -->Send
						Serial.println("Received Command to Send Data to Server");
						Send_New_Data_to_Server(); //Send some Data to Server
						Wait_for_Data = false; //No Need to Wait for Data
						break;
					}//END Case Send_Data_To_Server
					case Receive_Data_From_Server: { //Received What from Server? -->Receive
						Serial.println("Received Command to Receive Data from Server");
						Data_Received = false;		 //No Data Received yet...
						Timeout_Counter = 0;		 //Reset Timeout_Counter
						Wait_for_Data = true;		 //Need to Wait for Data from Server
						break;
					}
					default: { //Received What from Server? -->No_Command
						Serial.println("No Command Received From Server");
						Wait_for_Data = false;
						break;
					}//END default case
					}//END Switch (Command_Received)
				}
				else
				{//Data From Server Received? --> NO
					//Do Nothing, Go to while where continuity Check Carried out
				}
			}
			else
			{//Connected to Server? -->NO
				Statuses[ConnectedtoServer] = false;
				Connect_as_Client();
				return; //Continue
			}
		}
		else
		{//WiFi Connected? -->NO
			Statuses[WifiConnected] = false;
			Connect_to_WiFi();
			return; //Continue
		}

		if ((Wait_for_Data)&&((Timeout_Counter++) == 10))
		{
			Serial.println("Timeout Expired");
			Timeout_Expired = true;
		}
		else {
			if (Wait_for_Data) {
				//Serial.print("Waiting for Data From Server. Timeout = ");
				//Serial.println(Timeout_Counter);
				delay(1000);
				Timeout_Expired = false;
			}
		}	
		free(Bytes_of_Data_In);
	 }while((!Data_Received) && (Wait_for_Data) && !Timeout_Expired);
}

//-Attempt Connection to WiFi
void Connect_to_WiFi(void)
{
	int attempt_count = 20;
	bool network_available = false;
	
	WiFi.disconnect();
	Serial.println("Attempting Connection to: ");
	Serial.print(NetworkName);

	//Check If Desired Network is available, Return if unavailable
	int n = WiFi.scanNetworks(); 
	if (n == 0)
	{
		Serial.println("No Networks Found. Cannot Connect!");
		return;
	}
	else
	{
		for (int i = 0; i < n; ++i)
		{
			if (WiFi.SSID(i) == NetworkName)
				network_available = true;
			delay(10);
		}
		if (!network_available)
		{
			Serial.print("Network: ");
			Serial.print(NetworkName);
			Serial.print(" not available. Unable to Connect!");
			return;
		}
	}
	
	//If Network Available, Attempt Connection, Return if failed
	WiFi.begin(NetworkName.c_str(), NetworkPassword.c_str());
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
		attempt_count--;
		if (!attempt_count)
		{
			Serial.println("Password Incorrect! WiFi Connection Failed!");
			return;
		}
	}
	Serial.println("WiFi Connected!!");
	Serial.print("IP Address is:");
	Serial.print(WiFi.localIP());
	Serial.println("");
	return;
}

//-Attempt Connection to Server
void Connect_as_Client(void)
{
	Serial.println("Attempting Connection to Server...");
	if (client.connect(Host.c_str(), Port))
	{
		Serial.println("Connected to Server!");
	}
	else
		Serial.println("Connection to Server Not Possible.");
}

//-Pointer to all bytes received is returned
byte* receive_Data_From_Server(int &no_of_bytes_received)
{
	//int Received_New_Data = 0;		//No New Data Yet
	int size = 0;
	byte *ptr_Bytes_of_Data_In = NULL;

	while (client.available())			//While there is New Data Retreive it
	{
		size++;
		ptr_Bytes_of_Data_In = (byte*)realloc(ptr_Bytes_of_Data_In, size*sizeof(byte)); //grow array

		byte data_byte = client.read();					//read a byte
		ptr_Bytes_of_Data_In[size - 1] = data_byte;		//assign to byte array
		no_of_bytes_received = size;						//New Data Has been Retreived
	}
	if (no_of_bytes_received) {
		Serial.println("");
		Serial.print("No of Bytes Received: ");
		Serial.println(no_of_bytes_received);
	}
	
	return ptr_Bytes_of_Data_In;
} //END receive_Data_From_Server

byte* setup_mode_receive_Data_via_Serial(int &no_of_bytes_received)
{
	//int Received_New_Data = 0;		//No New Data Yet
	int size = 0;
	byte *ptr_Bytes_of_Data_In = NULL;

	while (Serial.available()) //While there is New Data Retreive it
	{
		size++;
		ptr_Bytes_of_Data_In = (byte*)realloc(ptr_Bytes_of_Data_In, size*sizeof(byte)); //grow array

		byte data_byte = Serial.read();					   //read a byte
		ptr_Bytes_of_Data_In[size - 1] = data_byte;		   //assign to byte array
		no_of_bytes_received = size;					   //New Data Has been Retreived
	}

	if (no_of_bytes_received) {
		Serial.println("");
		Serial.print("No of Bytes Received: ");
		Serial.println(no_of_bytes_received);
	}

	return ptr_Bytes_of_Data_In;
}

/*-Returns Type of Command if a Command is Received, else Returns No_Command
  -Passes Data to be Processed to Appropriate Processing Routine*/
int Data_Identification_Protocol(byte *Bytes_of_Data_In, int &no_of_Bytes_Received)
{
	//Data_Identification_Protocol Variables
	String Data_Start = "";
	String Data_ID = "";
	String Data_End = "";
	int Data_Start_Point = 7;
	int Data_End_Point;
	int Num_Bytes_in_Payload;
	byte *Data_Payload = NULL; //Will point array of byes which is Data_Payload

	//Print out Data to be Identified:
	/*Serial.println("Data Identification Protocol Dealing with: ");
	for (int i = 0; i < no_of_Bytes_Received; i++)
	{
		Serial.println(Bytes_of_Data_In[i]);
	}*/

	if (no_of_Bytes_Received > 12) //If Frame is Valid, i.e. meets minimum no. of bytes
	{
		//Extract Start of Data field: |D|
		for (int i = 0; i <= 2; i++) {
			Data_Start += (char)Bytes_of_Data_In[i];
		}
		if (Data_Start != "|D|")
		{
			Serial.println("No Start Data field detected |D|");
			return No_Command;
		}
		//If |D| was found then Extract End of Data field: |ED|
		for (int i = 0; i <= (no_of_Bytes_Received - 4); i++) {
			Data_End = "";		//Empty Data_End
			Data_End += (char)Bytes_of_Data_In[i];
			Data_End += (char)Bytes_of_Data_In[i + 1];
			Data_End += (char)Bytes_of_Data_In[i + 2];
			Data_End += (char)Bytes_of_Data_In[i + 3];
			
			if (Data_End == "|ED|") {
				Data_End_Point = i-1;
				break;
			}
		}
		if (Data_End != "|ED|")
		{
			Serial.println("No End Data field detected |ED|");
			return No_Command;
		}
		//If |ED| was found then Extract End of Data ID: |ID|
		for (int i = 3; i < Data_Start_Point; i++) {
			Data_ID += (char)Bytes_of_Data_In[i];
		}
		//Extract Data Payload
		Num_Bytes_in_Payload = ((Data_End_Point - Data_Start_Point) + 1);
		if (Num_Bytes_in_Payload) //If Num_Bytes_in_Payload > 0
		{
			Data_Payload = (byte*)realloc(Data_Payload, Num_Bytes_in_Payload*sizeof(byte));
			int j = 0;
			for (int i = Data_Start_Point; i <= Data_End_Point; i++) {
				Data_Payload[j] = Bytes_of_Data_In[i];
				j++;
			}
		}
		
	}
	else
	{
		Serial.println("Incorrect Frame");
		return No_Command;
	}

	//Data Stats:
	Serial.println("Received Data Stats:");
	Serial.print("Data ID: ");
	Serial.println(Data_ID);
	Serial.print("Data Start: ");
	Serial.println(Data_Start_Point);
	Serial.print("Data End: ");
	Serial.println(Data_End_Point);
	Serial.print("Number of Bytes in Payload: ");
	Serial.println(Num_Bytes_in_Payload);
	
	//Print out Payload only
	/*Serial.println("Data Payload:");
	Serial.print("|");
	for (int i = 0; i < Num_Bytes_in_Payload; i++) {
		Serial.print(Data_Payload[i]);
		Serial.print("|");
	}
	Serial.println("\n");
	Serial.println("");*/

	//Send Data_Payload to correct Processing Routing according to |ID|
	if (Data_ID == "|CM|") //If A Command was Received
	{
		Serial.println("Passing to Command Processing Fcn");
		//Pass to Command Processing Function
		int command_processing_result = process_received_command(Data_Payload, Num_Bytes_in_Payload);
		
		//Interpret Return value of Command Processing Function
		switch (command_processing_result) {
		case No_Command: {
			return No_Command;
			break;
		}//END Case No_Command
		case Send_Data_To_Server: {
			return Send_Data_To_Server;
			break;
		}//END Case Send_Data_To_Server
		case Receive_Data_From_Server: {
			return Receive_Data_From_Server;
			break;
		}//END Case Receive_Data_From_Server
		default: {
			return Receive_Data_From_Server;
		}
		}//END switch (command_received)
	}//END if (Data_ID == "|CM|")

	else if ((Data_ID == "|SI|") && (Operating_Mode == Normal_Mode)) //If Scheduling Information was Received
	{
		process_received_schedule(Data_Payload, Num_Bytes_in_Payload, false);
	}//END else if (Data_ID == "|SI|")

	else if (Data_ID == "|TI|") //If Time Information was Received
	{
		process_received_time(Data_Payload, Num_Bytes_in_Payload);
	}//END else if (Data_ID == "|TI|")

	else if (Data_ID == "|ND|") //Network Details Were Received
	{
		process_received_network_details(Data_Payload, Num_Bytes_in_Payload);
	}//END else if (Data_ID == "|ND|")
	
	return No_Command;
}

//Data Processing Subroutines
//Data Receive Processing Subroutines
//|CM| Command Processing
int process_received_command(byte *Data_Payload, int &Num_Bytes_in_Payload)
{
String Command = "";
for (int i = 0; i < Num_Bytes_in_Payload; i++)
{
	Command += (char)Data_Payload[i];
}
Serial.print("Command to Process is: ");
Serial.println(Command);

if (Command == "Send") {
	if (Operating_Mode == Normal_Mode) 
	{
		return Send_Data_To_Server;
	}
	else
	{
		Serial.println("This Command only Valid in Normal Operating Mode!");
		return No_Command;
	}
}

else if (Command == "Receive")
{
	if (Operating_Mode == Normal_Mode) {
		return Receive_Data_From_Server;
	}
	else
	{
		Serial.println("This Command only Valid in Normal Operating Mode!");
		return No_Command;
	}
}

else if (Command == "RunSched") {
	if (Operating_Mode == Normal_Mode) {
		Serial.println("Server says to Run Schedule");
		return No_Command;
	}
	else
	{
		Serial.println("This Command only Valid in Normal Operating Mode!");
		return No_Command;
	}
}

else if (Command == "OFF") {
	if (Operating_Mode == Normal_Mode) {
		Serial.println("Server says to Turn OFF Major Appliance");
		return No_Command;
	}
	else
	{
		Serial.println("This Command only Valid in Normal Operating Mode!");
		return No_Command;
	}
}

else if (Command == "ON") {
	if (Operating_Mode == Normal_Mode) {
		Serial.println("Server says to Turn ON Major Appliance");
		return No_Command;
	}
	else
	{
		Serial.println("This Command only Valid in Normal Operating Mode!");
		return No_Command;
	}
}

else if (Command == "Scan") //Display Available WiFi Networks to User
{
	int n = WiFi.scanNetworks(); //No of Networks found
	if (n == 0)
		Serial.println("No Networks Found");
	else
	{
		Serial.print(n);
		Serial.println(" Networks Found:");
		for (int i = 0; i < n; ++i)
		{
			// Print SSID and RSSI for each network found
			Serial.print(i + 1);
			Serial.print(": ");
			Serial.print(WiFi.SSID(i));
			Serial.print(" (");
			Serial.print(WiFi.RSSI(i));
			Serial.print(")");
			Serial.println((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*");
			delay(10);
		}
	}
	return No_Command;
}

else if (Command == "Connect") //Connect to WiFi Network
{
	Connect_to_WiFi();
}

else if (Command == "Disconnect") //Connect to WiFi Network
{
	if (WiFi.status() == WL_CONNECTED)
		WiFi.disconnect();
	else
		Serial.println("WiFi Not Connected");
}

else if (Command == "Status") //Display Status of Connection
{
	Serial.println("-----ALL RELEVANT WiFi STATUS INFORMATION-----");
	Serial.println("Stored Network Details are:");
	Serial.print("Network Name: ");
	Serial.println(NetworkName);
	Serial.print("Network Password: ");
	Serial.println(NetworkPassword);
	Serial.println("");

	Serial.println("Connection Details:");
	if (WiFi.status() == WL_CONNECTED) {
		Serial.print("Connected to: "); Serial.println(WiFi.SSID());
		Serial.print("Signal Strength: "); Serial.print(WiFi.RSSI()); Serial.println(" dBm");
		Serial.print("Gateway Address: "); Serial.println(WiFi.gatewayIP());
		Serial.print("IP Address: "); Serial.println(WiFi.localIP());
	}
	else {
		Serial.println("Not Connected to any WiFi Networks");
	}
}

else if (Command == "Done")
{
	Serial.println("Starting Normal Mode...");
	Operating_Mode = Normal_Mode;
}

else
Serial.println("No Valid Command Received");
return No_Command;
}
//|SI| Scheduling Processing

void process_received_schedule(byte *Data_Payload, int &Num_Bytes_in_Payload, bool startup)
{
	if (!startup) {
		Serial.println("New Schdule Data Received, Processing Schedule Now...");
		//Save Received Scheduling Info to Memory
		File f = SPIFFS.open("/device_scheduling_data", "w");
		if (!f) {
			Serial.println("File Open Failed");
		}
		else
		{
			Serial.println("File Open Successfull! Saving to Memory...");
			for (int i = 0; i < Num_Bytes_in_Payload; i++)
			{
				f.write(Data_Payload[i]);
			}
			f.close();
		}
	}
	else
	{
		Serial.println("Loading Schedule Data retrieved from Memory");
	}

	Serial.println("New Schdule Data Received, Processing Schedule Now...");
	int Day = 0, Hour_Start = 0, Minute_Start = 0, Hour_End = 0, Minute_End = 0;
	
	//First Clear out Old Schedule
	for (int Day_index = 0; Day_index < 7; Day_index++)
		for (int hour_index = 0; hour_index < 24; hour_index++)
			for (int minute_index = 0; minute_index < 60; minute_index++)
				device_schedule.hours_on_off[Day_index][hour_index][minute_index] = false;

	//Decode Received data device_schedule
	for (int i = 0; i < Num_Bytes_in_Payload; i+=10) 
	{//For each schedule in frame received decode and load into device_schedule
		for (int j = i; j < i + 10; j++)
		{
			if (j == i + 1) 
				Day = (char)Data_Payload[j] - 48;
			else if (j == i + 2) {
				String temp_Hour_Start = "";
				temp_Hour_Start += (char)Data_Payload[j];
				temp_Hour_Start += (char)Data_Payload[j+1];
				Hour_Start = temp_Hour_Start.toInt();
			}
			else if (j == i + 4) {
				String temp_Minute_Start = "";
				temp_Minute_Start += (char)Data_Payload[j];
				temp_Minute_Start += (char)Data_Payload[j + 1];
				Minute_Start = temp_Minute_Start.toInt();
			}
			else if (j == i + 6) {
				String temp_Hour_End = "";
				temp_Hour_End += (char)Data_Payload[j];
				temp_Hour_End += (char)Data_Payload[j + 1];
				Hour_End = temp_Hour_End.toInt();
			}
			else if (j == i + 8) {
				String temp_Minute_End = "";
				temp_Minute_End += (char)Data_Payload[j];
				temp_Minute_End += (char)Data_Payload[j + 1];
				Minute_End = temp_Minute_End.toInt();
			}
		}

		//Print out this decoded schedules Details
		/*Serial.print("Day: ");Serial.println(Day);
		Serial.print("Hour Start: ");Serial.println(Hour_Start);
		Serial.print("Hour End: ");Serial.println(Hour_End);
		Serial.print("Minute Start: ");Serial.println(Minute_Start);
		Serial.print("Minute End: ");Serial.println(Minute_End);*/

		//Load this schedule into device_schedule
		if (((Day <= 6) && (Day >= 0))
			&& ((Hour_Start <= 23) && (Hour_Start >= 0))
			&& ((Hour_End <= 23) && (Hour_End >= 0))
			&& ((Minute_Start <= 59) && (Minute_Start >= 0))
			&& ((Minute_End <= 59) && (Minute_End >= 0))
			&& (Hour_Start <= Hour_End)
			&& ((((Hour_Start == Hour_End) && (Minute_Start <= Minute_End))) || (Hour_Start != Hour_End)))
		{
			Serial.println("Good Schedule Data Received");
			//Make True according to Day Schedule
			for (int hour = Hour_Start; hour <= Hour_End; hour++)
			{
				if (Hour_Start != Hour_End) {
					if (hour == Hour_Start) {
						for (int minute = Minute_Start; minute <= 59; minute++) {
							device_schedule.hours_on_off[Day][hour][minute] = true;
						}
					}
					else if (hour == Hour_End) {
						for (int minute = 0; minute <= Minute_End; minute++)
						{
							device_schedule.hours_on_off[Day][hour][minute] = true;
						}
					}
					else { //Hour_Start < hour < Hour_End
						for (int minute = 0; minute <= 59; minute++) {
							device_schedule.hours_on_off[Day][hour][minute] = true;
						}
					}
				}
				else { //Hour_Start == Hour_End
					for (int minute = Minute_Start; minute <= Minute_End; minute++) {
						device_schedule.hours_on_off[Day][hour][minute] = true;
					}
				}
			}
		}
		else {
			Serial.println("Bad Schedule Data Received");
		}
	}
	//Print out Entire new Device_Schedule
	if (startup)
	{
		Serial.println("Schedule Data Retrieved: ");
		for (int Day_index = 0; Day_index < 7; Day_index++)
		{
			Serial.print("Day ");Serial.print(Day_index); Serial.println(":");
			for (int hour_index = 0; hour_index < 24; hour_index++)
			{
				Serial.print("Hour ");Serial.print(hour_index); Serial.println(":");
				for (int minute_index = 0; minute_index < 60; minute_index++)
				{
					Serial.print(device_schedule.hours_on_off[Day_index][hour_index][minute_index]);
				}
				Serial.println("");
			}
			Serial.println("");
			Serial.println("");
		}
	}
	
}
//|TI| Time Processing
void process_received_time(byte *Data_Payload, int &Num_Bytes_in_Payload)
{
	Serial.println("Processing a Time...");
}
//|ND| Network Details Processing
void process_received_network_details(byte *Data_Payload, int &Num_Bytes_in_Payload)
{
	Serial.println("Processing Network Details...");
	String SSID = "";
	String Password = "";
	
	//Extract NetworkName
	int i = 0;
	while (((char)Data_Payload[i] != '|') && i < Num_Bytes_in_Payload)
	{
		SSID += (char)Data_Payload[i];
		i++;
	}
	i++;

	//Extract NetworkPassword
	if (i < Num_Bytes_in_Payload)
	{
		while (((char)Data_Payload[i] != '|') && i < Num_Bytes_in_Payload)
		{
			Password += (char)Data_Payload[i];
			i++;
		}
	}
	
	NetworkName = SSID;
	NetworkPassword = Password;
	Serial.print("Network Name: ");
	Serial.println(NetworkName);
	Serial.print("Network Password: ");
	Serial.println(NetworkPassword);
}
//Data Storage system Subroutines
void Store_power_measurement(const float &measurement, const int &ID)
{
	int store_attempt = 0;
	bool file_open_success = false;
	power_measurement measurement_to_store; //Structure for Measurement
	RtcDateTime now = Rtc.GetDateTime();    //Get Time Measurement is Made (stored)

	SPIFFS.info(fs_info); //Check how much memory is left
	if (fs_info.totalBytes < (fs_info.usedBytes + 100)) {
		Serial.println("There is No More Memory available!!");
		return;
	}

	measurement_to_store.measurement = measurement;
	measurement_to_store.when_made.year = now.Year();
	measurement_to_store.when_made.month = now.Month();
	measurement_to_store.when_made.dayOfMonth = now.Day();
	measurement_to_store.when_made.hour = now.Hour();
	measurement_to_store.when_made.minute = now.Minute();
	measurement_to_store.when_made.second = now.Second();
	measurement_to_store.ID = ID;

	//Create Unique Name
	String Measurement_Name = "/Measurements/";
	//Measurement_Name += measurement_to_store.when_made.year;
	//Measurement_Name += "_";
	//Measurement_Name += measurement_to_store.when_made.month;
	//Measurement_Name += "_";
	//Measurement_Name += measurement_to_store.when_made.dayOfMonth;
	//Measurement_Name += "_";
	Measurement_Name += measurement_to_store.when_made.hour;
	Measurement_Name += "_";
	Measurement_Name += measurement_to_store.when_made.minute;
	Measurement_Name += "_";
	Measurement_Name += measurement_to_store.when_made.second;

	Serial.println();
	Serial.print("Name of file to Store Measurement in is: ");
	Serial.println(Measurement_Name);
	//Serial.print("No Of Bytes in Measurement is: ");
	//Serial.print(sizeof(measurement_to_store));
	//Serial.println();

	byte *ptr_to_measurement_to_store_bytes = (byte*)(void*)(&measurement_to_store);
	do {
		File f = SPIFFS.open(Measurement_Name, "w");
		if (!f) {
			Serial.println("File Open Failed");
			store_attempt++;
		}
		else
		{
			Serial.println("File Open Successfull!");
			file_open_success = true;
			for (int i = 0; i < sizeof(measurement_to_store); i++)
			{
				f.write(ptr_to_measurement_to_store_bytes[i]);
			}
			f.close();
		}
	} while ((store_attempt <= 10) && (!file_open_success));
}
bool Check_if_new_measurements_to_send(void)
{
	Dir dir = SPIFFS.openDir("/Measurements");
	if (dir.next()) { //Check for any files in directory Power_Readings
		return true;
	}
	return false;
}
void Display_Measurements(void)
{
	//power_measurement measurement_to_send; //Structure for Measurement
	//byte *ptr_to_measurement_to_send_bytes = (byte*)(void*)(&measurement_to_send);
	int no_of_readings = 0;
	Dir dir = SPIFFS.openDir("/Measurements");
	if (dir.next()) //Check for any files in directory Power_Readings
	{
		no_of_readings++;
		while (dir.next()) 
		{
			no_of_readings++;
		}
	}
	Serial.println("Measurements Directory Contains: ");
	Serial.print(no_of_readings);
	Serial.println(" Power Readings");
			
			/*Serial.print("File: ");
			Serial.println(dir.fileName());

			File f = dir.openFile("r");
			if (!f) {  //Check if File Opened Successfully
			Serial.println("file open failed");
			}
			else { //If it Did then Reconstruct Measurement Structure
			for (int i = 0; i < sizeof(measurement_to_send); i++)
			{
			ptr_to_measurement_to_send_bytes[i] = f.read();
			}
			Serial.print("With Measurement Value: ");
			Serial.println(measurement_to_send.measurement);
			Serial.println("");
			}*/
}

//Process and Send Data to Server
void Send_New_Data_to_Server(void)
{
	//Check if there is data to Send and Set New_Data Accordingly
	if (Check_if_new_measurements_to_send())
	{
		byte data_to_send[120];
		//create frame
		data_to_send[0] = '|';
		data_to_send[1] = 'D';
		data_to_send[2] = '|';
		data_to_send[3] = '|';
		data_to_send[4] = 'P';
		data_to_send[5] = 'R';
		data_to_send[6] = '|';
		Dir dir = SPIFFS.openDir("/Measurements"); //Open Directory
		if (dir.next()) //Check if reading is still there
		{
			File f = dir.openFile("r"); //Open File for reading
			if (!f) {					//Check if File Opened Successfully
				Serial.println("file open failed");
			}
			else { //If it Did then pack bytes to Send
				//Serial.print("File Size: ");
				//Serial.println(f.size());
				for (int i = 0; i < f.size(); i++)
				{
					data_to_send[i + 7] = f.read();
				}
				data_to_send[f.size() + 7] = '|';
				data_to_send[f.size() + 8] = 'E';
				data_to_send[f.size() + 9] = 'D';
				data_to_send[f.size() + 10] = '|';
				Serial.println("Frame of Data to Send: ");
				String data = "";
				for (int i = 0; i < f.size() + 11; i++)
				{
					Serial.print((char)data_to_send[i]);
					data += (char)data_to_send[i];
				}
				Serial.println("");
				client.println(data);
				SPIFFS.remove(dir.fileName()); //Remove File
			}
		}
		else {
			Serial.println("Measurement is gone!!");
		}
	}
	else
	{
		client.println("|D|No_New_Data|ED|");
	}
}

//Setup Routines
void RTC_SETUP(void) {
	Rtc.Begin();
	Wire.begin(SDA_pin, SCL_pin);
	RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
	if (!Rtc.IsDateTimeValid())
	{
		// Common Cuases:
		//    1) first time you ran and the device wasn't running yet
		//    2) the battery on the device is low or even missing
		Serial.println("RTC lost confidence in the DateTime!");
		// following line sets the RTC to the date & time this sketch was compiled
		// it will also reset the valid flag internally unless the Rtc device is
		// having an issue
		Rtc.SetDateTime(compiled);
	}
	if (!Rtc.GetIsRunning())
	{
		Serial.println("RTC was not actively running, starting now");
		Rtc.SetIsRunning(true);
	}
	RtcDateTime now = Rtc.GetDateTime();
	if (now < compiled)
	{
		Serial.println("RTC is older than compile time!  (Updating DateTime)");
		Rtc.SetDateTime(compiled);
	}
	else if (now > compiled)
	{
		Serial.println("RTC is newer than compile time. (this is expected)");
	}
	else if (now == compiled)
	{
		Serial.println("RTC is the same as compile time! (not expected but all is fine)");
	}

	// never assume the Rtc was last configured by you, so
	// just clear them to your needed state
	Rtc.Enable32kHzPin(false);
	Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);
}
void File_System_Setup(void)
{
	SPIFFS.begin();
	SPIFFS.info(fs_info);
	Serial.println("File System Info: ");
	Serial.print("Total Bytes: ");
	Serial.println(fs_info.totalBytes);
	Serial.print("Used Bytes: ");
	Serial.println(fs_info.usedBytes);
	Serial.print("BlockS ize: ");
	Serial.println(fs_info.blockSize);
	Serial.print("Max Open Files: ");
	Serial.println(fs_info.maxOpenFiles);

	//Serial.println("Formatting SPIFFS");
	//SPIFFS.format();
	//Serial.println("SPIFFS formatted");
}
void time_test(void) {
	if (!Rtc.IsDateTimeValid())
	{
		// Common Cuases:
		//    1) the battery on the device is low or even missing and the power line was disconnected
		Serial.println("RTC lost confidence in the DateTime!");
	}

	RtcDateTime now = Rtc.GetDateTime();
	printDateTime(now);
	RtcTemperature temp = Rtc.GetTemperature();
	//Serial.print(temp.AsFloat());
	//Serial.println("C");
}
void printDateTime(const RtcDateTime& dt)
{
	char datestring[20];
	snprintf_P(datestring,
		countof(datestring),
		PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
		dt.Month(),
		dt.Day(),
		dt.Year(),
		dt.Hour(),
		dt.Minute(),
		dt.Second());
	Serial.print(datestring);
}
void Retrieve_Schedule_information(void)
{
	byte *Data_Payload = NULL;
	int Num_Bytes_in_Payload = 0;

	if (SPIFFS.exists("/device_scheduling_data"))
	{
		Serial.println("Retrieving Stored Scheduling Data");
		File f = SPIFFS.open("/device_scheduling_data", "r");
		if (!f) {
			Serial.println("File open failed!");
		}
		else {
			Serial.println("File open successful!");
			Num_Bytes_in_Payload = f.size();
			Serial.print("Size of Scheduling Data: "); Serial.println(Num_Bytes_in_Payload);

			Data_Payload = (byte*)realloc(Data_Payload, Num_Bytes_in_Payload*sizeof(byte));
			
			for (int i = 0; i < Num_Bytes_in_Payload; i++)
			{
				Data_Payload[i] = f.read();
			}
			process_received_schedule(Data_Payload, Num_Bytes_in_Payload, true);
			free(Data_Payload);
		}
	}
	else 
	{
		Serial.println("No Stored Scheduling Data Found");
	}
}