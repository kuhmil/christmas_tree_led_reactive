#include <FastLED.h>

//Libraries
#include <Adafruit_NeoPixel.h>  //Library to simplify interacting with the LED strand
#ifdef __AVR__
#include <avr/power.h>   //Includes the library for power reduction registers if your chip supports them. 
#endif                   //More info: http://www.nongnu.org/avr-libc/user-manual/group__avr__power.htlm

//Constants (change these as necessary)
#define LED_PIN   A5  //Pin for the pixel strand. Does not have to be analog.
#define LED_TOTAL 175  //Change this to the number of LEDs in your strand.
#define LED_HALF  LED_TOTAL/2
#define AUDIO_PIN A0  //Pin for the envelope of the sound detector
#define KNOB_PIN  A1  //Pin for the trimpot 10K
#define BRIGHTNESS 255   /* Control the brightness of your leds */
#define SATURATION 255   /* Control the saturation of your leds */
#define LED_TYPE WS2812B /* I assume you have WS2812B leds, if not just change it to whatever you have */
const int switchPin = 3;
CRGB leds[LED_TOTAL];


//////////<Globals>
//  These values either need to be remembered from the last pass of loop() or 
//  need to be accessed by several functions in one pass, so they need to be global.

Adafruit_NeoPixel strand = Adafruit_NeoPixel(LED_TOTAL, LED_PIN, NEO_GRB + NEO_KHZ800);  //LED strand objetc

uint16_t gradient = 0; //Used to iterate and loop through each color palette gradually

uint8_t volume = 0;    //Holds the volume level read from the sound detector.
uint8_t last = 0;      //Holds the value of volume from the previous loop() pass.

float maxVol = 15;     //Holds the largest volume recorded thus far to proportionally adjust the visual's responsiveness.
float knob = 1023.0;   //Holds the percentage of how twisted the trimpot is. Used for adjusting the max brightness.
float avgVol = 0;      //Holds the "average" volume-level to proportionally adjust the visual experience.
float avgBump = 0;     //Holds the "average" volume-change to trigger a "bump."

bool bump = false;     //Used to pass if there was a "bump" in volume

//////////</Globals>


//////////<Standard Functions>

void setup() {    //Like it's named, this gets ran before any other function.

  Serial.begin(9600); //Sets data rate for serial data transmission.

  strand.begin(); //Initialize the LED strand object.
  strand.show();  //Show a blank strand, just to get the LED's ready for use.  
  FastLED.addLeds<LED_TYPE, LED_PIN>(leds, LED_TOTAL);
  
}


void loop() {  //This is where the magic happens. This loop produces each frame of the visual.
  int switchVal;
  switchVal = digitalRead(switchPin);
//  while(1){
    if (switchVal == LOW) {
      volume = analogRead(AUDIO_PIN);       //Record the volume level from the sound detector
      knob = analogRead(KNOB_PIN) / 1023.0; //Record how far the trimpot is twisted
      avgVol = (avgVol + volume) / 2.0;     //Take our "average" of volumes.
    
      //Sets a threshold for volume.
      //  In practice I've found noise can get up to 15, so if it's lower, the visual thinks it's silent.
      //  Also if the volume is less than average volume / 2 (essentially an average with 0), it's considered silent.
      if (volume < avgVol / 2.0 || volume < 15) volume = 0;
    
      //If the current volume is larger than the loudest value recorded, overwrite
      if (volume > maxVol) maxVol = volume;
    
      //This is where "gradient" is reset to prevent overflow.
      if (gradient > 1529) {
    
        gradient %= 1530;
    
        //Everytime a palette gets completed is a good time to readjust "maxVol," just in case
        //  the song gets quieter; we also don't want to lose brightness intensity permanently 
        //  because of one stray loud sound.
        maxVol = (maxVol + volume) / 2.0;
      }
    
      //If there is a decent change in volume since the last pass, average it into "avgBump"
      if (volume - last > avgVol - last && avgVol - last > 0) avgBump = (avgBump + (volume - last)) / 2.0;
    
      //if there is a notable change in volume, trigger a "bump"
      bump = (volume - last) > avgBump;
    
      Pulse();   //Calls the visual to be displayed with the globals as they are.
    
      gradient++;    //Increments gradient
    
      last = volume; //Records current volume for next pass
    
      delay(30);   //Paces visuals so they aren't too fast to be enjoyable
    }
    else{
      for (int j = 0; j < 255; j++) {
        for (int i = 0; i < LED_TOTAL; i++) {
          leds[i] = CHSV(i - (j * 2), BRIGHTNESS, SATURATION); /* The higher the value 4 the less fade there is and vice versa */ 
        }
        FastLED.show();
        delay(100); /* Change this to your hearts desire, the lower the value the faster your colors move (and vice versa) */
      }
    }
// }
}

