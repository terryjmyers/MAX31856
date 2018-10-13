/*
MAX31856 helper functions
GitHub.com/TerryJMyers
10/13/2018

Note that these functions were developed for a project where non-blocking code was implemented
I used the DRDY pin to trigger an ISR which set a read flag which I was scanning for, then I would
trigger MAX31856ReadRegisters().

Please see GitHub.com/TerryJMyers/ESPReflowController for example project

Usage:
	Simple:
		MAX31856ReadRegisters(); //Uses the struct MAX31856 defined here and reads in all registers
		MAX31856Calculate(); //Uses the registers in MAX31856.REG.* to calculate all of the doubles
		Serial.println(MAX31856.LTCT);

	Advanced (almost code):
		MAX31856_REG_Struct M; //Main program configuration struct saved to flash memory

		Setup {
			M.REG.CR0.CMODE = true;		//Set Auto conversion,
			M.REG.CR0.OCFAULT0 = false;	//Open Circuit detection
			M.REG.CR0.OCFAULT1 = true;
			M.REG.CR1.AVGSEL = 4;		//16 samples averaged
			MAX31856WriteRegisters(&M); //Write configuration down to MAX31856
		}

		Loop() {		
			MAX31856ReadRegisters(); //Update this files MAX31856 register
			String s;
			s += F("MAX31856 Register Mapping:\r\n");
			MAX31856GetREGMapString(s);
			MAX31856GetREGMapStringCR0(s);
			MAX31856GetREGMapStringCR1(s);
			MAX31856GetREGMapStringFAULT(s);
			MAX31856GetREGMapStringTemp(s);
			MAX31856GetREGMapStringFAULT(s);
			Serial.println(s;
			delay(1000);
		}


*/

										
static SPISettings MAX31856_SPISettings = SPISettings(5000000, MSBFIRST, SPI_MODE1); //CPOL = 0, CHPA = 1
#define TABLENGTH 8
struct MAX31856_REG_MAP_Struct {

	union {
		struct {
			byte Hz50_60 : 1;
			byte FAULTCLR : 1;
			byte FAULT : 1;
			byte CJ : 1;
			byte OCFAULT0 : 1;
			byte OCFAULT1 : 1;
			byte ONESHOT : 1;
			byte CMODE : 1;
		};
		byte WORD;
	} CR0;	//Address 0x00.  MAX31856.DATA[0]
	//------------------------
	union {
		struct {
			byte TCTYPE : 4;
			byte AVGSEL : 3;
			byte Reserved7 : 1;
		};
		byte WORD;
	} CR1;	//Address 0x01.  MAX31856.DATA[1]
	//------------------------
	union {
		struct {
			byte OPEN_FAULT_MASK : 1;
			byte OV_UV_FAULT_MASK : 1;
			byte TC_LOW_FAULT_MASK : 1;
			byte TC_HIGH_FAULT_MASK : 1;
			byte CJ_LOW_FAULT_MASK : 1;
			byte CJ_HIGH_FAULT_MASK : 1;
			byte Reserved6 : 1;
			byte Reserved7 : 1;
		};
		byte WORD;
	} MASK;	//Address 0x02.
	//------------------------
	int8_t CJHF;	//Address 0x03.
	//------------------------
	int8_t CJLF;	//Address 0x04.
	//------------------------
	union {
		struct {
			byte L;	//Address 0x06.
			byte H;	//Address 0x05.
		};
		int16_t LTHFT; //Divide by 16 or multiply by 0.0625;
		uint8_t WORD[2];
	} LTHFT;
	//------------------------
	union {
		struct {
			byte L;	//Address 0x08.
			byte H;	//Address 0x07.
		};
		int16_t LTLFT; //Divide by 16 or multiply by 0.0625;
		byte WORD[2];
	} LTLFT; //word 0x07-0x08
	//------------------------
	int8_t CJTO;	//Address 0x09.	 Divide by 16 or multiply by 0.0625;
	//------------------------
	union {
		struct {
			byte L;	//Address 0x0b. 
			byte H;	//Address 0x0a.
		};
		int16_t CJT; //Divide by 256 or multiply by 0.00390625;
		byte WORD[2];
	} CJT;
	//------------------------
	union {
		struct {
			byte H;		//Address 0x0c.
			byte M;		//Address 0x0d.
			byte L;		//Address 0x0e. 		
		};
		byte WORD[3];
	} LTC;
	//------------------------
	union {
		struct {
			byte OPEN : 1;
			byte OVUV : 1;
			byte TCLOW : 1;
			byte TCHIGH : 1;
			byte CJLOW : 1;
			byte CJHIGH : 1;
			byte TC_Range : 1;
			byte CJ_Range : 1;
		};
		byte WORD;
	} SR;	//Address 0x0f.
};
struct MAX31856_REG_Struct {
	struct MAX31856_REG_MAP_Struct REG;
	double CJHF;
	double CJLF;
	double LTHFT;
	double LTLFT;
	double CJTO;
	double CJT;
	double LTCT;
};

