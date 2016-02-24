
/***************************************************************************
              OPEN VFO v. 1.0 - IK8YFW - 2015
This is an ham radio based on 9850 DDS.

Author:   GIUSEPPE CALLIPO - ik8yfw@libero.it
License:  The software is released under Creative Commons (CC) license.

All text above, must be included in any redistribution
****************************************************************************/
/******************************************************************
 REV. 0.0.4 - 09.03.2015
 REV. 0.0.5 - 09.04.2015
 REV. 1.0.0 - 31.10.2015 
*******************************************************************/
#include <LiquidCrystal.h>

/******************************************************************
        DISPLAY HD44780 16 x 2 DEFINITION PARAMETERS
*******************************************************************/
// LCD RS pin to digital pin D9
// LCD Enable pin to digital pin D10
// LCD D4 pin to digital pin D11
// LCD D5 pin to digital pin D12
// LCD D6 pin to digital pin D13
// LCD D7 pin to digital pin D14 (A0)
// LCD R/W pin to ground
// LCD VSS pin to ground
// LCD VCC pin to 5V
LiquidCrystal lcd(9, 10, 11, 12, 13, 14);

/******************************************************************
        DDS 9850 DEFINITION PARAMETERS
*******************************************************************/
// Pin D5 - connect to AD9850 module word load clock pin (CLK)
// Pin D6 - connect to freq update pin (FQ)
// Pin D7 - connect to serial data load pin (DATA)
// Pin D8 - connect to reset pin (RST).

#define W_CLK 5       
#define FQ_UD 6       
#define DATA 7       
#define RESET 8      

#define pulseHigh(pin) {digitalWrite(pin, HIGH); digitalWrite(pin, LOW); }
int32_t freqVFOA =0; 
int32_t freqVFOB =0; 

int32_t DDS_MIN_FREQ =30000;    //30  KHz
int32_t DDS_MAX_FREQ =30000000;  //30  MHz

/***************************************************************************************
                     ENCODER DEFINITION PARAMETERS
****************************************************************************************/
//Pin D2 Used for generating interrupts using CLK signal
//Pin D3 Used for reading DT signal
//Pin D4 Used for the push button switch

#define PinCLK 2  
#define PinDT 3
#define PinSW 4

// Additional variables to read status
volatile boolean PastDT = 0;
volatile boolean update = false;

/***************************************************************************************
                     STEP/RIT  PARAMETERS
****************************************************************************************/
// Pin A6 used to read Analog, don't need to define it

int32_t iStep =0;       // iSetup used to store the frequency step
String strStep ="";     // strStep used to show the frequency step
int32_t oldStep =0;     // temporary

int32_t iRit =0;        // iSetup used to store the frequency rit
String strRit ="";
int32_t oldRit =0;      // temporary
int oldMENU_LEVEL = 99;

/**************************************************************************************
                      BUTTONS & SPEAKER SETUP
***************************************************************************************/
#define BTN_LN1 15            // the number of the button line card 2 - D15  - A1
#define BTN_LN2 16            // the number of the button line card 1 - D16  - A2

uint8_t BTN_A_STATUS = 0;           // pushbutton A status
uint8_t BTN_B_STATUS = 0;           // pushbutton B status
uint8_t BTN_C_STATUS = 0;           // pushbutton C status

//#define MODE_ACTIVE               // uncomment to change mode menu
/**************************************************
            MENU - STATUS VARIABLE SUPPORT
MENU_LEVEL 0:  A/B  A=B  BND
MENU_LEVEL 1:  MOD  SPL  RIT  
MENU_LEVEL 2:  SCN  UP   DWN

**************************************************/
uint8_t MENU_SELECT = 0; // 0 = operate / 1 = select
int     MENU_LEVEL = 0;  // 0 - 2 - Menu level      
uint8_t VFO_AB = 0;      // 0 = VFO A  - 1 = VFO B
uint8_t MODE_A = 0;      // 0 - 4 -VFO A
uint8_t MODE_B = 0;      // 0 - 4 - VFO B
uint8_t MODE_AUTO = 0;    // 0 = off / 1 = on 
uint8_t MAX_MENU_LEVEL = 1;
uint8_t MODE_RIT = 0;   // 0 - step 1- rit 
uint8_t SCAN_START_A = 0; // 0 - stop 1- start
uint8_t SCAN_START_B = 0; // 0 - stop 1- start
uint8_t SCAN_DIR = 0;     // 0 - UP - 1 - DOWN

