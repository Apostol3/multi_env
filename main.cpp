#include <string>
#include <memory>
#include <vector>
#include <algorithm>

#include <CLI/App.hpp>
#include <CLI/Validators.hpp>

#include "tiny-process-library/process.hpp"

#include "nlab.h"
#include "remote_env.h"
#include "tcp_stream.h"

class multi_env {
	std::string envs_uri_;
	std::string nlab_uri_;
	bool use_existing_{ false };
	int env_count_;
	std::string command_;

	std::unique_ptr<nlab> lab_{nullptr};
	std::vector<std::unique_ptr<remote_env>> envs_;
	std::vector<std::string> uris_;

	std::vector<std::unique_ptr<TinyProcessLib::Process>> sub_procs;

	void spawn_envs();

public:

	multi_env(std::string envs_uri, std::string nlab_uri,
		int env_count, std::string command, bool use_existing = false)
		: envs_uri_(envs_uri), nlab_uri_(nlab_uri), env_count_(env_count),
		command_(command), use_existing_(use_existing)
	{
	};

	void init_nlab();
	void init_envs();
	void connect_nlab();
	void connect_envs();
	void work();
	void cleanup();

	~multi_env()
	{
		for (auto& process : sub_procs) {
			process->kill();
		}
	}
};

void multi_env::init_nlab() {
	if (lab_)
		throw std::runtime_error("already initialized");

	auto colon_ind = nlab_uri_.find("://");
	if (colon_ind == std::string::npos)
		throw std::invalid_argument("couldn't parse connection URI");

	auto uri_net_part = nlab_uri_.substr(colon_ind + 3);
	auto scheme = nlab_uri_.substr(0, colon_ind);

	if (scheme == "tcp") {
		auto port_ind = uri_net_part.find(":");
		if (port_ind == std::string::npos)
			throw std::invalid_argument("couldn't parse connection URI");
		lab_ = std::make_unique<nlab>(std::make_unique<tcp_stream>(uri_net_part.substr(0, port_ind),
			uri_net_part.substr(port_ind + 1), 3072000));
	}
	else throw std::invalid_argument("unknown connection URI scheme");
}

void multi_env::init_envs() {
	if (!envs_.empty())
		throw std::runtime_error("already initialized");

	if (env_count_ <= 0)
		throw std::invalid_argument("env_count less that 0");

	auto colon_ind = envs_uri_.find("://");
	if (colon_ind == std::string::npos)
		throw std::invalid_argument("couldn't parse connection URI");

	auto uri_net_part = envs_uri_.substr(colon_ind + 3);
	auto scheme = envs_uri_.substr(0, colon_ind);

	if (scheme == "tcp") {
		auto port_ind = uri_net_part.find(":");
		if (port_ind == std::string::npos)
			throw std::invalid_argument("couldn't parse connection URI");

		int port = std::stoi(uri_net_part.substr(port_ind + 1));
		std::string host = uri_net_part.substr(0, port_ind);
		std::string proto_part = "tcp://";

		for (int i = 0; i < env_count_; i++) {
			std::string port_string = std::to_string(port++);
			envs_.emplace_back(std::make_unique<remote_env>(
				std::make_unique<tcp_stream>(host, port_string, 3072000)));

			uris_.emplace_back(proto_part + host + std::string(":") + port_string);
		}

	}
	else throw std::invalid_argument("unknown connection URI scheme");
}

void multi_env::connect_nlab() {
	if (lab_->connect())
		throw "nlab connection failed";
}

void multi_env::connect_envs() {

	std::string create_string = "creating " + std::to_string(env_count_) + " pipes: ";

	for (auto& i : uris_) {
		create_string += std::string("\"") + i + std::string("\" ");
	}

	std::cout << create_string << "\n";

	for (auto& i : envs_) {
		i->init();
	}

	std::cout << "pipes created\n";

	if (!use_existing_)
		spawn_envs();

	std::cout << "waiting for connection\n";

	for (auto& i : envs_)
	{
		i->wait();
	}

	std::cout << "all subs connected\n";

	e_start_info esi_n;
	esi_n.count = 0;
	esi_n.mode = send_modes::specified;

	std::cout << "waiting start info from subs\n";

	for (auto i = 0; i < envs_.size(); i++)
	{
		auto& env = envs_[i];

		e_start_info esi = env->get_start_info();

		if (esi_n.count == 0) {
			esi_n.incount = esi.incount;
			esi_n.outcount = esi.outcount;
		}
		else if (esi_n.incount != esi.incount || esi_n.outcount != esi.outcount) {
			throw std::runtime_error(std::string("Different specification received from ") + 
				uris_[i]);
		}

		esi_n.count += esi.count;

		std::cout << "get count: " << esi.count << ", incount: " << esi.incount 
			<< ", outcount: " << esi.outcount << " from " << uris_[i] << "\n";
	}

	std::cout << "received start info from subs.count: " << esi_n.count 
		<< ", incount: " << esi_n.incount 
		<< ", outcount: " << esi_n.outcount << "\n";

	lab_->set_start_info(esi_n);
	std::cout << "waiting start info from nlab\n";

	n_start_info nsi = lab_->get_start_info();

	std::cout << "received start info from nlab. count: " << nsi.count << "\n";

	for (auto& env : envs_) {
		n_start_info nsi_e;
		nsi_e.count = env->get_state().count;
		nsi_e.round_seed = lab_->get_state().round_seed;
		env->set_start_info(nsi_e);
	}

}