//Typically used to save a local copy of the running configuration in MAX31856.
//Recomend an additional MAX31856_REG_Struct in the main program that is saved to flash memory
MAX31856_REG_Struct MAX31856; 

String StringIndent(String s, uint8_t NumOfTabs) { //add tabs to formet things better		
	uint8_t StringLength = s.length();
	//Serial.print("StringLength "); Serial.println(StringLength);
	uint8_t ColumnNumber = NumOfTabs * TABLENGTH;
	//Serial.print("ColumnNumber "); Serial.println(ColumnNumber);
	uint8_t Difference = ColumnNumber - StringLength;
	//Serial.print("Difference "); Serial.println(Difference);
	uint8_t NumOfTabsToAdd = Difference / TABLENGTH;
	//Serial.print("NumOfTabsToAdd "); Serial.println(NumOfTabsToAdd);
	if (StringLength % TABLENGTH != 0) NumOfTabsToAdd++;
	//Serial.print("NumOfTabsToAdd2 "); Serial.println(NumOfTabsToAdd);

	for (int i = 0; i < NumOfTabsToAdd; i++) {
		s += F("\t");
	}
	return s;
	//Serial.println(s);

} //===============================================================================================
String ByteToBinary(byte b) {
	String s;
	for (int i = 7; i >= 0; i--) {
		s += String(bitRead(b, i));
	}
	return s;
} //===============================================================================================
String ByteToBinaryAndHex(uint8_t b) {
	String s; s.reserve(13);
	String hex = String(b, HEX);
	s = ByteToBinary(b); 
	s += F(" 0x"); 
	if (b <= 15) s += F("0");
	s += hex;
	return s;
} //===============================================================================================

/*
Pass in a MAX31856_REG_Struct and this routine will calculate all of the doubles in the root of MAX31856 from the REG values
Typical use: 
	MAX31856ReadRegisters(); //Uses the struct MAX31856 defined here and reads in all registers
	MAX31856Calculate(); //Uses the registers in MAX31856.REG.* to calculate all of the doubles
*/
void MAX31856Calculate(struct MAX31856_REG_Struct * M = &MAX31856) {
	M->CJHF = double(M->REG.CJHF);
	M->CJLF = double(M->REG.CJLF);
	M->LTHFT = double(M->REG.LTHFT.LTHFT) * 0.0625;
	M->LTLFT = double(M->REG.LTLFT.LTLFT) * 0.0625;
	M->CJTO = double(M->REG.CJTO) * 0.0625;
	M->CJT = double(M->REG.CJT.CJT) * 0.00390625;
	int32_t temp = M->REG.LTC.H << 16 | M->REG.LTC.M << 8 | M->REG.LTC.L;
	if (temp & 0x800000) temp |= 0xFF000000;  // fix sign
	temp >>= 5;  // bottom 5 bits are unused
	M->LTCT = double(temp) * 0.0078125;
} //===============================================================================================

