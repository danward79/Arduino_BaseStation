//Various Utility functions used in places.
//toFahreinheit
float toFahrenheit (float t)
{
	return ((t * 9/5) + 32);
}

//cloudBase calculation
float calculateCloudBase (float t, float d)
{ 
  return ( (t - d) * 400 );
}

float calculateDewpoint(float h, float t)
{
	float k = (log10(h)-2)/0.4343 + (17.62*t)/(243.12+t); 
	
	return 243.12*k/(17.62-k); 
}

//Report freeram
int freeRam () {
  extern int __heap_start, *__brkval; 
  int v; 

  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

//BST Start Calculation
int calculateBSTStartDate (int y)
{
	/*
	European Summer Time begins (clocks go forward) at 01:00 GMT on
	27 March 2011
	25 March 2012
	31 March 2013
	30 March 2014
	Formula used to calculate the beginning of European Summer Time:
	Sunday (31 - (5 * y ÷ 4 + 4) mod 7) March at 01:00 GMT
	(valid until 2099[4]), where y is the year, and for the nonnegative a, a mod b is the remainder of division after truncating both operands to an integer.
	*/
	
	return (31 - (5 * y / 4 + 4) % 7);
}

//BST End Calculation
int calculateBSTEndDate (int y)
{
	/*
	European Summer Time ends (clocks go backward) at 01:00 GMT on
	30 October 2011
	28 October 2012
	27 October 2013
	26 October 2014
	Formula used to calculate the end of European Summer Time:
	Sunday (31 - (5 * y ÷ 4 + 1) mod 7) October at 01:00 GMT
	*/
	
	return (31 - (5 * y / 4 + 1) % 7);
}