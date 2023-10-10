// Low power NeoPixel goggles example.  Makes a nice blinky display
// with just a few LEDs on at any time.

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_LSM303_U.h>
#include <Adafruit_NeoPixel.h>

#define PIN 12

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(32, PIN, NEO_GRB + NEO_KHZ800);
Adafruit_LSM303_Accel_Unified accel = Adafruit_LSM303_Accel_Unified(54321);
Adafruit_LSM303_Mag_Unified mag = Adafruit_LSM303_Mag_Unified(12345);

uint8_t  mode   = 1, // Current animation effect
         offset = 0; // Position of spinny eyes

uint8_t NUM_MODES = 5; // Number of modes to inform mode changes

uint8_t delay_time = 50; // Default delay
uint32_t color  = pixels.Color(102, 0, 255); // Start red
uint32_t prevTime;

float pos = 8;  // Starting center position of pupil
float increment = 2 * 3.14159 / 16; // distance between pixels in radians
float MomentumH = 0; // horizontal component of pupil rotational inertia
float MomentumV = 0; // vertical component of pupil rotational inertia

// Tuning constants. (a.k.a. "Fudge Factors)
// These can be tweaked to adjust the liveliness and sensitivity of the eyes.
const float friction = 0.995; // frictional damping constant.  1.0 is no friction.
const float swing = 60;  // arbitrary divisor for gravitational force
const float gravity = 200;  // arbitrary divisor for lateral acceleration
const float nod = 7.5; // accelerometer threshold for toggling modes

long nodStart = 0;
long nodTime = 2000;
long baseNumber = 0;

bool antiGravity = false;  // The pendulum will anti-gravitate to the top.
bool mirroredEyes = false; // The left eye will mirror the right.

const float halfWidth = 1.25; // half-width of pupil (in pixels)

// Pi for calculations - not the raspberry type
const float Pi = 3.14159;

void setup() {
  pixels.begin();
  pixels.show();
  pixels.setBrightness(85); // 1/3 brightness
  prevTime = millis();      
  
  // Initialize the sensors
  accel.begin();
  mag.begin();

  resetModes();
}

void patternPendulum(uint32_t color, float pos) {
   // Now re-compute the display
   for (int i = 0; i < 16; i++)
   {
      // Compute the distance bewteen the pixel and the center
      // point of the virtual pendulum.
      float diff = i - pos;
 
      // Light up nearby pixels proportional to their proximity to 'pos'
      if (fabs(diff) <= halfWidth) 
      {
         // do both eyes
         pixels.setPixelColor(i, color);
         if (mirroredEyes)
         {
           pixels.setPixelColor(31 - i, color);
         }
         else
         {
           pixels.setPixelColor(i + 16, color);
         }
      }
      else // all others are off
      {
         pixels.setPixelColor(i, 0);
         if (mirroredEyes)
         {
           pixels.setPixelColor(31 - i, 0);
         }
         else
         {
           pixels.setPixelColor(i + 16, 0);
         }
      }
   }
   // Now show it!
   pixels.show();
}

void patternSpin(uint32_t color) {
  uint8_t  i;
  uint32_t t;
  for (i = 0; i < 16; i++) {
    uint32_t c = 0;
    if (((offset + i) & 7) < 2) c = color; // 4 pixels on...
    pixels.setPixelColor(   i, c); // First eye
    pixels.setPixelColor(31 - i, c); // Second eye (flipped)
  }
  pixels.show();
  offset++;
  delay(delay_time);
  return;
}

void rainbowRing() {
  uint8_t r = random(0,255);
  uint8_t g = random(0,255);
  uint8_t b = random(0,255);
  color = pixels.Color(r,g,b);
  int position = 0;
  for (int i = 0; i < 16; i++) {
    pixels.setPixelColor((i + position) % 16, color);
    pixels.setPixelColor(31-(i + position), color);
    pixels.show();
    delay(delay_time);
  }
  position++;
  position %= 16;
}