/*
Get a String that can be printed out to UART or telnet to quickly display the registers in a human readable format
Typical Use:
	MAX31856ReadRegisters();
	String s;
	MAX31856GetREGMapString(s);
	Serial.print(s);

*/
void MAX31856GetREGMapString(String &s, struct MAX31856_REG_Struct * M = &MAX31856) {
	s += F("Register Memory Map:\r\n");
	s += F("\r\n	CR0:	"); s += ByteToBinaryAndHex(M->REG.CR0.WORD);
	s += F("\r\n	CR1:	"); s += ByteToBinaryAndHex(M->REG.CR1.WORD);
	s += F("\r\n	MASK:	"); s += ByteToBinaryAndHex(M->REG.MASK.WORD);
	s += F("\r\n	CJHF:	"); s += ByteToBinaryAndHex((uint8_t)M->REG.CJHF);
	s += F("\r\n	CJLF:	"); s += ByteToBinaryAndHex((uint8_t)M->REG.CJLF);
	s += F("\r\n	LTHFTH:	"); s += ByteToBinaryAndHex(M->REG.LTHFT.H);
	s += F("\r\n	LTHFTL:	"); s += ByteToBinaryAndHex(M->REG.LTHFT.L);
	s += F("\r\n	LTLFTH:	"); s += ByteToBinaryAndHex(M->REG.LTLFT.H);
	s += F("\r\n	LTLFTL:	"); s += ByteToBinaryAndHex(M->REG.LTLFT.L);
	s += F("\r\n	CJTO:	"); s += ByteToBinaryAndHex((uint8_t)M->REG.CJTO);
	s += F("\r\n	CJTH:	"); s += ByteToBinaryAndHex(M->REG.CJT.H);
	s += F("\r\n	CJTL:	"); s += ByteToBinaryAndHex(M->REG.CJT.L);
	s += F("\r\n	LTCBH:	"); s += ByteToBinaryAndHex(M->REG.LTC.H);
	s += F("\r\n	LTCBM:	"); s += ByteToBinaryAndHex(M->REG.LTC.M);
	s += F("\r\n	LTCBL:	"); s += ByteToBinaryAndHex(M->REG.LTC.L);
	s += F("\r\n	SR:	"); s += ByteToBinaryAndHex(M->REG.SR.WORD);
	s += F("\r\n");
} //===============================================================================================

/*
Get a String table that can be printed out to UART or telnet to quickly display Config Register 0 in a human readable format
Typical Use:
	MAX31856ReadRegisters(); 
	String s;
	MAX31856GetREGMapStringCR0(s);
	Serial.print(s);
*/
void MAX31856GetREGMapStringCR0(String &s, struct MAX31856_REG_Struct * M = &MAX31856) {
	s += F("CR0 (Configuration Register)\r\n");
	s += StringIndent(F("	MODE"), 2);			(MAX31856.REG.CR0.CMODE) ? s += F("Continuous Conversion Mode") : s += F("Normally Off (One Shot)	*Default"); s += F("\r\n");
	s += StringIndent(F("	1SHOT"), 2);		
		if (MAX31856.REG.CR0.CMODE) {//if continuous conversion this does nothing
			s += F("N/A: No effect in Continuous Conversion Mode");
		}
		else {
			(MAX31856.REG.CR0.ONESHOT) ? s += F("Conversion ready or active") : s += F("No conversion active	*Default");
			s += F("\r\n");
		}	
	s += StringIndent(F("	OCFAULT"), 2);
		if (MAX31856.REG.CR0.OCFAULT0 ==false && MAX31856.REG.CR0.OCFAULT1 == false) {
			s += F("Open Circuit Detection DISABLED");
		}
		else if (MAX31856.REG.CR0.OCFAULT0 == true && MAX31856.REG.CR0.OCFAULT1 == false) {	
			s += F("Open Circuit Detection ENABLED. Fault Detection Time ~");  (MAX31856.REG.CR0.CJ) ? s += F("13.3ms-15ms") : s += F("40ms-44ms");		
		}
		else if (MAX31856.REG.CR0.OCFAULT0 == false && MAX31856.REG.CR0.OCFAULT1 == true) {
			s += F("Open Circuit Detection ENABLED. Fault Detection Time ~");  (MAX31856.REG.CR0.CJ) ? s += F("33.4ms-37ms") : s += F("60ms-66ms");
		}
		else if (MAX31856.REG.CR0.OCFAULT0 == true && MAX31856.REG.CR0.OCFAULT1 == true) {
			s += F("Open Circuit Detection ENABLED. Fault Detection Time ~");  (MAX31856.REG.CR0.CJ) ? s += F("113.4ms-125ms") : s += F("140ms-154ms");
		}
		s += F("\r\n");
	s += StringIndent(F("	CJ"), 2);			(MAX31856.REG.CR0.CJ) ? s += F("CJ DISABLED") : s += F("CJ ENABLED	*Default"); s += F("\r\n");
	s += StringIndent(F("	FAULT"), 2);		(MAX31856.REG.CR0.FAULT) ? s += F("FAULT pin and fault bits are latched and requires a FAULTCLR cmd") : s += F("FAULT pin and fault bits are automatically Reset	*Default"); s += F("\r\n");
	s += StringIndent(F("	FAULTCLR"), 2);		(MAX31856.REG.CR0.FAULTCLR) ? s += F("Set to clear faults") : s += F("N/A: No effect in FAULT mode 0	*Default"); s += F("\r\n");
	s += StringIndent(F("	50/60Hz"), 2);		(MAX31856.REG.CR0.Hz50_60) ? s += F("50Hz notch filter") : s += F("60hz notch filter	*Default"); s += F("\r\n");

} //===============================================================================================

