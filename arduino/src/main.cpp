/**
 * Program      Esp32InternetRadio 
 * Author       2021-07-27 Charles Geiser (https://www.dodeka.ch)
 *              2024-10-24 Menu structure redesigned
 * 
 * Purpose      This program shows how to use the ESP32-audioI2S library 
 *              from "Wolle" aka "schreibfaul" to build a internet radio 
 *              It is an adaptation of his example program "Simple_WiFi_Radio".
 *              I used two MAX98357A DAC/Amplifier in stereo configuration.
 *              In contrast to the internet radio with the ESP8266, the serial 
 *              interface is available for inputs and outputs with the ESP32. 
 *              Therefore I designed the user interface as a simple CLI menu. 
 *              It allows the
 *                - selection of 24 radio stations (easy to expand), 
 *                - text-to-speech output with 3 examples in the languages 
 *                  English, German, Italian, 
 *                - volume control up and down, 
 *                - switching on and off of the loudspeaker 
 *                - display of the currently played radio station
 *                - redisplaying the menu. 
 *              
 * Parts        - ESP32 Doit DevKit V1
 *              - MAX98357A breakout board or UDA1334A 
 *              - Speaker 8 ohm / 0.5 W
 * 
 * Wiring                            .-----------------. 
 *               GPIO_NUM_25 -->     o LRC             |  
 *               GPIO_NUM_26 -->     o BCLK       MAX  |
 *               GPIO_NUM_27 -->     o DIN       98357 |
 *                                   o Gain            |   Spkr
 *                shutdown / mode -- o SD              |    _/|
 *                       GND -->     o GND             o---|  |
 *                       5V  -->     o Vin (5V)        o---|_ |
 *                                   `-----------------´     \|
 * 
 *              The shutdown pin SD has 4 functions, namely:                   optimal value
 *              1. Shutdown Mode               80.. 355mV < Vsd                 ==> 0V (GND)
 *              2. Mono Mode (Left+Right)/2    80.. 355mV < Vsd <  650..825mV   ==> 502.5mV
 *              3. Right Channel              650.. 825mV < Vsd < 1245..1500mV  ==> 1035mV
 *              4. Left Channel              1245..1500mV < Vsd < Vin           ==> 2000mV (safe choice)       
 * 
 *              If pin SD is left open, the on board voltage divider 100k/1000k determines  
 *              the voltage at the pin and the voltage Vsd = Vcc/11 = 5000/11 = 454mV is set. 
 *              Vsd is therefore within the permissible range for mono mode, slightly 
 *              below the optimum value of 502mV.
 * 
 *              For the right channel, we need to set an optimum voltage Vsd = 1035mV. 
 *              If we use a resistor of 560k in parallel with the built-in 1000k resistor, 
 *              we obtain a total resistance of 359k. This results in a voltage 
 *              Vsd = 5000 * 100 / 459 = 1089mv, which is within the permissible range, 
 *              slightly above the optimum value.
 * 
 *              To select the left channel, we connect 180k in parallel, thus obtaining 
 *              the total resistance of 152k and a voltage Vsd = 5000 * 100 / 252 = 1984mV, 
 *              which is in the safe range above the required value of 1500mV.    
 * 
 *              ---+-- Vin = 5V
 *                 |
 *             .---+  
 *             |   | 
 *            .-. .-. 
 *         Rs | | | | 1000k
 *            '-' '-'
 *             |   |          
 *             '---+ 
 *                 |          1500 < Vs < 5000 mv (left  chn, optimal 2000 mV) ==> Rs = 180k
 *           SD o--+---> Vs =  825 < Vs < 1245 mv (right chn, optimal 1035 mV) ==> Rs = 560k
 *                 |           355 < Vs <  650 mv (mono       optimal  502 mv) ==> Rs = no Rs
 *                .-.
 *                | |100k
 *                '-'
 *                 |
 *              ---+--- GND
 *   
 *  
 * Remarks      To run update the WiFi credentials and compile
 *
 * References   https://github.com/schreibfaul1
 *              https://how2electronics.com/esp32-music-audio-mp3-player/
 *              https://diyi0t.com/i2s-sound-tutorial-for-esp32/
 *              https://www.hackster.io/earlephilhower/esp8266-digital-radio-ee747f
 */

 #include <Arduino.h>
 #include <WiFi.h>
 #include "Audio.h"
 #include "config/wifi.h"
  
 // I2S pins
 #define I2S_LRC        GPIO_NUM_26  // LRC  of MAX98357
 #define I2S_BCLK       GPIO_NUM_27  // BCLK     "
 #define I2S_DOUT       GPIO_NUM_25  // DIN      "
 
 #define CLEAR_LINE     Serial.printf("\r%*c\r", 80, ' ')
 #define MIN_VOLUME     0
 #define MAX_VOLUME     40
 #define DEFAULT_VOLUME 15
 
 extern void heartbeat(uint8_t pin, uint8_t nBeats, uint8_t t, uint8_t duty);
 extern bool initWiFi(const char ssid[], const char password[], const char hostname[]);
 extern void printNearbyNetworks();
 extern void printConnectionDetails();
 
 void decrementVolume(const char*);
 void incrementVolume(const char*);
 void playRadio(const char*);
 void playMP3(const char*);
 void showMenu(const char*);
 void toggleSpeaker(const char*);
 
 // WiFi credentials 
 const char ssid[]     = WIFI_SSID;
 const char password[] = WIFI_PASSWORD;
 const char hostname[] = "esp32-radio";

 
 Audio audio;
 
 // Definition of the action and the menuitem
 using Action = void(&)(const char*);
 using MenuItem =  struct mi{ const char key; const char *txt; const char* arg; Action action; };
 
 MenuItem menu[] =
 {
   { '0', "MDR-Klassik",       "http://cdn.instream.audio:9288/stream", playRadio },
   { '1', "SRF1 AG-SO",        "https://streaming1.hostingmontevideo.com:7019/;",  playRadio },
   { '2', "SRF2",              "http://server.laradio.online:25224/live.mp3",        playRadio },
   { '3', "SRF3",              "http://playerservices.streamtheworld.com/api/livestream-redirect/CONTINENTAL_SC  ",        playRadio },
   { '4', "SRF4 News",         "https://streammax.alsolnet.com/radiorivadavia ",    playRadio },
   { '5', "Swiss Classic",     "http://stream.srg-ssr.ch/m/rsc_de/mp3_128",      playRadio },
   { '6', "Swiss Jazz",        "http://192.168.0.22:8080/himno-argentino.mp3",         playRadio },
   { '7', "SRF Musikwelle",    "http://192.168.0.22:8080/music2.mp3",       playRadio },
   { '8', "SRF Musikwelle",    "http://192.168.0.22:8080/music3.mp3",       playRadio },
   { '+', "Increment volume",      "", incrementVolume },
   { '-', "Decrement volume",      "", decrementVolume },
 };
 constexpr uint8_t nbrMenuItems = sizeof(menu) / sizeof(menu[0]);
 
 int currentStation     = 5;  // preselect Swiss Classic
 const char *currentUrl = menu[currentStation].arg;
 int currentVolume      = DEFAULT_VOLUME;
 
 /**
  * Display menu on monitor
  */
 void showMenu(const char* txt)
 {
   // title is packed into a formatted raw string
   Serial.print(
   R"TITLE(
 -----------------
  ESP32 Web Radio 
 -----------------
 )TITLE");
 
   for (int i = 0; i < nbrMenuItems; i++)
   {
     Serial.printf("[%c] %s\n", menu[i].key, menu[i].txt);
   }
   Serial.print("\nPress a key: ");
 }
 
 
 /**
  * Set volum to the next higher level (0..21)
  */
 void incrementVolume(const char* txt)
 {
   if (currentVolume < MAX_VOLUME) 
   {
     currentVolume++;
     audio.setVolume(currentVolume);
   }
   CLEAR_LINE;
   Serial.printf("Current Volume: %d", currentVolume);
 }
 
 
 /**
  * Set the volume to the next lower level (0..21)
  */
 void decrementVolume(const char* txt)
 {
   if (currentVolume > 0) 
   {
     currentVolume--;
     audio.setVolume(currentVolume); // 0...21
   }
   CLEAR_LINE;
   Serial.printf("Current Volume: %d", currentVolume);
 }
 
 
 /**
  * Toggle speaker on and off
  * When speaker is off and volume is at minimal value
  * the default volume is set when speaker is toggled on
  */
 void toggleSpeaker(const char* txt)
 {
   static bool spkrIsOn = true;
   if (spkrIsOn)
   {
     audio.setVolume(MIN_VOLUME);
     spkrIsOn = false;
     CLEAR_LINE;
     Serial.printf("Speaker is off");
   }
   else
   {
     if (currentVolume == MIN_VOLUME) currentVolume = DEFAULT_VOLUME;
     audio.setVolume(currentVolume);
     spkrIsOn = true;
     CLEAR_LINE;
     Serial.printf("Speaker is on");
   }
 }
 
 
 void playRadio(const char* txt)
 {
   audio.connecttohost(txt);
 }
 
 void playMP3(const char* file)
 {
   audio.connecttoFS(SPIFFS, file);   
 }
 
 /**
  * Get the keystroke from the operator and 
  * perform the corresponding action
  */
 void doMenu()
 {
   char key = Serial.read();
   CLEAR_LINE;
 
   for (int i = 0; i < nbrMenuItems; i++)
   {
     if (key == menu[i].key)
     {
       menu[i].action(menu[i].arg);
       break;
     }
   } 
 }
 
 
 
 /**
  * Initialize the audio subsystem
  */
 void initAudio()
 {
   audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
   audio.setVolume(currentVolume); // 0...21
   audio.connecttohost(currentUrl);
 
 /*   file = new AudioFileSourceSPIFFS("/stereotest440-445.mp3");
   id3 = new AudioFileSourceID3(file);
   id3->RegisterMetadataCB(MDCallback, (void*)"ID3TAG");
   out = new AudioOutputI2S();
   out->SetPinout(26,25,27);
   out->SetGain(0.1);
   mp3 = new AudioGeneratorMP3();
   mp3->begin(id3, out);  */   
 }
 
 /**
  * Returns true, as soon as msWait milliseconds have passed.
  * Supply a reference to an unsigned long variable to hold 
  * the previous milliseconds.
  */
 bool waitIsOver(uint32_t &msPrevious, uint32_t msWait)
 {
   return (millis() - msPrevious >= msWait) ? (msPrevious = millis(), true) : false;
 }
 
 
 void setup() 
 {
     Serial.begin(921600);
     pinMode(LED_BUILTIN, OUTPUT);
 
     if (! initWiFi(ssid, password, hostname))
     { 
       log_e("==> Connection to WLAN failed");
       while(true) heartbeat(LED_BUILTIN, 3, 1, 5);
     };
     SPIFFS.begin();
     printNearbyNetworks();
     printConnectionDetails();
     initAudio();
 }
  
 
 void loop()
 {
     static uint32_t msPrevious = millis();
     static bool done = false;
 
     audio.loop();
 
     // show menu once after all status and info messages have been displayed
     if (!done && waitIsOver(msPrevious, 5000)) { done = true; showMenu(""); }
 
     // handle keystrokes and the menu
     if (Serial.available()) doMenu();    
 }
  
 
 // optional event handlers
 void audio_info(const char *info){
     Serial.print("info        "); Serial.println(info);
 }
 void audio_id3data(const char *info){  //id3 metadata
     Serial.print("id3data     ");Serial.println(info);
 }
 void audio_eof_mp3(const char *info){  //end of file
     Serial.print("eof_mp3     ");Serial.println(info);
 }
 void audio_showstation(const char *info){
     Serial.print("station     ");Serial.println(info);
 }
 void audio_showstreaminfo(const char *info){
     Serial.print("streaminfo  ");Serial.println(info);
 }
 void audio_showstreamtitle(const char *info){
     Serial.print("streamtitle ");Serial.println(info);
 }
 void audio_bitrate(const char *info){
     Serial.print("bitrate     ");Serial.println(info);
 }
 void audio_commercial(const char *info){  //duration in sec
     Serial.print("commercial  ");Serial.println(info);
 }
 void audio_icyurl(const char *info){  //homepage
     Serial.print("icyurl      ");Serial.println(info);
 }
 void audio_lasthost(const char *info){  //stream URL played
     Serial.print("lasthost    ");Serial.println(info);
 }
 void audio_eof_speech(const char *info){
     Serial.print("eof_speech  ");Serial.println(info);
 }