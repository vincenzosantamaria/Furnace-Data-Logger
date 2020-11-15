
#include <SPI.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h> 
#include <SD.h>
File file;

#include "RTClib.h"
RTC_DS1307 rtc;


#define LED_R 9
#define LED_V 8
#define bottone_1 6
#define bottone_2 7


int schermata = 0;
int lampeggia = 0;

int sensT [10];  // ho messo 10 a muzzo... poi andrà regolato a seconda del numero di sensori effettivamente attaccati

int matrice_ore [24] [10];  //serve per salvare di volta in volta i dati medi delle ore per visualizzarle sullo schermino
                            // forse questa si può eliminare leggendo direttamente da sd e scrivendo a monitor. però mi pareva macchinoso

int minuto = 0;
int n_record = 0;
int ora = 0;
int conta_ore = 0;
int scrittura_SD = 0;
int scrittura_matrice_ore = 0;
int scrittura_matrice_giorno = 0;
int temp = 0;
int media_temperature = 0;
int media_ora = 0;
int temp_c = 0;

char stringa1[9]; // per memorizzare la stringa che include la data (dimensione massima data: 8 caratteri)
char stringa2[6]; // per memorizzare la stringa che include l'ora (dimensione massima ora: 6 caratteri)

// Data wire is plugged into digital pin 2 on the Arduino
#define ONE_WIRE_BUS 2

// Setup a oneWire instance to communicate with any OneWire device
OneWire oneWire(ONE_WIRE_BUS);  

// Pass oneWire reference to DallasTemperature library
DallasTemperature sensors(&oneWire);

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 20, 4);

int deviceCount = 0;


void setup(void)
{
  pinMode(LED_V,OUTPUT);
  pinMode(LED_R,OUTPUT);
  pinMode(bottone_1,INPUT);
  pinMode(bottone_2,INPUT);

  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  
  sensors.begin();  // Start up the library
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  lcd.begin(20, 4);
  lcd.setCursor(0, 0);
  
  lcd.print("Inizializzazione SD");
  if (!SD.begin(4)) {
  lcd.setCursor(0, 1);  
  lcd.println("FALLITA");
  delay(2000);
  lcd.clear();
  return;
  while (1);
  }
  lcd.setCursor(0, 1); 
  lcd.println("TUTTO OK");

  // conta quanti sensori sono attaccati
  deviceCount = sensors.getDeviceCount();
  delay(2000);
  lcd.clear();

}

void loop(void)
{ 

    DateTime now = rtc.now();
    lcd.setCursor(0, 0);
    sprintf(stringa1, "%2d/%02d/%d", now.day(), now.month(), now.year());
    lcd.print(stringa1);
    lcd.setCursor(12, 0);
    sprintf(stringa2, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    lcd.print(stringa2);

    digitalWrite(LED_R, LOW);

    // qui vorrei fare il blink del led ogni loop... ma mi pare che duri parecchio più dei 200 ms che ho messo di delay
    // forse il loop di per se dura parecchio? (in effettia volte l'orologio salta dei secondi)
    if (lampeggia == 0)  {
      digitalWrite(LED_V, HIGH);
      lampeggia = 1;
    } else {
      digitalWrite(LED_V, LOW);
      lampeggia = 0;
    }

    // per ora legge file da sd e li sbanfa a seriale
    
    if (digitalRead(bottone_2) == HIGH)  {
      file = SD.open("temp.txt");
      if (file == true) {
        lcd.clear();
        lcd.setCursor(0, 2);
        lcd.print("lettura da SD ");
        while(file.available()){
          Serial.write(file.read());
        }
      } else {
        lcd.clear();
        lcd.setCursor(0, 2);
        lcd.print("non c'è il file ");
      }
      file.close();
    }


    // leggi temperature
    sensors.requestTemperatures(); 
    

// se il minuto corrente non è multiplo di 2 (ma poi si può mettere a 5 o 10) legge le T e se le salva nel vettore sensT
// quando il minuto è multiplo di 2 salva su SD le medie dei valori immagazzinati nei minuti precedenti, tramite la variabile n_record
// su SD scrive anno,mese,giorno,ora,minuto, le temperature misurate e anche la media di tutte le T registrate (media_temperature)
// salva solo durante il primo loop del minuto multiplo, grazie alla variabile scrittura_SD, poi torna a leggere le T dai sensori


    if (now.minute()%2 != 0) {
      if (scrittura_SD == 1) {
        n_record = n_record +1;
        for (int i = 0;  i < deviceCount;  i++)  {
        sensT [i] = sensT [i] + sensors.getTempCByIndex(i);
        } 
      } else {
        scrittura_SD = 1;
      }
    }

      if (now.minute()%2 == 0) {
        if (scrittura_SD == 1) {
            lcd.clear();
            lcd.setCursor(0, 2);
            lcd.print("salvataggio su SD ");
            digitalWrite(LED_R, HIGH);  // non capisco perchè però il led non si accenda ??
            file = SD.open("temp.txt", FILE_WRITE);
          scrittura_SD = 0;
          conta_ore = conta_ore + 1;
          file.print(now.year());
          file.print(",");
          file.print(now.month());
          file.print(",");
          file.print(now.day());
          file.print(",");
          file.print(now.hour());
          file.print(",");
          file.print(now.minute());
          file.print(",");
          for (int i = 0;  i < deviceCount;  i++)  {
            file.print(sensT [i] /n_record);
            file.print(",");
            media_temperature = media_temperature + sensT [i] /n_record;
            sensT[i] = 0;
          }
          media_temperature = media_temperature / deviceCount;
          file.print(media_temperature);
          file.println();
          delay(2000);
          file.close();
          media_temperature = 0;
          
          n_record = 0;
        } else {
          n_record = n_record +1;
          for (int i = 0;  i < deviceCount;  i++)  {
          sensT [i] = sensT [i] + sensors.getTempCByIndex(i);
          } 
        }
      }

// qui volevo fare un pezzo di codice che mi andasse a salvare nella matrice_ore la media dei valori degli ultimi 12 valori 
// letti dalla scheda SD (12 valori se si usa f campionamento 5 min). stavo un po' provando con il comando seek() di fargli prendere
// gli ultimi valori, ma poi mi sono piantato
// usando una matrice poi è più semplice richiamare i dati da sbanfare a schermo, piuttosto che fare queste operazioni dentro agli if "schermata" (vedi sotto)

//      if ((now.hour() != ora) && conta_ore != 0) {
//        
//      }


// qui schiacciando il pulsante_1 si cambia schermata, potendo visualizzare i dati istantanei o le medie orarie.
// ancora da fare la parte sulle medie orarie (vedi su)
// i pulsanti poi andranno messi su interrupt credo  

    if (digitalRead(bottone_1) == HIGH)  {
      schermata = schermata +1;
      lcd.clear();
    }
    if (schermata == 25)  {
      schermata = 0;
    }


    if (schermata == 0) {
      lcd.setCursor(0, 2);
      lcd.print("valori istantanei ");
      lcd.setCursor(0, 3);
      lcd.print("T media ");
      for (int i = 0;  i < deviceCount;  i++)  {
        temp_c = temp_c + sensors.getTempCByIndex(i);
        }
      temp_c = temp_c/deviceCount;
      lcd.print(temp_c);
    } else {
      lcd.setCursor(0, 2);
      lcd.print("valori medi ore ");
      lcd.print(schermata - 1);
      lcd.setCursor(0, 3);
      lcd.print("T media ");
      lcd.print(matrice_ore[schermata-1][deviceCount +2]);
    } 

    minuto = now.minute();
    ora = now.hour();
    temp_c = 0;


  delay(200);
}
