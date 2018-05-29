#pragma once

#include <memory>
#include "remote_env.h"

class nlab {
	std::unique_ptr<base_stream> _pipe;
	verification_header _lasthead{};
	env_state _state{};
	n_restart_info _lrinfo{};

public:
	static const unsigned VERSION = 0x00000100;
	
	verification_header get_header() const
	{
		return _lasthead;
	}

	n_restart_info get_restart_info() const
	{
		return _lrinfo;
	}

	env_state get_state() const
	{
		return _state;
	}

	int connect();
	n_start_info get_start_info();
	int set_start_info(const e_start_info& inf);
	n_send_info get();
	int set(const e_send_info& inf);
	int restart(const e_restart_info& inf);
	int stop();
	int disconnect();

	explicit nlab(std::unique_ptr<base_stream>&& a) :
		_pipe(std::move(a))
	{
	}

	nlab(const nlab& a) = delete;
	nlab& operator=(nlab&& a) = default;

};