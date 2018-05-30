#include "remote_env.h"

#include <iostream>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>

using namespace rapidjson;

int remote_env::init()
{
	dom_buffer_.resize(dom_default_sz_ / sizeof(char));
	stack_buffer_.resize(stack_default_sz_ / sizeof(char));
	pipe_->create();
	return 0;
}

int remote_env::wait()
{
	pipe_->wait();
	return 0;
}

e_start_info remote_env::get_start_info()
{
	void* buf = nullptr;
	size_t sz = 0;

	packet_type ptype;
	while (sz == 0)
	{
		pipe_->receive(&buf, sz);
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

	state_.mode = esi.mode;
	state_.count = esi.count;
	state_.incount = esi.incount;
	state_.outcount = esi.outcount;
	return esi;
}

int remote_env::set_start_info(const n_start_info& inf)
{
	if (state_.mode == send_modes::specified && inf.count != state_.count)
	{
		throw;
	}

	if (state_.mode == send_modes::undefined && state_.count != 0 && inf.count > state_.count)
	{
		throw;
	}

	state_.count = inf.count;
	state_.round_seed = inf.round_seed;

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

	pipe_->send(s.GetString(), s.GetSize());
	return 0;
}

e_send_info remote_env::get()
{
	void* buf = nullptr;
	size_t sz = 0;
	packet_type ptype;

	while (sz == 0)
	{
		pipe_->receive(&buf, sz);
	}

	MemoryPoolAllocator<> dom_allocator{ dom_buffer_.data(), dom_buffer_.size() * sizeof(char) };
	MemoryPoolAllocator<> stack_allocator{ stack_buffer_.data(),
		stack_buffer_.size() * sizeof(char) };

	GenericDocument<UTF8<>, MemoryPoolAllocator<>, MemoryPoolAllocator<>>
		doc(&dom_allocator, stack_allocator.Capacity(), &stack_allocator);

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
	lasthead_ = esi.head;

	if (esi.head != verification_header::ok)
	{
		if (esi.head == verification_header::restart)
		{
			auto& data = desi["score"];
			lrinfo_.result.clear();
			lrinfo_.result.reserve(data.Size());
			for (auto i = data.Begin(); i != data.End(); i++)
			{
				lrinfo_.result.push_back(i->GetDouble());
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

	if (dom_allocator.Size() >dom_buffer_.size() * sizeof(char)) {
		dom_buffer_.resize(dom_allocator.Size() / sizeof(char));
	}

	if (stack_allocator.Size() > stack_buffer_.size() * sizeof(char))
	{
		stack_buffer_.resize(stack_allocator.Size() / sizeof(char));
	}

	return esi;
}

int remote_env::set(const n_send_info& inf)
{
	using StringBufferType = GenericStringBuffer<UTF8<>, MemoryPoolAllocator<>>;
	MemoryPoolAllocator<> dom_allocator{ dom_buffer_.data(), dom_buffer_.size() * sizeof(char) };
	MemoryPoolAllocator<> stack_allocator{ stack_buffer_.data(),
		stack_buffer_.size() * sizeof(char) };

	StringBufferType s{ &dom_allocator, dom_allocator.Capacity() };
	Writer< StringBufferType, UTF8<>, UTF8<>, MemoryPoolAllocator<> > doc(s, &stack_allocator);
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

	pipe_->send(s.GetString(), s.GetSize());

	if (dom_allocator.Size() >dom_buffer_.size() * sizeof(char))
	{
		dom_buffer_.resize(dom_allocator.Size() / sizeof(char));
	}

	return 0;
}

int remote_env::restart(const n_restart_info& inf)
{
	state_.count = inf.count;
	state_.round_seed = inf.round_seed;

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

	pipe_->send(s.GetString(), s.GetSize());

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

	pipe_->send(s.GetString(), s.GetSize());

	terminate();
	return 0;
};

int remote_env::terminate()
{
	pipe_->close();

	dom_buffer_.resize(dom_default_sz_ / sizeof(char));
	dom_buffer_.shrink_to_fit();

	stack_buffer_.resize(stack_default_sz_ / sizeof(char));
	stack_buffer_.shrink_to_fit();

	return 0;
}
