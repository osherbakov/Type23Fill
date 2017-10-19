#include <CRC.h>

//--------------------------------------------------------------
// Pin assignments
//--------------------------------------------------------------
const byte   PIN_B = 2;
const byte   PIN_C = 3;
const byte   PIN_D = 4;
const byte   PIN_E = 5;
const byte   PIN_F = 6;
//--------------------------------------------------------------

//--------------------------------------------------------------
// Delays for the appropriate timings in millisecs
//--------------------------------------------------------------
const int   tM = 50;    
const int   tA = 50;
const int   tE = 100;    // REQ -> Fill
const int   tZ = 500;    // End -> New Fill

//--------------------------------------------------------------
// Delays for the appropriate timings in usecs
//--------------------------------------------------------------
const int   tJ = 25;
const int   tK1 = 425;
const int   tK2 = 425;
const int   tL = 50;


// The proper delays to generate 8kHz
//Correction factor for the timing
int tCorr = clockCyclesToMicroseconds(50); // a digitalWrite is about 50 cycles
int  _bitPeriod = 1000000 / 2000;
int tT = _bitPeriod/2; //  - tCorr; 


//--------------------------------------------------------------
// Timeouts in ms
//--------------------------------------------------------------
const int   tB = 200;      // Query -> Response from Radio
const int   tD = 1000;     // PIN_C Pulse Width
const int   tG = 1000;     // PIN_B Pulse Wodth
const int   tH = 1000;     // BAD HIGH - > REQ LOW

const int   tF = 3000;     // End of fill - > response
const long  tC = 5*60*1000L;  // 5 minutes - > until REQ


// Implement majority logic on reading
byte GetPinLevel(byte PinNumber)
{
   byte r1 = digitalRead(PinNumber);
   byte r2 = digitalRead(PinNumber);
   byte r3 = digitalRead(PinNumber);
   
   return ( (r1 == r2) || (r1 == r3) ? r1 : r2) ;
}

typedef enum FILL_TYPE{
  TYPE1 = 1, 
  TYPE2 = 2, 
  TYPE3 = 3, 
} FILL_TYPE;

typedef enum RESULT{
  ERR = -2,
  TIMEOUT = -1,
  OK = 0
} RESULT;

static FILL_TYPE   gFillType = TYPE3;

static byte Type1_PIN_B = HIGH;
static byte Type1_PIN_E = HIGH;

void  SetFillType(byte newType)
{
  Serial.print("Set Fill Type = "); Serial.println(newType, HEX);
  gFillType = (FILL_TYPE) newType;
}

void SendQuery(byte Data)
{
  // Set up pins mode and levels
  pinMode(PIN_B, OUTPUT);    // make a pin active
  pinMode(PIN_E, OUTPUT);    // make a pin active
  digitalWrite(PIN_E, HIGH);  // Set clock to High
  delayMicroseconds(tJ);      // Setup time for clock high

  // Output the first data bit of the request
  digitalWrite(PIN_B, (Data & 0x01) ? LOW : HIGH);  // Output data bit
  Data >>= 1;
  delayMicroseconds(tK1);    // Satisfy Setup time tK1

    // Pulse the clock
  digitalWrite(PIN_E, LOW);  // Drop Clock to LOW 
  delayMicroseconds(tT);     // Hold Clock in LOW for tT
  digitalWrite(PIN_E, HIGH);  // Bring Clock to HIGH

    // Output the rest of the data
  for(byte i = 0; i < 7; i++)
  {
    // Send next data bit    
    digitalWrite(PIN_B, (Data & 0x01) ? LOW : HIGH);  // Output data bit
    Data >>= 1;
    delayMicroseconds(tT);     // Hold Clock in HIGH for tT (setup time)

    digitalWrite(PIN_E, LOW);  // Drop Clock to LOW 
    delayMicroseconds(tT);     // Hold Clock in LOW for tT
    digitalWrite(PIN_E, HIGH);  // Bring Clock to HIGH
  }
  delayMicroseconds(tK2 - tT);  // Wait there
  
  // Release PIN_B and PIN_E - the Radio will drive it
  pinMode(PIN_B, INPUT_PULLUP);    // Tristate the pin
  pinMode(PIN_E, INPUT_PULLUP);    // Tristate the pin
  digitalWrite(PIN_B, HIGH);  // Turn on 20 K Pullup
  digitalWrite(PIN_E, HIGH);  // Turn on 20 K Pullup
}

void SendByte(byte Data)
{
  pinMode(PIN_D, OUTPUT);    // make a pin active
  pinMode(PIN_E, OUTPUT);    // make a pin active
  digitalWrite(PIN_E, HIGH);  // Bring Clock to HIGH
  for(byte i = 0; i < 8; i++)
  {
    digitalWrite(PIN_D, (Data & 0x01) ? LOW : HIGH);  // Output data bit
    Data >>= 1;
    delayMicroseconds(tT);    // Satisfy Setup time tT
    digitalWrite(PIN_E, LOW);  // Drop Clock to LOW 
    delayMicroseconds(tT);     // Hold Clock in LOW for tT
    digitalWrite(PIN_E, HIGH);  // Bring Clock to HIGH
  }
}

void SendCell(byte *p_cell)
{
  delay(tE);     // Delay before sending first bit
  for (byte i = 0; i < 16; i++)
  {
    SendByte(*p_cell++);
  }
}

signed char GetEquipmentType()
{
  digitalWrite(PIN_E, HIGH);      // Turn_on the pullup register
  pinMode(PIN_E, INPUT_PULLUP);   // make a pin input with pullup
  byte  PreviousState = LOW;
  unsigned long Timeout = millis() + tB;
  byte i = 0;

  while(millis() <= Timeout)
  {
    byte NewState = digitalRead(PIN_E);
    if( PreviousState != NewState  )
    {
      if( NewState == LOW )
      {
        i++;
        if( i >= 40)
        {
          Serial.print("GetEquipmentType = "); Serial.println(GetPinLevel(PIN_B), HEX);
          return (GetPinLevel(PIN_B) == LOW) ? TYPE3 : TYPE2;
        }
      }
      PreviousState = NewState;
    }
  }
  return TIMEOUT;
}

//
//  Wait for the first Fill request functions
// For the First Fill request the timeout is 5 minutes!!!!!!!!!!!!!!!!
//  For Type 2 and 3 Fills - the pin_B should be set as input to check the if fill was OK.
//    But we don't check for the previous Fill
// Additionally, for the Type1 fill request the PIN_C may stay LOW pretty long
//  so we will timeout with good code if PIN_C stays low
signed char WaitFirstReqType1()
{
  byte  PreviousState = HIGH;
  signed char Result = TIMEOUT;

  pinMode(PIN_C, INPUT_PULLUP);    // make pin input
  digitalWrite(PIN_C, HIGH);  // Set pullup 
  delayMicroseconds(tK1);    // Satisfy Setup time tK1

  unsigned long Timeout = millis() + tC;
  while ( millis() <= Timeout )
  {
    byte NewState = GetPinLevel(PIN_C);

    if(PreviousState != NewState)
    {
      Serial.print("WaitFirstReqType1 C = "); Serial.println(NewState, HEX);

      PreviousState = NewState;
      if( NewState == HIGH ) {
        return OK;
      }else {
        Timeout = millis() + tD;
        Result = OK;
      }
    }
  }
  return Result;
}

signed char WaitFirstReqType23()
{
  byte  PreviousState = HIGH;

  pinMode(PIN_C, INPUT_PULLUP);    // make pin input
  pinMode(PIN_B, INPUT_PULLUP);    // make pin input
  digitalWrite(PIN_B, HIGH);  // Set pullup 
  digitalWrite(PIN_C, HIGH);  // Set pullup 
  delayMicroseconds(tK1);    // Satisfy Setup time tK1

  unsigned long Timeout = millis() + tC;
  while ( millis() <= Timeout )
  {
    byte NewState = GetPinLevel(PIN_C);

    if(PreviousState != NewState)
    {
      Serial.print("WaitFirstReqType23 C = ");   Serial.println(NewState, HEX);

      PreviousState = NewState;
      if( NewState == HIGH ) {
        return OK;
      }
    }
  }
  return TIMEOUT;
}

