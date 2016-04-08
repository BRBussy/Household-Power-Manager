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
#include "rebuild_received_anything.h"

//Special Data Types

//Scheduling Information Structure
struct scheduling_information { //Declare scheduling_information struct type
	int ID; //Device ID
	bool hours_on_off[12][31][24]; //hours_on_off[Months, Days, Hours]
};
//Time and Date Structure
struct time_and_date { //Declare time_and_date struct type
	int month;
	int day;
	int hour;
	int minute;
	int second;
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
	String NetworkName = "BernysWAP";
	String NetworkPassword = "bionicle123#";
	
	//Client Server Details
	String Host = "10.0.0.9";
	int Port = 6950;

	//Major Appliance Variables
	
	//Other Uncategorized Variables
	int Operating_Mode;
	bool Statuses[2] = {};
	WiFiClient client;
	RtcDS3231 Rtc;

// the setup function runs once when you press reset or power the board
void setup() {
	//Hardware Setup, pin modes etc.
	pinMode(SetupModeButton, INPUT);
	RTC_SETUP();
	//Software Setup
	Operating_Mode = Normal_Mode; 
}//End setup();

// the loop function runs over and over again until power down or reset
void loop() {
	Serial.begin(BAUD);
	delay(10);
	//Check if User is Requesting SetupMode
	if (!digitalRead(SetupModeButton)) {
		Operating_Mode = Setup_Mode;
	}

	switch (Operating_Mode) {
	case Setup_Mode:
	{
		Serial.println("Running Setup Mode:");
		
		//Setup Mode Variables
		String inputString = "";
		String SSID = "";
		String Password = "";

		//Setup Mode Code
		Serial.println("Waiting For Input from Serial:");

		while (Operating_Mode == Setup_Mode)
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
		}
		break;
	}//End Setup Mode

	case Normal_Mode:
	{
		Send_Receive_Protocol();
		time_test();
		//scheduling_information firstschedule;
		//Serial.println(sizeof(firstschedule));
		delay(1000);
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
		Serial.println("Connection Successful!");
		client.println("First Communication!");
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
	return ptr_Bytes_of_Data_In;
} //END receive_Data_From_Server

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
	Serial.println("");
	Serial.println("Received Data Stats:");
	Serial.print("Data ID: ");
	Serial.println(Data_ID);
	Serial.print("Data Start: ");
	Serial.println(Data_Start_Point);
	Serial.print("Data End: ");
	Serial.println(Data_End_Point);
	Serial.print("Number of Bytes in Payload: ");
	Serial.println(Num_Bytes_in_Payload);
	Serial.println("Data Payload:");
	Serial.print("|");
	for (int i = 0; i < Num_Bytes_in_Payload; i++) {
		Serial.print(Data_Payload[i]);
		Serial.print("|");
	}
	Serial.println("\n");
	Serial.println("");

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
	else if (Data_ID == "|SI|") //If Scheduling Information was Received
	{
		process_received_schedule(Data_Payload, Num_Bytes_in_Payload);
	}//END else if (Data_ID == "|SI|")
	else if (Data_ID == "|TI|") //If Time Information was Received
	{
		process_received_time(Data_Payload, Num_Bytes_in_Payload);
	}//END else if (Data_ID == "|TI|")
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
	if (Command == "Send")
		return Send_Data_To_Server;
	else if (Command == "Receive")
		return Receive_Data_From_Server;
	else if (Command == "RunSched") {
		Serial.println("Server says to Run Schedule");
		return No_Command;
	}
	else if (Command == "OFF") {
		Serial.println("Server says to Turn OFF Major Appliance");
		return No_Command;
	}
	else if (Command == "ON") {
		Serial.println("Server says to Turn ON Major Appliance");
		return No_Command;
	}
	else
		return No_Command;
}
//|SI| Scheduling Processing
void process_received_schedule(byte *Data_Payload, int &Num_Bytes_in_Payload)
{
	Serial.println("Processing a Schedule...");
	scheduling_information firstschedule;
	rebuild_received_data(Data_Payload, Num_Bytes_in_Payload, firstschedule);
	Serial.println(firstschedule.ID);

}
//|TI| Time Processing
void process_received_time(byte *Data_Payload, int &Num_Bytes_in_Payload)
{
	Serial.println("Processing a Time...");
}

//Data Send Processing Subroutines
void Send_New_Data_to_Server(void)
{
	bool New_Data_to_Send = false;
	//Check if there is data to Send and Set New_Data Accordingly
	if (New_Data_to_Send)
	{
		//Send New Data
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
	Serial.print(temp.AsFloat());
	Serial.println("C");
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