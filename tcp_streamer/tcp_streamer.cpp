#include <boost/asio.hpp>
#include <boost/array.hpp>

using boost::asio::ip::tcp;

int main(int argc, char* argv[]) {
  const int M = 1000;

  boost::asio::io_service io_service;

  tcp::resolver resolver(io_service);
  tcp::resolver::query query(tcp::v4(), "localhost", "49162");
  tcp::resolver::iterator iterator = resolver.resolve(query);

  tcp::socket s(io_service);
  boost::asio::connect(s, iterator);

  boost::array<double, N> data;
  for (int i = 0; i < data.size(); ++i) data[i] = i + 1;
  boost::asio::write(s, boost::asio::buffer(data, N * sizeof(double)));

  return 0;
}