signed char  WaitFirstReq()
{
  return (gFillType == TYPE1) ? WaitFirstReqType1() : WaitFirstReqType23();
}


//
//  Wait for the normal Fill request functions
//  For Type 2 and 3 Fills - the pin_B is checked to see if the fill was OK.
//
signed char  WaitReqType1()
{

  byte  PreviousState_C = HIGH;
  signed char Result = TIMEOUT;
  
  unsigned long Timeout = millis() + tF;

  pinMode(PIN_C, INPUT_PULLUP);    // make pin input
  digitalWrite(PIN_C, HIGH);  // Set pullup 
  delayMicroseconds(tK1);    // Satisfy Setup time tK1
  
  while(millis() <= Timeout)  
  {
    byte NewState_C = GetPinLevel(PIN_C);

    if(PreviousState_C != NewState_C)  
    {
      Serial.print("WaitReqType1 C = "); Serial.println(NewState_C, HEX);
      
      PreviousState_C = NewState_C;
      if( NewState_C == HIGH ) {
        return OK;
      }else {
        Timeout = millis() + tD;
        Result = OK;
      }
    }
  }
  return Result;
}

signed char  WaitReqType23()
{

  byte  PreviousState_B = HIGH;
  byte  PreviousState_C = HIGH;
  signed char Result = OK;
  unsigned long Timeout = millis() + tF;

  pinMode(PIN_C, INPUT_PULLUP);    // make pin input
  pinMode(PIN_B, INPUT_PULLUP);    // make pin input
  digitalWrite(PIN_C, HIGH);  // Set pullup 
  digitalWrite(PIN_B, HIGH);  // Set pullup 
  delayMicroseconds(tK1);    // Satisfy Setup time tK1
  
  while(millis() <= Timeout)  
  {
    byte NewState_C = GetPinLevel(PIN_C);
    byte NewState_B = GetPinLevel(PIN_B);

    if(PreviousState_C != NewState_C)  
    {
      Serial.print("WaitReqType23 C = ");   Serial.println(NewState_C, HEX);
      
      PreviousState_C = NewState_C;
      if( NewState_C == HIGH ) 
      {
        return Result;
      }
    }
    if(PreviousState_B != NewState_B)
    {
      if( NewState_B == LOW ) 
      {
        Serial.print("WaitReqType23 B = ");   Serial.println(NewState_B, HEX);
        Result = ERR;  // Bad CRC
      }
      PreviousState_B = NewState_B;
    }
  }
  return TIMEOUT;
}

signed char WaitReq()
{
  return (gFillType == TYPE1) ? WaitReqType1() : WaitReqType23();
}


//
//  Wait for the Last Fill request functions
//  For Type 2 and 3 Fills - the pin_B is checked to see if the fill was OK.
// The last request is special - we return JUST AS PIN_C goes LOW...
//

signed char WaitLastReqType1()
{
  byte  PreviousState_C = HIGH;
  unsigned long Timeout = millis() + tF;

  pinMode(PIN_C, INPUT_PULLUP);    // make pin input
  digitalWrite(PIN_C, HIGH);  // Set pullup 
  delayMicroseconds(tK1);    // Satisfy Setup time tK1

  while(millis() <= Timeout)  
  {
    byte NewState_C = GetPinLevel(PIN_C);

    if(PreviousState_C != NewState_C)  
    {
      Serial.print("WaitLastReqType1 C = ");   Serial.println(NewState_C, HEX);

      if( NewState_C == LOW ) 
      {
          return OK;
      }
    }
  }
  return TIMEOUT;
}

signed char WaitLastReqType23()
{
  byte  PreviousState_C = HIGH;
  byte  PreviousState_B = HIGH;
  signed char Result = OK;
  unsigned long Timeout = millis() + tF;

  pinMode(PIN_C, INPUT_PULLUP);    // make pin input
  pinMode(PIN_B, INPUT_PULLUP);    // make pin input
  digitalWrite(PIN_C, HIGH);  // Set pullup 
  digitalWrite(PIN_B, HIGH);  // Set pullup 
  delayMicroseconds(tK1);    // Satisfy Setup time tK1

  while(millis() <= Timeout)  
  {
    byte NewState_C = GetPinLevel(PIN_C);
    byte NewState_B = GetPinLevel(PIN_B);

    if(PreviousState_C != NewState_C)  
    {
      Serial.print("WaitLastReqType23 C = ");   Serial.println(NewState_C, HEX);

      if( NewState_C == LOW ) 
      {
          return Result;
      }
    }
    if(PreviousState_B != NewState_B)
    {
      if( NewState_B == LOW ) 
      {
        Serial.print("WaitLastReqType23 B = ");   Serial.println(NewState_B, HEX);
        Result = ERR;  // Bad CRC
      }
      PreviousState_B = NewState_B;
    }
  }
  return TIMEOUT;
}

signed char WaitLastReq()
{
  return (gFillType == TYPE1) ? WaitLastReqType1() : WaitLastReqType23();
}


void StartHandshake()
{
  digitalWrite(PIN_B, HIGH);
  digitalWrite(PIN_C, HIGH);
  digitalWrite(PIN_D, HIGH);
  digitalWrite(PIN_E, HIGH);
  digitalWrite(PIN_F, HIGH);
  pinMode(PIN_B, INPUT_PULLUP);
  pinMode(PIN_C, INPUT_PULLUP);
  pinMode(PIN_D, OUTPUT);
  pinMode(PIN_E, OUTPUT);
  pinMode(PIN_F, OUTPUT);
  delay(200);

  // Drop PIN_D first
  digitalWrite(PIN_D, LOW);
  digitalWrite(PIN_F, LOW); // Drop PIN_F after delay
  delay(tA);                // Pin D pulse width
  // Bring PIN_D up again
  digitalWrite(PIN_D, HIGH);
}

void EndHandshake()
{
  digitalWrite(PIN_B, HIGH);
  digitalWrite(PIN_C, HIGH);
  digitalWrite(PIN_D, HIGH);
  digitalWrite(PIN_E, HIGH);
  digitalWrite(PIN_F, LOW);
  pinMode(PIN_B, INPUT_PULLUP);
  pinMode(PIN_C, INPUT_PULLUP);
  pinMode(PIN_D, OUTPUT);
  pinMode(PIN_E, OUTPUT);
  pinMode(PIN_F, OUTPUT);
}

void  EndFill()
{
  delayMicroseconds(tL);
  digitalWrite(PIN_F, HIGH);
  delay(tZ);
}


void AcquireBusType23()
{
  SetFillType(TYPE3);
  digitalWrite(PIN_B, HIGH);
  digitalWrite(PIN_C, HIGH);
  digitalWrite(PIN_D, HIGH);
  digitalWrite(PIN_E, HIGH);
  digitalWrite(PIN_F, HIGH);
  pinMode(PIN_B, INPUT_PULLUP);
  pinMode(PIN_C, INPUT_PULLUP);
  pinMode(PIN_D, OUTPUT);
  pinMode(PIN_E, OUTPUT);
  pinMode(PIN_F, OUTPUT);
  delayMicroseconds(tJ);
}

void AcquireBusType1()
{
  SetFillType(TYPE1);
  digitalWrite(PIN_B, Type1_PIN_B);
  digitalWrite(PIN_C, HIGH);
  digitalWrite(PIN_D, HIGH);
  digitalWrite(PIN_E, Type1_PIN_E);
  digitalWrite(PIN_F, HIGH);

  pinMode(PIN_B, OUTPUT);
  pinMode(PIN_C, INPUT_PULLUP);
  pinMode(PIN_D, OUTPUT);
  pinMode(PIN_E, OUTPUT);
  pinMode(PIN_F, OUTPUT);
  delayMicroseconds(tJ);
}


