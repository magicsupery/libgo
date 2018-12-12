#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/date_time.hpp>
#include <libgo/coroutine.h>

static const uint16_t port = 43332;
using namespace boost::asio;
using namespace boost::asio::ip;
using boost::system::error_code;

io_service ios;

tcp::endpoint addr(address::from_string("127.0.0.1"), port);

static int recv_num = 0;
static int total_us = 0;

void client()
{

	tcp::socket s(ios);
	try
	{
		s.connect(addr);
		std::string msg = "112312312321321312412412312";
		int n = s.write_some(buffer(msg));
		//printf("client send msg [%d] %s\n", (int)msg.size(), msg.c_str());
		char buf[12];
		boost::posix_time::ptime t1 = boost::posix_time::microsec_clock::local_time();
		n = s.receive(buffer(buf, n));
		boost::posix_time::ptime t2 = boost::posix_time::microsec_clock::local_time();

		recv_num ++;
		total_us = total_us + (t2 -t1).total_microseconds();
		//printf("client recv msg [%d] %.*s\n", n, n, buf);

		printf("current avg us is %f\n", float(total_us) / recv_num);
	}catch(...)
	{
		s.close();
	}

}

int main()
{

	int i = 0;
	while(i < 3000)
	{
		go client;
		i++;
	}
	co_sched.Stop();
    // 单线程执行
    co_sched.Start();
    return 0;
}

