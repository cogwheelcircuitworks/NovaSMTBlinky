

#include <ATtinyTimer.h>
#include <FontAlphaNum35.h>
#include <FontAlphaNum57.h>
#include <NovaDotMatrix.h>
#include <NovaDotMatrixCommands.h>

/*

  NovaDotmMatrix

  NovaDotmMatrix aka 'NovaSMTBlinky' is a ATTiny micronctroller-based board with 35 LEDs arranged in a dot matrix

  Everything is implemented in libraries/NovaDotMatrix/, so this top-level sketch is just boilerplate.

*/

NovaDotMatrix novadotmatrix; // instantiate a novadotmatrix object

ATtinyTimer attinytimer; // uses the attinytimer library also

void setup() {
  novadotmatrix.Setup(); 
}

void loop() {
  novadotmatrix.Loop(); 
}