void ReleaseBus()
{
  digitalWrite(PIN_B, HIGH);
  digitalWrite(PIN_C, HIGH);
  digitalWrite(PIN_D, HIGH);
  digitalWrite(PIN_E, HIGH);
  digitalWrite(PIN_F, HIGH);
  delay(tB);
  pinMode(PIN_B, INPUT_PULLUP);
  pinMode(PIN_C, INPUT_PULLUP);
  pinMode(PIN_D, INPUT_PULLUP);
  pinMode(PIN_E, INPUT_PULLUP);
  pinMode(PIN_F, INPUT_PULLUP);
}


void PrintCell(byte *cell)
{
  CRC  calc_crc;
  unsigned char crc_res;
    
  Serial.print("Send Cell:");
  for(int i = 0; i < 15; i++)
  {
      Serial.print(" 0x");Serial.print(cell[i], HEX);
  }      
  Serial.print(" CRC: 0x");
  Serial.print(cell[15], HEX);
  calc_crc.ini();
  calc_crc.blk(cell, 15);
  crc_res = calc_crc.crc() & 0x00FF;
  Serial.print(" (TRK:0x");
  Serial.print(crc_res, HEX);
  Serial.print(" TEK:0x");
  Serial.print(crc_res ^ 0xFF, HEX);
  Serial.println(")");
}

signed char TestKeyCell(byte *cell)
{
  CRC  calc_crc;
  signed char Result = 0;

  calc_crc.ini();
  calc_crc.appnd(cell, 16);
  cell[15] ^= 0xFF;
  
  PrintCell(cell); 
  SendCell(cell);
  Result = (gFillType == TYPE1) ? WaitReqType1() : WaitReqType23();
  return Result;
}

signed char TestLastKeyCell(byte *cell)
{
  CRC  calc_crc;
  signed char Result = 0;

  calc_crc.ini();
  calc_crc.appnd(cell, 16);
  cell[15] ^= 0xFF;
  
  PrintCell(cell); 
  SendCell(cell);
  Result = (gFillType == TYPE1) ? WaitLastReqType1() : WaitLastReqType23();
  return Result;
}

signed char TestCell(byte *cell)
{
  CRC  calc_crc;
  signed char Result = OK;

  calc_crc.ini();
  calc_crc.appnd(cell, 16);
  
  for(int i = 0; i < 256; i++)
  {
    PrintCell(cell); 
    SendCell(cell);
    Result = (gFillType == TYPE1) ? WaitReqType1() : WaitReqType23();
    if( Result == 0)
    {
      // Good CRC
      return OK;
    }else if( Result == ERR)  {
      // Bad CRC - change it (increment)
      cell[15] = i;
      Serial.print(".");
    }else  {  
      // Timeout or other things
      Serial.println("Timeout");
      return Result;
    }
  }
  return Result;
}

signed char TestLastCell(byte *cell)
{
  CRC  calc_crc;
  signed char Result = OK;

  calc_crc.ini();
  calc_crc.appnd(cell, 16);
  
  for(int i = 0; i < 256; i++)
  {
    PrintCell(cell); 
    SendCell(cell);
    Result = (gFillType == TYPE1) ? WaitLastReqType1() : WaitLastReqType23();
    if( Result == 0) {
      // Good CRC
      return OK;
    }else if(Result == ERR) {
      // Bad CRC - change it (increment)
      cell[15] = i;
      Serial.print(".");
    }else{  
      // Timeout or other things
      Serial.println("Timeout");
      return Result;
    }
  }
  return Result;
}


signed char TestAnyCell(byte *cell)
{
  CRC  cacl_crc;
  signed char Result = OK;

  for(int i = 0; i < 256; i++)
  {
    PrintCell(cell); 
    SendCell(cell);
    Result = (gFillType == TYPE1) ? WaitReqType1() : WaitReqType23();
    if( Result == 0)
    {
      // Good CRC
      return OK;
    }else if( Result == ERR)  {
      // Bad CRC - change it (increment)
      cell[15] = i;
      Serial.print(".");
    }else  {  
      // Timeout or other things
      Serial.println("Timeout");
      return Result;
    }
  }
  return Result;
}

signed char TestLastAnyCell(byte *cell)
{
  CRC  cacl_crc;
  signed char Result = OK;

  for(int i = 0; i < 256; i++)
  {
    PrintCell(cell); 
    SendCell(cell);
    Result = (gFillType == TYPE1) ? WaitLastReqType1() : WaitLastReqType23();
    if( Result == 0) {
      // Good CRC
      return OK;
    }else if(Result == ERR) {
      // Bad CRC - change it (increment)
      cell[15] = i;
      Serial.print(".");
    }else{  
      // Timeout or other things
      Serial.println("Timeout");
      return Result;
    }
  }
  return Result;
}

// Tag values
const byte NO_FILL_TAG = 0x7D;
const byte COMSEC_TAG = 0x7D;
const byte COMSEC_DATA = 0x00;
const byte COLDSTART_TAG = 0x8C;



// QUERY bytes
const byte QUERY_TYPE2  = 0x02;
const byte QUERY_TYPE3  = 0x03;

//
//  Cells that specify (tag) what type of the cell will follow
//

// The cell is filled with NO_FILL tags
byte no_fill_tag_cell[] =
{
  NO_FILL_TAG, 
  NO_FILL_TAG, 
  NO_FILL_TAG, NO_FILL_TAG, NO_FILL_TAG, NO_FILL_TAG, NO_FILL_TAG, NO_FILL_TAG, 
  NO_FILL_TAG, NO_FILL_TAG, NO_FILL_TAG, NO_FILL_TAG, NO_FILL_TAG, NO_FILL_TAG, NO_FILL_TAG, 
  0x00  
};

byte comsec_tag_cell[] =
{
  COMSEC_TAG, 
  COMSEC_DATA, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00  //  CRC
};

byte coldstart_tag_cell[] =
{
  COLDSTART_TAG, 
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00  // CRC
};

byte comsec_key_cell_1[] =
{
  0x3e, 0x1e, 0x77, 0x9e, 0x5c, 0xff, 0x13, 0x8e,
  0x95, 0x76, 0xe0, 0x94, 0xd9, 0x8a, 0xd3, 0x35  // CRC
};

byte comsec_key_cell_2[] =
{
  0x60, 0x50, 0x00, 0x09, 0x38, 0xa4, 0x5a, 0x66, 
  0x68, 0x3b, 0xa2, 0x5f, 0x03, 0x01, 0xd0, 0x92 // CRC
};


byte comsec_key_cell_3[] =
{
   0x41, 0xa0, 0xc1, 0x93, 0x61, 0x43, 0xa2, 0xc6, 
   0x7d, 0x05, 0x5e, 0x94, 0xb5, 0xdc, 0x44, 0x8a // CRC
};

byte comsec_key_cell_4[] =
{
   0xe0, 0x6d, 0x64, 0x81, 0x06, 0xa5, 0x95, 0xd6, 
   0x97, 0x94, 0x89, 0x58, 0xe4, 0x89, 0xeb, 0xaf  // CRC
};

byte comsec_key_cell_5[] =
{
   0x8d, 0xad, 0x61, 0x1c, 0x58, 0x01, 0x39, 0xb7, 
   0x47, 0x87, 0x85, 0x5d, 0xaf, 0x26, 0x12, 0x04  // CRC
};

byte comsec_key_cell_6[] =
{
   0x47, 0xd2, 0x13, 0xee, 0x77, 0xcd, 0xff, 0x49, 
   0x05, 0xb1, 0x47, 0x3b, 0x2b, 0xca, 0x5f, 0x3a  // CRC
};


byte comsec_key_kek[] =
{
   0x47, 0xd2, 0x13, 0xee, 0x77, 0xcd, 0xff, 0x49, 
   0x05, 0xb1, 0x47, 0x3b, 0x2b, 0xca, 0x5f, 0x3a  // CRC
};