/***************************************************
                      MAIN SETUP                                     
***************************************************/
void setup(){
   
  // SETUP the LCD's number of columns and rows: 
  lcd.begin(16, 2);
  
  
  //SETUP 9850 - configure arduino data pins for output
  pinMode(FQ_UD, OUTPUT);
  pinMode(W_CLK, OUTPUT);
  pinMode(DATA, OUTPUT);
  pinMode(RESET, OUTPUT);
  pulseHigh(RESET);
  pulseHigh(W_CLK);
  pulseHigh(FQ_UD);  // this pulse enables serial mode - Datasheet page 12 figure 10
 
  //SETUP encoder 
  pinMode(PinCLK,INPUT);
  pinMode(PinDT,INPUT);  
  pinMode(PinSW,INPUT);
  attachInterrupt (1,ISR_DT,FALLING);   // interrupt 1 is always connected to pin 3 on Arduino UNO

  //SETUP buttons
  pinMode(BTN_LN1,INPUT);
  pinMode(BTN_LN2,INPUT);
  
  // Print wellcome message to the LCD.
  //         ****************
  lcd.print(" VFO v.1.0.0 ");
  delay(2000);
 
  // Start freq.
  freqVFOA = 7000000;
  freqVFOB = 14000000;
  DDS_sendFrequency(freqVFOA);  // freq  7 MHz
 
  // Init Step 
  ANALOG_readStep();
  MENU_refreshActiveVFO();
 
}

/***************************************************
                      MAIN LOOP DEMO                                    
***************************************************/
void loop() {
  
  // Read & Process Button Status
  MENU_readButtonsStatus();

  if (MENU_SELECT==0){ // OPERATE MODE
  
    if (MODE_RIT ==0 ){  
      // Read Step
      ANALOG_readStep();
    }else
    {
      // Read Rit
      ANALOG_readRit();
    }
    
    // Show Step or Rit
    DISPLAY_showStepRit();

    // Elaborate Set
    DDS_elaborateEncoder();

    // Check Scan
    DDS_checkSCAN();
    
  }
  else{ // MENU MODE
    DDS_elaborateEncoder();
    
    DISPLAY_showMENU();
  }
  
  // Wait a bit
  //delay(500);
    
}

/************************************************************************************************************
*                                        INTERNAL FUNCTIONS 
*************************************************************************************************************/

int32_t CheckLimit(int32_t freq){
  if (freq<DDS_MIN_FREQ){
    if (SCAN_START_A==1 || SCAN_START_B==1){
      if (SCAN_DIR==0) {SCAN_DIR=1;} else {SCAN_DIR = 0;}
    }
    return DDS_MIN_FREQ;
  }
  if (freq>DDS_MAX_FREQ){
     if (SCAN_START_A==1 || SCAN_START_B==1){
      if (SCAN_DIR==0) {SCAN_DIR=1;} else {SCAN_DIR = 0;}
    }
    return DDS_MAX_FREQ;
  }
  
  
  return freq;
}

