#include "nlab.h"

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

using namespace rapidjson;

int nlab::connect()
{
	_pipe->connect();
	return 0;
}

n_start_info nlab::get_start_info()
{
	void* buf = nullptr;
	size_t sz = 0;

	while (sz == 0)
	{
		_pipe->receive(&buf, sz);
	}

	Document doc;

	doc.Parse(static_cast< char* >(buf));
	if (doc.HasParseError())
	{
		throw std::runtime_error("nlab::get_start_info failed. JSON parse error");
	}

	packet_type ptype = packet_type(doc["type"].GetInt());

	if (ptype != packet_type::n_start_info)
	{
		throw std::runtime_error("nlab::get_start_info failed. Unknown packet type");
	}

	n_start_info nsi;
	auto& desi = doc["n_start_info"];

	nsi.count = desi["count"].GetUint64();
	nsi.round_seed = desi["round_seed"].GetUint64();

	_state.count = nsi.count;
	_state.round_seed = nsi.round_seed;

	return nsi;
}

int nlab::set_start_info(const e_start_info& inf) 
{
	_state.count = inf.count;
	_state.mode = inf.mode;
	_state.incount = inf.incount;
	_state.outcount = inf.outcount;

	StringBuffer s;
	Writer< StringBuffer > doc(s);

	doc.StartObject();
	doc.String("version");
	doc.Int(static_cast<int>(VERSION));
	doc.String("type");
	doc.Int(static_cast<int>(packet_type::e_start_info));
	doc.String("e_start_info");
	doc.StartObject();
	doc.String("mode");
	doc.Int(static_cast<int>(inf.mode));
	doc.String("count");
	doc.Uint64(inf.count);
	doc.String("incount");
	doc.Uint64(inf.incount);
	doc.String("outcount");
	doc.Uint64(inf.outcount);
	doc.EndObject();
	doc.EndObject();
	s.Put('\0');

	_pipe->send(s.GetString(), s.GetSize());
	return 0;
}

n_send_info nlab::get()
{
	void* buf = nullptr;
	size_t sz = 0;
	

	while (sz == 0)
	{
		_pipe->receive(&buf, sz);
	}

	Document doc;

	doc.Parse(static_cast< char* >(buf));
	if (doc.HasParseError())
	{
		throw std::runtime_error("nlab::get failed. JSON parse error");
	}

	packet_type ptype;
	ptype = packet_type(doc["type"].GetInt());
	if (ptype != packet_type::n_send_info)
	{
		throw std::runtime_error("nlab::get failed. Unknown packet type");
	}

	n_send_info nsi;
	auto& dnsi = doc["n_send_info"];
	nsi.head = verification_header(dnsi["head"].GetInt());
	_lasthead = nsi.head;

	if (nsi.head != verification_header::ok)
	{
		if (nsi.head == verification_header::restart)
		{
			_lrinfo.count = dnsi["count"].GetUint64();
			_lrinfo.round_seed = dnsi["round_seed"].GetUint64();
		}

		return nsi;
	}

	auto& data = dnsi["data"];
	nsi.data.reserve(data.Size());
	for (auto i = data.Begin(); i != data.End(); i++)
	{
		nsi.data.emplace_back();

		env_task& cur_tsk = nsi.data.back();
		cur_tsk.reserve(i->Size());
		for (auto j = i->Begin(); j != i->End(); j++)
		{
			cur_tsk.push_back(j->GetDouble());
		}
	}

	return nsi;
}

int nlab::set(const e_send_info & inf)
{
	StringBuffer s;
	Writer< StringBuffer > doc(s);
	doc.StartObject();
	doc.String("type");
	doc.Int(static_cast<int>(packet_type::e_send_info));
	doc.String("e_send_info");
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

int nlab::restart(const e_restart_info & inf)
{
	StringBuffer s;
	Writer< StringBuffer > doc(s);
	doc.StartObject();
	doc.String("type");
	doc.Int(static_cast<int>(packet_type::e_send_info));
	doc.String("e_send_info");
	doc.StartObject();
	doc.String("head");
	doc.Int(static_cast<int>(verification_header::restart));

	doc.String("score");
	doc.StartArray();
	for (auto& i : inf.result)
	{
		doc.Double(i);
	}
	doc.EndArray();

	doc.EndObject();
	doc.EndObject();
	s.Put('\0');

	_pipe->send(s.GetString(), s.GetSize());

	return 0;
}

int nlab::stop()
{
	StringBuffer s;
	Writer< StringBuffer > doc(s);
	doc.StartObject();
	doc.String("version");
	doc.Int(static_cast<int>(VERSION));
	doc.String("type");
	doc.Int(static_cast<int>(packet_type::e_send_info));
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
}

int nlab::disconnect()
{
	_pipe->disconnect();
	return 0;
}