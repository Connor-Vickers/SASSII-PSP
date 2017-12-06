#include <SPI.h>
#include "crc.h"

#define SELECTOR1 2
#define SELECTOR2 3
#define CSADC 4
#define CSTEMP 5
#define RESETA 6
#define RESETB 7
#define RESETC 8

#define adc_query_len 4
byte adc_query[adc_query_len] = {0x58, 0xFF, 0xFF, 0xFF};
#define adc_setup_len 3
byte adc_setup[adc_setup_len] = {0x08, 0x20, 0x07};//set mode register so that conversion complete in under 80ms(time a micro pirani conversion takes) , 0x00, 0x00};

#define adc_setch1_len 3
byte adc_setch1[adc_setch1_len] = {0x10, 0x00, 0x90};
#define adc_setch2_len 3
byte adc_setch2[adc_setch2_len] = {0x10, 0x00, 0x91};


#define temp_query_len 3
byte temp_query[temp_query_len] = {0x50, 0xFF, 0xFF};
#define temp_setup_len 2
byte temp_setup[temp_setup_len] = {0x08, 0x80};
//temp startup setting
//0x8 0x80




#define buff_len 30
byte buff[buff_len];
byte temp_buff[buff_len];

#define micro_query "@253PR1?;FF"

void setup() {
  pinMode(SELECTOR1, OUTPUT);
  pinMode(SELECTOR2, OUTPUT);
  pinMode(CSADC, OUTPUT);
  pinMode(CSTEMP, OUTPUT);
  pinMode(RESETA, OUTPUT);
  pinMode(RESETB, OUTPUT);
  pinMode(RESETC, OUTPUT);

  digitalWrite(SELECTOR1, HIGH);
  digitalWrite(SELECTOR2, HIGH);
  digitalWrite(CSADC, HIGH);
  digitalWrite(CSTEMP, HIGH);
  digitalWrite(RESETA, LOW);
  digitalWrite(RESETB, LOW);
  digitalWrite(RESETC, LOW);

  Serial.begin(19200);
  Serial.setTimeout(50);
  SPI.begin();

  SPI.beginTransaction(SPISettings(100000, MSBFIRST, SPI_MODE3));

  digitalWrite(CSTEMP, LOW);
  buffcopy(temp_setup, temp_buff, 0, temp_setup_len);
  SPI.transfer(temp_buff, temp_setup_len);
  digitalWrite(CSTEMP, HIGH);

  digitalWrite(CSADC, LOW);
  buffcopy(adc_setup, temp_buff, 0, adc_setup_len);
  SPI.transfer(temp_buff, adc_setup_len);
  digitalWrite(CSADC, HIGH);
}

//general utility functions

void buffcopy(byte *from, byte *to, int starting_at, int len){
  int i;
  for(i = 0; i < len; i++){
     to[i+starting_at] = from[i];
  }
}

//functions to select which device to talk to

void selectA(){
  digitalWrite(SELECTOR1, LOW);
  digitalWrite(SELECTOR2, LOW);
}

void selectB(){
  digitalWrite(SELECTOR1, HIGH);
  digitalWrite(SELECTOR2, LOW);
}

void selectC(){
  digitalWrite(SELECTOR1, LOW);
  digitalWrite(SELECTOR2, HIGH);
}

void selectBus(){
  digitalWrite(SELECTOR1, HIGH);
  digitalWrite(SELECTOR2, HIGH);
}

//functions relating to serial

void error(){
  clearSerial();
  Serial.write('?');
  Serial.flush();
}

void clearSerial(){
  while(Serial.available()){Serial.read();}
}

//crc functions

void generateCRC(int len){
  uint16_t crc = crc16(0, buff, len);
  buff[len] = crc / 0x100;
  buff[len + 1] = crc % 0x100;
}

bool isCRCbad(int len){
  uint16_t crc = crc16(0, buff, len);
  return (buff[len] !=  crc / 0x100) || (buff[len + 1] != crc % 0x100);
}

//manual command and related helper functions

void manualMicro(){
  delay(1);
  Serial.write(buff+3, buff[2]);//buff[2] = command length
  byte responceLen = Serial.readBytes(buff+3, 30);
  buff[2] = responceLen;
  generateCRC(responceLen + 3);
  selectBus();
  delay(1);
  Serial.write(buff, responceLen + 5);
}

void manualSPI(){
  int len = buff[2];//command and responce lengths are the same
  SPI.transfer(buff+3, len);
  //deselect
  digitalWrite(CSADC, HIGH);
  digitalWrite(CSTEMP, HIGH);
  //send responce
  generateCRC(len + 3);
  Serial.write(buff, len + 5);
}