void DDS_checkSCAN(){
if (SCAN_START_A==1 || SCAN_START_B==1){
  if (SCAN_START_A==1) {
      if (SCAN_DIR==0){       
        freqVFOA =freqVFOA+iStep;
        freqVFOA = CheckLimit(freqVFOA);
        DDS_sendFrequency(freqVFOA);
        DISPLAY_showFrequency(freqVFOA);
      }else{
        freqVFOA =freqVFOA-iStep;
        freqVFOA = CheckLimit(freqVFOA);
        DDS_sendFrequency(freqVFOA);
        DISPLAY_showFrequency(freqVFOA);
      }
      delay(100);
      return;
    }

  if (SCAN_START_B==1) {                 
      if (SCAN_DIR==0){       
        freqVFOB =freqVFOB+iStep;
        freqVFOB = CheckLimit(freqVFOB);
        DDS_sendFrequency(freqVFOB);
        DISPLAY_showFrequency(freqVFOB);
      }else{
        freqVFOB =freqVFOB-iStep;
        freqVFOB = CheckLimit(freqVFOB);
        DDS_sendFrequency(freqVFOB);
        DISPLAY_showFrequency(freqVFOB);
      }
      delay(100);
      return;
    }
  } 
}

void DISPLAY_showMENU()
{
  // print step
  if (MENU_LEVEL == oldMENU_LEVEL){
     return;
  }else{
    oldMENU_LEVEL = MENU_LEVEL;
  
    // Refresh display
    lcd.setCursor(0, 0);
    lcd.clear();
             //xxxxxxxxxxxxxxxx
    lcd.print("            M:"); lcd.print(MENU_LEVEL);
    lcd.setCursor(0, 1);
    lcd.print(MENU_decodeMenuLevel (MENU_LEVEL));
  }
}    


/********************************************************************************************************/
//* Function:  READ BUTTONS STATUS
//*
//* Description:
//*              Read and decode button status.
//*
/********************************************************************************************************/
void MENU_readButtonsStatus(){
   uint8_t BTN_STATUS_0 = 0;
   uint8_t BTN_STATUS_1 = 0;
   uint8_t BTN_STATUS_2 = 0;

   BTN_STATUS_0 = digitalRead(PinSW);
   // check if ENCODER pushbutton is pressed
   if (BTN_STATUS_0==LOW) {
     // Change select menu/operate mode
     if (MENU_SELECT==0) {MENU_SELECT=1;} else {MENU_SELECT=0;  MENU_refreshActiveVFO();}  
     oldMENU_LEVEL = 99;
     delay(400);
     return;
   }

   //MENU_LEVEL 0:  A/B  MOD  BND     or    A/B A=B BND if  MODE_ACTIVE is not defined
   //MENU_LEVEL 1:  SCN       RIT  
   //MENU_LEVEL 2:  A/B  A=B  SPL
   BTN_STATUS_1 = digitalRead(BTN_LN1);
   BTN_STATUS_2 = digitalRead(BTN_LN2);
   if (MENU_LEVEL == 0){
       // PUSH BUTTON A
       if (BTN_STATUS_1 == HIGH && BTN_STATUS_2 == LOW){
         delay(400);
         if (VFO_AB == 0) {VFO_AB = 1;} else {VFO_AB = 0;}; 

         MENU_refreshActiveVFO();

         return;
       }  
       // PUSH BUTTON B
       
       #ifdef MODE_ACTIVE
       if (BTN_STATUS_1 == LOW && BTN_STATUS_2 == HIGH){
         delay(400);
         
         if (VFO_AB == 0) {
           MODE_A ++; if (MODE_A>3) {MODE_A=0;}
         } else {
           MODE_B ++; if (MODE_B>3) {MODE_B=0;}
         };
                  
         MENU_refreshActiveVFO();

         return;
       }
       #else
       if (BTN_STATUS_1 == LOW && BTN_STATUS_2 == HIGH){
         delay(400);
        
         if (VFO_AB == 0) {
             freqVFOB = freqVFOA;
         } 
         else 
         {
             freqVFOA = freqVFOB;
         }; 

         return;
       }
       #endif 
       // PUSH BUTTON C
       if (BTN_STATUS_1 == HIGH && BTN_STATUS_2 == HIGH){
         delay(400);
          int32_t freq = MENU_getActiveBandVFO ();
          uint8_t actual_band = MENU_decodeBandFromFreq(freq);
          actual_band ++ ; if (actual_band>8) actual_band = 0;
          int32_t freq_b = MENU_setBand (actual_band);
          if (VFO_AB == 0) {freqVFOA = freq_b;} else {freqVFOB = freq_b;}; 
          oldStep=99;oldRit=99;
          MENU_refreshActiveVFO();
          MENU_refreshAutoMODE();

          return;
       }
   }

   //MOD    RIT       
   if (MENU_LEVEL == 1){
       // PUSH BUTTON A
       if (BTN_STATUS_1 == HIGH && BTN_STATUS_2 == LOW){
         delay(400);
         
         if (VFO_AB == 0) {
                     if (SCAN_START_A==0) { SCAN_START_A=1; }else { SCAN_START_A=0; }
                   } else 
                   {
                     if (SCAN_START_B==0) { SCAN_START_B=1; }else { SCAN_START_B=0; }
                   }; 
      
         return;
         
       }  
       // PUSH BUTTON B
       if (BTN_STATUS_1 == LOW && BTN_STATUS_2 == HIGH){
         delay(400);
         // NO ACTION   
         return;
       }
       // PUSH BUTTON C
       if (BTN_STATUS_1 == HIGH && BTN_STATUS_2 == HIGH){
         delay(400);
         if (MODE_RIT==0) { MODE_RIT=1; }else { MODE_RIT=0; iRit=0; }
         MENU_refreshActiveVFO();
         DISPLAY_showStepRit();

         return;
       }
   }
   // NOT ACTIVE AT MOMENT
   //if (MENU_LEVEL == 2){
   // if (VFO_AB == 0) {freqVFOB = freqVFOA;} else {freqVFOA = freqVFOB;}; 
   //}
   
}

