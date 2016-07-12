# asio-examples

## Contents:

### serial_port_stream_processor: 

This example is a mashup of code from:

1. http://stackoverflow.com/questions/38256712/c-semaphore-confusion,
2. https://gist.github.com/yoggy/3323808 (from which I believe the SO question code is derived), and 
3. http://www.boost.org/doc/libs/1_61_0/doc/html/boost_asio/examples/cpp03_examples.html, the (multi-threaded) http server 3 example.

The use case implemented by this example is one of:

1. Connecting to a serial port
2. Reading a stream of double-precision floating point values over the serial port
3. Performing operations over a sliding window of this streaming data. That is, given a window size of N and a stream size of M, M-N+1 windows of size N data are processed. The intended operation is the discrete fourier transform (DFT) of the windowed data. However, this example simply multiplies the window data by 2.
4. Output each processed window of data in the sequence of the sliding window.



### tcp_stream_processor:

### tcp_streamer:
