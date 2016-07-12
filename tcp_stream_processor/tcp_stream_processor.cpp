#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/noncopyable.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/thread/thread.hpp>

typedef boost::shared_ptr<boost::asio::ip::tcp::socket> socket_ptr;

class SerialHandler
    : public boost::enable_shared_from_this<SerialHandler>,
      private boost::noncopyable {
 public:
  // This is the size of the serial port read buffer in number of doubles
  static const int SERIAL_PORT_READ_BUF_SIZE = 32; // This is 256 bytes

  typedef boost::shared_ptr<SerialHandler> pointer;

  SerialHandler(boost::asio::io_service& io_service, std::size_t N)
      : N_(N), io_service_(io_service),
        in_strand_(io_service_), out_strand_(io_service_),
        currentIndex_(0), currentEnd_(0), outIndex_(0) {
  }

  void AsyncReadSome(socket_ptr sock) {
    // The in_strand_.wrap call wraps the handler to create another handler that
    // is to be dispatched on the strand. This serializes the execution of
    // the handlers (resulting from multiple async_read_some completion events)
    // to synchronize the access to the queue, currentIndex_, and currentEnd_
    // members. This has the same effect as using the semaphore or condition
    // variable except it is handled internally by the asio framework.
    sock->async_read_some(
        boost::asio::buffer(rawBuffer),
        in_strand_.wrap(boost::bind(&SerialHandler::HandlePortOnReceive, this,
                                    shared_from_this(),
                                    sock,
                                    boost::asio::placeholders::error,
                                    boost::asio::placeholders::bytes_transferred)));
  }

  void HandlePortOnReceive(pointer self,
                           socket_ptr sock,
                           const boost::system::error_code& ec,
                           size_t bytes_transferred) {
    if (!ec) {
      // Since only doubles are sent, bytes_transferred is divisible by sizeof(double)
      std::size_t numDoubles = bytes_transferred / sizeof(double);
      // Push new data to back of queue
      std::copy(self->rawBuffer.begin(), self->rawBuffer.begin() + numDoubles,
                std::back_inserter(self->q_));
      // Because the handling of this handler is serialized with the in_strand_,
      // another call to AsyncReadSome() can be made as soon as the data from
      // rawBuffer is copied to the queue
      this->AsyncReadSome(sock);
      // Slide the window along the queue from beginning to end and for each
      // window process the data
      self->currentEnd_ += numDoubles;
      while (self->currentIndex_ + self->N_ <= self->currentEnd_) {
        // Allocate a shared_array and copy the window of data to it
        boost::shared_array<double> data_window(new double[self->N_]);
        std::copy(self->q_.begin(), self->q_.begin() + self->N_,
                  data_window.get());
        // This acts to post an event to the io_service event loop that is to
        // be handled by the SerialHandler::RunFFT handler. This event is
        // completion of collecting the next window of data for FFT. The call
        // to self->io_service_.post does not block, so incorporating this in
        // a loop that slides the window across all data in the current queue
        // has the effect of mapping this operation to the multiple threads
        // that run the io_service. This provides a degree of scalability as
        // the FFT operation on individual windows are independent and can be
        // performed in parallel on multi-core systems where we have one thread
        // per core
        self->io_service_.post(boost::bind(
            &SerialHandler::RunFFT, this, self->currentIndex_, data_window, self));
        ++(self->currentIndex_);
        // slide the window along
        self->q_.pop_front();
      }
    }
  }

  // For this example, the DFT is not computed, instead multiply by 2 and
  // write to std::cout
  void RunFFT(std::size_t currentIndex, boost::shared_array<double> data_window,
              pointer self) {
    // Here, for simplicity, the operation is in place, but for DFT, one
    // needs to allocate an output shared_array of fftw_complex to store the
    // output and pass that along to HandleFFTResults
    for (int i = 0; i < self->N_; ++i) data_window[i] *= 2;
    // This acts to post an event to the io_service event loop that is to
    // be handled by the SerialHandler::HandleFFTResult handler. This event
    // is the completion of the DFT on the data window indexed by currentIndex.
    // The out_strand_.wrap call wraps the handler to create another handler
    // that is to be dispatched on the strand. This serializes the execution of
    // these handlers to synchronize the access to the out_ and outIndex_
    // members. The result has the effect of reducing the DFT results produced
    // by possibly multiple worker threads to a single thread
    self->io_service_.post(self->out_strand_.wrap(boost::bind(
        &SerialHandler::HandleFFTResults, this, currentIndex, data_window, self)));
  }

  // For actual FFT results, the input argument fft_result should be of type
  // boost::shared_array<fftw_complex>
  void HandleFFTResults(std::size_t currentIndex,
                        boost::shared_array<double> fft_result, pointer self) {
    // if the currentIndex matches the current outIndex_, then write result to
    // std::cout and increment outIndex_ until the DFT result for that index
    // is not found in the map.
    if (currentIndex == self->outIndex_) {
      for (int i = 0; i < self->N_; ++i) std::cout << fft_result[i] << " ";
      std::cout << std::endl;
      std::map<std::size_t, boost::shared_array<double> >::const_iterator pos;
      while ((pos = self->out_.find(++(self->outIndex_))) != self->out_.end()) {
        boost::shared_array<double> const& fft_res = pos->second;
        for (int i = 0; i < self->N_; ++i) std::cout << fft_res[i] << " ";
        std::cout << std::endl;
        // Optionally erase the element from the map to avoid memory creep
        self->out_.erase(pos);
      }
    } else {
      // Note that if the currentIndex does not match the current outIndex_,
      // then currentIndex must be strictly greater than the current outIndex_.
      // In this case, insert the DFT result into the map with the currentIndex
      // as the index
      self->out_.insert(std::make_pair(currentIndex, fft_result));
    }
  }

 private:
  // This is the size of the DFT to process
  const std::size_t N_;
  // The io_service
  boost::asio::io_service& io_service_;
  // The strand that serializes handling of the async read completion events
  boost::asio::io_service::strand in_strand_;
  // The strand that serializes handling of the fft completion events
  boost::asio::io_service::strand out_strand_;
  // Use boost::array as raw buffer since the type and size of data to be
  // read is known and fixed
  boost::array<double, SerialHandler::SERIAL_PORT_READ_BUF_SIZE> rawBuffer;

  // Operations reading and writing the following three variables must be
  // wrapped in the in_strand_

  // dequeue to manage sliding window of data
  std::deque<double> q_;
  // index corresponding to current window of N doubles in stream to process
  std::size_t currentIndex_;
  // accumulated index to the end of the queue
  std::size_t currentEnd_;

  // Operations reading and writing the following two variables must be
  // wrapped in the out_strand_

  // The database of DFT output indexed by the window indices from input stream
  // For actual DFT output, the value should be of type
  // boost::shared_array<fftw_complex>
  std::map<std::size_t, boost::shared_array<double> > out_;
  // index corresponding to next DFT output yet to be processed
  std::size_t outIndex_;
};

