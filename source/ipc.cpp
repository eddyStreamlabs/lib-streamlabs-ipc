// A custom IPC solution to bypass electron IPC.
// Copyright(C) 2017 Streamlabs (General Workings Inc)
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110 - 1301, USA.

#include "ipc.hpp"
#include <sstream>

std::string ipc::base::make_unique_id(std::string name, std::vector<type> parameters) {
	// Implement similar behavior to C/C++ compilers, which put parameter type
	//  into the generated function name in order to allow overloading of the
	//  same function, even when exported.
	// This behavior might not be desired, but allows some amount of flexibility.

	std::string uq = name;
	if (parameters.size() > 0) {
		uq += "_";
		for (type p : parameters) {
			switch (p) {
				case (const int)ipc::type::Null:
					uq += "N0";
					break;
				case (const int)ipc::type::Float:
					uq += "F4";
					break;
				case (const int)ipc::type::Double:
					uq += "F8";
					break;
				case (const int)ipc::type::Int32:
					uq += "I4";
					break;
				case (const int)ipc::type::Int64:
					uq += "I8";
					break;
				case (const int)ipc::type::UInt32:
					uq += "U4";
					break;
				case (const int)ipc::type::UInt64:
					uq += "U8";
					break;
				case (const int)ipc::type::String:
					uq += "PS";
					break;
				case (const int)ipc::type::Binary:
					uq += "PB";
					break;
			}
		}
	}
	return uq;
}

static ipc::log_callback_t g_cb;
static void* g_cb_data;

void ipc::set_log_callback(log_callback_t cb, void* data) {
	g_cb = cb;
	g_cb_data = data;
}

void ipc::log(const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	if (g_cb) {
		g_cb(g_cb_data, fmt, args);
	}
	va_end(args);
}

std::string ipc::vectortohex(const std::vector<char>& buf) {
	static const char hexchars[] = {
		'0', '1','2','3','4','5','6','7','8','9',
		'A','B','C','D','E','F'
	};
	std::stringstream tohex;

	size_t sz = buf.size();
	for (size_t idx = 0; idx < sz; idx++) {
		unsigned char ch = (unsigned char)buf[idx];
		char rv = ch & 0xF;
		char lv = (ch >> 4) & 0xF;
		tohex << hexchars[lv] << hexchars[rv] << " ";
		if (idx > 100) // Abort as this is gonna be too long.
			break;
	}
	return tohex.str();
}

size_t ipc::message::function_call::size() {
	size_t size = sizeof(size_t)
		+ uid.size() /* timestamp */
		+ class_name.size()
		+ function_name.size()
		+ sizeof(uint8_t) /* arguments */;
	for (ipc::value& v : arguments) {
		size += v.size();
	}
	return size;
}

size_t ipc::message::function_call::serialize(std::vector<char>& buf, size_t offset) {
	if ((buf.size() - offset) < size()) {
		throw std::exception((const std::exception&)"Buffer too small");
	}
	size_t noffset = offset;

	reinterpret_cast<size_t&>(buf[noffset]) = size();
	noffset += sizeof(size_t);

	noffset += uid.serialize(buf, noffset);
	noffset += class_name.serialize(buf, noffset);
	noffset += function_name.serialize(buf, noffset);

	reinterpret_cast<uint8_t&>(buf[noffset]) = (uint8_t)arguments.size();
	noffset += sizeof(uint8_t);

	for (ipc::value& v : arguments) {
		noffset += v.serialize(buf, noffset);
	}

	return noffset - offset;
}

size_t ipc::message::function_call::deserialize(std::vector<char>& buf, size_t offset) {
	if ((buf.size() - offset) < sizeof(size_t)) {
		throw std::exception((const std::exception&)"Buffer too small");
	}

	size_t size = reinterpret_cast<const size_t&>(buf[offset]);
	if ((buf.size() - offset) < size) {
		throw std::exception((const std::exception&)"Buffer too small");
	}
	size_t noffset = offset + sizeof(size_t);

	noffset += uid.deserialize(buf, noffset);
	noffset += class_name.deserialize(buf, noffset);
	noffset += function_name.deserialize(buf, noffset);

	uint8_t cnt = reinterpret_cast<uint8_t&>(buf[noffset]);
	noffset += sizeof(uint8_t);
	this->arguments.resize(cnt);
	for (size_t idx = 0; idx < cnt; idx++) {
		noffset += this->arguments[idx].deserialize(buf, noffset);
	}

	return noffset - offset;
}

