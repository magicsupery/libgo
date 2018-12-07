/************************************************
 * libgo sample5
*************************************************/

/***********************************************
 * 结合boost.asio, 使网络编程变得更加简单.
 * 如果你不喜欢boost.asio, 这个例子可以跳过不看.
************************************************/
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include "coroutine.h"

static const uint16_t port = 43332;
using namespace boost::asio;
using namespace boost::asio::ip;
using boost::system::error_code;

io_service ios;

tcp::endpoint addr(address::from_string("127.0.0.1"), port);

void echo_server()
{
    tcp::acceptor acc(ios, addr, true);
	while(true)
	{
        std::shared_ptr<tcp::socket> s(new tcp::socket(ios));
        acc.accept(*s);
        go [s]{
			try
			{
				char buf[1024];
				auto n = s->read_some(buffer(buf));
				n = s->write_some(buffer(buf, n));
				error_code ignore_ec;
				n = s->read_some(buffer(buf, 1), ignore_ec);
			}catch(...)
			{
				s->close();
			}

			
        };
    }
}

int main()
{
    go echo_server;

    // 单线程执行
    co_sched.Start();
    return 0;
}