byte hopset_cell_111[] =
{
  0x8C,     // No L7 L8, Last
  0x6F,     // Net 111
  0x00,     // S1-S4    - No screen
    0x00,   
    0xF0,   // LO = 30MHz
    0x90,   // HI = 88Mhz
  0x00,     // LO Band Type A = 0
    0x01,   // Start of the band
    0x18,   //  @ 37MHz
    
    0xC1,   // End of the band
    0x90,   //  @ 40MHz
    
    0x03,   // Start of the band
    0x20,   //  @ 50Mhz
    
    0xC7,   // End of the band
    0x30,   //  @ 76MHz
  0x00  //CRC
};

byte hopset_cell_222[] =
{
  0x8C,     // No L7 L8, Last
  0xDE,     // Net 222
  0x00,     // S1-S4    - No screen
    0x00,   
    0xF0,   // LO = 30MHz
    0x90,   // HI = 88Mhz
  0x00,     // LO Band Type A = 0
    0x01,   // Start of the band
    0x18,   //  @ 37MHz
    
    0xC1,   // End of the band
    0x90,   //  @ 40MHz
    
    0x03,   // Start of the band
    0x20,   //  @ 50Mhz
    
    0xC7,   // End of the band
    0x30,   //  @ 76MHz
  0x00  //CRC
};

byte hopset_cell_333[] =
{
  0x8D,     // No L7 L8, Last
  0x4D,     // Net 333
  0x00,     // S1-S4    - No screen
    0x00,   
    0xF0,   // LO = 30MHz
    0x90,   // HI = 88Mhz
  0x00,     // LO Band Type A = 0
    0x01,   // Start of the band
    0x18,   //  @ 37MHz
    
    0xC1,   // End of the band
    0x90,   //  @ 40MHz
    
    0x03,   // Start of the band
    0x20,   //  @ 50Mhz
    
    0xC7,   // End of the band
    0x30,   //  @ 76MHz
  0x00  //CRC
};


byte hopset_cell_444[] =
{
  0x8D,     // No L7 L8, Last
  0xBC,     // Net 444
  0x00,     // S1-S4    - No screen
    0x00,   
    0xF0,   // LO = 30MHz
    0x90,   // HI = 88Mhz
  0x00,     // LO Band Type A = 0
    0x01,   // Start of the band
    0x18,   //  @ 37MHz
    
    0xC1,   // End of the band
    0x90,   //  @ 40MHz
    
    0x03,   // Start of the band
    0x20,   //  @ 50Mhz
    
    0xC7,   // End of the band
    0x30,   //  @ 76MHz
  0x00  //CRC
};


byte hopset_cell_555[] =
{
  0x8E,     // No L7 L8, Last
  0x2B,     // Net 555
  0x00,     // S1-S4    - No screen
    0x00,   
    0xF0,   // LO = 30MHz
    0x90,   // HI = 88Mhz
  0x00,     // LO Band Type A = 0
    0x01,   // Start of the band
    0x18,   //  @ 37MHz
    
    0xC1,   // End of the band
    0x90,   //  @ 40MHz
    
    0x03,   // Start of the band
    0x20,   //  @ 50Mhz
    
    0xC7,   // End of the band
    0x30,   //  @ 76MHz
  0x00  //CRC
};


byte hopset_cell_666[] =
{
  0x8E,     // No L7 L8, Last
  0x9A,     // Net 666
  0x00,     // S1-S4    - No screen
    0x00,   
    0xF0,   // LO = 30MHz
    0x90,   // HI = 88Mhz
  0x00,     // LO Band Type A = 0
    0x01,   // Start of the band
    0x18,   //  @ 37MHz
    
    0xC1,   // End of the band
    0x90,   //  @ 40MHz
    
    0x03,   // Start of the band
    0x20,   //  @ 50Mhz
    
    0xC7,   // End of the band
    0x30,   //  @ 76MHz
  0x00  //CRC
};

byte hopset_cell_1[] =
{
  0x8C,     // No L7 L8, Last
  0x01,     // Net 1
  0x00,     // S1-S4
    0x00,   
    0x00,   // LO = 30MHz
    0x02,   // HI = 30Mhz + F
  0x07,     // LO Band Type A = 7
    0x10, 
    0x03, 
    
    0x20, 
    0x0A, 
    
    0x30, 
    0x10, 
    
    0xA0, 
    0x17, 
  0x00  //CRC
};

byte hopset_cell_2[] =
{
  0x8C,     // no L7, L8, Last
  0x02,     // Net 2
  0x00,     // S1-S4
    0x00,   
    0x00,   // LO = 30MHz
    0x02,   // HI = 30Mhz + F
  0x07,     // LO Band Type A = 7
    0x10, 
    0x05, 
    
    0x10, 
    0x10, 
    
    0x10, 
    0x18, 
    
    0x10, 
    0x1F, 
  0x00  //CRC
};

byte hopset_cell_3[] =
{
  0x8C,     // No L7 L8, Last
  0x03,     // Net 3
  0x00,     // S1-S4
    0x00,   
    0x00,   // LO = 30MHz
    0x02,   // HI = 30Mhz + F
  0x04,     // LO Band Type A = 4
    0x10, 
    0x05, 
    
    0xA0, 
    0x10, 
    
    0x00, 
    0x18, 
    
    0xC0, 
    0x1F, 
  0x00  //CRC
};

byte hopset_cell_4[] =
{
  0x8C,     // No L7 L8, Last
  0x04,     // Net 4
  0x00,     // S1-S4
    0x00,   
    0x00,   // LO = 30MHz
    0x02,   // HI = 30Mhz + F
  0x02,     // LO Band Type A = 2
    0x20, 
    0x05, 
    
    0x30, 
    0x10, 
    
    0xA0, 
    0x14, 
    
    0xA0, 
    0x1C, 
  0x00  //CRC
};

byte hopset_cell_5[] =
{
  0x8C,     // No L7 L8, Last
  0x05,     // Net 5
  0x00,     // S1-S4
    0x00,   
    0x00,   // LO = 30MHz
    0x02,   // HI = 30Mhz + F
  0x05,     // LO Band Type A = 5
    0x10, 
    0x01, 
    
    0x10, 
    0x10, 
    
    0x30, 
    0x18, 
    
    0x10, 
    0x1E, 
  0x00  //CRC
};

byte hopset_cell_6[] =
{
  0x8C,     // No L7 L8, Last
  0x06,     // Net 6
  0x00,     // S1-S4
    0x00,   
    0x00,   // LO = 30MHz
    0x02,   // HI = 30Mhz + F
  0x04,     // LO Band Type A = 4
    0x10, 
    0x08, 
    
    0x10, 
    0x12, 
    
    0x10, 
    0x18, 
    
    0x10, 
    0x19, 
  0x00  //CRC
};


//
//  Cold start transec cell
//
byte transec_cell_cold[] =
{
   0x0a, 0xf7, 0x12, 0x4a, 0xe7, 0x5d, 0x82, 0x53, 
   0x38, 0x18, 0x08, 0xf0, 0x0c, 0xc7, 0x3b, 0xcf // CRC
};

byte transec_cell_1[] =
{
   0x67, 0xd2, 0x28, 0x41, 0xc8, 0xd3, 0x18, 0x83, 
   0xf7, 0x51, 0xd6, 0x74, 0x32, 0xe9, 0xce, 0x0d // CRC
};

byte transec_cell_2[] =
{
   0x5b, 0x33, 0x8f, 0x8a, 0xf3, 0x7f, 0x75, 0xf5, 
   0xcb, 0x77, 0x05, 0x21, 0x84, 0x9d, 0x5e, 0x9f   // CRC
};

byte transec_cell_3[] =
{
   0xc8, 0xf2, 0xc0, 0xbb, 0x92, 0x6b, 0x9b, 0x5d, 
   0x13, 0x8c, 0x28, 0xa6, 0x76, 0x9b, 0x06, 0xeb   // CRC
};

