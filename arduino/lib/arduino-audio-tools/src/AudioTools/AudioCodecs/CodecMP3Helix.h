#include "AudioTools.h"
#include "AudioTools/AudioLibs/VS1053Stream.h"

URLStream url("ssid","password");  // or replace with ICYStream to get metadata
VS1053Stream vs1053; // final output
StreamCopy copier(vs1053, url); // copy url to decoder

void setup(){
  Serial.begin(115200);
  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Info);  

  // setup vs1053
  auto cfg = vs1053.defaultConfig();
  cfg.is_encoded_data = true; // vs1053 is accepting encoded data
  // Use your custom pins or define in AudioCodnfig.h
  //cfg.cs_pin = VS1053_CS; 
  //cfg.dcs_pin = VS1053_DCS;
  //cfg.dreq_pin = VS1053_DREQ;
  //cfg.reset_pin = VS1053_RESET;
  vs1053.begin(cfg);

  // mp3 radio
  url.begin("http://stream.srg-ssr.ch/m/rsj/mp3_128","audio/mp3");

}

void loop(){
  copier.copy();
}