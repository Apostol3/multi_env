#pragma once

#include "remote_env.h"

#define ASIO_STANDALONE
#include <asio.hpp>
#include <cstring>
#include <limits>

using asio::ip::tcp;

class tcp_stream : public base_stream
{
	asio::io_service io_service_;
	tcp::socket sock_;
	tcp::acceptor acceptor_;
	void* buf_;
	size_t buf_size_;
	bool server_;
	std::string host_;
	std::string port_;
	size_t static const max_internal_buffer = 16384;

public:
	explicit tcp_stream(size_t buf_size);
	tcp_stream(std::string host, std::string port, size_t buf_size);
	tcp_stream(const tcp_stream&) = delete;

	tcp_stream& operator =(const tcp_stream& a) = delete;
	~tcp_stream();
	void receive(void** ppd, size_t& sz) override;
	void send(const void* pd, size_t sz) override;
	bool is_connected() const override;
	void connect() override;

	void create() override;
	void wait() override;
	void disconnect() override;
	void close() override;
};

inline tcp_stream::tcp_stream(std::string host, std::string port, size_t buf_size)
	: sock_(io_service_), acceptor_(io_service_), buf_size_(buf_size), server_(false),
	host_(host), port_(port)
{
	buf_ = new char[buf_size];
}

inline tcp_stream::~tcp_stream()
{
	delete[] static_cast<char*>(buf_);
}

inline void tcp_stream::receive(void** ppd, size_t& sz)
{
	asio::error_code ec;
	sz = 0;
	size_t sz_part;

	sz_part = sock_.read_some(asio::buffer(buf_, max_internal_buffer), ec);

	if (ec == asio::error::would_block)
	{
		sz = 0;
		return;
	}

	if (ec != asio::error_code())
		asio::detail::throw_error(ec, "receive_from");

	sz = sz_part;

	if (static_cast<char*>(buf_)[sz_part - 1] != '\0')
	{
		auto part_start = static_cast< char* >(buf_);
		try
		{
			do
			{
				part_start += sz_part;
				sz_part = sock_.read_some(asio::buffer(part_start, max_internal_buffer), ec);
				if (ec == asio::error::would_block)
				{
					//TODO: add some sleep here
					sz_part = 0;
					continue;
				}
				sz += sz_part;
				if (sz + max_internal_buffer > buf_size_)
					throw std::runtime_error("receive buffer overflow");
			} while (static_cast<char*>(part_start)[sz_part - 1] != '\0');
		}
		catch (std::exception&)
		{
			throw;
		}

	}

	*ppd = buf_;
}

inline void tcp_stream::send(const void* pd, size_t sz)
{
	asio::write(sock_, asio::buffer(static_cast< const char* >(pd), sz));
}

inline bool tcp_stream::is_connected() const
{
	return sock_.is_open();
}

inline void tcp_stream::connect()
{
	server_ = false;

	sock_ = tcp::socket(io_service_);

	tcp::resolver resolver(io_service_);
	tcp::resolver::query query(host_, port_);
	tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);

	asio::connect(sock_, endpoint_iterator);

}

inline void tcp_stream::create()
{
	if (acceptor_.is_open())
		acceptor_.close();

	int port_num;

	try
	{
		port_num = std::stoi(port_.c_str());
	}
	catch (std::invalid_argument&)
	{
		throw std::invalid_argument("tcp error: invalid port");
	}

	if (port_num < 0 || port_num > std::numeric_limits<unsigned short>::max())
		throw std::invalid_argument("tcp error: invalid port");

	acceptor_ = tcp::acceptor(io_service_, tcp::endpoint(tcp::v4(),
		static_cast<unsigned short>(port_num)));

	acceptor_.non_blocking(true);
	server_ = true;
}

inline void tcp_stream::wait() {
	if (!server_)
		return;

	asio::error_code ec;
	sock_ = tcp::socket(io_service_);
	while (true)
	{
		acceptor_.accept(sock_, ec);
		if (ec == asio::error::would_block)
		{
			continue;
		}
		if (ec != asio::error_code())
		{
			asio::detail::throw_error(ec, "receive_from");
		}
		break;
	}
}

inline void tcp_stream::disconnect()
{
	sock_.close();
}

inline void tcp_stream::close()
{
	if (acceptor_.is_open())
		acceptor_.close();
	sock_.close();
}

