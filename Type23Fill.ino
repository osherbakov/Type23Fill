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
int  _bitPeriod = 1000000 / 8000;
int tT = _bitPeriod/2 - tCorr; 


//--------------------------------------------------------------
// Timeouts in ms
//--------------------------------------------------------------
const int   tB = 500;      // Query -> Response from Radio
const int   tD = 1000;     // PIN_C Pulse Width
const int   tG = 1000;     // PIN_B Pulse Wodth
const int   tH = 1000;     // BAD HIGH - > REQ LOW

const int   tF = 4000;     // End of fill - > response
const long  tC = 5*60*1000L;  // 5 minutes - > until REQ


// Implement majority logic on reading
byte GetPinLevel(byte PinNumber)
{
   byte r1 = digitalRead(PinNumber);
   byte r2 = digitalRead(PinNumber);
   byte r3 = digitalRead(PinNumber);
   
   return ( (r1 == r2) || (r1 == r3) ? r1 : r2) ;
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
  pinMode(PIN_B, INPUT);    // Tristate the pin
  pinMode(PIN_E, INPUT);    // Tristate the pin
  digitalWrite(PIN_B, HIGH);  // Turn on 20 K Pullup
  digitalWrite(PIN_E, HIGH);  // Turn on 20 K Pullup
}

void SendByte(byte Data)
{
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

byte GetEquipmentType()
{
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
          return (GetPinLevel(PIN_B) == LOW) ? 1 : 0;
        }
      }
      PreviousState = NewState;
    }
  }
  return -1;
}

byte WaitFirstReq()
{
  byte  PreviousState = HIGH;

  pinMode(PIN_C, INPUT);    // make pin input
  pinMode(PIN_B, INPUT);    // make pin input
  digitalWrite(PIN_B, HIGH);  // Set pullup 

  unsigned long Timeout = millis() + tC;
  while ( millis() <= Timeout )
  {
    byte NewState = GetPinLevel(PIN_C);

    if(PreviousState != NewState)
    {
      Serial.print("C = ");
      Serial.println(NewState, HEX);

      PreviousState = NewState;
      if( NewState == HIGH )
      {
        return 0;
      }
    }
  }
  return -1;
}


byte WaitReq()
{

  byte  PreviousState_B = HIGH;
  byte  PreviousState_C = HIGH;
  byte  Result = 0;
  unsigned long Timeout = millis() + tF;

  pinMode(PIN_C, INPUT);    // make pin input
  pinMode(PIN_B, INPUT);    // make pin input
  digitalWrite(PIN_C, HIGH);  // Set pullup 
  digitalWrite(PIN_B, HIGH);  // Set pullup 

  while(millis() <= Timeout)  
  {
    byte NewState_C = GetPinLevel(PIN_C);
    byte NewState_B = GetPinLevel(PIN_B);

    if(PreviousState_C != NewState_C)  
    {
      Serial.print("C = ");
      Serial.println(NewState_C, HEX);
      
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
        Serial.print("B = ");
        Serial.println(NewState_B, HEX);
        Result = 1;  // Bad CRC
      }
      PreviousState_B = NewState_B;
    }
  }
  return -1;
}

byte WaitLastReq()
{
  byte  PreviousState_C = HIGH;
  byte  PreviousState_B = HIGH;
  byte  Result = 0;
  unsigned long Timeout = millis() + tF;

  while(millis() <= Timeout)  
  {
    byte NewState_C = GetPinLevel(PIN_C);
    byte NewState_B = GetPinLevel(PIN_B);
    digitalWrite(PIN_C, HIGH);  // Set pullup 
    digitalWrite(PIN_B, HIGH);  // Set pullup 

    if(PreviousState_C != NewState_C)  
    {
      Serial.print("C = ");
      Serial.println(NewState_C, HEX);

      if( NewState_C == LOW ) 
      {
          return Result;
      }
    }
    if(PreviousState_B != NewState_B)
    {
      if( NewState_B == LOW ) 
      {
        Serial.print("B = ");
        Serial.println(NewState_B, HEX);
        Result = 1;  // Bad CRC
      }
      PreviousState_B = NewState_B;
    }
  }
  return -1;
}


