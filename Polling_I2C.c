
// the hardware target is a Chipkit uC32.  The hardware register names could be changed to 
// adapt to a different processor.

// polling I2C implementation because wire library blocks
#define I2BUFSIZE 128
int i2buf[I2BUFSIZE];
int i2in, i2out;


/*  I2C write only implementation using polling of the hardware registers */
/*  the functions do not block */
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