/*
Get a String table that can be printed out to UART or telnet to quickly display Config Register 1 in a human readable format
Typical Use:
	MAX31856ReadRegisters();
	String s;
	MAX31856GetREGMapStringCR1(s);
	Serial.print(s);
*/
void MAX31856GetREGMapStringCR1(String &s, struct MAX31856_REG_Struct * M = &MAX31856) {

	s += F("CR1 (Configuration Register)\r\n");
	uint8_t AveragedSamples;
	if (M->REG.CR1.AVGSEL == 0) {
		s += F("	AVGSEL: Sample Averaging DISABLED\r\n"); AveragedSamples = 1;
	} else if (M->REG.CR1.AVGSEL == 1) {
		s += F("	AVGSEL: 2 Samples Averaged\r\n"); AveragedSamples = 2;
	}
	else if (M->REG.CR1.AVGSEL == 2) {
		s += F("	AVGSEL: 4 Samples Averaged\r\n"); AveragedSamples = 4;
	}
	else if (M->REG.CR1.AVGSEL == 3) {
		s += F("	AVGSEL: 8 Samples Averaged\r\n"); AveragedSamples = 8;
	}
	else if (M->REG.CR1.AVGSEL >= 4 && M->REG.CR1.AVGSEL <= 7) {
		s += F("	AVGSEL: 16 Sample Averaging Enabled\r\n");	AveragedSamples = 16;
	}
	else {
		s += F("	AVGSEL: ERROR detecing sample averaging status\r\n"); AveragedSamples = 0;
	}

	float Tconv;
	float TconvMax;
	if (M->REG.CR0.CMODE) { //automatic conversion
		if (M->REG.CR0.Hz50_60) { //50hz
			Tconv = 98.0		+ float(AveragedSamples - 1) * 20.0;
			TconvMax = 110.0	+ float(AveragedSamples - 1) * 20.0;
		}
		else {//60hz
			Tconv = 82.0		+ float(AveragedSamples - 1) * 16.67;
			TconvMax = 90.0		+ float(AveragedSamples - 1) * 16.67;
		}
	}
	else {//one shot conversion
		if (M->REG.CR0.Hz50_60) { //50hz
			Tconv = 169.0		+ float(AveragedSamples - 1) * 40.0;
			TconvMax = 185.0	+ float(AveragedSamples - 1) * 40.0;
		}
		else {//60hz
			Tconv = 143.0		+ float(AveragedSamples - 1) * 33.3;
			TconvMax = 155.0	+ float(AveragedSamples - 1) * 33.3;
		}
	}
	s += F("	Estimated Conversion Time:	"); s += String(Tconv); s += F("ms typical, ");  s += String(TconvMax); s += F("ms Maximum\r\n");
	s += F("	TC Type:	");
	if (M->REG.CR1.TCTYPE == 0)	s += F("B");
	if (M->REG.CR1.TCTYPE == 1)	s += F("E");
	if (M->REG.CR1.TCTYPE == 2)	s += F("J");
	if (M->REG.CR1.TCTYPE == 3)	s += F("K");
	if (M->REG.CR1.TCTYPE == 4)	s += F("N");
	if (M->REG.CR1.TCTYPE == 5)	s += F("R");
	if (M->REG.CR1.TCTYPE == 6)	s += F("S");
	if (M->REG.CR1.TCTYPE == 7)	s += F("T");
	if (M->REG.CR1.TCTYPE >= 8 && M->REG.CR1.TCTYPE <= 11)	s += F("Voltage 8 X gain");
	if (M->REG.CR1.TCTYPE >= 12 && M->REG.CR1.TCTYPE <= 15)	s += F("Voltage 32 X gain");
	s += F("\r\n");
} //===============================================================================================