int32_t MENU_getActiveBandVFO (){
    if (VFO_AB == 0) {return freqVFOA;} else {return freqVFOB;}; 
}

void MENU_refreshActiveVFO(){

     if (VFO_AB == 0) {
     // Refresh vfo a
        DDS_sendFrequency(freqVFOA+iRit);
        DISPLAY_showFrequency(freqVFOA);
     } else {
     // Refresh vfo b
        DDS_sendFrequency(freqVFOB+iRit);
        DISPLAY_showFrequency(freqVFOB);
     }; 

    if (MODE_RIT ==0 ){  
     DISPLAY_refreshStep();
    }else
    {
      DISPLAY_refreshRit();
    }

}

/********************************************************************************************************/
//* Function:  DECODE MODE
//*
//* Description:
//*              Decode mode to string
//*
/********************************************************************************************************/
String MENU_decodeMode (uint8_t iMode){
  String sMode = "";
  switch(iMode){
   case 0:  sMode="  CW"; break;
   case 1:  sMode=" LSB"; break;
   case 2:  sMode=" USB"; break;
   case 3:  sMode="  AM"; break;
   default: sMode=" USB"; break;
  };
  return sMode;
} 

/********************************************************************************************************/
//* Function:  DECODE WAVE LENGHT
//*
//* Description:
//*              Decode mode to string
//*
/********************************************************************************************************/
String MENU_decodeWaveLen (int32_t freq){

  String out = "";
  int32_t intWl = 300000/(freq/1000);
  if (intWl>167 && intWl < 560) return "  MW"; 
  if (intWl>561 && intWl < 2000) return "  LW"; 
  if (intWl>2000) return " VLF"; 
  if (intWl<100) return(" " +String(intWl)+"m");  
  return(" " +String(intWl));  
  
} 


/********************************************************************************************************/
//* Function:  DECODE MODE
//*
//* Description:
//*              Decode mode to string
//*
/********************************************************************************************************/
//MENU_LEVEL 0:  A/B  MOD  BND
//MENU_LEVEL 1:  SCN       R/S  
//MENU_LEVEL 2:  A/B  A=B  SPL
String MENU_decodeMenuLevel (uint8_t iMenu){
  String sMenu = "";
  switch(iMenu){
    
#ifdef MODE_ACTIVE   
   case 0: sMenu="A/B MOD BND"; break;
#else
   case 0: sMenu="A/B A=B BND"; break;
#endif 
   case 1: sMenu="SCN     R/S"; break;
   case 2: sMenu="A/B A=B SPL"; break;
   default: sMenu=""; break;
  };
  return sMenu;
} 

