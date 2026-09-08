#include "USB.h"
#include "USBHIDConsumerControl.h"
#include <TFT_eSPI.h>       
#include <LittleFS.h>
#include <Preferences.h>

#define ERSINKEDIFPS 60 
#define TWOBFPS 120
#define ZEROTWOFPS 60
#define CIRILLAFPS 20
#define LUCYCIGARETTEFPS 20
#define LUCYDAVIDMOONFPS 5
#define LUCYBGFPS 5

Preferences preferences;
TFT_eSPI tft = TFT_eSPI();  
TFT_eSprite sprit = TFT_eSprite(&tft);
USBHIDConsumerControl ConsumerControl;

// --- Global Image Buffer ---
uint16_t* frameBuffer; 
uint16_t* volumeupBuffer;
uint16_t* volumedownBuffer;
uint16_t* volumeBuffer;
uint16_t* volumeselectBuffer;
uint16_t* gifselectBuffer;
uint16_t* uielements[2];


uint16_t uicolor = 0x738e;

        int overlayX = 35;
        int overlayY = 35;
        int overlayW = 170;
        int overlayH = 170;
        int feather = 5; // Width of the 25% dimmed outer alpha border

        int overlayvolumeX = 87;   // 97
        int overlayvolumeY = 20;   // 30
        int overlayvolumeW = 66;   // 46
        int overlayvolumeH = 53;   // 33
        
        int tilesize = 50;
        int padding = 2;
        int uipositions[9][2] = { 
          {overlayX+feather +tilesize*0,overlayY+feather +tilesize*0},
          {overlayX+feather +tilesize*1,overlayY+feather +tilesize*0},
          {overlayX+feather +tilesize*2,overlayY+feather +tilesize*0},
          {overlayX+feather +tilesize*0,overlayY+feather +tilesize*1},
          {overlayX+feather +tilesize*1,overlayY+feather +tilesize*1},
          {overlayX+feather +tilesize*2,overlayY+feather +tilesize*1},
          {overlayX+feather +tilesize*0,overlayY+feather +tilesize*2},
          {overlayX+feather +tilesize*1,overlayY+feather +tilesize*2},
          {overlayX+feather +tilesize*2,overlayY+feather +tilesize*2} 
          
          };
          int activeuielements= 2;
          int selecteduielement = 0;

          bool buttonpressed = false;
          bool volumechangemode = false;
          bool gifchangemode = false;
          bool nomode = true;

float actualFPS = 0.0;
unsigned long frameCount = 0;
unsigned long lastFPSTime = 0;



