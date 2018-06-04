#pragma once

#include <cstdint>
#include <memory>

#include "remote_env.h"

class nlab {
	std::unique_ptr<base_stream> pipe_;
	verification_header lasthead_{};
	env_state state_{};
	n_restart_info lrinfo_{};

	static const size_t dom_default_sz_ = 64 * 1024u;
	static const size_t stack_default_sz_ = 4 * 1024u;

	size_t last_dom_buffer_sz_{};
	size_t last_stack_buffer_sz_{};

	std::vector<std::uint8_t> dom_buffer_;
	std::vector<std::uint8_t> stack_buffer_;

public:
	static const unsigned VERSION = 0x00000100;
	
	verification_header get_header() const
	{
		return lasthead_;
	}

	n_restart_info get_restart_info() const
	{
		return lrinfo_;
	}

	env_state get_state() const
	{
		return state_;
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
		pipe_(std::move(a))
	{
	}

	nlab(const nlab& a) = delete;
	nlab& operator=(nlab&& a) = default;

};