/*
Get a String table that can be printed out to UART or telnet to quickly display the Fault Mask Register in a human readable format
Typical Use:
	MAX31856ReadRegisters();
	String s;
	MAX31856GetREGMapStringMASK(s);
	Serial.print(s);
*/
void MAX31856GetREGMapStringMASK(String &s, struct MAX31856_REG_Struct * M = &MAX31856) {
	s += F("MASK (Fault Mask Register)");
	s += F("\r\n	(bit5)CJ High FAULT Mask =	"); s += M->REG.MASK.CJ_HIGH_FAULT_MASK;
	s += F("\r\n	(bit4)CJ Low FAULT Mask =	"); s += M->REG.MASK.CJ_LOW_FAULT_MASK; 
	s += F("\r\n	(bit3)TC High FAULT Mask =	"); s += M->REG.MASK.TC_HIGH_FAULT_MASK;
	s += F("\r\n	(bit2)TC Low FAULT Mask =	"); s += M->REG.MASK.TC_LOW_FAULT_MASK; 
	s += F("\r\n	(bit1)OV/UV FAULT Mask =	"); s += M->REG.MASK.OV_UV_FAULT_MASK;
	s += F("\r\n	(bit0)Open FAULT Mask =	"); s += M->REG.MASK.OPEN_FAULT_MASK;
	s += F("\r\n");
} //===============================================================================================