String MENU_decodeVfo (){
    if (VFO_AB == 0) {return "VA";} else {return "VB";}; 
} 

/********************************************************************************************************/
//* Function:  DECODE BANDPLAN MODE
//*
//* Description:
//*              Decode mode to the bandplan
//*
/********************************************************************************************************/
uint8_t MENU_bandPlan (int32_t freq){

  // TODO: To encode band
  int iMode = 0; 
  if (freq<9500000){ // < 9.5 MHz
       iMode = 1;      // LSB
  }

  if (freq>=9500000){ // >= 9.5 MHz
       iMode = 2;      // USB
  }
  
  // Set Mode to VFA A or B
  if (VFO_AB==0){      
    MODE_A = iMode;      // 0 - 4 -VFO A
  }else{
    MODE_B = iMode;      // 0 - 4 - VFO B
   }
  return iMode;
} 

/********************************************************************************************************/
//* Function:  DECODE BAND
//*
//* Description:
//*              Decode band freq
//*
/********************************************************************************************************/
int32_t MENU_setBand (uint8_t iBand){

  int32_t freq = 14000000;
  switch(iBand){
   case 0: freq=1840000; break;   //  1840000 - 160 MT
   case 1: freq=3650000; break;  //  3650000 -  80 MT
   case 2: freq=7000000; break;  //  7000000 -  40 MT
   case 3: freq=10100000; break;  // 10100000 -  30 MT
   case 4: freq=14000000; break;   // 14000000 -  20 MT
   case 5: freq=18100000; break;   // 18100000 -  17 MT
   case 6: freq=21000000; break;   // 21000000 -  15 MT
   case 7: freq=24950000; break;   // 24950000 -  12 MT
   case 8: freq=28000000; break;   // 28000000 -  10 MT
  
   default: freq=14000000; break; // 
  };
  return freq;

}

uint8_t MENU_decodeBandFromFreq(int32_t freq){

  if (freq <= 1840000) return 0;
  if (freq > 1840000 && freq <= 5000000) return 1;
  if (freq > 5000000 && freq <= 8000000) return 2;
  if (freq > 8000000 && freq <= 12000000) return 3;
  if (freq > 12000000 && freq <= 16000000) return 4;
  if (freq > 16000000 && freq <= 19000000) return 5;
  if (freq > 19000000 && freq <= 23000000) return 6;
  if (freq > 23000000 && freq <= 27000000) return 7;
  if (freq >= 2700000) return 8;

}

/********************************************************************************************************/
//* Function:  DISPLAY Show Step
//*
//* Description:
//*              The function show the step
//*
/********************************************************************************************************/
void DISPLAY_showStepRit()
{
   // If STEP MODE WAS SELECTED
   if (MODE_RIT ==0){  

      // print step
      if (iStep == oldStep){
         return;
      }else{
          oldStep = iStep;
          lcd.setCursor(12, 1);
          lcd.print(strStep);
      
          if (iStep != 1000000){ // No reset if step 1MHz was set
             if (VFO_AB==0){
               freqVFOA = (freqVFOA/(iStep*10))*(iStep*10);                  
             }else{
               freqVFOB = (freqVFOB/(iStep*10))*(iStep*10);
             }
         }
    }
  }else{   // ELSEWHERE RIT MODE
      // print rit
      if (iRit == oldRit){
         return;
      }else{
         oldRit = iRit;
         lcd.setCursor(12, 1);
         lcd.print(strRit);
         
         MENU_refreshActiveVFO();
         delay(100);

      }
  }
  
}

void DISPLAY_refreshStep()
{
  lcd.setCursor(12, 1);
  lcd.print(strStep);
}