int currentFrame = 0;
int totalFrames = 22; // Update this number as you upload more frames!
bool doublespeed = false;
int curgif = 0;
int gifcount = 5;
// --- Rotary Encoder Setup ---
const int pinA = 5; 
const int pinB = 4; 
volatile int encoderSteps = 0; 
const int8_t encoderStates[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

void IRAM_ATTR readEncoder() {
  static uint8_t old_AB = 3; 
  static int8_t internalState = 0; 
  old_AB <<= 2; 
  old_AB |= (digitalRead(pinA) << 1) | digitalRead(pinB); 
  internalState += encoderStates[old_AB & 0x0F];

  if (internalState > 3) {
    encoderSteps++;
    internalState = 0;
  } else if (internalState < -3) {
    encoderSteps--;
    internalState = 0;
  }
}

// --- External Push Button Media Pins ---
const int btnPausePin = 6; 
const int btnNextPin  = 7; 
const int btnPrevPin  = 8; 
const int btnChangeAnim = 17;



// --- Display & Volume Tracking ---
int virtualVolume = 25; 
const int maxVolumeSteps = 50;

int lastvirtualVolume = 0;

unsigned long lastButtonCheck = 0;
unsigned long lastTime = 0;
unsigned long lastsaverTime = 0;
unsigned long lastinteraction = 0;


const unsigned long buttonInterval = 50;
unsigned long animInterval = 150;
unsigned long saverInterval = (15*1000);

void setup() {
  Serial.begin(115200);

  preferences.begin("storage", false);
  // virtualVolume = preferences.getInt("volume", 15);                                                                           //perma save uncomment
  
  lastvirtualVolume = virtualVolume;
  
  tft.init();
  tft.setRotation(0);
  tft.setSwapBytes(false);
  tft.fillScreen(TFT_BLACK);
  sprit.createSprite(240, 240);
  sprit.setSwapBytes(true);
  
  
  ConsumerControl.begin();
  USB.begin();

  // Encoder configuration
  pinMode(pinA, INPUT_PULLUP);
  pinMode(pinB, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pinA), readEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pinB), readEncoder, CHANGE);

  // External Push Button configuration
  pinMode(btnPausePin, INPUT_PULLUP);
  pinMode(btnNextPin, INPUT_PULLUP);
  pinMode(btnPrevPin, INPUT_PULLUP);
  pinMode(btnChangeAnim, INPUT_PULLUP);

  // --- LittleFS & Memory Setup ---
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
  } else {
    // Calculate and print remaining storage space
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    size_t freeBytes = totalBytes - usedBytes;
    int framesLeft = freeBytes / 115200; // 240x240 image
    
    Serial.println("--- Storage Info ---");
    Serial.printf("Total Space: %.2f MB\n", totalBytes / (1024.0 * 1024.0));
    Serial.printf("Used Space:  %.2f MB\n", usedBytes / (1024.0 * 1024.0));
    Serial.printf("Free Space:  %.2f MB\n", freeBytes / (1024.0 * 1024.0));
    Serial.printf("You can fit approximately %d more frames!\n", framesLeft);
    Serial.println("--------------------");
  }

  // Allocate 240x240 x 2 for byte on the heap once to prevent memory fragmentation

  frameBuffer = (uint16_t*)ps_malloc(240 * 240 * 2);
  volumeBuffer = (uint16_t*)ps_malloc(46 * 33 * 2);

  volumeupBuffer = (uint16_t*)ps_malloc(46 * 33 * 2);
  volumedownBuffer = (uint16_t*)ps_malloc(46 * 33 * 2);

  volumeselectBuffer = (uint16_t*)ps_malloc(45 * 45 * 2);
  gifselectBuffer = (uint16_t*)ps_malloc(45 * 45 * 2);

uielements[0] = volumeselectBuffer;
uielements[1] = gifselectBuffer;

  if (frameBuffer == NULL) {
    Serial.println("Error: PSRAM allocation failed! Check IDE settings.");
  }
   if (volumeupBuffer == NULL) {
    Serial.println("Error: PSRAM allocation failed! Check IDE settings.");
  }
     if (volumedownBuffer == NULL) {
    Serial.println("Error: PSRAM allocation failed! Check IDE settings.");
  }
  
    //   int fps = map(virtualVolume, 0, maxVolumeSteps, 10, 100);
    // if(fps > 60){currentFrame++; doublespeed = true;} else{doublespeed = false;}
    // animInterval = 1000 / (doublespeed?fps/4:fps);

    if (volumeupBuffer != NULL) {
      fs::File file = LittleFS.open("/volume_1.raw", "r");
      if (file) {
        file.read((uint8_t*)volumeupBuffer, 46 * 33 *2);
        file.close();
      } else {
        Serial.println("Warning: Could not open volumeup! Upload the file to the flash again");
      }
    }
        if (volumedownBuffer != NULL) {
      fs::File file = LittleFS.open("/volume_0.raw", "r");
      if (file) {
        file.read((uint8_t*)volumedownBuffer, 46 * 33 *2);
        file.close();
      } else {
        Serial.println("Warning: Could not open volumedown! Upload the file to the flash again");
      }
    }
       if (volumeselectBuffer != NULL) {
      fs::File file = LittleFS.open("/volumeselect.raw", "r");
      if (file) {
        file.read((uint8_t*)volumeselectBuffer, 45 * 45 *2);
        file.close();
      } else {
        Serial.println("Warning: Could not open volumedown! Upload the file to the flash again");
      }
    }
       if (gifselectBuffer != NULL) {
      fs::File file = LittleFS.open("/gifselect.raw", "r");
      if (file) {
        file.read((uint8_t*)gifselectBuffer, 45 * 45 *2);
        file.close();
      } else {
        Serial.println("Warning: Could not open volumedown! Upload the file to the flash again");
      }
    }

}

