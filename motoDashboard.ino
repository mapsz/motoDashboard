#include <SoftwareSerial.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <FreqCount.h>
#include <TinyGPS++.h>


//Pins
#define displayTx 8
#define gpsRx 4
#define termPower 9
#define termData 10
#define fuelPower A1
#define fuelData A2

//Display
SoftwareSerial displaySerial(false, displayTx); // RX, TX
class Display{
  private:
    int speed = 0;
    int fuel = 0;
    int rpm = 0;
    float volts = 14.3;
    float temp1 = 0;
    float temp2 = 0;

    void endSend(){
      displaySerial.write(0xff);displaySerial.write(0xff);displaySerial.write(0xff);
      displaySerial.flush();
    }
    void send(String to, int val){
      displaySerial.print(to);
      displaySerial.write(".val=");
      displaySerial.print(val);
      this->endSend();
    }
    void send(String to, String val){
      displaySerial.print(to);
      displaySerial.print(".txt=\"");
      displaySerial.print(val);      
      displaySerial.print("\"");
      this->endSend();
    }
  public:
    Display(){
      this->setup();
    }
    void setup(){
      displaySerial.begin(9600);
    }
    void setSpeed(int v){
      if(this->rpm != v){
        this->rpm = v;
        this->send("main.speed", v);
      }
    }
    void setRpm(int v){
      if(this->rpm != v){
        this->rpm = v;
        this->send("main.rpm", v);
      }
    }
    void setFuel(int v){
      if(this->fuel != v){
        this->fuel = v;
        this->send("main.fuel", v);
      }
    }
    void setTemp1(float v){
      if(this->temp1 != v){
        this->temp1 = v;
        this->send("main.temp1", String(v).toInt());
      }
    }
    void setTemp2(float v){
      if(this->temp2 != v){
        this->temp2 = v;
        this->send("main.temp2", String(v).toInt());
      }
    }
    void setVolts(float v){
      if(this->volts != v){
        this->volts = v;
        this->send("main.volts", String(v));
      }
    }    
    void setSpeed(float v){
      if(this->volts != v){
        this->volts = v;
        this->send("main.speed.val", 14);
      }
    }

};
Display display;

//GPS
TinyGPSPlus tynyGps;
SoftwareSerial gpsSerial(gpsRx, false);
class GPS{

  private:
    int speed;
    unsigned long last_update;
    int update_time = 500;



  public:
    void update(){


      if((millis() - this->last_update) < this->update_time){
        return;
      }

      while (gpsSerial.available() > 0)
        tynyGps.encode(gpsSerial.read());

      Serial.print("chars - ");
      Serial.println(tynyGps.charsProcessed());
      Serial.print("fails - ");
      Serial.println(tynyGps.failedChecksum());
      Serial.print("sentences - ");
      Serial.println(tynyGps.sentencesWithFix());
      Serial.print("satelites - ");
      Serial.println(tynyGps.satellites.value());

      if(tynyGps.satellites.value() == 0){
        this->speed = -1;
        this->last_update = millis();
        return;
      }

      if(tynyGps.satellites.value() < 5){
        this->speed = tynyGps.satellites.value();
        this->last_update = millis();
        return;
      }

      if(tynyGps.speed.kmph() > 10){
        this->speed = (int) tynyGps.speed.kmph();
        this->last_update = millis();
        return;
      }



      this->last_update = millis();

    }

    int getSpeed(){
      return this->speed;
    }
};
GPS gps;

//Temperature
OneWire oneWire(termData);
DallasTemperature sensors(&oneWire);
class Temperature{

    private:
      bool doPrint = false;
      float temp_1;
      float temp_2;
      int readCount = 0;
      unsigned long last_update;
      unsigned long on;
      unsigned long off;

