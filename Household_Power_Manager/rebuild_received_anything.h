#pragma once
#include <Arduino.h> 

//General Rebuiling function prototype
template <class T> void rebuild_received_data(byte *Data_Payload, const int &Num_Bytes_in_Payload, T& rebuilt_variable)
{
	byte *ptr_to_rebuilt_variable_bytes = (byte*)(void*)(&rebuilt_variable);
	for (int i = 0; i < Num_Bytes_in_Payload; i++) {
		ptr_to_rebuilt_variable_bytes[i] = Data_Payload[i];
	}
}