byte transec_cell_4[] =
{
   0xda, 0xa0, 0xbc, 0x76, 0x31, 0xa6, 0x83, 0x3d, 
   0x5a, 0xae, 0x6b, 0x60, 0x67, 0x76, 0x6b, 0x50   // CRC
};

byte transec_cell_5[] =
{
   0x15, 0x8b, 0xa7, 0x77, 0x75, 0xcd, 0xd8, 0x5d, 
   0x4a, 0xd4, 0x4c, 0xfe, 0x17, 0x50, 0x83, 0xa8   // CRC
};

byte transec_cell_6[] =
{
   0x43, 0xb9, 0x4a, 0x65, 0x47, 0x9d, 0xa5, 0x3d, 
   0xd9, 0xf4, 0x70, 0x55, 0x70, 0x92, 0xa0, 0xd6   // CRC
};

byte lockout_band_cell[] =
{
  0x7E,       // Set ID = L8, Last
  0x38,       // Set Number (56)
  0x28,       // LO Band = 1 MHz (40 channels)
    0x50, 
    0x78,     // 33 MHz
    
    0x50,     // 35 MHz
    0xC8, 
    
    0x51,     // 37 MHz
    0x18, 
    
    0x51,     // 39 MHz
    0x68, 
   
     0xFF, 
     0x00, 
   
     0xFF, 
     0x00, 
  0x00  // CRC
};

byte lockout_bitmap_cell[] =
{
  0x1F,       // Set ID = 1, last
     0x64,    // Start of the BM (200kHz)
     0x64, 

     0x10, 
     0x20, 
     0x30, 
     0x40, 
     
     0x10, 
     0x20, 
     0x30, 
     0x40, 

     0x10, 
     0x20, 
     0x30, 
     0x40, 
  0x00  // CRC
};

byte lockout_band_cell_2[] =
{
  0x2E,       // Set ID = 2, Last
  0x0F,       // Set Number (55)
  0x03,       // LO Band
     0x01, 
     0x70, 
   
     0x11, 
     0x75, 
   
     0x21, 
     0xA0, 
   
     0x31, 
     0xB0, 
   
     0x41, 
     0xC0, 
   
     0x51, 
     0xD0, 
  0x00  // CRC
};


byte lockout_7_50_54_MHz_cell[] =
{
  0x6F,       // Set ID = 6, Last
  0x64,       // LO Start freq (in 200 kHz) 50MHz -> (50-30)*5 = 100
  0x64,       // LO Band
     0xFF, 
     0xFF, 
   
     0xFF, 
     0xFF, 
   
     0xFF, 
     0xFF, 
   
     0xFF, 
     0xFF, 
   
     0xFF, 
     0xFF, 
   
     0xFF, 
     0xFF, 
  0x00  // CRC
};

byte lockout_8_50_54_MHz_cell[] =
{
  0x7F,       // Continuation cell, Last
  0x70,       // LO Start freq (in 200 kHz) 52.4MHz -> (52.4-30)*5 = 112 = 0x70
  0x70,       
     0xFF, 
     0xFF, 
   
     0xFF, 
     0xFF, 
   
     0xFF, 
     0xFF, 
   
     0xFF, 
     0x00, 
   
     0xFF, 
     0x00, 
   
     0xFF, 
     0x00, 
  0x00  // CRC
};



// CUE = 30Mhz, MAN = 40MHz, CH1 = 50MHz, CH2 = 60MHz and so on...
byte single_channel_cell[] =
{
  0x00, 0x01, 
  0xFF,
  0x00, 0x01, 0x90, 
  0x32, 0x04, 0xB0, 
  0x64, 0x07, 0xD0, 
  0x96, 0x0A, 0xF0,
  0x00  // CRC
};

// Time cell
byte TOD_cell[] =
{
  0x00, 0x02, 
  0x20, 0x17,   // 2017 - year
  0x09, 0x15,   // MM DD
  0x02, 0x58,   // Julian Day
  0x17,         // Hours
  0x45,         // Minutes
  0x07,         // Seconds
  0x00,         // mSec
  0x00, 
  0x00, 
  0x00,
  0x00  // CRC
};


byte loadset_type2[22][16] = {
		{ 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x77 }, // TEK1 EMPTY
		{ 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x77 }, // TEK2 EMPTY
		{ 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x77 }, // TEK3 EMPTY
		{ 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x77 }, // TEK4 EMPTY
		{ 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x77 }, // TEK5 EMPTY
		{ 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x77 }, // TEK6 EMPTY
		{ 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x77 }, // MAN EMPTY
		{ 0x8C, 0x0D, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x01, 0xD1, 0x87, 0x00, 0x02, 0xE1, 0x82, 0x80 }, // CH1 hopset - 30-40MHz (190), remove every even and every fourth
		{ 0xE9, 0x32, 0x67, 0xB0, 0x35, 0x96, 0x53, 0x74, 0x79, 0x4A, 0xEF, 0x88, 0xB5, 0x06, 0x83, 0x16 },	// CH1 transec
		{ 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x77 }, // CH2 EMPTY
		{ 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x77 }, // CH3 EMPTY
		{ 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x77 }, // CH4 EMPTY
		{ 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x77 }, // CH5 EMPTY
		{ 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x7D, 0x77 }, // CH6 EMPTY
		{ 0x0E, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00 }, // L1
		{ 0x1E, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x9E }, // L2
		{ 0x2E, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x7F }, // L3
		{ 0x3E, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xE1 }, // L4
		{ 0x4E, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFE }, // L5
		{ 0x5E, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x60 }, // L6
		{ 0x6E, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x81 }, // L7
		{ 0x7E, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x1F }  // L8
};

unsigned char non_icom_fill[4][16] = {
    { 0x89, 0x2C, 0x00, 0xA8, 0xF0, 0x81, 0x78, 0xA0, 0xB8, 0x01, 0x66, 0xC3, 0x1F, 0x53, 0x9C, 0xAD }, 
    { 0x03, 0x9A, 0x3C, 0xFF, 0xE7, 0x63, 0x80, 0x37, 0x7C, 0x0F, 0xF4, 0x10, 0x7F, 0x8C, 0xFF, 0x2A },
    { 0x06, 0x10, 0x19, 0xA1, 0x3E, 0x03, 0xEA, 0xC4, 0xCF, 0x05, 0x3A, 0xC7, 0x2F, 0x53, 0xC3, 0xA9 },
    { 0x3C, 0xC3, 0x70, 0xE0, 0xE5, 0x53, 0x80, 0x2A, 0x12, 0x15, 0xA7, 0x91, 0xB5, 0xA5, 0xDD, 0xA7 }
};




//
//  Loadset specifially designed for CARNG training
//

// CUE = 34.4Mhz, MAN = 40.225MHz, CH1 = 50MHz, CH2 = 60MHz and so on...
byte single_channel_CA[] =
{
  0x00, 0x01, 
  0xFF,
  0x0B, 0x01, 0x99, 
  0x32, 0x04, 0xB0, 
  0x64, 0x07, 0xD0, 
  0x96, 0x0A, 0xF0,
  0x00  // CRC
};


byte hopset_cell_Chan1_CA[] =
{
  0x8C,     // No L7 L8, Last
  0x6F,     // Net 111
  0x00,     // S1-S4    - No screen
    0x00,   
    0x70,   // LO = 30MHz (0x000)
    0x11,   // HI = 37Mhz (0x118 - 1)
  0x00,     // LO Band Type A = 0
    0x10,   // 1 freq of 30.4MHz (0x010)
    0x10,   //  
    
    0x10,   // 1 freq of 32.25MHz (0x05A)
    0x5A,   //  

    0x10,   // 1 freq of 34.4MHz (0x0B0)
    0xB0,   //  
    
    0x10,   // 1 freq of 34.825 (0x0C1)
    0xC1,   // 
    
  0x00  //CRC
};

