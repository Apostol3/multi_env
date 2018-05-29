#pragma once

#include "messages.h"

class base_env
{
public:
	virtual int init() = 0;
	virtual e_start_info get_start_info() = 0;
	virtual int set_start_info(const n_start_info& inf) = 0;
	virtual e_send_info get() = 0;
	virtual int set(const n_send_info& inf) = 0;
	virtual int restart(const n_restart_info& inf) = 0;
	virtual verification_header get_header() const = 0;
	virtual e_restart_info get_restart_info() const = 0;
	virtual int stop() = 0;
	virtual int terminate() = 0;
	virtual env_state get_state() const = 0;

	base_env() = default;

	virtual ~base_env() = default;
};