/*
Get a String table that can be printed out to UART or telnet to quickly display the Fault Register in a human readable format
Typical Use:
	MAX31856ReadRegisters();
	String s;
	MAX31856GetREGMapStringFAULT(s);
	Serial.print(s);
*/
void MAX31856GetREGMapStringFAULT(String &s, struct MAX31856_REG_Struct * M = &MAX31856) {
	s += F("Fault Status Register SR)\r\n");
	s += StringIndent(F("	CJ Range"), 2);		(M->REG.SR.CJ_Range) ? s += F("	ERROR: The Cold-Junction temperature is outside of the normal operating range.") : s += F("	OK: The Cold-Junction temperature is within the normal operating range (-55°C to +125°C for types E,J, K, N, and T; -50°C to + 125°C for types R and S; 0 to 125°C for type B).)"); s += F("\r\n");
	s += StringIndent(F("	TC Range"), 2);		(M->REG.SR.TC_Range) ? s += F("	ERROR: The Thermocouple Hot Junction temperature is outside of the normal operating range.") : s += F("	OK: The Thermocouple Hot Junction temperature is within the normal operating range"); s += F("\r\n");
	s += StringIndent(F("	CJHIGH"), 2);		(M->REG.SR.CJHIGH) ? s += F("	ERROR: The Thermocouple Hot Junction temperature is outside of the normal operating range.") : s += F("	OK: The Cold-Junction temperature is higher than the cold-junction temperature high threshold"); s += F("\r\n");
	s += StringIndent(F("	CJLOW"), 2);		(M->REG.SR.CJLOW) ? s += F("	ERROR: The Cold-Junction temperature is lower than the cold-junction temperature low threshold") : s += F("	OK: The Cold-Junction temperature is at or higher than the cold-junction temperature low threshold "); s += F("\r\n");
	s += StringIndent(F("	TCHIGH"), 2);		(M->REG.SR.TCHIGH) ? s += F("	ERROR: The Thermocouple Temperature is higher than the thermocouple temperature high threshold.") : s += F("	OK: The Thermocouple Temperature is higher than the thermocouple temperature high threshold "); s += F("\r\n");
	s += StringIndent(F("	TCLOW"), 2);		(M->REG.SR.TCLOW) ? s += F("	ERROR: Thermocouple temperature is lower than the thermocouple temperature low threshold. ") : s += F("	OK: Thermocouple temperature is at or higher than the thermocouple temperature low threshold "); s += F("\r\n");
	s += StringIndent(F("	OVUV"), 2);			(M->REG.SR.OVUV) ? s += F("	ERROR: The input voltage is negative or greater than VDD") : s += F("	OK: The input voltage is positive and less than VDD"); s += F("\r\n");
	s += StringIndent(F("	OPEN"), 2);			(M->REG.SR.OPEN) ? s += F("	ERROR: An open circuit such as broken thermocouple wires has been detected. ") : s += F("	OK: No open circuit or broken thermocouple wires are detected "); s += F("\r\n");
} //===============================================================================================

/*
Get a String table that can be printed out to UART or telnet to quickly display all of the other registers in a human readable format
Typical Use:
	MAX31856ReadRegisters(); //Uses the struct MAX31856 defined here and reads in all registers
	MAX31856Calculate(); //Uses the registers in MAX31856.REG.* to calculate all of the doubles
	String s;
	MAX31856GetREGMapStringTemp(s); //Uses the registers in MAX31856.REG.* to calculate all of the doubles
	Serial.print(s);
*/
void MAX31856GetREGMapStringTemp(String &s, struct MAX31856_REG_Struct * M = &MAX31856) {
	s += F("CJHF (Cold Junction High Fault Threshold): "); s += M->CJHF; s += F("C\r\n");
	s += F("CJLF (Cold Junction Low Fault Threshold): "); s += M->CJLF; s += F("C\r\n");
	s += F("LTHFTH + LTHFTL (Linearized Temperature High Fault Threshold MSB + LSB): ");   s += String(M->LTHFT, 3); s += F("C\r\n");
	s += F("LTLFTH + LTLFTL (Linearized Temperature Low Fault Threshold MSB + LSB): ");   s += String(M->LTLFT, 3); s += F("C\r\n");
	s += F("CJTO (Cold Junction Temperature Offset): "); s += String(M->CJTO, 3); s += F("C\r\n");
	s += F("CJTH + CJTL (Cold Junction Temperature): "); s += String(M->CJT, 3); s += F("C\r\n");
	s += F("LTCMH + LTCBM + LTCBL (Linearized TC Temperature): "); s += String(M->LTCT, 4); s += F("C\r\n");
} //===============================================================================================