void DISPLAY_refreshRit()
{
  lcd.setCursor(12, 1);
  lcd.print(strRit);
}
/********************************************************************************************************/
//* Function:  DISPLAY Show Frequency
//*
//* Description:
//*              The function show the frequency
//*
/********************************************************************************************************/
void DISPLAY_showFrequency(int32_t freq)
{   
    // Refresh display
    lcd.setCursor(0, 0);
    lcd.clear();
    lcd.print(MENU_decodeVfo ());
    lcd.setCursor(0, 1);
    lcd.print(MENU_decodeMenuLevel (MENU_LEVEL));
  
    //   0              15
    // 0 ****************
    // 1 **************** 
    
    //MM.HHH.DD
    byte freq_M=int(freq/1000000);
    byte freq_100k=((freq/100000)%10);
    byte freq_10k=((freq/10000)%10);
    byte freq_1k=((freq/1000)%10);
    byte freq_100=((freq/100)%10);
    byte freq_10=((freq/10)%10);
    //freq_1=int((freq/1)%10);
    if (freq_M>9) {
          lcd.setCursor(3, 0);
    }
    else{
          lcd.setCursor(4, 0);
    }
    
    // print
    lcd.print(freq_M);
    lcd.print(".");
    
    lcd.print(freq_100k);
    lcd.print(freq_10k);
    lcd.print(freq_1k);
    
    lcd.print(".");
    
    lcd.print(freq_100);
    lcd.print(freq_10);

    // decode and print mode
#ifdef ACTIVE_MODE    
    uint8_t mode = 0;
    if (iStep==1000000){ // if Step == 1MHz --> AUTOMODE
        mode = MENU_bandPlan (freq);
    }else {
        mode = MENU_getActiveMode ();
    }
    lcd.print(MENU_decodeMode (mode));
#else
    lcd.print(MENU_decodeWaveLen (freq));
#endif
    
    DISPLAY_refreshStep();

}

void MENU_refreshAutoMODE(){
  
   int32_t freq = MENU_getActiveBandVFO ();
#ifdef ACTIVE_MODE
   uint8_t mode = 0;
   mode = MENU_bandPlan (freq);
   lcd.setCursor(12, 0);
   lcd.print(MENU_decodeMode (mode));
#else
   lcd.print(MENU_decodeWaveLen (freq));
#endif
}

/********************************************************************************************************/
//* Function:  Elaborate Encoder Update
//*
//* Description:
//*              The function elaborate encoder update
//*
/********************************************************************************************************/
void DDS_elaborateEncoder()
{
if (update){
    update = false;
    if (PastDT){ // ROTATE ANTI-CLOCKWISE
    
    if (MENU_SELECT == 1) // SELECT MODE
    {
       MENU_LEVEL--; if (MENU_LEVEL<0) { MENU_LEVEL = MAX_MENU_LEVEL; };
       DISPLAY_showMENU();      
    }
    else{ // OPERATE MODE

      if (SCAN_START_A==1 || SCAN_START_B==1){
        SCAN_DIR = 1;
      }else{
      
          if (VFO_AB==0){      
            freqVFOA =freqVFOA-iStep;
            freqVFOA = CheckLimit(freqVFOA);
            DDS_sendFrequency(freqVFOA+iRit);
            DISPLAY_showFrequency(freqVFOA);
          }else{
            freqVFOB =freqVFOB-iStep;
            freqVFOB = CheckLimit(freqVFOB);
            DDS_sendFrequency(freqVFOB+iRit);
            DISPLAY_showFrequency(freqVFOB);
          }
          
      }
    }
      
    }else{ // ROTATE CLOCKWISE
      
    if (MENU_SELECT == 1) // SELECT MODE
    {
        MENU_LEVEL++;  if (MENU_LEVEL>MAX_MENU_LEVEL) { MENU_LEVEL = 0; };
        DISPLAY_showMENU();      
    }
    else{ // OPERATE MODE
      if (SCAN_START_A==1 || SCAN_START_B==1){
          SCAN_DIR = 0;
      }else{
            if (VFO_AB==0){       
              freqVFOA =freqVFOA+iStep;
              freqVFOA = CheckLimit(freqVFOA);
              DDS_sendFrequency(freqVFOA+iRit);
              DISPLAY_showFrequency(freqVFOA);
            }else{
              freqVFOB =freqVFOB+iStep;
              freqVFOB = CheckLimit(freqVFOB);
              DDS_sendFrequency(freqVFOB+iRit);
              DISPLAY_showFrequency(freqVFOB);
          }
      }
    }
      
    }
  }
}