void rainbowSparkle() {
  uint8_t r = random(0,255);
  uint8_t g = random(0,255);
  uint8_t b = random(0,255);
  color = pixels.Color(r,g,b);
  //int position = 0;
  for (int i = 0; i < 16; i++) {
    uint8_t eye_1 = random(0,15);
    uint8_t eye_2 = 31 - (random(0,15)); 
    pixels.setPixelColor(eye_1, color);
    pixels.setPixelColor(eye_2, color);
    pixels.show();
    delay(delay_time);
    pixels.setPixelColor(eye_1, 0);
    pixels.setPixelColor(eye_2, 0);
    pixels.show();
  }
  //position++;
  //position %= 16;
}

void setPixelHeatColor (int Pixel, byte temperature) {
  // Scale 'heat' down from 0-255 to 0-191
  byte t192 = round((temperature/255.0)*191);
 
  // calculate ramp up from
  byte heatramp = t192 & 0x3F; // 0..63
  heatramp <<= 2; // scale up to 0..252
 
  // figure out which third of the spectrum we're in:
  if( t192 > 0x80) {                     // hottest
    pixels.setPixelColor(Pixel, pixels.Color(255, 255, heatramp));
    pixels.setPixelColor(31 - Pixel, pixels.Color(255, 255, heatramp));
  } else if( t192 > 0x40 ) {             // middle
    pixels.setPixelColor(Pixel, pixels.Color(255, heatramp, 0));
    pixels.setPixelColor(31 - Pixel, pixels.Color(255, heatramp, 0));
  } else {                               // coolest
    pixels.setPixelColor(Pixel, pixels.Color(heatramp, 0, 0));
    pixels.setPixelColor(31 - Pixel, pixels.Color(heatramp, 0, 0));
  }
}

void Fire(int Cooling, int Sparking, int SpeedDelay) {
  uint8_t NUM_LEDS = 16;
  static byte heat[16];
  int cooldown;
 
  // Step 1.  Cool down every cell a little
  for( int i = 0; i < NUM_LEDS; i++) {
    cooldown = random(0, ((Cooling * 10) / NUM_LEDS) + 2);
   
    if(cooldown>heat[i]) {
      heat[i]=0;
    } else {
      heat[i]=heat[i]-cooldown;
    }
  }
 
  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for( int k= NUM_LEDS - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
  }
   
  // Step 3.  Randomly ignite new 'sparks' near the bottom
  if( random(255) < Sparking ) {
    int y = random(7);
    heat[y] = heat[y] + random(160,255);
    //heat[y] = random(160,255);
  }

  // Step 4.  Convert heat to LED colors
  for( int j = 0; j < NUM_LEDS; j++) {
    setPixelHeatColor(j, heat[j] );
    //setPixelHeatColor(NUM_LEDS - j, heat[j] );
  }

  pixels.show();
  delay(SpeedDelay);
}

// Reset to default
void resetModes()
{
  antiGravity = false;
  mirroredEyes = false;

  /// spin-up
  spin(pixels.Color(255, 0, 0), 1, 500);
  spin(pixels.Color(0, 255, 0), 1, 500);
  spin(pixels.Color(0, 0, 255), 1, 500);
  spinUp();
}

// gradual spin up
void spinUp()
{
  for (int i = 300; i > 0;  i -= 20)
  {
    spin(pixels.Color(255, 255, 255), 1, i);
  }
  pos = 0;
  // leave it with some momentum and let it 'coast' to a stop
  MomentumH = 3;
}

// Gradual spin down
void spinDown()
{
  for (int i = 1; i < 300; i++)
  {
    spin(pixels.Color(255, 255, 255), 1, i += 20);
  }
  // Stop it dead at the top and let it swing to the bottom on its own
  pos = 0;
  MomentumH = MomentumV = 0;
}


// utility function for feedback on mode changes.
void spin(uint32_t color, int count, int time)
{
  for (int j = 0; j < count; j++)
  {
    for (int i = 0; i < 16; i++)
    {
      pixels.setPixelColor(i, color);
      pixels.setPixelColor(31 - i, color);
      pixels.show();
      delay(max(time / 16, 1));
      pixels.setPixelColor(i, 0);
      pixels.setPixelColor(31 - i, 0);
      pixels.show();
    }
  }
}