void StartHandshake()
{
  pinMode(PIN_D, OUTPUT);
  pinMode(PIN_E, OUTPUT);
  pinMode(PIN_F, OUTPUT);
  digitalWrite(PIN_D, HIGH);
  digitalWrite(PIN_E, HIGH);
  digitalWrite(PIN_F, HIGH);
  delay(200);

  // Drop PIN_D first
  digitalWrite(PIN_D, LOW);
  //delay(tM);
  // Drop PIN_F after delay
  digitalWrite(PIN_F, LOW);
  delay(tA);    // Pin D pulse width
  // Bring PIN_D up again
  digitalWrite(PIN_D, HIGH);
}

void EndHandshake()
{
  pinMode(PIN_B, INPUT);
  pinMode(PIN_C, INPUT);
  pinMode(PIN_D, OUTPUT);
  pinMode(PIN_E, OUTPUT);
  digitalWrite(PIN_B, HIGH);
  digitalWrite(PIN_C, HIGH);
  digitalWrite(PIN_D, HIGH);
  digitalWrite(PIN_E, HIGH);
}

void StartFill()
{
  pinMode(PIN_B, INPUT);
  pinMode(PIN_C, INPUT);
  pinMode(PIN_D, OUTPUT);
  pinMode(PIN_E, OUTPUT);
  digitalWrite(PIN_B, HIGH);
  digitalWrite(PIN_C, HIGH);
  digitalWrite(PIN_D, HIGH);
  digitalWrite(PIN_E, HIGH);
}

void  EndFill()
{
  delayMicroseconds(tL);
  digitalWrite(PIN_F, HIGH);
  delay(tZ);
}


void AcquireBus()
{
  digitalWrite(PIN_B, HIGH);
  digitalWrite(PIN_C, HIGH);
  digitalWrite(PIN_D, HIGH);
  digitalWrite(PIN_E, HIGH);
  digitalWrite(PIN_F, HIGH);
  pinMode(PIN_B, INPUT);
  pinMode(PIN_C, INPUT);
  pinMode(PIN_D, OUTPUT);
  pinMode(PIN_E, OUTPUT);
  pinMode(PIN_F, OUTPUT);
  delayMicroseconds(tJ);
}

void AcquireBusType1()
{
  digitalWrite(PIN_B, HIGH);
  digitalWrite(PIN_C, HIGH);
  digitalWrite(PIN_D, HIGH);
  digitalWrite(PIN_E, HIGH);
  digitalWrite(PIN_F, HIGH);

  pinMode(PIN_B, OUTPUT);
  pinMode(PIN_C, INPUT);
  pinMode(PIN_D, OUTPUT);
  pinMode(PIN_E, OUTPUT);
  pinMode(PIN_F, INPUT);
  delayMicroseconds(tJ);
}


void ReleaseBus()
{
  pinMode(PIN_B, INPUT);
  pinMode(PIN_C, INPUT);
  pinMode(PIN_D, INPUT);
  pinMode(PIN_E, INPUT);
  pinMode(PIN_F, INPUT);
  delay(tB);
  digitalWrite(PIN_B, HIGH);
  digitalWrite(PIN_C, HIGH);
  digitalWrite(PIN_D, HIGH);
  digitalWrite(PIN_E, HIGH);
  digitalWrite(PIN_F, HIGH);
}


void PrintKeyCell(byte *cell)
{
  Serial.print("Send Key Cell:");
  for(int i = 0; i < 15; i++)
  {
      Serial.print(" 0x");Serial.print(cell[i], HEX);
  }      
  Serial.print(" CRC: 0x");
  Serial.println(cell[15], HEX);
}

