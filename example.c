/*
 This example is not meant to be compiled without errors, but just some lines of code to show how to use the functions.  The
 code basically samples audio at 16 khz, runs a decimation filter, and outputs audio to a I2C D/A converter at 8 khz rate.  If
 the wire library were used, you would see on a scope that you are using 3/4 of your processing time just waiting on the 
 I2C writes to the D/A audio out.  The routines here do not waste time.
 */
 
 #define D2A_AUDIO  0x63  //  audio output address
 
// polling I2C implementation
#define I2BUFSIZE 128
int i2buf[I2BUFSIZE];
int i2in, i2out;

setup(){

 pinMode( 45, INPUT );
 pinMode( 46, INPUT );  // ?? needed for i2c, I do not know if these are needed
 
  I2C1CONSET = 0x8000;      // I2C on bit
  I2C1BRG = 90;             // 400 khz
  i2stop();                 // clear any extraneous info that devices may have detected on powerup
  
  i2cd( D2A_AUDIO, 8, 0x00 );  // half scale audio out 0x800 for 12 bit d/a converter

  attachCoreTimerService( dsp_core );        // audio processing

  i2flush();   // although we didn't write anywhere near 128 entries into the queue with this example, if
               // we did have many I2C devices to initialize, we would call i2flush to empty the queue when needed.
}

loop(){

     i2poll();       // keep the i2c bus running
     
     if( audio_flag ){
    // digitalWrite( BACKLIGHT, LOW );     // scope trigger
        audio *= 2;         // gain
        audio += 2048;         // 12 bits output D/A mid point
        i2cd( D2A_AUDIO, ((audio >> 8 )& 0xff), audio & 0xff );
        audio_flag = 0;
    // digitalWrite( BACKLIGHT, HIGH );
    }
    
    // do more useful work here in loop.....
}

void i2cd( unsigned char addr, unsigned char reg, unsigned char dat ){

    i2start( addr );
    i2send( (int) reg );                    // register or 1st data byte if no registers in device
    i2send( (int) dat );
    i2stop();
}

uint32_t dsp_core( uint32_t timer ){
long sample;
static long w[32];     // delay line
static int win;
static int flip;
int i,j;
// cw filter variables
static long wc[32];
static int wcin;

   sample = analogRead( A0 );  // 10 bits in
   
   // run a decimation filter for 16k to 8k rate
   i = win;
   w[win++] = sample - 512;   // store in delay line, remove dc component of the signal 
   win &= 31;
   
   flip ^= 1;
   if( flip ){   // only need to run the filter at the slower rate
      sample = 0;   // accumulator 
 
      for( j = 0; j < 32; j++ ){
        sample +=  fc[j] * w[i++];
        i &= 31;
      }
      audio = sample >> 15;
      
      if( mode == CW ){    // run the cw filter at 8k rate
         sample = 0;
         i = wcin;
         wc[wcin++] = audio;
         wcin &= 31;
         
         for( j = 0; j < 32; j++ ){
           sample +=  fcw[j] * wc[i++];
           i &= 31;
         }     
         audio = sample >> 15;
      }
      
      audio_flag = 1;
   }

   return timer + 2500;   // 2500 == 16k, 5000 == 8k sample rate, 6250 == 6400 hz sample rate
}

/*  I2C write only implementation using polling of the hardware registers */
/*  functions do not block */
/*  call i2poll() in loop() to keep everything going */
// use some upper bits in the buffer for control
#define ISTART 0x100
#define ISTOP  0x200
void i2start( unsigned char adr ){
int dat;
  // shift the address over and add the start flag
  dat = ( adr << 1 ) | ISTART;
  i2send( dat );
}

void i2send( int data ){   // just save stuff in the buffer
  i2buf[i2in++] = data;
  i2in &= (I2BUFSIZE - 1);
}

void i2stop( ){
   i2send( ISTOP );   // que a stop condition
}


void i2flush(){  // needed during setup, call poll to empty out the buffer.  This one does block.

  while( i2poll() ); 
}

