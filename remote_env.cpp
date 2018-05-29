#include "remote_env.h"

#include <iostream>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>

using namespace rapidjson;

int remote_env::init()
{
	_pipe->create();
	return 0;
}

int remote_env::wait()
{
	_pipe->wait();
	return 0;
}

e_start_info remote_env::get_start_info()
{
	void* buf = nullptr;
	size_t sz = 0;

	packet_type ptype;
	while (sz == 0)
	{
		_pipe->receive(&buf, sz);
	}

	Document doc;

	doc.Parse(static_cast< char* >(buf));
	if (doc.HasParseError())
	{
		throw std::runtime_error("GetStartInfo failed. JSON parse error");
	}

	int pver = doc["version"].GetUint();

	if (pver != VERSION)
	{
		throw std::runtime_error("GetStartInfo failed. Deprecated");
	}

	ptype = packet_type(doc["type"].GetInt());

	if (ptype != packet_type::e_start_info)
	{
		throw std::runtime_error("GetStartInfo failed. Unknown packet type");
	}

	e_start_info esi;
	auto& desi = doc["e_start_info"];

	esi.mode = send_modes(desi["mode"].GetInt());
	esi.count = desi["count"].GetUint64();
	esi.incount = desi["incount"].GetUint64();
	esi.outcount = desi["outcount"].GetUint64();

	_state.mode = esi.mode;
	_state.count = esi.count;
	_state.incount = esi.incount;
	_state.outcount = esi.outcount;
	return esi;
}

int remote_env::set_start_info(const n_start_info& inf)
{
	if (_state.mode == send_modes::specified && inf.count != _state.count)
	{
		throw;
	}

	if (_state.mode == send_modes::undefined && _state.count != 0 && inf.count > _state.count)
	{
		throw;
	}

	_state.count = inf.count;
	_state.round_seed = inf.round_seed;

	StringBuffer s;
	Writer< StringBuffer > doc(s);

	doc.StartObject();
	doc.String("type");
	doc.Int(static_cast<int>(packet_type::n_start_info));
	doc.String("n_start_info");
	doc.StartObject();
	doc.String("count");
	doc.Uint64(inf.count);
	doc.String("round_seed");
	doc.Uint64(inf.round_seed);
	doc.EndObject();
	doc.EndObject();
	s.Put('\0');

	_pipe->send(s.GetString(), s.GetSize());
	return 0;
}

e_send_info remote_env::get()
{
	void* buf = nullptr;
	size_t sz = 0;
	packet_type ptype;

	while (sz == 0)
	{
		_pipe->receive(&buf, sz);
	}

	Document doc;

	doc.Parse(static_cast< char* >(buf));
	if (doc.HasParseError())
	{
		std::cout << "Invalid JSON: " << static_cast< char* >(buf) << std::endl;
		throw std::runtime_error("Get failed. JSON parse error");
	}


	ptype = packet_type(doc["type"].GetInt());
	if (ptype != packet_type::e_send_info)
	{
		throw std::runtime_error("Get failed. Unknown packet type");
	}

	e_send_info esi;
	auto& desi = doc["e_send_info"];
	esi.head = verification_header(desi["head"].GetInt());
	_lasthead = esi.head;

	if (esi.head != verification_header::ok)
	{
		if (esi.head == verification_header::restart)
		{
			auto& data = desi["score"];
			_lrinfo.result.clear();
			_lrinfo.result.reserve(data.Size());
			for (auto i = data.Begin(); i != data.End(); i++)
			{
				_lrinfo.result.push_back(i->GetDouble());
			}
		}

		return esi;
	}

	auto& data = desi["data"];
	esi.data.reserve(data.Size());

	for (auto i = data.Begin(); i != data.End(); i++)
	{
		esi.data.emplace_back();

		env_task& cur_tsk = esi.data.back();
		cur_tsk.reserve(i->Size());
		for (auto j = i->Begin(); j != i->End(); j++)
		{
			cur_tsk.push_back(j->GetDouble());
		}
	}

	return esi;
}

int remote_env::set(const n_send_info& inf)
{
	StringBuffer s;
	Writer< StringBuffer > doc(s);
	doc.StartObject();
	doc.String("type");
	doc.Int(static_cast<int>(packet_type::n_send_info));
	doc.String("n_send_info");
	doc.StartObject();
	doc.String("head");
	doc.Int(static_cast<int>(inf.head));

	if (inf.head == verification_header::ok)
	{
		doc.String("data");
		doc.StartArray();
		for (auto& i : inf.data)
		{
			doc.StartArray();
			for (auto& j : i)
			{
				doc.Double(j);
			}

			doc.EndArray();
		}

		doc.EndArray();
	}

	doc.EndObject();
	doc.EndObject();
	s.Put('\0');

	_pipe->send(s.GetString(), s.GetSize());

	return 0;
}

int remote_env::restart(const n_restart_info& inf)
{
	_state.count = inf.count;
	_state.round_seed = inf.round_seed;

	StringBuffer s;
	Writer< StringBuffer > doc(s);
	doc.StartObject();
	doc.String("type");
	doc.Int(static_cast<int>(packet_type::n_send_info));
	doc.String("n_send_info");
	doc.StartObject();
	doc.String("head");
	doc.Int(static_cast<int>(verification_header::restart));
	doc.String("count");
	doc.Uint64(inf.count);
	doc.String("round_seed");
	doc.Uint64(inf.round_seed);
	doc.EndObject();
	doc.EndObject();
	s.Put('\0');

	_pipe->send(s.GetString(), s.GetSize());

	return 0;
}

int remote_env::stop()
{
	StringBuffer s;
	Writer< StringBuffer > doc(s);
	doc.StartObject();
	doc.String("type");
	doc.Int(static_cast<int>(packet_type::n_send_info));
	doc.String("n_send_info");
	doc.StartObject();
	doc.String("head");
	doc.Int(static_cast<int>(verification_header::stop));
	doc.EndObject();
	doc.EndObject();
	s.Put('\0');

	_pipe->send(s.GetString(), s.GetSize());
	_pipe->close();
	return 0;
};

int remote_env::terminate()
{
	_pipe->close();
	return 0;
}