void multi_env::spawn_envs() {
	for (auto i = 0; i < uris_.size(); i++)
	{
		std::string launch = command_ + std::string(" --uri ") + uris_[i];
		sub_procs.emplace_back(std::make_unique<TinyProcessLib::Process>(launch));
	}

	std::string spawn_string = "all subs started. PID:  ";

	for (auto& process : sub_procs) {
		spawn_string += std::string("") + std::to_string(process->get_id()) + std::string(" ");
	}

	std::cout << spawn_string << "\n";

}

void multi_env::cleanup() {
	for (auto& process : sub_procs)
	{
		process->close_stdin();
		process->get_exit_status();
	}
}

void multi_env::work() {
	std::cout << "working\n";

	bool all_go = false;

	n_send_info nsi_e;
	nsi_e.head = verification_header::ok;

	while (true) {
		e_send_info esi_n;
		esi_n.head = verification_header::ok;

		esi_n.data.reserve(lab_->get_state().count);

		for (auto i = 0; i < envs_.size(); i++) {
			auto& env = envs_[i];

			if (env->get_header() != verification_header::ok && !all_go) {
				esi_n.data.insert(esi_n.data.end(), env->get_state().count, env_task{ });
				continue;
			}

			e_send_info esi = env->get();

			if (esi.head == verification_header::restart) {
				esi_n.data.insert(esi_n.data.end(), env->get_state().count, env_task{ });
			} else if (esi.head != verification_header::ok) {
				std::cout << "got " << static_cast<int>(esi.head) << " header from "
					<< uris_[i] << ". stopping other environments and nlab\n";

				for (auto& e : envs_) {
					if (e->get_header() == verification_header::ok ||
						e->get_header() == verification_header::restart) {
						e->stop();
					}
					e->terminate();
				}

				lab_->stop();
				std::cout << "stopped\n";
				return;
			}

			for (auto& task : esi.data) {
				esi_n.data.emplace_back();
				esi_n.data.back().swap(task);
			}
		}

		all_go = std::all_of(envs_.begin(), envs_.end(), 
			[](auto& env) { return env->get_header() == verification_header::restart; });

		if (all_go) {
			e_restart_info eri_n;
			eri_n.result.reserve(lab_->get_state().count);
			for (auto& env : envs_) {
				auto lrinfo = env->get_restart_info();
				eri_n.result.insert(eri_n.result.end(), lrinfo.result.begin(), lrinfo.result.end());
			}

			lab_->restart(eri_n);
		} else {
			lab_->set(esi_n);
		}

		n_send_info nsi = lab_->get();

		if (nsi.head == verification_header::restart) {
			for (auto& env : envs_)
			{
				n_restart_info nri_e;
				nri_e.count = env->get_state().count;
				nri_e.round_seed = lab_->get_state().round_seed;
				env->restart(nri_e);
			}
			continue;
		} else if (nsi.head != verification_header::ok) {
			std::cout << "got " << static_cast<int>(nsi.head)
				<< " header from nlab. stopping environments\n";
			for (auto& env : envs_)
			{
				env->stop();
				env->terminate();
			}
			std::cout << "stopped\n";
			return;
		}

		auto nsi_current = nsi.data.begin();
		for (auto& env : envs_)
		{
			size_t count = env->get_state().count;
			if (env->get_header() != verification_header::ok && !all_go) {
				nsi_current += count;
				continue;
			}

			nsi_e.data.resize(count);

			for (size_t i = 0; i < count; ++i)
			{
				nsi_e.data[i].swap(*(nsi_current+i));
			}

			nsi_current += count;
			env->set(nsi_e);
		}

	}
}

int main(int argc, char** argv) {
	CLI::App app{"multiplexer utility for nlab"};

	std::string envs_uri = "tcp://127.0.0.1:15005";
	std::string nlab_uri = "tcp://127.0.0.1:5005";

	bool existing;
	int count;
	std::string command;

	app.add_option("-I,--envs-uri",	envs_uri,
		"environments URI in format 'tcp://hostname:port'", true);

	app.add_option("-O,--nlab-uri",	nlab_uri,
		"nlab URI in format 'tcp://hostname:port'", true);

	app.add_flag("-e,--existing", existing,
		"do not spawn environments, just connect to them");

	app.add_option("count", count, "count of environments to start")
		->check(CLI::Range(0, 1024))
		->required(true);

	app.add_option("command", command, "command to execute environments")
		->required(true);

	CLI11_PARSE(app, argc, argv);

	multi_env menv{ envs_uri, nlab_uri, count, command, existing };

	try	{
		menv.init_nlab();
		menv.init_envs();

		std::cout << "connecting to nlab\n";
		menv.connect_nlab();
		std::cout << "connected\n";

		menv.connect_envs();
		menv.work();
		menv.cleanup();
	}
	catch (std::exception& e) {
		std::cerr << e.what();
		return -1;
	}
	
	return 0;
}