void PrintCell(byte *cell)
{
  Serial.print("Send Cell:");
  for(int i = 0; i < 15; i++)
  {
      Serial.print(" 0x");Serial.print(cell[i], HEX);
  }      
  Serial.print(" CRC: 0x");
  Serial.println(cell[15], HEX);
}

byte TestKeyCell(byte *cell)
{
  CRC  cacl_crc;
  byte Result = 0;

  cacl_crc.ini();
  cacl_crc.appnd(cell, 16);
  cell[15] ^= 0xFF;
  
  PrintKeyCell(cell); 
  SendCell(cell);
  Result = WaitReq();
  return Result;
}

byte TestLastKeyCell(byte *cell)
{
  CRC  cacl_crc;
  byte Result = 0;

  cacl_crc.ini();
  cacl_crc.appnd(cell, 16);
  cell[15] ^= 0xFF;
  
  PrintKeyCell(cell); 
  SendCell(cell);
  Result = WaitLastReq();    
  return Result;
}

byte TestCell(byte *cell)
{
  CRC  cacl_crc;
  byte Result = 0;

  cacl_crc.ini();
  cacl_crc.appnd(cell, 16);
  
  for(int i = 0; i < 256; i++)
  {
    PrintCell(cell); 
    SendCell(cell);
    Result = WaitReq();    
    if( Result == 0)
    {
      // Good CRC
      return 0;
    }else if(Result == 1)
    {
      // Bad CRC - change it (increment)
//      cell[15] = i;
      Serial.print(".");
    }else
    {  
      // Timeout or other things
      Serial.println("Timeout");
      return Result;
    }
  }
  return Result;
}

byte TestLastCell(byte *cell)
{
  CRC  cacl_crc;
  byte Result = 0;

  cacl_crc.ini();
  cacl_crc.appnd(cell, 16);
  
  for(int i = 0; i < 256; i++)
  {
    PrintCell(cell); 
    SendCell(cell);
    Result = WaitLastReq();    
    if( Result == 0)
    {
      // Good CRC
      return 0;
    }else if(Result == 1)
    {
      // Bad CRC - change it (increment)
//      cell[15] = i;
      Serial.print(".");
    }else
    {  
      // Timeout or other things
      Serial.println("Timeout");
      return Result;
    }
  }
  return Result;
}

// Tag values
const byte NO_FILL = 0x7D;
const byte COMSEC_TAG = 0x7D;
const byte COMSEC_DATA = 0x00;


// QUERY bytes
const byte MODE2  = 0x02;
const byte MODE3  = 0x03;