void loop() {
  unsigned long currentTime = millis();
  unsigned long currentanimTime = currentTime;
  
  
  // 1. Handle Rotary Encoder Volume Changes
  int currentSteps = 0;
  noInterrupts(); 
  currentSteps = encoderSteps;
  encoderSteps = 0;
  interrupts();



  if (currentSteps != 0) {
    if (currentSteps > 0) {
      for (int i = 0; i < currentSteps; i++) {
        if(volumechangemode){
        ConsumerControl.press(CONSUMER_CONTROL_VOLUME_INCREMENT);
        delay(5);
        ConsumerControl.release();
        memcpy(volumeBuffer, volumeupBuffer, 46 * 33 * 2);

        }else if(gifchangemode){
          curgif++;
          currentFrame = 0;
          if(curgif>=gifcount){curgif = 0;}
        }
        else{selecteduielement++;
}
        // ConsumerControl.press(CONSUMER_CONTROL_VOLUME_INCREMENT);
        // delay(20);
        // ConsumerControl.release();
        // delay(20);
        // virtualVolume++;
       

        //center 120 30
      Serial.println("up");
//tft.pushImage(97,15,46,33,volumeupBuffer,0xFFFF);
      }
    } else {
      int stepsDown = -currentSteps;
      for (int i = 0; i < stepsDown; i++) {


if(volumechangemode){

        ConsumerControl.press(CONSUMER_CONTROL_VOLUME_DECREMENT);
        delay(5);
        ConsumerControl.release();
       memcpy(volumeBuffer, volumedownBuffer, 46 * 33 * 2);
}else if(gifchangemode){
          curgif-=1;
          currentFrame = 0;
          if(curgif<0){curgif = gifcount-1;}
        }

else{selecteduielement-=1;
}
        
        // ConsumerControl.press(CONSUMER_CONTROL_VOLUME_DECREMENT);
        // delay(20);
        // ConsumerControl.release();
        // delay(20);
      
      // virtualVolume -= 1;
      
     Serial.println("down");
//tft.pushImage(97,15,46,33,volumedownBuffer,0xFFFF);
      }

    }


if(selecteduielement>=activeuielements){
selecteduielement = 0;

}else if(selecteduielement<0){
selecteduielement = activeuielements-1;

}

    // virtualVolume = constrain(virtualVolume, 0, maxVolumeSteps);
    
    lastinteraction = millis();
    Serial.println("lastint");


    // Dynamically adjust cat speed based on volume level
   // int fps = map(virtualVolume, 0, maxVolumeSteps, 5, 120);
   //if(fps > 60){currentFrame++; doublespeed = true;} else{doublespeed = false;}
    //animInterval =(doublespeed ? 1000 / (fps/3) : 1000 / fps);
   //animInterval =1000 / fps;

  }

  // 2. Handle External Push Buttons (Pause, Next, Previous)
  if (currentTime - lastButtonCheck >= buttonInterval) {
    lastButtonCheck = currentTime;
    if (digitalRead(btnPausePin) == LOW) {
      ConsumerControl.press(CONSUMER_CONTROL_PLAY_PAUSE);
      delay(4);
      ConsumerControl.release();
      delay(200); 
    }
    else if (digitalRead(btnNextPin) == LOW) {
      ConsumerControl.press(CONSUMER_CONTROL_SCAN_NEXT);
      delay(4);
      ConsumerControl.release();
      delay(200);
    }
    else if (digitalRead(btnPrevPin) == LOW) {
      ConsumerControl.press(CONSUMER_CONTROL_SCAN_PREVIOUS);
      delay(4);
      ConsumerControl.release();
      delay(200);
    }
        else if (digitalRead(btnChangeAnim) == LOW) {
      //curgif++;
      // currentFrame = 0;
      // if(curgif>=gifcount){curgif = 0;}

      buttonpressed = true;
      lastinteraction = millis();


      if(volumechangemode){
volumechangemode =false;
Serial.println("volume change mode off");
      }
      if(gifchangemode){
gifchangemode =false;
Serial.println("gif change mode off");
      }if(!nomode){
        nomode = true;
        buttonpressed = false;

      }
      

      //  Serial.println(curgif);
      delay(200);
    }
  }

  // 3. Render Animation and Volume Arc from LittleFS
  if (currentanimTime - lastTime >= animInterval) {
    lastTime = currentanimTime;

    // Track frame count
  frameCount++;

  // Calculate and print FPS every 1 second
  if (currentanimTime - lastFPSTime >= 1000) {
    actualFPS = (frameCount * 1000.0) / (currentanimTime - lastFPSTime);
    int targetFPS = 1000 / animInterval;

   // Serial.printf("[FPS] Target: %d | Actual: %.1f\n", targetFPS, actualFPS);

    frameCount = 0;
    lastFPSTime = currentanimTime;
  }
    if (currentFrame >= totalFrames) { currentFrame = 0; }
    
    char filename[32];
    if(curgif == 0){
    sprintf(filename, "/cirilla_%d.raw", currentFrame);
    totalFrames = 19;
      uicolor = 0x738e;
        int fps = CIRILLAFPS;

   animInterval =1000 / fps;

    }else if (curgif == 1){
      sprintf(filename, "/2b_%d.raw", currentFrame);
    totalFrames = 15;
    uicolor = 0x501f;
    int fps = TWOBFPS;

   animInterval =1000 / fps;

    }else if (curgif == 2){
      sprintf(filename, "/lucycigarette_%d.raw", currentFrame);
    totalFrames = 24;
    uicolor = 0xb81f;
        int fps = LUCYCIGARETTEFPS;

   animInterval =1000 / fps;
   

    }else if (curgif == 3){
      sprintf(filename, "/lucydavidmoon_%d.raw", currentFrame);
    totalFrames = 0;
    uicolor = 0xc617;
        int fps = LUCYDAVIDMOONFPS;

   animInterval =1000 / fps;
   

    }else if (curgif == 4){
      sprintf(filename, "/lucybg_%d.raw", currentFrame);
    totalFrames = 0;
    uicolor = 0x4fe4;
        int fps = LUCYBGFPS;

   animInterval =1000 / fps;
   

    }


    
    // Create the dynamic filename (e.g., "/cat_0.raw", "/cat_1.raw")
    
    // Read the file directly into our pre-allocated memory buffer
    if (frameBuffer != NULL) {
      fs::File file = LittleFS.open(filename, "r");
      if (file) {
        file.read((uint8_t*)frameBuffer, 240 * 240 *2);
        file.close();
      } else {
        Serial.printf("Warning: Could not open %s\n", filename);
      }
    }

    

if (frameBuffer != NULL) {
      
      // --- 1. THE ALPHA BLEND DIMMING ---
      // Check if the volume knob was turned within the last 2 seconds
if (millis() - lastinteraction < 3000) {


if(nomode){


  for (int y = overlayY; y < overlayY + overlayH; y++) {
    for (int x = overlayX; x < overlayX + overlayW; x++) {
      int index = (y * 240) + x;
      uint16_t p = frameBuffer[index];

      // Check if pixel is within the outer feather region
      if (x < overlayX + feather || x >= overlayX + overlayW - feather ||
          y < overlayY + feather || y >= overlayY + overlayH - feather) {
        // Outer Border: 25% Darkened
        frameBuffer[index] = ((p & 0xF7DE) >> 1) + ((p & 0xE79C) >> 2);
      } else {
        // Inner Center: 50% Darkened
        frameBuffer[index] = (p & 0xF7DE) >> 1;
      }
    }
  }

}

}


      // --- 2. PUSH TO SPRITE ---
      // Push the (now partially darkened) image into the sprite
      sprit.pushImage(0, 0, 240, 240, frameBuffer);

      // --- 3. DRAW SOLID UI ON TOP ---
      if (millis() - lastinteraction < 3000) {
     


        // Draw a crisp border around the dimmed area (optional but looks great)
        if(nomode){
      if(selecteduielement == 0){

      
      for (int index = 0; index< 45 * 45;index++){
      
      if(volumeselectBuffer[index] == 0xFFFF ){
        volumeselectBuffer[index] = (uicolor^0xDDDD);
      }
         if(gifselectBuffer[index] == 0xFFFF){
        gifselectBuffer[index] = uicolor;
      }

      }
     
     }else if (selecteduielement == 1){

        for (int index = 0; index< 45 * 45;index++){
             if(gifselectBuffer[index] == 0xFFFF){
               gifselectBuffer[index] = (uicolor^0xDDDD);
             }
             if(volumeselectBuffer[index] == 0xFFFF ){
           volumeselectBuffer[index] = uicolor;
      }
      
        }
     
     
     }
   uielements[0] = volumeselectBuffer;
  uielements[1] = gifselectBuffer;
        
        sprit.drawRoundRect(overlayX+feather, overlayY+feather, overlayW-(2*feather), overlayH-(2*feather),5, uicolor);
        for(int i = 0; i< activeuielements;i++){                                       //rectangles filler for testing

sprit.fillRoundRect(uipositions[i][0]+padding,uipositions[i][1]+padding,tilesize-(padding*2),tilesize-(padding*2), 10,uicolor);
}
sprit.fillRoundRect(uipositions[selecteduielement][0],uipositions[selecteduielement][1],tilesize,tilesize, 10, (uicolor^0xDDDD));

              for(int i = 0; i< activeuielements;i++){                                       //rectangles filler for testing
Serial.println(uicolor, HEX);
sprit.pushImage(uipositions[i][0]+padding,uipositions[i][1]+padding,45,45,uielements[i]);
}
       if (volumeselectBuffer != NULL) {
      fs::File file = LittleFS.open("/volumeselect.raw", "r");
      if (file) {
        file.read((uint8_t*)volumeselectBuffer, 45 * 45 *2);
        file.close();
      } else {
        Serial.println("Warning: Could not open volumedown! Upload the file to the flash again");
      }
    }
       if (gifselectBuffer != NULL) {
      fs::File file = LittleFS.open("/gifselect.raw", "r");
      if (file) {
        file.read((uint8_t*)gifselectBuffer, 45 * 45 *2);
        file.close();
      } else {
        Serial.println("Warning: Could not open volumedown! Upload the file to the flash again");
      }
    }

        // Map the volume to the width of our bar (leave 4px padding inside the border)
      //  int barWidth = map(virtualVolume, 0, maxVolumeSteps, 0, 192);



        }else if(volumechangemode){

            for (int y = overlayvolumeY; y < overlayvolumeY + overlayvolumeH; y++) {
    for (int x = overlayvolumeX; x < overlayvolumeX + overlayvolumeW; x++) {
      int index = (y * 240) + x;
      uint16_t p = frameBuffer[index];

      // Check if pixel is within the outer feather region
      if (x < overlayvolumeX + feather || x >= overlayvolumeX + overlayvolumeW - feather ||
          y < overlayvolumeY + feather || y >= overlayvolumeY + overlayvolumeH - feather) {
        // Outer Border: 25% Darkened
        frameBuffer[index] = ((p & 0xF7DE) >> 1) + ((p & 0xE79C) >> 2);
      } else {
        // Inner Center: 50% Darkened
        frameBuffer[index] = (p & 0xF7DE) >> 1;
      }
    }
  } 

          for (int index = 0; index< 46 * 33;index++){
             if(volumeBuffer[index] == 0xFFFF){
               volumeBuffer[index] = uicolor;
             }
            
      
        }
        currentSteps++;
sprit.pushImage(0, 0, 240, 240, frameBuffer);

sprit.drawRoundRect(overlayvolumeX+feather, overlayvolumeY+feather, overlayvolumeW-(2*feather), overlayvolumeH-(2*feather),5, (uicolor^0xDDDD));


sprit.pushImage(97,30,46,33,volumeBuffer,0xFFFF);

  }else if(gifchangemode){
    sprit.setTextColor(uicolor);


    sprit.drawSmoothCircle(120, 120, 119, uicolor, 0x0000);
    // sprit.drawSmoothCircle(120, 120, 115, uicolor, 0x0000);

    sprit.drawCentreString(String(curgif+1),120,120,7);


  }



        




        // Draw the solid white volume indicator inside the dimmed glass box
       // sprit.fillRect(24, 194, barWidth, 22, TFT_WHITE);
       if(buttonpressed && nomode){

       
        switch(selecteduielement){
          case 0: 
          
          volumechangemode = true;
          Serial.println("volume change mode on");
          buttonpressed = false;
          
          break;
          case 1:
          gifchangemode = true;
          Serial.println("gif change mode on");
          buttonpressed = false;
          break;
          default:
          break;


        }
        nomode = false;
        
        }


      }

      // 4. Push the fully composited image to the screen
      sprit.pushSprite(0, 0);
    }
    
    currentFrame++;
   // if(doublespeed == true){currentFrame++;}
  
  }
  if(lastvirtualVolume != virtualVolume){
    unsigned long currentsavertime = millis();

  if (currentsavertime - lastinteraction >= saverInterval) {

   // preferences.putInt("volume", virtualVolume);                                                                          //perma save uncomment
    Serial.println("current volume not saved uncomment!");
    lastvirtualVolume = virtualVolume;

  }

  }


}