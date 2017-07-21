# I2C_polling
A write only, non-blocking, I2C set of functions for a Chipkit uC32

The Wire library for the chipkit uC32 blocks on start and stop commands.  A typical sequence of start, a couple of data bytes and stop will block for the entire set of transactions.  Even though the data bytes are loaded via an interupt, your program will just be waiting for everything to finish and the stop condition to complete.

The functions here do not block.  The polling function must be called from your loop function as often as you like to keep the state sequenced code running.
