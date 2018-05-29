#pragma once

#include <vector>
#include <cstddef>
#include <memory>

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
	std::unique_ptr<base_stream> _pipe;
	verification_header _lasthead;
	env_state _state;
	e_restart_info _lrinfo;

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
		return _lasthead;
	}

	e_restart_info get_restart_info() const override
	{
		return _lrinfo;
	}

	env_state get_state() const override
	{
		return _state;
	}

	explicit remote_env(std::unique_ptr<base_stream>&& a) :
		_pipe(std::move(a)), _lasthead(), _state(), _lrinfo()
	{
	}

	remote_env(const remote_env& a) = delete;
	remote_env& operator=(remote_env&& a) = default;
};