/********************************************************************************************************/
//* Function:  MENU Get Active Mode
//*
//* Description:
//*              The function return the mode fro the active vfo
//*
/********************************************************************************************************/
uint8_t MENU_getActiveMode ()
{
  if (VFO_AB==0){      
    return MODE_A ;
  }else{
    return MODE_B ;
  }
}




/********************************************************************************************************/
//* Function:  ANALOG Calculate STEP
//*
//* Description:
//*              The function calculate the step from analog input
//*
/********************************************************************************************************/
void ANALOG_readStep()
{
  // Read from input
  int potValue = analogRead(A6);
  float fValue = potValue * (10/1023.0);
  long calcValue = (long) (fValue);
  
  // Decode status
  switch(calcValue){
    case 0: iStep=10;      strStep="  10"; break;
    case 1: iStep=100;     strStep=" 100"; break;
    case 2: iStep=500;     strStep=" 500"; break;
    case 3: iStep=1000;    strStep="  1k"; break;
    case 4: iStep=5000;    strStep="  5k"; break;
    case 5: iStep=10000;   strStep=" 10k"; break;
    case 6: iStep=25000;   strStep=" 25k"; break;
    case 7: iStep=50000;   strStep=" 50k"; break;
    case 8: iStep=100000;  strStep="100k"; break;
    case 9: iStep=1000000; strStep="  1M"; break;
    default: iStep=1000000;strStep="  1M"; break;
  }
  
}


/********************************************************************************************************/
//* Function:  ANALOG Calculate RIT
//*
//* Description:
//*              The function calculate the rit from analog input
//*              //-5000 **** 0 **** +5000
/********************************************************************************************************/
void ANALOG_readRit()
{
  byte rit_1k =0;
  byte rit_100 =0;
  strRit = "";
  
  // Read from input
  int potValue = analogRead(A6);
  iRit = (potValue -500)*10; 

  if (iRit>=0){
    rit_1k=((iRit/1000)%10);
    rit_100=((iRit/100)%10);
    strRit = "r+";
  }else{
    rit_1k=((abs(iRit)/1000)%10);
    rit_100=((abs(iRit)/100)%10);
    strRit = "r-";
  }
  
  // Decode status
  strRit = strRit + String(rit_1k)+String(rit_100);
  
}


/********************************************************************************************************/
//* Function:  INTERRUPT ENCODER DATA CHECK
//*
//* Description:
//*              The function hook any change on the encoder
//*
/********************************************************************************************************/
void ISR_DT()
{
  PastDT=(boolean)digitalRead(PinCLK);
  update = true; 
}

/********************************************************************************************************/
//* Function:  DDS TRANSFER BYTE
//*
//* Description:
//*              transfers a byte, a bit at a time, LSB first to the 9850 via serial DATA line
//*
/********************************************************************************************************/
void DDS_tfr_byte(byte data)
{
  for (int i=0; i<8; i++, data>>=1) {
    digitalWrite(DATA, data & 0x01);
    pulseHigh(W_CLK);   //after each bit sent, CLK is pulsed high
  }
}

/********************************************************************************************************/
//* Function:  DDS SEND FREQUENCY
//*
//* Description:
//*              frequency calc from datasheet page 8 = <sys clock> * <frequency tuning word>/2^32
//*
/********************************************************************************************************/
void DDS_sendFrequency(double frequency) {
  int32_t freq = frequency * 4294967295/125000000;  // note 125 MHz clock on 9850
  for (int b=0; b<4; b++, freq>>=8) {
    DDS_tfr_byte(freq & 0xFF);
  }
  DDS_tfr_byte(0x000);   // Final control byte, all 0 for 9850 chip
  pulseHigh(FQ_UD);  // Done!  Should see output
}