size_t ipc::message::function_reply::size() {
	size_t size = sizeof(size_t)
		+ uid.size() /* timestamp */
		+ error.size() /* error */
		+ sizeof(uint8_t) /* values */;
	for (ipc::value& v : values) {
		size += v.size();
	}
	return size;
}

size_t ipc::message::function_reply::serialize(std::vector<char>& buf, size_t offset) {
	if ((buf.size() - offset) < size()) {
		throw std::exception((const std::exception&)"Buffer too small");
	}
	size_t noffset = offset;

	reinterpret_cast<size_t&>(buf[noffset]) = size();
	noffset += sizeof(size_t);

	noffset += uid.serialize(buf, noffset);
	noffset += error.serialize(buf, noffset);

	reinterpret_cast<uint8_t&>(buf[noffset]) = (uint8_t)this->values.size();
	noffset += sizeof(uint8_t);
	for (ipc::value& v : values) {
		noffset += v.serialize(buf, noffset);
	}

	return noffset - offset;
}

size_t ipc::message::function_reply::deserialize(std::vector<char>& buf, size_t offset) {
	if ((buf.size() - offset) < sizeof(size_t)) {
		throw std::exception((const std::exception&)"Buffer too small");
	}

	size_t size = reinterpret_cast<const size_t&>(buf[offset]);
	if ((buf.size() - offset) < size) {
		throw std::exception((const std::exception&)"Buffer too small");
	}
	size_t noffset = offset + sizeof(size_t);

	noffset += uid.deserialize(buf, noffset);
	noffset += error.deserialize(buf, noffset);

	uint8_t cnt = reinterpret_cast<uint8_t&>(buf[noffset]);
	noffset += sizeof(uint8_t);
	this->values.resize(cnt);
	for (size_t idx = 0; idx < cnt; idx++) {
		noffset += this->values[idx].deserialize(buf, noffset);
	}

	return noffset - offset;
}

size_t ipc::message::authenticate::size() {
	size_t size = sizeof(size_t)
		+ name.size()
		+ password.size();
	return size;
}

size_t ipc::message::authenticate::serialize(std::vector<char>& buf, size_t offset) {
	if ((buf.size() - offset) < size()) {
		throw std::exception((const std::exception&)"Buffer too small");
	}
	size_t noffset = offset;

	reinterpret_cast<size_t&>(buf[noffset]) = size();
	noffset += sizeof(size_t);

	noffset += name.serialize(buf, noffset);
	noffset += password.serialize(buf, noffset);

	return noffset - offset;
}

size_t ipc::message::authenticate::deserialize(std::vector<char>& buf, size_t offset) {
	if ((buf.size() - offset) < sizeof(size_t)) {
		throw std::exception((const std::exception&)"Buffer too small");
	}

	size_t size = reinterpret_cast<const size_t&>(buf[offset]);
	if ((buf.size() - offset) < size) {
		throw std::exception((const std::exception&)"Buffer too small");
	}
	size_t noffset = offset + sizeof(size_t);

	noffset += name.deserialize(buf, noffset);
	noffset += password.deserialize(buf, noffset);

	return noffset - offset;
}

size_t ipc::message::authenticate_reply::size() {
	size_t size = sizeof(size_t)
		+ auth.size();
	return size;
}

size_t ipc::message::authenticate_reply::serialize(std::vector<char>& buf, size_t offset) {
	if ((buf.size() - offset) < size()) {
		throw std::exception((const std::exception&)"Buffer too small");
	}
	size_t noffset = offset;

	reinterpret_cast<size_t&>(buf[noffset]) = size();
	noffset += sizeof(size_t);

	noffset += auth.serialize(buf, noffset);

	return noffset - offset;
}

size_t ipc::message::authenticate_reply::deserialize(std::vector<char>& buf, size_t offset) {
	if ((buf.size() - offset) < sizeof(size_t)) {
		throw std::exception((const std::exception&)"Buffer too small");
	}

	size_t size = reinterpret_cast<const size_t&>(buf[offset]);
	if ((buf.size() - offset) < size) {
		throw std::exception((const std::exception&)"Buffer too small");
	}
	size_t noffset = offset + sizeof(size_t);

	noffset += auth.deserialize(buf, noffset);

	return noffset - offset;
}