// The cell is filled with NO_FILL tags
byte no_fill_cell[] =
{
  NO_FILL, 
  NO_FILL, NO_FILL, NO_FILL, NO_FILL, NO_FILL, NO_FILL, NO_FILL, 
  NO_FILL, NO_FILL, NO_FILL, NO_FILL, NO_FILL, NO_FILL, NO_FILL, 
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

byte comsec_key_cell_1[] =
{
  0x52, 0x34, 0xDC, 0x11, 0x11, 0x22, 0x66, 0x54, 
  0x33, 0x57, 0x76, 0x88, 0x87, 0x67, 0x00, 
  0x00  // CRC
};

byte comsec_key_cell_2[] =
{
  0xD2, 0x34, 0xDC, 0x22, 0x11, 0x22, 0x66, 0x54, 
  0x33, 0x57, 0x76, 0x88, 0x87, 0x67, 0x80, 
  0x00  // CRC
};


byte comsec_key_cell_3[] =
{
  0x33, 0x34, 0xDC, 0x33, 0x11, 0x22, 0x66, 0x54, 
  0x33, 0x57, 0x76, 0x88, 0x87, 0x67, 0x40, 
  0x00  // CRC
};

byte comsec_key_cell_4[] =
{
  0xD2, 0x34, 0xDC, 0x44, 0x11, 0x22, 0x66, 0x54, 
  0x33, 0x57, 0x7F, 0x88, 0x87, 0x67, 0x20, 
  0x00  // CRC
};

byte comsec_key_cell_5[] =
{
  0x67, 0x34, 0xDC, 0x55, 0x11, 0x22, 0x66, 0x54, 
  0x33, 0x77, 0x76, 0x88, 0x87, 0x67, 0x10, 
  0x00  // CRC
};

byte comsec_key_cell_6[] =
{
  0x87, 0x34, 0xDC, 0x66, 0x00, 0x22, 0x66, 0x54, 
  0x33, 0x57, 0x76, 0x88, 0x87, 0x67, 0x08, 
  0x00  // CRC
};


byte hopset_cell[] =
{
  0x8C,     // No L7 L8, Last
  0x80,     // Net 128
  0x25,     // S1-S4
    0x6F,   
    0x1F,   // LO = 40MHz
    0x32,   // HI = 50Mhz
  0x07,     // LO Band Type A = MAX
    0x00, 
    0x30, 
    
    0x14, 
    0x00, 
    
    0x33, 
    0x30, 
    
    0x41, 
    0xC0, 
  0x00  //CRC
};

byte hopset_cell_1[] =
{
  0x8C,     // No L7 L8, Last
  0x65,     // Net 101
  0x00,     // S1-S4
    0x90,   
    0x01,   // LO = 40MHz
    0x32,   // HI = 50Mhz
  0x07,     // LO Band Type A = MAX
    0x11, 
    0x90, 
    
    0x11, 
    0x91, 
    
    0x11, 
    0x92, 
    
    0x11, 
    0x93, 
  0x00  //CRC
};

byte hopset_cell_2[] =
{
  0x8C,     // no L7, L8, Last
  0xCA,     // Net 202
  0x00,     // S1-S4
    0x20,   
    0x03,   // LO = 50MHz
    0x4B,   // HI = 60Mhz
  0x07,     // LO Band Type A = MAX
    0x13, 
    0x20, 
    
    0x13, 
    0x21, 
    
    0x13, 
    0x22, 
    
    0x13, 
    0x23, 
  0x00  //CRC
};

byte hopset_cell_3[] =
{
  0x8D,     // No L7 L8, Last
  0x2F,     // Net 303
  0x00,     // S1-S4
    0xB0,   
    0x04,   // LO = 60MHz
    0x64,   // HI = 70Mhz
  0x12,     // LO Band Type A = MAX
    0x14, 
    0xB0, 
    
    0x14, 
    0xB1, 
    
    0x14, 
    0xB2, 
    
    0x14, 
    0xB3, 
  0x00  //CRC
};

byte hopset_cell_4[] =
{
  0x8D,     // No L7 L8, Last
  0x94,     // Net 404
  0x00,     // S1-S4
    0x40,   
    0x06,   // LO = 70MHz
    0x7D,   // HI = 80Mhz
  0x0F,     // LO Band Type A = MAX
    0x16, 
    0x40, 
    
    0x16, 
    0x41, 
    
    0x16, 
    0x42, 
    
    0x16, 
    0x43, 
  0x00  //CRC
};

byte hopset_cell_5[] =
{
  0x8E,     // No L7, L8, Last
  0x2C,     // Net 556
  0x00,     // S1 only
    0x50,   
    0x00,   // LO = 32MHz
    0x1E,   // HI = 42Mhz
  0x28,     // LO Band Type A = 1 MHz
    0x10, 
    0x50, 
    
    0x10, 
    0x51, 
    
    0x10, 
    0x52, 
    
    0x10, 
    0x53, 
  0x00  //CRC
};

byte hopset_cell_6[] =
{
  0x8E,     // No L7, L8, Last
  0xA6,     // Net 678
  0x00,     // S1 only
    0xB0,   
    0x04,   // LO = 60MHz
    0x7D,   // HI = 87Mhz
  0x22,     // LO Band Type A = MAX
    0x14, 
    0xB0, 
    
    0x14, 
    0xB1, 
    
    0x14, 
    0xB2, 
    
    0x14, 
    0xB3, 
  0x00  //CRC
};


byte transec_cell[] =
{
  0xAA, 
  0x12, 0x22, 0x33, 0x44, 0x55, 0x66, 0x88, 
  0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 
  0x12   // CRC
};

byte transec_cell_1[] =
{
  0x11, 
  0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 
  0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 
  0x12   // CRC
};

byte transec_cell_2[] =
{
  0x22, 
  0x22, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 
  0x66, 0x77, 0x11, 0x99, 0xAA, 0xBB, 0xCC, 
  0x12   // CRC
};

byte transec_cell_3[] =
{
  0x33, 
  0x33, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 
  0x66, 0x77, 0x22, 0x99, 0xAA, 0xBB, 0xCC, 
  0x12   // CRC
};

byte transec_cell_4[] =
{
  0x44, 
  0x44, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 
  0x66, 0x77, 0x33, 0x99, 0xAA, 0xBB, 0xCC, 
  0x12   // CRC
};

byte transec_cell_5[] =
{
  0x55, 
  0x44, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 
  0x66, 0x77, 0x44, 0x99, 0xAA, 0xBB, 0xCC, 
  0x12   // CRC
};

byte transec_cell_6[] =
{
  0x66, 
  0x66, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 
  0x66, 0x77, 0x55, 0x99, 0xAA, 0xBB, 0xCC, 
  0x12   // CRC
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
   
     0x00, 
     0x00, 
   
     0x00, 
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
   
     0x00, 
     0x00, 
   
     0x00, 
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
  0x20, 0x17,   // 2011 - year
  0x07, 0x30,   // MM DD
  0x00, 0x43,   // Julian Day
  0x07,         // Hours
  0x07,         // Minutes
  0x07,         // Seconds
  0x00,         // mSec
  0x00, 
  0x00, 
  0x00,
  0x00  // CRC
};


byte coldstart_cell[] =
{
  0x8C, 
  0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
  0x00  // CRC
};


void setup()
{
  Serial.begin(115200);
}

void loop()
{
  byte Equipment = 0xFF;

  Serial.println("**********************************");

//  FillESet1();
  
  
//  FullType3Fill();
//  FullType3FillColdStart();
//  FullType1TEK1();
//  FullType1TEK2();
//  FullType1TEK3();
//  FullType3FillNoTEK();
  FullType2FillNoTEK();
  
  while(1)
  {
  }
  

  while(1)
  {
      AcquireBus();
      Equipment = 0xFF;
      while(Equipment == 0xFF)
      {
        delay(2000);
        Serial.println("SendQuery");
        StartHandshake();
        SendQuery(MODE3);
        Equipment = GetEquipmentType();
        EndHandshake();
        // Serial.print("EquipmentType = 0x");
        // Serial.println(Equipment, HEX);
      }
  
      Serial.println("WaitFirstReq");
      WaitFirstReq();      
    
      Serial.println("StartFill");
      StartFill();
   
      Serial.println("Loading");
      TestCell(no_fill_cell);
      TestCell(no_fill_cell);
      TestCell(no_fill_cell);
      TestCell(no_fill_cell);
      TestCell(no_fill_cell);
      TestCell(no_fill_cell);
  
  //    TestCell(no_fill_cell);
      TestCell(coldstart_cell);
      TestCell(transec_cell);
  //    transec_cell[1]++;
  //    transec_cell[8]++;
    
      TestCell(hopset_cell_1);
      TestCell(transec_cell);
  //    transec_cell[2]++;
  //    transec_cell[9]++;
  
      TestCell(hopset_cell_2);
      TestCell(transec_cell);
  //    transec_cell[3]++;
  //    transec_cell[10]++;
  
      TestCell(hopset_cell_3);
      TestCell(transec_cell);
  //    transec_cell[4]++;
  //    transec_cell[11]++;
  
      TestCell(hopset_cell_4);
      TestCell(transec_cell);
  //    transec_cell[5]++;
  //    transec_cell[12]++;
  
      TestCell(hopset_cell_5);
      TestCell(transec_cell);
  //    transec_cell[6]++;
  //    transec_cell[13]++;
  
      TestCell(hopset_cell_6);
      TestCell(transec_cell);
  //    transec_cell[7]++;
  //    transec_cell[14]++;
  
  
    // Send SingleChannel cells
  //    TestCell(single_channel_cell);
  
    // Send lockout cells
      TestCell(no_fill_cell);
      TestCell(no_fill_cell);
      TestCell(no_fill_cell);
      TestCell(no_fill_cell);
  
      TestCell(no_fill_cell);
      TestCell(no_fill_cell);
      TestCell(no_fill_cell);
      TestLastCell(no_fill_cell);
    
      EndFill();
      ReleaseBus();
  
      Serial.println("Done !!!!");
      delay(5000);
  }
}




void FullType3Fill()
{
    byte Equipment = 0xFF;
    Serial.println("**********Starting FullType3 Fill***********");
    AcquireBus();
    Equipment = 0xFF;
    while(Equipment == 0xFF)
    {
      delay(2000);
      Serial.println("SendQuery");
      StartHandshake();
      SendQuery(MODE3);
      Equipment = GetEquipmentType();
      EndHandshake();
      // Serial.print("EquipmentType = 0x");
      // Serial.println(Equipment, HEX);
    }

    Serial.println("WaitFirstReq");
    WaitFirstReq();      

    Serial.println("StartFill");
    StartFill();

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

//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);

  // Send Cold Start MAN cells
//  TestCell(no_fill_cell);
  TestCell(coldstart_cell);
  TestCell(transec_cell);

// Send hopset and transec cells
//    TestCell(hopset_cell_1);
//    TestCell(lockout_8_50_54_MHz_cell);

//    TestCell(hopset_cell_2);
//    TestCell(lockout_8_50_54_MHz_cell);

//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);


    TestCell(hopset_cell_1);
    TestCell(transec_cell_1);

    TestCell(hopset_cell_2);
    TestCell(transec_cell_2);

    TestCell(hopset_cell_3);
    TestCell(transec_cell_3);

    TestCell(hopset_cell_4);
    TestCell(transec_cell_4);

    TestCell(hopset_cell_5);
    TestCell(transec_cell_5);

    TestCell(hopset_cell_6);
    TestCell(transec_cell_6);

    TestCell(TOD_cell);
    TestCell(single_channel_cell);
  
  // Send lockout cells
//    TestCell(lockout_band_cell);
//    TestCell(lockout_bitmap_cell);
//    TestCell(lockout_band_cell_2);

    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);

    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
//    TestCell(lockout_7_50_54_MHz_cell);
//    TestLastCell(lockout_8_50_54_MHz_cell);

    TestLastCell(no_fill_cell);
  
  delay(500);
  EndFill();
  delay(500);
  ReleaseBus();
  
  Serial.println("FullType3 Done !!!!");
  delay(5000);
}