byte hopset_cell_Chan2_CA[] =
{
  0x8C,     // No L7 L8, Last
  0xDE,     // Net 222
  0x00,     // S1-S4    - No screen
    0x90,   
    0xF1,   // LO = 40MHz (0x190)
    0x31,   // HI = 50Mhz (0x320 - 1)
  0x00,     // LO Band Type A = 0
    0x11,   // 1 freq of 40.225MHz (0x199)
    0x99,   //  
    
    0xFF,   // Start of the band
    0x00,   //  Skip
    
    0xFF,   // Start of the band
    0x00,   //  Skip
    
    0xFF,   // Start of the band
    0x00,   //  Skip
  0x00  //CRC
};

byte hopset_cell_Chan3_CA[] =
{
  0x8D,     // No L7 L8, Last
  0x4D,     // Net 333
  0x00,     // S1-S4    - No screen
    0x30,   
    0xF7,   // LO = 76MHz (0x730)
    0x90,   // HI = 88Mhz (0x910 - 1)
  0x00,     // LO Band Type A = 0
    0xFF,   // Start of the band
    0x00,   //  Skip
    
    0xFF,   // Start of the band
    0x00,   //  Skip
    
    0xFF,   // Start of the band
    0x00,   //  Skip
    
    0xFF,   // Start of the band
    0x00,   //  Skip
  0x00  //CRC
};


byte hopset_cell_Chan4_CA[] =
{
  0x8D,     // No L7 L8, Last
  0xBC,     // Net 444
  0x00,     // S1-S4    - No screen
    0x00,   
    0x70,   // LO = 30MHz (0x000)
    0x11,   // HI = 37Mhz (0x118 - 1)
  0x00,     // LO Band Type A = 0
    0x10,   // 1 freq of 30.4MHz (0x010)
    0x10,   //  
    
    0x10,   // 1 freq of 32.25MHz (0x05A)
    0x5A,   //  

    0x10,   // 1 freq of 34.4MHz (0x0B0)
    0xB0,   //  
    
    0x10,   // 1 freq of 34.825 (0x0C1)
    0xC1,   // 
    
  0x00  //CRC
};


byte hopset_cell_Chan5_CA[] =
{
  0x8E,     // No L7 L8, Last
  0x2B,     // Net 555
  0x00,     // S1-S4    - No screen
    0x90,   
    0xF1,   // LO = 40MHz (0x190)
    0x31,   // HI = 50Mhz (0x320 - 1)
  0x00,     // LO Band Type A = 0
    0x11,   // 1 freq of 40.225MHz (0x199)
    0x99,   //  
    
    0xFF,   // Start of the band
    0x00,   //  Skip
    
    0xFF,   // Start of the band
    0x00,   //  Skip
    
    0xFF,   // Start of the band
    0x00,   //  Skip
  0x00  //CRC
};


byte hopset_cell_Chan6_CA[] =
{
  0x8E,     // No L7 L8, Last
  0x9A,     // Net 666
  0x00,     // S1-S4    - No screen
    0x30,   
    0xF7,   // LO = 76MHz (0x730)
    0x90,   // HI = 88Mhz (0x910 - 1)
  0x00,     // LO Band Type A = 0
    0xFF,   // Start of the band
    0x00,   //  Skip
    
    0xFF,   // Start of the band
    0x00,   //  Skip
    
    0xFF,   // Start of the band
    0x00,   //  Skip
    
    0xFF,   // Start of the band
    0x00,   //  Skip
  0x00  //CRC
};

byte lockout1_CA[] =  { 0x0E, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00 }; // L1
byte lockout2_CA[] =  { 0x1E, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x9E }; // L2
byte lockout3_CA[] =  { 0x2E, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x7F }; // L3
byte lockout4_CA[] =  { 0x3E, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xE1 }; // L4
byte lockout5_CA[] =  { 0x4E, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFE }; // L5
byte lockout6_CA[] =  { 0x5E, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x60 }; // L6
byte lockout7_CA[] =  { 0x6E, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x81 }; // L7
byte lockout8_CA[] =  { 0x7E, 0x01, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x1F }; // L8

void setup()
{
  Serial.begin(115200);
}

void loop()
{

  Serial.println("**********************************");

  
//  Type3Full();
//  Type3ColdStart();
//  Type1TEK1();
//  Type1TEK2();
//  Type1TEK3();
//  Type3NoTEK();
//  Type3TOD();

//	Type2Any();

//	  Type3CA();
//    Type2CAColdStart();

//    Type1TEK1();
//    Type1TEK2();
//    Type1TEK3();
    
//    Type3CANoTEK();
//    Type1KEK();

    NonIcomFill();
    ESet1();
    TransecCh1();
    
//  Type2NoTEK();
  
  while(1)
  {
  }
}

void Type3CA()
{
    char Equipment = TIMEOUT;
    Serial.println("**********Starting Type3CA Fill***********");
    AcquireBusType23();
    Equipment = TIMEOUT;
    while(Equipment == TIMEOUT)
    {
      delay(2000);
      Serial.println("SendQuery");
      StartHandshake();
      SendQuery(QUERY_TYPE3);
      Equipment = GetEquipmentType();
      EndHandshake();
    }

    Serial.println("WaitFirstReq");
    WaitFirstReq();      

    Serial.println("Sending Type3CA Fill !!!!");

// Send KeyTag and KeyData  Cells 
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_1);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_2);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_3);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_4);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_5);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_6);

//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);

  // Send Cold Start MAN cells
    TestCell(coldstart_tag_cell);
    TestCell(transec_cell_cold);

  // Send hopset and transec cells
    TestCell(hopset_cell_Chan1_CA);
    TestCell(transec_cell_1);

    TestCell(hopset_cell_Chan2_CA);
    TestCell(transec_cell_2);

    TestCell(hopset_cell_Chan3_CA);
    TestCell(transec_cell_3);

    TestCell(hopset_cell_Chan4_CA);
    TestCell(transec_cell_4);

    TestCell(hopset_cell_Chan5_CA);
    TestCell(transec_cell_5);

    TestCell(hopset_cell_Chan6_CA);
    TestCell(transec_cell_6);

  // Send Type 3 TOD and Single Channel
    TestCell(TOD_cell);
    TestCell(single_channel_CA);
  
  // Send lockout cells
    TestCell(lockout1_CA);
    TestCell(lockout2_CA);
    TestCell(lockout3_CA);
    TestCell(lockout4_CA);

    TestCell(lockout5_CA);
    TestCell(lockout6_CA);
    TestCell(lockout7_CA);
    TestLastCell(lockout8_CA);

//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);

//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestLastCell(no_fill_tag_cell);
  
    delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
  
    Serial.println("Type3CA Done !!!!");
    delay(8000);
}

void Type3CANoTEK()
{
    char Equipment = TIMEOUT;
    Serial.println("**********Starting Type3CA No TEK Fill***********");
    AcquireBusType23();
    Equipment = TIMEOUT;
    while(Equipment == TIMEOUT)
    {
      delay(2000);
      Serial.println("SendQuery");
      StartHandshake();
      SendQuery(QUERY_TYPE3);
      Equipment = GetEquipmentType();
      EndHandshake();
    }

    Serial.println("WaitFirstReq");
    WaitFirstReq();      

    Serial.println("Sending Type3CA No TEK Fill !!!!");

// Send KeyTag and KeyData  Cells 
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_1);
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_2);
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_3);
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_4);
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_5);
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_6);

    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);

  // Send Cold Start MAN cells
    TestCell(coldstart_tag_cell);
    TestCell(transec_cell_cold);

// Send hopset and transec cells
    TestCell(hopset_cell_Chan1_CA);
    TestCell(transec_cell_1);

    TestCell(hopset_cell_Chan2_CA);
    TestCell(transec_cell_2);

    TestCell(hopset_cell_Chan3_CA);
    TestCell(transec_cell_3);

    TestCell(hopset_cell_Chan4_CA);
    TestCell(transec_cell_4);

    TestCell(hopset_cell_Chan5_CA);
    TestCell(transec_cell_5);

    TestCell(hopset_cell_Chan6_CA);
    TestCell(transec_cell_6);

  // Send Type 3 TOD and Single Channel
    TestCell(TOD_cell);
    TestCell(single_channel_CA);
  
  // Send lockout cells
    TestCell(lockout1_CA);
    TestCell(lockout2_CA);
    TestCell(lockout3_CA);
    TestCell(lockout4_CA);

    TestCell(lockout5_CA);
    TestCell(lockout6_CA);
    TestCell(lockout7_CA);
    TestLastCell(lockout8_CA);