      void turnOn(){
        digitalWrite(termPower, HIGH);
        this->on = millis() + 1;
        this->off = 0;
        this->readCount = 0;
      }
      void turnOff(){
        digitalWrite(termPower, LOW);
        this->off = millis() + 1;
        this->on = 0;
        this->readCount = 0;
      }
      void readTemps(){
        
        if(this->readCount == 0){
          sensors.setWaitForConversion(false);
          sensors.requestTemperatures();         
          sensors.setWaitForConversion(true);          
        }

        
        float temp_1 = float(sensors.getTempCByIndex(0));
        float temp_2 = float(sensors.getTempCByIndex(1));

        if(temp_1 != 85.00){this->temp_1 = temp_1;}
        if(temp_2 != 85.00){this->temp_2 = temp_2;}

        
        Serial.println("Temperature: data - " + String(get1()) + " 2 - " + String(get2()));

        this->last_update = millis();
        this->readCount++;
      }
      
    public:
      Temperature(){
        this->setup();
        this->temp_1 = 0;
        this->off = 1;
        this->on = 0;
        this->last_update;
        this->update();
      }      
      void setup(){
        pinMode(termPower, OUTPUT);
        sensors.begin();
      }
      void update(){

          //Turn on
          if(this->off > 0 && ((millis()+1 - this->off) > 5000)){
            if(this->doPrint)Serial.println("Temperature: Turn on " + String(millis()) + " " + String(this->off));
            this->turnOn();
          }

          //Turn off
          if(this->on > 0 && (millis()+1 - this->on) > 700){
            Serial.println("Temperature: Turn off " + String(millis()) + " " + String(this->on));
            this->turnOff();
            return;
          }
          
          if(this->on && (millis() - this->last_update) > 550){
            Serial.println("Temperature: " + String(this->readCount) + " read " + String(millis()) + " " + String(this->last_update));
            this->readTemps();
            return;
          }
      }
      float get1(){
        return this->temp_1;
      }
      float get2(){
        return this->temp_2;
      }
};
Temperature temp;

//Fuel
class Fuel{
  private:
    bool doPrint = false;
    int fuel;
    int readCount = 0;
    unsigned long last_update;
    unsigned long on;
    unsigned long off;
    int pinPower = fuelPower;
    int pinData = fuelData;
    int resistor = 1000;
    int offTime = 5000;
    


    void setup(){
      pinMode(termPower, OUTPUT);
      sensors.begin();
    }
    void turnOn(){
      digitalWrite(pinPower, HIGH);
      this->on = millis() + 1;
      this->off = 0;
      this->readCount = 0;
    }
    void turnOff(){
      digitalWrite(pinPower, LOW);
      this->off = millis() + 1;
      this->on = 0;
      this->readCount = 0;
    }
    void read(){
      this->fuel = analogRead(pinData);

      Serial.print("Fuel: data - " + String(this->fuel));
      Serial.println();

      
      this->last_update = millis();
      this->readCount++;
    }
    
  public:
    Fuel(){
      this->setup();
      this->fuel = 0;
      this->off = 1;
      this->on = 0;
      this->last_update;
      this->update();
    }
    void update(){
      //Turn on
      if(this->off > 0 && ((millis()+1 - this->off) > this->offTime)){
        if(this->doPrint)Serial.println("Fuel: Turn on " + String(millis()) + " " + String(this->off));
        this->turnOn();
      }

      //Turn off
      if(this->on > 0 && (millis()+1 - this->on) > 700){
        Serial.println("Fuel: Turn off " + String(millis()) + " " + String(this->on));
        this->turnOff();
        return;
      }
      
      if(this->on && (millis() - this->last_update) > 550){
        Serial.println("Fuel: " + String(this->readCount) + " read " + String(millis()) + " " + String(this->last_update));
        this->read();
        return;
      }
    }    
    int get(){
      return this->fuel;
    }
  
};
Fuel fuel;



void setup() {
  
  delay(2000);  

  //Serial start
  Serial.begin(9600);  
  FreqCount.begin(1000);
  gpsSerial.begin(9600);

}

void loop() {

  
  {//Temperature
    //Update
    temp.update();
    //Display
    display.setTemp1(temp.get1());
    display.setTemp2(temp.get2());
  }

  {//Fuel
    //Update
    fuel.update();
    //Display    
    display.setFuel(fuel.get());
  }

  {//Tachometer
    if (FreqCount.available()) {
      display.setRpm(FreqCount.read());
    }
  }

  {//GPS


    //update
    gps.update();
    //Display    
    display.setSpeed(gps.getSpeed());

  }

  


}