// choose a color based on the compass heading and proximity to "pos".
uint32_t selectColor(float heading)
{
  uint32_t newColor;
  long baseNumber = random(0, 100);

  // Choose eye color based on the compass heading
  if (heading < 60) //yellow
  {
    newColor = pixels.Color(255, 255, baseNumber);
  }
  else if (heading < 120) //green
  {
    newColor = pixels.Color(0, 255, baseNumber);
  }
  else if (heading < 180) //blue
  {
    newColor = pixels.Color(0, baseNumber, 255);
  }
  else if (heading < 240) //red
  {
    newColor = pixels.Color(255, 0, baseNumber);
  }
  else if (heading < 300) //purple
  {
    newColor = pixels.Color(102, baseNumber, 255);
  }
  else // 300-360 //orange
  {
    newColor = pixels.Color(255, 128, baseNumber);
  }

  return newColor;
}

// monitor orientation for mode-change 'gestures'
void CheckForNods(sensors_event_t event)
{
  if (event.acceleration.x > nod)
  {
    if (millis() - nodStart > nodTime)
    {
      nodStart = millis(); // reset timer
      spinDown();
      mode = mode - 1;
      if (mode < 1) {
        mode = (NUM_MODES);
      }
    }
  }
  else if (event.acceleration.x < -(nod + 1))
  {
    if (millis() - nodStart > nodTime)
    {
      spinUp();
      nodStart = millis(); // reset timer
      mode = mode + 1;
      if (mode > (NUM_MODES)) {
        mode = 1;
      }
    }
  }
  else if (event.acceleration.y > nod)
  {
    if (millis() - nodStart > nodTime)
    {
      antiGravity = false;
      spinUp();
      nodStart = millis(); // reset timer
      delay_time = delay_time * 2;
      if (delay_time > 500)
      {
        delay_time = 500;
      }
    }
  }
  else if (event.acceleration.y < -nod)
  {
    if (millis() - nodStart > nodTime)
    {
      antiGravity = true;
      spinDown();
      nodStart = millis(); // reset timer
      delay_time = delay_time / 2;
      if (delay_time < 5)
      {
        delay_time = 5;
      }
    }
  }
  else // no nods in progress
  {
    nodStart = millis(); // reset timer
  }
}


void loop() {

  // Read the magnetometer and determine the compass heading:
  sensors_event_t event;
  mag.getEvent(&event);

  // Calculate the angle of the vector y,x from magnetic North
  float heading = (atan2(event.magnetic.y, event.magnetic.x) * 180) / Pi;

  // Normalize to 0-360 for a compass heading
  if (heading < 0)
  {
    heading = 360 + heading;
  }

  // Now read the accelerometer to control the motion.
  accel.getEvent(&event);

  // Check for mode change commands
  CheckForNods(event);

  // apply a little frictional damping to keep things in control and prevent perpetual motion
  MomentumH *= friction;
  MomentumV *= friction;

  // Calculate the horizontal and vertical effect on the virtual pendulum
  // 'pos' is a pixel address, so we multiply by 'increment' to get radians.
  float TorqueH = cos(pos * increment);  // peaks at top and bottom of the swing
  float TorqueV = sin(pos * increment);    // peaks when the pendulum is horizontal

  // Add the incremental acceleration to the existing momentum
  // This code assumes that the accelerometer is mounted upside-down, level
  // and with the X-axis pointed forward.  So the Y axis reads the horizontal
  // acceleration and the inverse of the Z axis is gravity.
  // For other orientations of the sensor, just change the axis to match.
  MomentumH += TorqueH * event.acceleration.y / swing;
  if (antiGravity)
  {
    MomentumV += TorqueV * event.acceleration.z / gravity;
  }
  else
  {
    MomentumV -= TorqueV * event.acceleration.z / gravity;
  }

  // Calculate the new position
  pos += MomentumH + MomentumV;

  // handle the wrap-arounds at the top
  while (round(pos) < 0) pos += 16.0;
  while (round(pos) > 15) pos -= 16.0;
  
  color = selectColor(heading);
  switch (mode) {

    case 1: // Spinny wheels (8 LEDs on at a time)
      patternSpin(color);
      break;

    case 2: //
      rainbowSparkle();
      break;

    case 3: //
      rainbowRing();
      break;
      
    case 4: // Pendulum
      patternPendulum(color, pos);
      break;
      
    case 5:
      Fire(55,120,15);
      break;
  }
}