/*
Write the Struct down to the MAX31856
Typical Use:
	MAX31856WriteRegisters();
	OR
	MAX31856WriteRegisters(&Saved_Configuration); // where Saved_Configuration is data type MAX31856_REG_Struct and perhaps saved to flash memory
*/
void MAX31856WriteRegisters(struct MAX31856_REG_Struct * M = &MAX31856) {	
	//Load up a buffer to send to MAX31856
	uint8_t BufferOut[13] = { 0 };
	BufferOut[0] = 0x80;  //Set the Address Word to the first address and set bit 7 to signify write
	BufferOut[1] = M->REG.CR0.WORD;
	BufferOut[2] = M->REG.CR1.WORD;
	BufferOut[3] = M->REG.MASK.WORD;
	BufferOut[4] = (uint8_t)M->REG.CJHF;
	BufferOut[5] = (uint8_t)M->REG.CJLF;
	BufferOut[6] = M->REG.LTHFT.H;
	BufferOut[7] = M->REG.LTHFT.L;
	BufferOut[8] = M->REG.LTLFT.H;
	BufferOut[9] = M->REG.LTLFT.L;
	BufferOut[10] = (uint8_t)M->REG.CJTO;
	BufferOut[11] = M->REG.CJT.H;
	BufferOut[12] = M->REG.CJT.L;
	//The last 4 registers are read only no need to clock these

	SPI.beginTransaction(MAX31856_SPISettings);	
	SPI.transferBytes(&BufferOut[0], NULL, sizeof(BufferOut)); //Write the entire register image. Note: SPI.transferBytes(MOSI, MISO, SIZE)
	SPI.endTransaction();
}//===============================================================================================

/*
Read in the registers of the MAX31856, and updating the passed in struct (or the root one contained in this file)
Typical Use:
	MAX31856ReadRegisters(); //Updates MAX31856 in this file
*/
void MAX31856ReadRegisters(struct MAX31856_REG_Struct * M = &MAX31856) {

	uint8_t BufferIn[17] = { 0 };
	uint8_t BufferOut[sizeof(BufferIn)] = { 0 };
	SPI.beginTransaction(MAX31856_SPISettings);	
	SPI.transferBytes(&BufferOut[0], &BufferIn[0], sizeof(BufferIn)); //Read up the entire configuration. Note: SPI.transferBytes(MOSI, MISO, SIZE)
	SPI.endTransaction();

						//BufferIn[0] is whatever was clocked in during the address phase so is useless
	M->REG.CR0.WORD		= BufferIn[1];
	M->REG.CR1.WORD		= BufferIn[2];
	M->REG.MASK.WORD	= BufferIn[3];
	M->REG.CJHF			= (int8_t)BufferIn[4];
	M->REG.CJLF			= (int8_t)BufferIn[5];
	M->REG.LTHFT.H		= BufferIn[6];
	M->REG.LTHFT.L		= BufferIn[7];
	M->REG.LTLFT.H		= BufferIn[8];
	M->REG.LTLFT.L		= BufferIn[9];
	M->REG.CJTO			= (int8_t)BufferIn[10];
	M->REG.CJT.H		= BufferIn[11];
	M->REG.CJT.L		= BufferIn[12];
	M->REG.LTC.H		= BufferIn[13];
	M->REG.LTC.M		= BufferIn[14];
	M->REG.LTC.L		= BufferIn[15];
	M->REG.SR.WORD		= BufferIn[16];
}//===============================================================================================

/*
Set the MAX31856 to the factory default values then writing the structure down to the MAX31856
*/
void MAX31856SetFactoryDefault(struct MAX31856_REG_Struct * M = &MAX31856) {

	M->REG.CR0.WORD =	0x00;	//CR0
	M->REG.CR1.WORD =	0x03;	//CR1
	M->REG.MASK.WORD =	0xff;	//MASK
	M->REG.CJHF =		0x7f;	//CJHF
	M->REG.CJLF =		0xc0;	//CJLF
	M->REG.LTHFT.H =	0x7f;	//LTHFTH
	M->REG.LTHFT.L =	0xff;	//LTHFTL
	M->REG.LTLFT.H =	0x80;	//LTLFTH
	M->REG.LTLFT.L =	0x00;	//LTLFTL
	M->REG.CJTO =		0x00;	//CJTO
	M->REG.CJT.H =		0x00;	//CJTH
	M->REG.CJT.L =		0x00;	//CJTL
	M->REG.LTC.H =		0x00;	//LTCH
	M->REG.LTC.M =		0x00;	//LTCM
	M->REG.LTC.L =		0x00;	//LTCL
	M->REG.SR.WORD =	0x00;	//SR

	MAX31856WriteRegisters(M);
} //===============================================================================================
