#include "remote_env.h"

#include <iostream>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/reader.h>
#include <rapidjson/error/en.h>

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

struct e_send_info_parser : public BaseReaderHandler<UTF8<>, e_send_info_parser>
{
	bool StartObject() { 
		switch (state_)
		{
		case kExpectMainObjectStart:
			state_ = kExpectMainNameOrEnd;
			return true;
		case kExpectPacketObjectStart:
			state_ = kExpectPacketNameOrEnd;
			return true;
		default:
			return false;
		}
	}
    
    bool EndObject(SizeType) {
		switch (state_)
		{
		case kExpectMainNameOrEnd:
			if (!got_type_ || !got_packet_ || !got_payload_ || !got_head_)
			{
				throw std::runtime_error("Get failed. Required JSON fields are missing");
			}
			return true;
		case kExpectPacketNameOrEnd:
			state_ = kExpectMainNameOrEnd;
			return true;
		default:
			return false;
		}
	}

	bool Key(const Ch* str, SizeType len, bool copy) {
		switch (state_)
		{
		case kExpectMainNameOrEnd:
			if (!strncmp(str, "type", len)) {
				got_type_ = true;
				state_ = kExpectType;
				return true;
			}
			else if (!strncmp(str, "e_send_info", len)) {
				got_packet_ = true;
				state_ = kExpectPacketObjectStart;
				return true;
			}
			else {
				return false;
			}
		case kExpectPacketNameOrEnd:
			if (!strncmp(str, "head", len))
			{
				got_head_ = true;
				state_ = kExpectHead;
				return true;
			}
			else if (!strncmp(str, "data", len))
			{
				state_ = kExpectDataStart;
				return true;
			}
			else if (!strncmp(str, "score", len))
			{
				state_ = kExpectScoreStart;
				return true;
			}
			else
			{
				return false;
			}
		default:
			return false;
		}
	}

	bool StartArray() {
		switch (state_)
		{
		case kExpectEnvDataStartOrEnd:
			state_ = kExpectEnvDataOrEnd;
			return true;
		case kExpectDataStart:
			got_payload_ = true;
			new_data_.reserve(expected_inputs);
			result->data.reserve(expected_envs);
			state_ = kExpectEnvDataStartOrEnd;
			return true;
		case kExpectScoreStart:
			lrinfo->result.clear();
			lrinfo->result.reserve(expected_envs);
			got_payload_ = true;
			state_ = kExpectScoreOrEnd;
			return true;
		default:
			return false;
		}
	}

    bool EndArray(SizeType) {
		switch (state_)
		{
		case kExpectEnvDataOrEnd:
			result->data.push_back({});
			result->data.back().swap(new_data_);
			new_data_.reserve(expected_inputs);
			state_ = kExpectEnvDataStartOrEnd;
			return true;
		case kExpectEnvDataStartOrEnd:
			state_ = kExpectPacketNameOrEnd;
			return true;
		case kExpectScoreOrEnd:
			state_ = kExpectPacketNameOrEnd;
			return true;
		default:
			return false;
		}
	}


	bool Double(double a) {
		switch (state_)
		{
		case kExpectEnvDataOrEnd:
			new_data_.emplace_back(a);
			return true;
		case kExpectScoreOrEnd:
			lrinfo->result.emplace_back(a);
			return true;
		default:
			return false;
		}
	}

	bool Int64(int64_t a)
	{
		switch (state_)
		{
		case kExpectType:
			if (packet_type(a) != packet_type::e_send_info)
			{
				throw std::runtime_error("Get failed. Unknown packet type");
			}
			state_ = kExpectMainNameOrEnd;
			return true;
		case kExpectHead:
			result->head = verification_header(a);
			state_ = kExpectPacketNameOrEnd;
			return true;
		default:
			return Double(static_cast<double>(a));
		}
	}

	bool Null() { return Int64(0); }

    bool Bool(bool a) { return Int64(a?1:0); }

    bool Int(int a) { return Int64(a); }

    bool Uint(unsigned a) { return Int64(a); }

    bool Uint64(uint64_t a) { return Int64(static_cast<int64_t>(a)); }

	bool Default() { return false; }

	e_send_info* result{nullptr};
	e_restart_info* lrinfo{nullptr};

	size_t expected_envs{0};
	size_t expected_inputs{0};

private:
	enum State
	{
		kExpectMainObjectStart,
		kExpectMainNameOrEnd,
		kExpectType,
		kExpectPacketObjectStart,
		kExpectPacketNameOrEnd,
		kExpectHead,
		kExpectDataStart,
		kExpectEnvDataStartOrEnd,
		kExpectEnvDataOrEnd,
		kExpectScoreStart,
		kExpectScoreOrEnd
	}state_{kExpectMainObjectStart};

	bool got_type_{ false };
	bool got_packet_{ false };
	bool got_payload_{ false };
	bool got_head_{ false };

	env_task new_data_;
};

e_send_info remote_env::get()
{
	void* buf = nullptr;
	size_t sz = 0;
	packet_type ptype;

	while (sz == 0)
	{
		pipe_->receive(&buf, sz);
	}

	e_send_info esi;

	MemoryPoolAllocator<> stack_allocator{ stack_buffer_.data(),
		stack_buffer_.size() * sizeof(char) };

	GenericReader<UTF8<>, UTF8<>, MemoryPoolAllocator<>> reader(&stack_allocator,
		stack_buffer_.capacity());
	StringStream ss(static_cast<char*>(buf));

	e_send_info_parser handler;
	handler.expected_envs = state_.count;
	handler.expected_inputs = state_.incount;
	handler.result = &esi;
	handler.lrinfo = &lrinfo_;

	reader.Parse(ss, handler);

	if (reader.HasParseError())
	{
		std::cout << "Invalid JSON: " << static_cast< char* >(buf) << std::endl;
		ParseErrorCode e = reader.GetParseErrorCode();
		size_t o = reader.GetErrorOffset();
		std::cout << "Error: " << GetParseError_En(e) << "\n";
		std::cout << " at offset " << o << " near '" << std::string((char*)buf).substr(o, 10) << "...'\n";
		throw std::runtime_error("Get failed. JSON parse error");
	}

	lasthead_ = esi.head;
	if (esi.head != verification_header::ok)
	{
		esi.data.clear();
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
