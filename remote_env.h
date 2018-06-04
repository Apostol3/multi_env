#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "env.h"

class base_stream
{
public:

	virtual ~base_stream() = default;
	virtual void receive(void** ppd, std::size_t& sz) = 0;
	virtual void send(const void* pd, std::size_t sz) = 0;
	virtual bool is_connected() const = 0;

	virtual void connect() = 0;
	virtual void disconnect() = 0;

	virtual void create() = 0;
	virtual void wait() = 0;
	virtual void close() = 0;
};

enum class packet_type
{
	none = 0,
	e_start_info,
	n_start_info,
	n_send_info,
	e_send_info,
	n_restart_info,
	e_restart_info
};

class remote_env : public base_env
{
	std::unique_ptr<base_stream> pipe_;
	verification_header lasthead_;
	env_state state_;
	e_restart_info lrinfo_;

	static const size_t dom_default_sz_ = 64 * 1024u;
	static const size_t stack_default_sz_ = 4 * 1024u;

	size_t last_dom_buffer_sz_{};
	size_t last_stack_buffer_sz_{};

	std::vector<std::uint8_t> dom_buffer_;
	std::vector<std::uint8_t> stack_buffer_;

public:

	static const unsigned VERSION = 0x00000100;

	int init() override;
	int wait();

	e_start_info get_start_info() override;
	int set_start_info(const n_start_info& inf) override;

	e_send_info get() override;
	int set(const n_send_info& inf) override;
	int restart(const n_restart_info& inf) override;

	int stop() override;
	int terminate() override;

	verification_header get_header() const override
	{
		return lasthead_;
	}

	e_restart_info get_restart_info() const override
	{
		return lrinfo_;
	}

	env_state get_state() const override
	{
		return state_;
	}

	explicit remote_env(std::unique_ptr<base_stream>&& a) :
		pipe_(std::move(a)), lasthead_(), state_(), lrinfo_()
	{
	}

	remote_env(const remote_env& a) = delete;
	remote_env& operator=(remote_env&& a) = default;
};


