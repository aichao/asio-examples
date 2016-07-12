# asio-examples

If you are reading this, I hope these examples are useful for you. I would love to get some feedback and if you have questions, please send me an email at:

alanchao8@gmail.com

Best,

Alan

## Contents:

### serial_port_stream_processor: 

This example is a mashup of code from:

1. http://stackoverflow.com/questions/38256712/c-semaphore-confusion,
2. https://gist.github.com/yoggy/3323808 (from which I believe the SO question code is derived), and 
3. http://www.boost.org/doc/libs/1_61_0/doc/html/boost_asio/examples/cpp03_examples.html, the (multi-threaded) http server 3 example.

The use case implemented by this example is one of:

1. Connecting to a serial port
2. Reading a stream of double-precision floating point values over the serial port
3. Performing operations over a sliding window of this streaming data. That is, given a window size of N and a stream size of M, M-N+1 windows of size N data are processed. The intended operation on each window of data is the discrete fourier transform (DFT). However, this example simply multiplies the windowed data by two. This is because I do not have FFTW installed.
4. Output each processed window of data in the sequence of the sliding window.

This example illustrates the use of boost::asio as an event loop for not just I/O events but also user generated events, allowing for the concurrent handling of these events using a pool of threads (i.e., the Leader/Followers concurrency pattern). For multi-core systems, this provides scalability to the independent processing of multiple windows of data as is the use case. This example can be viewed as implementing map-reduce over the pool of threads where the operation for separate windows of data is mapped to separate threads, and the results of these operations are gathered to a single thread so that the output is guaranteed to be in the sequence of the windows of data. Finally, the example is implemented using only the boost::asio framework; that is, no additional synchronization mechanism is needed.

This example depends on boost_1_53_0 or later and compiles successfully for boost_1_61_0 (the latest version as of 2016-07-11). Linking to the boost thread and system libraries is required. The example does not use any C++11 features explicitly, it uses the boost equivalents instead, but compilation used `-std=C++11`. Conversion of this example to C++11 is straightforward.

Unfortunately, I do not have the capability to test this example with a device that streams known signals over the serial port. Instead, the following two examples were created to do surrogate testing using tcp sockets.

To modify this example so that it actually provides a solution to the oscilloscope problem in the SO question, one needs to:

1. Replace the multiply by two operation with DFT using FFTW in SerialHandler::RunFFT
2. Replace the output code in `SerialHandler::HandleFFTResults`. Right now it just writes to `std::cout`
3. Actually perform error handling. Right now there is no code to handle asio errors
4. Check the code in `SerialConnection::connect()` to make sure it is correct.
5. Handle the case where the serial port is disconnected and then reconnected. Right now, the connect function is called when the `SerialConnection` is constructed (and it is private). The simple use case is that once the connection is made to the serial port, data is streaming, and this connection remains until after the user stops the application. It may be better to make `connect()` public so that a new connection can be made without having to construct a new SericalConnection. However, I do not know what happens when the serial port is disconnected. Does that trigger an error in `async_read_some()`. I do know that for tcp sockets, when the tcp_streamer closes its socket to the tcp_stream_processor, this triggers an error in `async_read_some()`.

### tcp_stream_processor:

This is the same example except a tcp socket is used instead of a serial port.

### tcp_streamer:

This example simply streams a sequence of doubles from 1 to M over a tcp socket to the tcp_stream_processor.