void FullType3FillNoTEK()
{
    byte Equipment = 0xFF;
    
    Serial.println("**********Starting FullType3 No TEK Fill***********");
    AcquireBus();
    Equipment = 0xFF;
    while(Equipment == 0xFF)
    {
      delay(2000);
      Serial.println("SendQuery");
      StartHandshake();
      SendQuery(MODE3);
      Equipment = GetEquipmentType();
      EndHandshake();
      // Serial.print("EquipmentType = 0x");
      // Serial.println(Equipment, HEX);
    }

    Serial.println("WaitFirstReq");
    WaitFirstReq();      

    Serial.println("StartFill");
    StartFill();

    Serial.println("Sending FullType3 No TEK Fill !!!!");

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

    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);

  // Send Cold Start MAN cells
//  TestCell(no_fill_cell);
  TestCell(coldstart_cell);
  TestCell(transec_cell);

// Send hopset and transec cells
//    TestCell(hopset_cell_1);
//    TestCell(lockout_8_50_54_MHz_cell);

//    TestCell(hopset_cell_2);
//    TestCell(lockout_8_50_54_MHz_cell);

//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);


    TestCell(hopset_cell_1);
    TestCell(transec_cell_1);

    TestCell(hopset_cell_2);
    TestCell(transec_cell_2);

    TestCell(hopset_cell_3);
    TestCell(transec_cell_3);

    TestCell(hopset_cell_4);
    TestCell(transec_cell_4);

    TestCell(hopset_cell_5);
    TestCell(transec_cell_5);

    TestCell(hopset_cell_6);
    TestCell(transec_cell_6);


    TestCell(TOD_cell);
    TestCell(single_channel_cell);
  

  // Send lockout cells
//    TestCell(lockout_band_cell);
//    TestCell(lockout_bitmap_cell);
//    TestCell(lockout_band_cell_2);

    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);

    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
//    TestCell(lockout_7_50_54_MHz_cell);
//    TestLastCell(lockout_8_50_54_MHz_cell);

    TestLastCell(no_fill_cell);
  
  delay(500);
  EndFill();
  delay(500);
  ReleaseBus();
  
  Serial.println("FullType3 No TEK Done !!!!");
  delay(5000);
}


void FullType3FillColdStart()
{
    byte Equipment = 0xFF;
    
    Serial.println("**********Starting FullType3 ColdStart Fill***********");
    AcquireBus();
    Equipment = 0xFF;
    while(Equipment == 0xFF)
    {
      delay(2000);
      Serial.println("SendQuery");
      StartHandshake();
      SendQuery(MODE3);
      Equipment = GetEquipmentType();
      EndHandshake();
      // Serial.print("EquipmentType = 0x");
      // Serial.println(Equipment, HEX);
    }

    Serial.println("WaitFirstReq");
    WaitFirstReq();      

    Serial.println("StartFill");
    StartFill();

    Serial.println("Sending FullType3 Cold Start Fill !!!!");

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

//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);

  // Send Cold Start MAN cells
//  TestCell(no_fill_cell);
  TestCell(coldstart_cell);
  TestCell(transec_cell);

// Send hopset and transec cells
//    TestCell(hopset_cell_1);
//    TestCell(lockout_8_50_54_MHz_cell);

//    TestCell(hopset_cell_2);
//    TestCell(lockout_8_50_54_MHz_cell);

    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);

    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);