class SerialConnection {
 public:
  SerialConnection(short port, std::size_t N)
      : handler_ptr(new SerialHandler(io_service_, N)),
        acceptor_(io_service_,
                  boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)) {
    // ctor initiates an asynchronous accept (tcp specific)
    this->startAccept();
  }

  // This creates a pool of numThreads threads to run the io_service
  void run(short numThreads) {
    for (short i = 0; i < numThreads; ++i)
      threads_.create_thread(boost::bind(&boost::asio::io_service::run,
                                         &io_service_));
  }

  void joinThreads() {
    threads_.join_all();
  }

 private:
  // tcp specific stuff
  void startAccept() {
    socket_ = socket_ptr(new boost::asio::ip::tcp::socket(io_service_));
    acceptor_.async_accept(*socket_,
                           boost::bind(&SerialConnection::handleAccept, this,
                                       boost::asio::placeholders::error));
  }
  void handleAccept(boost::system::error_code const& ec) {
    if (!ec) {
      handler_ptr->AsyncReadSome(socket_);
    }
  }

  // The io_service
  boost::asio::io_service io_service_;
  // The pool of threads
  boost::thread_group threads_;
  // Shared pointer to the SerialHandler object on the heap
  SerialHandler::pointer handler_ptr;

  // tcp specific stuff
  socket_ptr socket_;
  boost::asio::ip::tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
  // Define number of threads to run io_service, the port, and
  // the size of the DFT
  // I have 4 cores so 7 + 1 (main) threads can run in parallel
  const short numThreads = 7;
  // I'm running OS X and this is an OS X ephemeral port
  const short port = 49162;
  // I set this small for testing, for spectral analysis this may be large
  const std::size_t N = 32;
  // Construct the SerialConnection, which initiates the async accept
  SerialConnection serialConn(port, N);
  // Run the io_service on the pool of threads
  serialConn.run(numThreads);

  // At this point the main thread is free to execute whatever it wants,
  // including possibly entering into a UI event loop.

  // Wait for all threads in the pool to exit before exiting main().
  serialConn.joinThreads();
  return 0;
}
