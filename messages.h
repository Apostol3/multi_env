#pragma once

#include <vector>

using env_task = std::vector< double >;

enum class verification_header
{
	ok = 0,
	restart,
	stop,
	fail
};

enum class send_modes
{
	specified = 0,
	undefined
};

struct e_start_info
{
	send_modes mode{ send_modes::specified };
	size_t count{ 0 };
	size_t incount{ 0 };
	size_t outcount{ 0 };
};

struct n_start_info
{
	size_t count{ 0 };
	size_t round_seed{ 0 };
};

struct n_send_info
{
	verification_header head{ verification_header::fail };
	std::vector< env_task > data;
};

struct e_send_info
{
	verification_header head{ verification_header::fail };
	std::vector< env_task > data;
};

struct n_restart_info
{
	size_t count{ 0 };
	size_t round_seed{ 0 };
};

struct e_restart_info
{
	std::vector< double > result;
};

struct env_state
{
	send_modes mode{ send_modes::specified };
	size_t count{ 0 };
	size_t incount{ 0 };
	size_t outcount{ 0 };
	size_t round_seed{ 0 };
};