//    TestCell(hopset_cell_1);
//    TestCell(transec_cell_1);

//    TestCell(hopset_cell_2);
//    TestCell(transec_cell_2);

//    TestCell(hopset_cell_3);
//    TestCell(transec_cell_3);

//    TestCell(hopset_cell_4);
//    TestCell(transec_cell_4);

//    TestCell(hopset_cell_5);
//    TestCell(transec_cell);

//    TestCell(hopset_cell_6);
//    TestCell(transec_cell);


//    TestCell(TOD_cell);
//    TestCell(single_channel_cell);
  

  // Send lockout cells
//    TestCell(lockout_band_cell);
//    TestCell(lockout_bitmap_cell);
//    TestCell(lockout_band_cell_2);

    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);

    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
//    TestCell(lockout_7_50_54_MHz_cell);
//    TestLastCell(lockout_8_50_54_MHz_cell);

    TestLastCell(no_fill_cell);
  
  delay(500);
  EndFill();
  delay(500);
  ReleaseBus();
  
  Serial.println("FullType3 ColdStart Done !!!!");
  delay(5000);
}


void FullType1TEK1()
{
   Serial.println("**********Starting FullType1 TEK1 Fill***********");
   AcquireBusType1();
   Serial.println("WaitFirstReq");
   WaitFirstReq();      

   Serial.println("Sending FullType1 TEK1 Fill !!!!");

    TestKeyCell(comsec_key_cell_1);
    delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
    
    Serial.println("FullType1 TEK1 Done !!!!");

    delay(8000);
}

