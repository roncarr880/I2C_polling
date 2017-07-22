# I2C_polling
A write only, non-blocking, I2C master mode set of functions for a Chipkit uC32.

There seems to be some confusion about what non-blocking means in regards to the Wire library. Yes it hangs your whole program when there is an issue on the I2C bus, but it also blocks in the sense that it waits for all transactions to complete.  This behavior greatly reduces the amount of processing power you have available.  The I2C bus is slow, why wait?

The functions here do not block. The functions are state sequenced and return immediately if there is nothing for them to do. If there is something to do, they will typically load a single register and return immediately.  The polling function must be called from your loop function as often as you like to keep the state sequenced code running.

One function, i2flush(), is blocking and would typically only be used in your setup() routine.  Since your not in loop() yet and the functions need i2poll() to be called in order to complete the transfer of data, you would queue up some transactions and then call i2flush() to empty the queue.  Once your code starts running loop(), you would no longer need to call i2flush().