void manualCommand(int readLen){

  int command_len = buff[2];
 //check that thae read length and CRC are correct
  if(readLen < 6 || readLen != command_len + 5 || isCRCbad(command_len + 3)){
    error();
    return;
  }
  
  switch(buff[1]){//switch based on sensor type
    case 0:
      digitalWrite(CSADC, LOW);
      manualSPI();
      break;
    case 1:
      digitalWrite(CSTEMP, LOW);
      manualSPI();
      break;
    case 2:
      selectA();
      manualMicro();
      break;
    case 3:
      selectB();
      manualMicro();
      break;
    case 4:
      selectC();
      manualMicro();
      break;
    default:
      error();
      break;
  }
}

//query command and related helper functions

uint16_t microConvert(){
  uint16_t returnVal = 0;
  bool onesVaild = temp_buff[0] >= '0' && temp_buff[0] <= '9';
  bool hasDecimal = temp_buff[1] == '.';
  bool tenthsVaild = temp_buff[2] >= '0' && temp_buff[2] <= '9';
  bool hundredthsValid = temp_buff[3] >= '0' && temp_buff[3] <= '9';
  bool hasExponent = temp_buff[4] == 'E';
  bool hasSign = temp_buff[5] == '-' || temp_buff[5] == '+';
  bool exponentVaild = temp_buff[6] >= '0' && temp_buff[6] <= '9';

  bool allOk = onesVaild && hasDecimal && tenthsVaild && hundredthsValid && hasExponent && hasSign && exponentVaild;
  if(allOk){
    uint16_t ones = temp_buff[0] - '0';
    uint16_t tenths = temp_buff[2] - '0';
    uint16_t hundredths = temp_buff[3] - '0';
    uint16_t exponent = temp_buff[6] - '0';
    returnVal = ones*1000 + tenths*100 + hundredths*10 + exponent;

    if(temp_buff[5] == '-'){
      returnVal += 0x8000;
    }
  }

  //clear temp_buff
  int i;
  for(i = 0; i < 7; i++){
    temp_buff[i] = 0;
  }
  return returnVal;
}

uint16_t getMicroValue(){
  delay(1);
  clearSerial();
  Serial.write(micro_query);
  Serial.readBytes(temp_buff, 7);
  if(temp_buff[0] == '@'){
    Serial.readBytes(temp_buff, 7);
  }
  return microConvert();
}

void queryCommand() {

  selectA();
  uint16_t a = getMicroValue();

  selectB();
  uint16_t b = getMicroValue();
  
  selectC();
  uint16_t c = getMicroValue();
  
  buff[0] = 'Q';

  //ADC
  
  digitalWrite(CSADC, LOW);
  //set adc to read channel 1
  buffcopy(adc_setch1, temp_buff, 0, adc_setch1_len);
  SPI.transfer(temp_buff, adc_setch1_len);
  delay(1);
  //start ADC conversion ~60ms
  buffcopy(adc_setup, temp_buff, 0, adc_setup_len);
  SPI.transfer(temp_buff, adc_setup_len);
  delay(35);
  //read adc
  buffcopy(adc_query, temp_buff, 0, adc_query_len);
  SPI.transfer(temp_buff, adc_query_len);
  buff[1] = temp_buff[1];
  buff[2] = temp_buff[2];
  buff[3] = temp_buff[3];
  delay(1);
  //set adc to read channel 2
  buffcopy(adc_setch2, temp_buff, 0, adc_setch2_len);
  SPI.transfer(temp_buff, adc_setch2_len);
  delay(1);
  //start ADC conversion ~60ms
  buffcopy(adc_setup, temp_buff, 0, adc_setup_len);
  SPI.transfer(temp_buff, adc_setup_len);
  delay(35);
  //read adc
  buffcopy(adc_query, temp_buff, 0, adc_query_len);
  SPI.transfer(temp_buff, adc_query_len);
  digitalWrite(CSADC, HIGH);
  buff[4] = temp_buff[1];
  buff[5] = temp_buff[2];
  buff[6] = temp_buff[3];


//todo adc calibration?

  digitalWrite(CSTEMP, LOW);
  buffcopy(temp_query, temp_buff, 0, temp_query_len);
  SPI.transfer(temp_buff, temp_query_len);
  digitalWrite(CSTEMP, HIGH);
  buff[7] = temp_buff[1];
  buff[8] = temp_buff[2];

  buff[9] = a / 0x100;
  buff[10] = a % 0x100;

  buff[11] = b / 0x100;
  buff[12] = b % 0x100;

  buff[13] = c / 0x100;
  buff[14] = c % 0x100;
  
  generateCRC(15);

  clearSerial();  
  selectBus();
  delay(1);
  Serial.write(buff, 17);
}


void loop() {
  if(Serial.available()){
    int readLen = Serial.readBytes(buff, Serial.available());
    switch (buff[0]) {
      case 'Q':
        queryCommand();
        break;
      case 'M':
        manualCommand(readLen);
        break;
      default:
        error();
        break;
    }
  }
  delay(10);
}