//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);

//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestLastCell(no_fill_tag_cell);
  
    delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
  
    Serial.println("Type3CA No TEK Done !!!!");
    delay(8000);
}

void Type2CA()
{
    char Equipment = TIMEOUT;
    Serial.println("**********Starting Type2CA Fill***********");
    AcquireBusType23();
    Equipment = TIMEOUT;
    while(Equipment == TIMEOUT)
    {
      delay(2000);
      Serial.println("SendQuery");
      StartHandshake();
      SendQuery(QUERY_TYPE3);
      Equipment = GetEquipmentType();
      EndHandshake();
    }

    Serial.println("WaitFirstReq");
    WaitFirstReq();      

    Serial.println("Sending Type2CA Fill !!!!");

// Send KeyTag and KeyData  Cells 
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_1);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_2);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_3);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_4);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_5);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_6);

//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);

// Send Cold Start MAN cells
    TestCell(coldstart_tag_cell);
    TestCell(transec_cell_cold);

  // Send hopset and transec cells
    TestCell(hopset_cell_Chan1_CA);
    TestCell(transec_cell_1);

    TestCell(hopset_cell_Chan2_CA);
    TestCell(transec_cell_2);

    TestCell(hopset_cell_Chan3_CA);
    TestCell(transec_cell_3);

    TestCell(hopset_cell_Chan4_CA);
    TestCell(transec_cell_4);

    TestCell(hopset_cell_Chan5_CA);
    TestCell(transec_cell_5);

    TestCell(hopset_cell_Chan6_CA);
    TestCell(transec_cell_6);

// Send Type 3 TOD and Single Channel
//    TestCell(TOD_cell);
//    TestCell(single_channel_CA);
  
  // Send lockout cells
//    TestCell(lockout1_CA);
//    TestCell(lockout2_CA);
//    TestCell(lockout3_CA);
//    TestCell(lockout4_CA);

//    TestCell(lockout5_CA);
//    TestCell(lockout6_CA);
//    TestCell(lockout7_CA);
//    TestLastCell(lockout8_CA);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);

    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestLastCell(no_fill_tag_cell);
  
    delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
  
    Serial.println("Type2CA Done !!!!");
    delay(8000);
}


void Type2CAColdStart()
{
    char Equipment = TIMEOUT;
    Serial.println("**********Starting Type2CA Cold Start Fill***********");
    AcquireBusType23();
    Equipment = TIMEOUT;
    while(Equipment == TIMEOUT)
    {
      delay(2000);
      Serial.println("SendQuery");
      StartHandshake();
      SendQuery(QUERY_TYPE3);
      Equipment = GetEquipmentType();
      EndHandshake();
    }

    Serial.println("WaitFirstReq");
    WaitFirstReq();      

    Serial.println("Sending Type2CA Cold Start Fill !!!!");

// Send KeyTag and KeyData  Cells 
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_1);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_2);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_3);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_4);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_5);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_6);

//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);

// Send Cold Start MAN cells
    TestCell(coldstart_tag_cell);
    TestCell(transec_cell_cold);

  // Send hopset and transec cells
//    TestCell(hopset_cell_Chan1_CA);
//    TestCell(transec_cell_1);
//    TestCell(hopset_cell_Chan2_CA);
//    TestCell(transec_cell_2);
//    TestCell(hopset_cell_Chan3_CA);
//    TestCell(transec_cell_3);
//    TestCell(hopset_cell_Chan4_CA);
//    TestCell(transec_cell_4);
//    TestCell(hopset_cell_Chan5_CA);
//    TestCell(transec_cell_5);
//    TestCell(hopset_cell_Chan6_CA);
//    TestCell(transec_cell_6);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);

    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);

// Send Type 3 TOD and Single Channel
//    TestCell(TOD_cell);
//    TestCell(single_channel_CA);
  
  // Send lockout cells
//    TestCell(lockout1_CA);
//    TestCell(lockout2_CA);
//    TestCell(lockout3_CA);
//    TestCell(lockout4_CA);

//    TestCell(lockout5_CA);
//    TestCell(lockout6_CA);
//    TestCell(lockout7_CA);
//    TestLastCell(lockout8_CA);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);

    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestLastCell(no_fill_tag_cell);
  
    delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
  
    Serial.println("Type2CA Cold Start Done !!!!");
    delay(8000);
}


void NonIcomFill()
{
    int i;
    char Equipment = TIMEOUT;
    Serial.println("**********Starting NonIcomFill Fill***********");
    AcquireBusType1();

    Serial.println("WaitFirstReq");
    WaitFirstReq();      

    Serial.println("Sending NonIcomFill Fill !!!!");

    for(i = 0; i < 3; i++) {
      TestAnyCell(non_icom_fill[i]);
    }
    TestLastAnyCell(non_icom_fill[i]);
    
  delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
  
    Serial.println("NonIcomFill Done !!!!");
    delay(8000);
}


void Type2Any()
{
	  int i;
    char Equipment = TIMEOUT;
    Serial.println("**********Starting Type2Any Fill***********");
    AcquireBusType23();
    Equipment = TIMEOUT;
    while(Equipment == TIMEOUT)
    {
      delay(2000);
      Serial.println("SendQuery");
      StartHandshake();
      SendQuery(QUERY_TYPE3);
      Equipment = GetEquipmentType();
      EndHandshake();
    }

    Serial.println("WaitFirstReq");
    WaitFirstReq();      

    Serial.println("Sending Type2Any Fill !!!!");

	  for(i = 0; i < 21; i++) {
		  TestAnyCell(loadset_type2[i]);
	  }
    TestLastAnyCell(loadset_type2[i]);
    
	delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
  
    Serial.println("Type2Any Done !!!!");
    delay(8000);
}


void Type3TOD()
{
    char Equipment = TIMEOUT;
    Serial.println("**********Starting Type3TOD Fill***********");
    AcquireBusType23();
    Equipment = TIMEOUT;
    while(Equipment == TIMEOUT)
    {
      delay(2000);
      Serial.println("SendQuery");
      StartHandshake();
      SendQuery(QUERY_TYPE3);
      Equipment = GetEquipmentType();
      EndHandshake();
    }

    Serial.println("WaitFirstReq");
    WaitFirstReq();      

    Serial.println("Sending Type3TOD Fill !!!!");

    TestLastCell(TOD_cell);
    delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
  
    Serial.println("Type3TOD Done !!!!");
    delay(8000);
}


void Type3Full()
{
    char Equipment = TIMEOUT;
    Serial.println("**********Starting FullType3 Fill***********");
    AcquireBusType23();
    Equipment = TIMEOUT;
    while(Equipment == TIMEOUT)
    {
      delay(2000);
      Serial.println("SendQuery");
      StartHandshake();
      SendQuery(QUERY_TYPE3);
      Equipment = GetEquipmentType();
      EndHandshake();
    }

    Serial.println("WaitFirstReq");
    WaitFirstReq();      

    Serial.println("Sending FullType3 Fill !!!!");

// Send KeyTag and KeyData  Cells 
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_1);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_2);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_3);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_4);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_5);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_6);

//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);

  // Send Cold Start MAN cells
//  TestCell(no_fill_tag_cell);
    TestCell(coldstart_tag_cell);
    TestCell(transec_cell_cold);