void FullType1TEK2()
{
   Serial.println("**********Starting FullType1 TEK2 Fill***********");
   AcquireBusType1();
   Serial.println("WaitFirstReq");
   WaitFirstReq();      

   Serial.println("Sending FullType1 TEK2 Fill !!!!");

    TestKeyCell(comsec_key_cell_2);
    delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
    
    Serial.println("FullType1 TEK2 Done !!!!");
    delay(8000);
}

void FullType1TEK3()
{
   Serial.println("**********Starting FullType1 TEK3 Fill***********");
   AcquireBusType1();
   Serial.println("WaitFirstReq");
   WaitFirstReq();      

   Serial.println("Sending FullType1 TEK3 Fill !!!!");

    TestKeyCell(comsec_key_cell_3);
    delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
    
    Serial.println("FullType1 TEK3 Done !!!!");
    delay(8000);
}

void FillESet1()
{
   Serial.println("**********Starting ESet 1 Fill***********");
   AcquireBusType1();
   Serial.println("WaitFirstReq");
   WaitFirstReq();      

   Serial.println("Sending ESet 1 Fill !!!!");

    TestCell(hopset_cell);
    TestCell(transec_cell);

    delay(500);
    EndFill();
    delay(500);
    ReleaseBus();
    
    Serial.println("ESet 1 Done !!!!");
    delay(8000);
}


void FullType2FillNoTEK()
{
    byte Equipment = 0xFF;
    
    Serial.println("**********Starting FullType2 No TEK Fill***********");
    AcquireBus();
    Equipment = 0xFF;
    while(Equipment == 0xFF)
    {
      delay(2000);
      Serial.println("SendQuery");
      StartHandshake();
      SendQuery(MODE3);
      Equipment = GetEquipmentType();
      EndHandshake();
      // Serial.print("EquipmentType = 0x");
      // Serial.println(Equipment, HEX);
    }

    Serial.println("WaitFirstReq");
    WaitFirstReq();      

    Serial.println("StartFill");
    StartFill();

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

    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);

    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);

  // Send Cold Start MAN cells
//  TestCell(no_fill_cell);
  TestCell(coldstart_cell);
  TestCell(transec_cell);

// Send hopset and transec cells
//    TestCell(hopset_cell_1);
//    TestCell(lockout_8_50_54_MHz_cell);

//    TestCell(hopset_cell_2);
//    TestCell(lockout_8_50_54_MHz_cell);

//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);
//    TestCell(no_fill_cell);


    TestCell(hopset_cell_1);
    TestCell(transec_cell_1);

    TestCell(hopset_cell_2);
    TestCell(transec_cell_2);

    TestCell(hopset_cell_3);
    TestCell(transec_cell_3);

    TestCell(hopset_cell_4);
    TestCell(transec_cell_4);

    TestCell(hopset_cell_5);
    TestCell(transec_cell_5);

    TestCell(hopset_cell_6);
    TestCell(transec_cell_6);


//    TestCell(TOD_cell);
//    TestCell(single_channel_cell);
  

  // Send lockout cells
//    TestCell(lockout_band_cell);
//    TestCell(lockout_bitmap_cell);
//    TestCell(lockout_band_cell_2);

    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);

    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
    TestCell(no_fill_cell);
//    TestCell(lockout_7_50_54_MHz_cell);
//    TestLastCell(lockout_8_50_54_MHz_cell);

    TestLastCell(no_fill_cell);
  
  delay(500);
  EndFill();
  delay(500);
  ReleaseBus();
  
  Serial.println("FullType2 No TEK Done !!!!");
  delay(5000);
}