int i2poll(){    // everything happens here.  Call this from loop.
static int state = 0;
static int data;
static int delay_counter;

   if( delay_counter ){   // the library code has a delay after loading the transmit buffer and before the status bits are tested for transmit active
     --delay_counter;
     return (16 + delay_counter);
   }
   
   switch( state ){    
      case 0:      // idle state or between characters
        if( i2in != i2out ){   // get next character
           data = i2buf[i2out++];
           i2out &= (I2BUFSIZE - 1 );
           if( data & ISTART ){   // start
              data &= 0xff;
              // set start condition
              I2C1CONSET = 1;
              state = 1; 
           }
           else if( data & ISTOP ){  // stop
              // set stop condition
              I2C1CONSET = 4;
              state = 3;
           }
           else{   // just data to send
              I2C1TRN = data;
              delay_counter = 10;   // delay for transmit active to come true
              state = 2;
           }
        }
      break; 
      case 1:  // wait for start to clear, send saved data which has the address
         if( (I2C1CON & 1) == 0  ){
            state = 2;
            I2C1TRN = data;
            delay_counter = 10;
         }
      break;
      case 2:  // wait for ack/nack done and tbuffer empty, blind to success or fail
         if( (I2C1STAT & 0x4001 ) == 0){  
            state = 0;
         }
      break;
      case 3:  // wait for stop to clear
         if( (I2C1CON & 4 ) == 0 ){
            state = 0;
            delay_counter = 10;  // a little delay just for fun at the end of a sequence
         }
      break;    
   }
   
   // detect any error bits and see if can recover, advance to next start in the buffer
   // totally blind for now until we check some error bits

   if( i2in != i2out ) return (state + 8);
   else return state;
}

// here are the filter coefficients if your interested in them.  Normally these would be declared before the dsp_core function.
//   lowpass fir decimation filter constants, 3000 cutoff 4000 stop at 16k rate
long fc[ 32 ] = { 
// 0 . 006820q15 , 
 223,
// , 0 . 011046q15 , 
 361,
// , - 0 . 004225q15 , 
 -138,
// , - 0 . 016811q15 , 
  -550,
// , - 0 . 001921q15 , 
  -62,
// , 0 . 021797q15 , 
  714,
// , 0 . 012515q15 , 
  410,
// , - 0 . 024048q15 , 
  -788,
// , - 0 . 028414q15 , 
  -931,
// , 0 . 020730q15 , 
  679,
// , 0 . 051289q15 , 
  1680,
// , - 0 . 006587q15 , 
  -215,
// , - 0 . 087749q15 , 
  -2875,
// , - 0 . 036382q15 , 
  -1192,
// , 0 . 186091q15 , 
  6097,
// , 0 . 403613q15 , 
  13225,
// , 0 . 403613q15 , 
  13225,
// , 0 . 186091q15 , 
  6097,
// , - 0 . 036382q15 , 
  -1192,
// , - 0 . 087749q15 , 
  -2875,
// , - 0 . 006587q15 , 
  -215,
// , 0 . 051289q15 , 
  1680,
// , 0 . 020730q15 , 
  679,
// , - 0 . 028414q15 , 
  -931,
// , - 0 . 024048q15 , 
  -788,
// , 0 . 012515q15 , 
  410,
// , 0 . 021797q15 , 
  714,
// , - 0 . 001921q15 , 
  -62,
// , - 0 . 016811q15 , 
  -550,
// , - 0 . 004225q15 , 
  -138,
// , 0 . 011046q15 , 
  361,
// , 0 . 006820q15  
  223
};


// cw filter 400 hz wide centered at about 600 hz
int fcw[ 32 ] = { 
// - 0 . 000426q15 , 
	  -13,
// , - 0 . 003116q15 , 
	  -102,
// , - 0 . 005389q15 , 
	  -176,
// , - 0 . 003948q15 , 
	  -129,
// , 0 . 001048q15 , 
	  34,
// , 0 . 004523q15 , 
	  148,
// , - 0 . 001170q15 , 
	  -38,
// , - 0 . 020961q15 , 
	  -686,
// , - 0 . 051978q15 , 
	  -1703,
// , - 0 . 082194q15 , 
	  -2693,
// , - 0 . 094538q15 , 
	  -3097,
// , - 0 . 075093q15 , 
	  -2460,
// , - 0 . 021324q15 , 
	  -698,
// , 0 . 054266q15 , 
	  1778,
// , 0 . 127619q15 , 
	  4181,
// , 0 . 172683q15 , 
	  5658,
// , 0 . 172683q15 , 
	  5658,
// , 0 . 127619q15 , 
	  4181,
// , 0 . 054266q15 , 
	  1778,
// , - 0 . 021324q15 , 
	  -698,
// , - 0 . 075093q15 , 
	  -2460,
// , - 0 . 094538q15 , 
	  -3097,
// , - 0 . 082194q15 , 
	  -2693,
// , - 0 . 051978q15 , 
	  -1703,
// , - 0 . 020961q15 , 
	  -686,
// , - 0 . 001170q15 , 
	  -38,
// , 0 . 004523q15 , 
	  148,
// , 0 . 001048q15 , 
	  34,
// , - 0 . 003948q15 , 
	  -129,
// , - 0 . 005389q15 , 
	  -176,
// , - 0 . 003116q15 , 
	  -102,
// , - 0 . 000426q15  
	  -13,
};