// Send hopset and transec cells
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);


    TestCell(hopset_cell_111);
    TestCell(transec_cell_1);

    TestCell(hopset_cell_222);
    TestCell(transec_cell_2);

    TestCell(hopset_cell_333);
    TestCell(transec_cell_3);

    TestCell(hopset_cell_444);
    TestCell(transec_cell_4);

    TestCell(hopset_cell_555);
    TestCell(transec_cell_5);

    TestCell(hopset_cell_666);
    TestCell(transec_cell_6);

    TestCell(TOD_cell);
    TestCell(single_channel_cell);
  
  // Send lockout cells
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);

    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestLastCell(no_fill_tag_cell);
  
    delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
  
    Serial.println("FullType3 Done !!!!");
    delay(8000);
}


void Type3NoTEK()
{
    char Equipment = TIMEOUT;
    
    Serial.println("**********Starting Type3 No TEK Fill***********");
    AcquireBusType23();
    Equipment = TIMEOUT;
    while(Equipment == TIMEOUT)
    {
      delay(2000);
      Serial.println("SendQuery");
      StartHandshake();
      SendQuery(QUERY_TYPE3);
      Equipment = GetEquipmentType();
      EndHandshake();
    }

    Serial.println("WaitFirstReq");
    WaitFirstReq();      

    Serial.println("Sending Type3 No TEK Fill !!!!");

// Send KeyTag and KeyData  Cells 
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_1);
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_2);
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_3);
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_4);
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_5);
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_6);

    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);

  // Send Cold Start MAN cells
//  TestCell(no_fill_tag_cell);
    TestCell(coldstart_tag_cell);
    TestCell(transec_cell_cold);

// Send hopset and transec cells
    TestCell(hopset_cell_111);
    TestCell(transec_cell_1);

    TestCell(hopset_cell_222);
    TestCell(transec_cell_2);

    TestCell(hopset_cell_333);
    TestCell(transec_cell_3);

    TestCell(hopset_cell_444);
    TestCell(transec_cell_4);

    TestCell(hopset_cell_555);
    TestCell(transec_cell_5);

    TestCell(hopset_cell_666);
    TestCell(transec_cell_6);


    TestCell(TOD_cell);
    TestCell(single_channel_cell);
  

  // Send lockout cells
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);

    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestLastCell(no_fill_tag_cell);
  
    delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
  
    Serial.println("Type3 No TEK Done !!!!");
    delay(8000);
}


void Type3ColdStart()
{
    char Equipment = TIMEOUT;
    
    Serial.println("**********Starting Type3 ColdStart Fill***********");
    AcquireBusType23();
    Equipment = TIMEOUT;
    while(Equipment == TIMEOUT)
    {
      delay(2000);
      Serial.println("SendQuery");
      StartHandshake();
      SendQuery(QUERY_TYPE3);
      Equipment = GetEquipmentType();
      EndHandshake();
    }

    Serial.println("WaitFirstReq");
    WaitFirstReq();      

    Serial.println("Sending Type3 Cold Start Fill !!!!");

// Send KeyTag and KeyData  Cells 
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_1);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_2);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_3);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_4);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_5);
    TestCell(comsec_tag_cell);
    TestKeyCell(comsec_key_cell_6);

//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);
//    TestCell(no_fill_tag_cell);

  // Send Cold Start MAN cells
//  TestCell(no_fill_tag_cell);
    TestCell(coldstart_tag_cell);
    TestCell(transec_cell_cold);

// Send hopset and transec cells
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);

    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);

  // Send lockout cells
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);

    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestLastCell(no_fill_tag_cell);
  
  delay(500);
  EndFill();
  delay(500);
  ReleaseBus();
  
  Serial.println("Type3 ColdStart Done !!!!");
  delay(8000);
}


void Type1TEK1()
{
   Serial.println("**********Starting Type1 TEK1 Fill***********");
   AcquireBusType1();
   Serial.println("WaitFirstReq");
   WaitFirstReq();
   Serial.println("Sending Type1 TEK1 Fill !!!!");

    TestKeyCell(comsec_key_cell_1);
    delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
    
    Serial.println("Type1 TEK1 Done !!!!");

    delay(8000);
}

void Type1TEK2()
{
   Serial.println("**********Starting Type1 TEK2 Fill***********");
   AcquireBusType1();
   Serial.println("WaitFirstReq");
   WaitFirstReq();

   Serial.println("Sending Type1 TEK2 Fill !!!!");

    TestKeyCell(comsec_key_cell_2);
    delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
    
    Serial.println("Type1 TEK2 Done !!!!");
    delay(8000);
}

void Type1TEK3()
{
   Serial.println("**********Starting Type1 TEK3 Fill***********");
   AcquireBusType1();
   Serial.println("WaitFirstReq");
   WaitFirstReq();

   Serial.println("Sending Type1 TEK3 Fill !!!!");

    TestKeyCell(comsec_key_cell_3);
    delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
    
    Serial.println("Type1 TEK3 Done !!!!");
    delay(8000);
}


void Type1KEK()
{
   Serial.println("**********Starting Type1 KEK Fill***********");
   AcquireBusType1();
   Serial.println("WaitFirstReq");
   WaitFirstReq();

   Serial.println("Sending Type1 KEK Fill !!!!");

    TestKeyCell(comsec_key_cell_6);
    delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
    
    Serial.println("Type1 KEK Done !!!!");
    delay(8000);
}

void ESet1()
{
   Serial.println("**********Starting ESet 1 Fill***********");
   AcquireBusType1();
   Serial.println("WaitFirstReq");
   WaitFirstReq();

   Serial.println("Sending ESet 1 Fill !!!!");

    TestCell(coldstart_tag_cell);
    TestLastCell(transec_cell_cold);

    delay(500);
    ReleaseBus();
    
    Serial.println("ESet 1 Done !!!!");
    delay(8000);
}

void TransecCh1()
{
   Serial.println("**********Starting TransecCh1 ONLY Fill***********");
   AcquireBusType1();
   Serial.println("WaitFirstReq");
   WaitFirstReqType1();      

   Serial.println("Sending TransecCh1 ONLY Fill !!!!");

    TestCell(hopset_cell_111);
    TestLastCell(transec_cell_1);

    delay(500);
    ReleaseBus();
    
    Serial.println("TransecCh1 ONLY  Done !!!!");
    delay(8000);
}


void Type2NoTEK()
{
    char Equipment = TIMEOUT;
    
    Serial.println("**********Starting FullType2 No TEK Fill***********");
    AcquireBusType23();
    Equipment = TIMEOUT;
    while(Equipment == TIMEOUT)
    {
      delay(2000);
      Serial.println("SendQuery");
      StartHandshake();
      SendQuery(QUERY_TYPE3);
      Equipment = GetEquipmentType();
      EndHandshake();
      // Serial.print("EquipmentType = 0x");
      // Serial.println(Equipment, HEX);
    }

    Serial.println("WaitFirstReq");
    WaitFirstReq();      

    Serial.println("Sending FullType2 No TEK Fill !!!!");
// Send KeyTag and KeyData  Cells 
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_1);
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_2);
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_3);
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_4);
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_5);
//    TestCell(comsec_tag_cell);
//    TestKeyCell(comsec_key_cell_6);

    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);

    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);

  // Send Cold Start MAN cells
//  TestCell(no_fill_tag_cell);
    TestCell(coldstart_tag_cell);
    TestCell(transec_cell_cold);

// Send hopset and transec cells
    TestCell(hopset_cell_111);
    TestCell(transec_cell_1);

    TestCell(hopset_cell_222);
    TestCell(transec_cell_2);

    TestCell(hopset_cell_333);
    TestCell(transec_cell_3);

    TestCell(hopset_cell_444);
    TestCell(transec_cell_4);

    TestCell(hopset_cell_555);
    TestCell(transec_cell_5);

    TestCell(hopset_cell_666);
    TestCell(transec_cell_6);


//    TestCell(TOD_cell);
//    TestCell(single_channel_cell);
  

  // Send lockout cells
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);

    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestCell(no_fill_tag_cell);
    TestLastCell(no_fill_tag_cell);
  
//  delay(500);
  EndFill();
  delay(500);
  ReleaseBus();
  
  Serial.println("FullType2 No TEK Done !!!!");
  delay(8000);
}