//////////</Standard Functions>

//////////<Helper Functions>


//PULSE
//Pulse from center of the strand
void Pulse() {

  fade(0.75);   //Listed below, this function simply dims the colors a little bit each pass of loop()

  //Advances the gradient to the next noticeable color if there is a "bump"
  if (bump) gradient += 64;

  //If it's silent, we want the fade effect to take over, hence this if-statement
  if (volume > 0) {
    uint32_t col = Rainbow(gradient); //Our retrieved 32-bit color

    //These variables determine where to start and end the pulse since it starts from the middle of the strand.
    //  The quantities are stored in variables so they only have to be computed once.
    int start = LED_HALF - (LED_HALF * (volume / maxVol));
    int finish = LED_HALF + (LED_HALF * (volume / maxVol)) + strand.numPixels() % 2;
    //Listed above, LED_HALF is simply half the number of LEDs on your strand. ↑ this part adjusts for an odd quantity.

    for (int i = start; i < finish; i++) {

      //"damp" creates the fade effect of being dimmer the farther the pixel is from the center of the strand.
      //  It returns a value between 0 and 1 that peaks at 1 at the center of the strand and 0 at the ends.
      float damp = float(
                     ((finish - start) / 2.0) -
                     abs((i - start) - ((finish - start) / 2.0))
                   )
                   / float((finish - start) / 2.0);

      //Sets the each pixel on the strand to the appropriate color and intensity
      //  strand.Color() takes 3 values between 0 & 255, and returns a 32-bit integer.
      //  Notice "knob" affecting the brightness, as in the rest of the visuals.
      //  Also notice split() being used to get the red, green, and blue values.
      strand.setPixelColor(i, strand.Color(
                             split(col, 0) * pow(damp, 2.0) * knob,
                             split(col, 1) * pow(damp, 2.0) * knob,
                             split(col, 2) * pow(damp, 2.0) * knob
                           ));
    }
    //Sets the max brightness of all LEDs. If it's loud, it's brighter.
    //  "knob" was not used here because it occasionally caused minor errors in color display.
    strand.setBrightness(255.0 * pow(volume / maxVol, 2));
  }

  //This command actually shows the lights. If you make a new visualization, don't forget this!
  strand.show();
}



//Fades lights by multiplying them by a value between 0 and 1 each pass of loop().
void fade(float damper) {

  //"damper" must be between 0 and 1, or else you'll end up brightening the lights or doing nothing.
  if (damper >= 1) damper = 0.99;

  for (int i = 0; i < strand.numPixels(); i++) {

    //Retrieve the color at the current position.
    uint32_t col = (strand.getPixelColor(i)) ? strand.getPixelColor(i) : strand.Color(0, 0, 0);

    //If it's black, you can't fade that any further.
    if (col == 0) continue;

    float colors[3]; //Array of the three RGB values

    //Multiply each value by "damper"
    for (int j = 0; j < 3; j++) colors[j] = split(col, j) * damper;

    //Set the dampened colors back to their spot.
    strand.setPixelColor(i, strand.Color(colors[0] , colors[1], colors[2]));
  }
}


uint8_t split(uint32_t color, uint8_t i ) {

  //0 = Red, 1 = Green, 2 = Blue

  if (i == 0) return color >> 16;
  if (i == 1) return color >> 8;
  if (i == 2) return color >> 0;
  return -1;
}


//This function simply take a value and returns a gradient color
//  in the form of an unsigned 32-bit integer

//The gradient returns a different, changing color for each multiple of 255
//  This is because the max value of any of the 3 LEDs is 255, so it's
//  an intuitive cutoff for the next color to start appearing.
//  Gradients should also loop back to their starting color so there's no jumps in color.

uint32_t Rainbow(unsigned int i) {
  if (i > 1529) return Rainbow(i % 1530);
  if (i > 1274) return strand.Color(255, 0, 255 - (i % 255));   //violet -> red
  if (i > 1019) return strand.Color((i % 255), 0, 255);         //blue -> violet
  if (i > 764) return strand.Color(0, 255 - (i % 255), 255);    //aqua -> blue
  if (i > 509) return strand.Color(0, 255, (i % 255));          //green -> aqua
  if (i > 255) return strand.Color(255 - (i % 255), 255, 0);    //yellow -> green
  if (i < 255) return strand.Color(255,255,255);
  //else return Rainbow(i % 1530);
  
  return strand.Color(255, i, 0);                               //red -> yellow
}

//////////</Helper Functions>
