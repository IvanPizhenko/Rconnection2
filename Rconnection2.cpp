/*
 *  C++ Interface to Rserve
 *  Copyright (C) 2004-8 Simon Urbanek, All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; version 2.1 of the License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Leser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Although this code is licensed under LGPL v2.1, we strongly encourage
 *  everyone modifying this software to contribute back any improvements and
 *  bugfixes to the project for the benefit all other users. Thank you.
 *
 *  $Id$
 */

/* external defines:
   SWAPEND  - needs to be defined for platforms with inverse endianess related to Intel
   see also SOCK_ERROR, MAIN and other defines in sisocks.h
   */

/* locally generated status error and return codes:
   -1  - operation failed (e.g. connect failed)
   -2  - handhake failed
   -3  - invalid ID string
   -4  - protocol not supported
   -5  - not connected
   -6  - - unused -
   -7  - remote connection close
   -8  - malformed packet
   -9  - send error
   -10 - out of memory
   -11 - operation is unsupported (e.g. unix login while crypt is not linked)
   -12 - eval didn't return a SEXP (possibly the server is too old/buggy or crashed)
   */


#include "Rconnection2.h"
#include "sisocks.h"

#ifdef unix
#include <sys/un.h>
#include <unistd.h>
#else
#define AF_LOCAL -1
#endif

#if defined HAVE_NETINET_TCP_H && defined HAVE_NETINET_IN_H
#define CAN_TCP_NODELAY
#include <netinet/tcp.h>
#include <netinet/in.h>
#endif

#ifdef Win32
#define CAN_TCP_NODELAY
#endif

#include "Rsrv.h"

#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif

// NOTE: 0103 compatibility has not been established! use at your own risk!
static const char *myID = "Rsrv0103QAP1"; /* this client supports up to protocol version 0103 */

#define IS_LIST_TYPE_(TYPE) ((TYPE) == XT_LIST || (TYPE) == XT_LIST_NOTAG || (TYPE) == XT_LIST_TAG)
#define IS_SYMBOL_TYPE_(TYPE) ((TYPE) == XT_SYM || (TYPE) == XT_SYMNAME)

// [IP] Custom commands
#define CMD_CustomStatus 0x50

#if defined(BUILDING_RCONNECTION2_DLL) && defined(INSTANTIATE_STD_TEMPLATES)
template class RCONNECTION2_API std::basic_string < char >;
template class RCONNECTION2_API std::vector < char>;
template class RCONNECTION2_API std::vector < unsigned int >;
template class RCONNECTION2_API std::vector < std::string >;
template class RCONNECTION2_API std::shared_ptr < Rconnection2::MessageBuffer >;
template class RCONNECTION2_API std::shared_ptr<Rconnection2::Rexp>;
template class RCONNECTION2_API std::weak_ptr<Rconnection2::Rexp>;
#endif

namespace Rconnection2 {

	Rmessage::Rmessage()
		:
		complete_(0),
		len_(0)
	{
		memset(&header_, 0, sizeof(header_));
	}

	Rmessage::Rmessage(int cmd)
		:
		complete_(1),
		len_(0)
	{
		memset(&header_, 0, sizeof(header_));
		header_.cmd = cmd;
	}

	Rmessage::Rmessage(int cmd, const char *txt)
		:
		complete_(1),
		len_(0)
	{
		memset(&header_, 0, sizeof(header_));
		int tl = strlen(txt) + 1;
		if ((tl & 3) > 0)
			tl = (tl + 4) & 0xffffc; // allign the text
		len_ = tl + 4; // message length is tl + 4 (short format only)
		header_.cmd = cmd;
		header_.len = len_;
		alloc_data_only(tl + 16);
		memset(get_data(), 0, tl + 16);
		*((int*)get_data()) = itop(SET_PAR(DT_STRING, tl));
		strcpy(get_data() + 4, txt);
	}

	Rmessage::Rmessage(int cmd, const void *buf, int dlen, int raw_data)
		:
		complete_(1),
		len_(0)
	{
		memset(&header_, 0, sizeof(header_));
		len_ = (raw_data) ? dlen : (dlen + 4);
		header_.cmd = cmd;
		header_.len = len_;
		alloc_data_only(len_);
		memcpy(get_data(), (raw_data) ? buf : ((char*)buf + 4), dlen);
		if (!raw_data)
			*((int*)get_data()) = itop(SET_PAR(DT_BYTESTREAM, dlen));
	}

	Rmessage::Rmessage(int cmd, int i)
		:
		complete_(1),
		len_(0)
	{
		memset(&header_, 0, sizeof(header_));
		len_ = 8; // DT_INT+len (4) + payload-1xINT (4)
		header_.cmd = cmd;
		header_.len = len_;
		alloc_data_only(8);
		*((int*)get_data()) = itop(SET_PAR(DT_INT, 4));
		((int*)get_data())[1] = itop(i);
	}

	int Rmessage::read(int s)
	{
		complete_ = 0;
		int n = recv(s, (char*)&header_, sizeof(header_), 0);
		if (n != sizeof(header_))
		{
			closesocket(s);
			s = -1;
			return (n == 0) ? -7 : -8;
		}
		Rsize_t i = len_ = header_.len = ptoi(header_.len);
		header_.cmd = ptoi(header_.cmd);
		header_.dof = ptoi(header_.dof);
		header_.res = ptoi(header_.res);
		if (header_.dof > 0)   // skip past DOF if present
		{
			char sb[256];
			int k = header_.dof;
			while (k > 0)
			{
				n = recv(s, sb, (k > 256) ? 256 : k, 0);
				if (n < 1)
				{
					closesocket(s);
					s = -1;
					return -8; // malformed packet
				}
				k -= n;
			}
		}
		if (i > 0)
		{
			alloc_data_only(i);
			char *dp = get_data();
			while (i > 0 && (n = recv(s, dp, i, 0)) > 0)
			{
				dp += n;
				i -= n;
			}
			if (i > 0)
			{
				closesocket(s);
				s = -1;
				return -8;
			}
		}
		parse();
		complete_ = 1;
		return 0;
	}

	void Rmessage::parse()
	{
		par_.clear();
		if (len_ < 4) return;
		char *c = get_data(), *eop = c + len_;
		while (c < eop)
		{
			int hs = 4;
			unsigned int *pp = (unsigned int*)c;
			unsigned int p1 = ptoi(pp[0]);

			Rsize_t len = p1 >> 8;
			if ((p1&DT_LARGE) > 0)
			{
				hs += 4;
				unsigned int p2 = ptoi(pp[1]);
				len |= ((Rsize_t)p2) << 24;
			}
#ifdef DEBUG_CXX
			std::cout << "  par " << par_.size() << ": " << (p1 & 0x3f)  
				<< " length " << len << "\n";
#endif
			par_.push_back((unsigned int*)c);
			c += hs;
			c += len;
		}
	}

	int Rmessage::send(int s)
	{
		int failed = 0;
		header_.cmd = itop(header_.cmd);
		header_.len = itop(header_.len);
		header_.dof = itop(header_.dof);
		header_.res = itop(header_.res);
		if (::send(s, (char*)&header_, sizeof(header_), 0) != sizeof(header_))
			failed = -1;
		if (!failed && len_ > 0 && (Rsize_t)::send(s, get_data(), len_, 0) != len_)
			failed = -1;
		header_.cmd = ptoi(header_.cmd);
		header_.len = ptoi(header_.len);
		header_.dof = ptoi(header_.dof);
		header_.res = ptoi(header_.res);
		return failed;
	}

	Rexp::Rexp(const std::shared_ptr<Rmessage>& msg) 
	:
		len_(0),
		type_(-1),
		data_(NULL),
		next_(NULL),
		buffer_(msg->get_buffer())
	{
		#ifdef DEBUG_CXX
		std::cout << "new Rexp@" << (void*)this << std::endl;
		#endif
		int hl=1;
		const unsigned int *hp = msg->get_par(0);
		Rsize_t plen=hp[0]>>8;
		if ((hp[0]&DT_LARGE)>0) {
			hl++;
			plen|=((Rsize_t)hp[1])<<24;
		}
		next_ = parseBytes(hp+hl);
	}
	
	Rexp::Rexp(const unsigned int *pos, const std::shared_ptr<MessageBuffer>& buffer)
	:
		len_(0),
		type_(-1),
		data_(NULL),
		next_(NULL),
		buffer_(buffer)
	{
#ifdef DEBUG_CXX
		std::cout << "new Rexp@" << static_cast<void*>(this) << std::endl;
#endif
		next_ = parseBytes(pos);
	}

	Rexp::Rexp(int type, const char *data, int len, std::shared_ptr<Rexp> attr)
	:
		len_(len),
		type_(type),
		data_(NULL),
		next_(NULL),
		attr_(attr)
	{
#ifdef DEBUG_CXX
		std::cout << "new Rexp2@" << static_cast<void*>(this) << std::endl;
#endif
		if (len)
		{
			buffer_ = std::make_shared<MessageBuffer>(len);
			data_ = buffer_->get<char>();
			memcpy(data_, data, len);
		}
		next_ = (char*)data + len;
	}

	std::shared_ptr<Rexp> Rexp::create(const std::shared_ptr<Rmessage>& msg)
	{
		int hl = 1;
		const unsigned int* d = msg->get_par(0);
		Rsize_t plen = d[0] >> 8;
		if ((d[0] & DT_LARGE) > 0)
		{
			hl++;	
			plen |= ((Rsize_t)d[1]) << 24;
		}

		return createFromBytes(d + hl, msg->get_buffer());
	}
		
	std::shared_ptr<Rexp> Rexp::createFromBytes(const unsigned int* d, const std::shared_ptr<MessageBuffer>& buffer)
	{
		int type = ptoi(*d) & 0x3f;

#ifdef DEBUG_CXX
		std::cout << "new_parsed_Rexp(" << (void*)(d) << ") type=" << type << std::endl;
#endif

		std::shared_ptr<Rexp> expr;

		switch (type)
		{
			case XT_ARRAY_INT:
			case XT_INT:
			{
				auto p = Rinteger::create(d, buffer);
				expr = std::shared_ptr<Rexp>(p, static_cast<Rexp*>(p.get()));
				break;
			}

			case XT_ARRAY_DOUBLE:
			case XT_DOUBLE:
			{
				auto p = Rdouble::create(d, buffer);
				expr = std::shared_ptr<Rexp>(p, static_cast<Rexp*>(p.get()));
				break;
			}

			case XT_VECTOR:
			{
				auto p = Rvector::create(d, buffer);
				expr = std::shared_ptr<Rexp>(p, static_cast<Rexp*>(p.get()));
				break;
			}

			case XT_STR:
			{
				auto p = Rstring::create(d, buffer);
				expr = std::shared_ptr<Rexp>(p, static_cast<Rexp*>(p.get()));
				break;
			}

			case XT_SYM:
			case XT_SYMNAME:
			{
				auto p = Rsymbol::create(d, buffer);
				expr = std::shared_ptr<Rexp>(p, static_cast<Rexp*>(p.get()));
				break;
			}

			case XT_ARRAY_STR:
			{
				auto p = Rstrings::create(d, buffer);
				expr = std::shared_ptr<Rexp>(p, static_cast<Rexp*>(p.get()));
				break;
			}

			default:
			{
				if (IS_LIST_TYPE_(type))
				{
					auto p = Rlist::create(d, buffer);
					expr = std::shared_ptr<Rexp>(p, static_cast<Rexp*>(p.get()));
				}
				else
					expr = Rexp::createFromBytes(d, buffer);
				break;
			}
		}

		return expr;
	}

	char *Rexp::parseBytes(const unsigned int *pos)
	{
		// plen is not used
		int hl = 1;
		unsigned int p1 = ptoi(pos[0]);
		len_ = p1 >> 8;
		if ((p1&XT_LARGE) > 0)
		{
			hl++;
			len_ |= ((Rsize_t)(ptoi(pos[1]))) << 24;
		}

		data_ = (char*)(pos + hl);

		if (p1&XT_HAS_ATTR)
		{
			attr_ = Rexp::createFromBytes((unsigned int*)data_, buffer_);
			len_ -= attr_->next_ - data_;
			data_ = attr_->next_;
		}
		type_ = p1 & 0x3f;

#ifdef DEBUG_CXX
		std::cout << "Rexp(type=" << type_ << ", len=" << len_ 
			<< ", attr=" << (void*)attr_.get() << ")\n";
#endif

		return data_ + len_;
	}

	void Rexp::store(char *buf) const
	{
		int hl = 4;
		unsigned int *i = (unsigned int*)buf;
		i[0] = SET_PAR(type_, len_);
		i[0] = itop(i[0]);
		if (len_ > 0x7fffff)
		{
			buf[0] |= XT_LARGE;
			i[1] = itop(len_ >> 24);
			hl += 4;
		}
		memcpy(buf + hl, data_, len_);
	}

	std::shared_ptr<Rexp> Rexp::attribute(const char *name) const
	{
		return (attr_ && IS_LIST_TYPE_(attr_->get_type())) ? static_cast<Rlist*>(attr_.get())->entryByTagName(name) : std::shared_ptr<Rexp>();
	}

	const std::vector<std::string>& Rexp::attributeNames() const
	{
		if (!attr_ || !IS_LIST_TYPE_(attr_->get_type()))
		{
			attrnames_.clear();
		}
		else if (attrnames_.empty())
		{
			std::shared_ptr<Rexp> L = attr_;
			while (L && IS_LIST_TYPE_(L->get_type()))
			{
				Rlist* LL = static_cast<Rlist*>(L.get());
				if (LL->get_tag() && IS_SYMBOL_TYPE_(LL->get_tag()->get_type()))
					attrnames_.push_back(std::string(((Rsymbol*)LL->get_tag().get())->symbolName()));
				L = LL->get_tail();
			}
		}
		return attrnames_;
	}

	void Rinteger::fix_content()
	{
		if (!data_) return;
#ifdef SWAPEND
		int *i = (int*) data_;
		int *j = (int*) (data_+len_);
		while (i<j)
		{
			*i=ptoi(*i);
			i++;
		}
#endif
	}

	void Rdouble::fix_content()
	{
		if (!data_) return;
#ifdef SWAPEND
		double *i = (double*) data_;
		double *j = (double*) (data_+len_);
		while (i<j)
		{
			*i=ptod(*i);
			i++;
		}
#endif
	}

	void Rsymbol::fix_content()
	{
		if (type_ == XT_SYM && *data_ == 3) name_ = data_ + 4; // normally the symbol should consist of a string SEXP specifying its name - no further content is defined as of now
		if (type_ == XT_SYMNAME) name_ = data_; // symname consists solely of the name
#ifdef DEBUG_CXX
		std::cout << "SYM " << (void*)this <<" \"" << name_ << "\"\n";
#endif
	}

	inline Rsize_t align_length(Rsize_t len)
	{
		return (((len) + 3L) & (rlen_max ^ 3L));
	}

	template<typename T>
	static inline char* putValue(char* p, T v)
	{
		memcpy(p, &v, sizeof(v));
		return p + sizeof(v);
	}

	static inline char* putLength(char* p, Rsize_t len)
	{
		Rsize_t txlen = len - 4;
		if (len > 0xfffff0)
		{
			p = putValue(p,  itop(SET_PAR(PAR_TYPE(((unsigned char*) p)[4] | XT_LARGE), txlen & 0xffffff)));
			p = putValue(p, itop(txlen >> 24));
		}
		else p = putValue(p, itop(SET_PAR(PAR_TYPE(ptoi(*p)), txlen)));
		return p;
	}

	std::shared_ptr<Rstrings> Rstrings::create(const std::vector<std::string>& sv)
	{
		// compute required buffer length
		Rsize_t len = 4, padding_len = 0; 
		for (const auto& s : sv)
			len += s.length() + 1 + ((!s.empty() && (unsigned char)s[0] == 0xFF) ? 1 : 0);
		if (len > 0xfffff0) len += 4;
		while (len & 3) { ++len; ++padding_len; }

#ifdef DEBUG_CXX
		std::cout << "Rstrings::create(): BufferLen=" << len << std::endl;
#endif

		// allocate buffer
		std::shared_ptr<MessageBuffer> buffer = std::make_shared<MessageBuffer>(len);

		// put element type
		char* p0 = buffer->get<char>();
		putValue(p0, static_cast<unsigned>(itop(XT_ARRAY_STR)));
		
		// put total length
		char* p = putLength(p0, len);

		for (const auto& s : sv)
		{
			if (!s.empty() && (unsigned char)s[0] == 0xFF) *p++ = (char)0xFF;
			strcpy(p, s.c_str());
			p += s.length() + 1;
		}
        for (Rsize_t k = 0; k < padding_len; ++k) *p++ = 1;

		return create(buffer->get<unsigned int>(), buffer);
	}

	void Rstrings::fix_content()
	{
		cont_.clear();
		char *c = data_;
		unsigned i = 0;
		while (i < len_)
		{
			char* p = c;
			while (*c && i < len_) ++c, ++i;
			if (i < len_)
			{
				cont_.push_back(p);
				++c; ++i;
			}
		}
	}

	void Rlist::fix_content()
	{
		char *ptr = data_;
		char *eod = data_ + len_;
#ifdef DEBUG_CXX
		std::cout << "Rlist::fix_content data_=" <<  (void*) ptr <<", type=" << type_ <<"\n";
#endif
		if (type_ == XT_LIST)
		{
			/* old-style lists */
			head_ = Rexp::createFromBytes((unsigned int*)ptr, buffer_);
			if (head_)
			{
				ptr = head_->get_next();
				if (ptr < eod)
				{
					tail_ = Rexp::createFromBytes((unsigned int*)ptr, buffer_);
					if (tail_)
					{
						ptr = static_cast<Rlist*>(tail_.get())->next_;
						if (ptr < eod)
							tag_ = Rexp::createFromBytes((unsigned int*)ptr, buffer_);
						if (tail_->get_type() != XT_LIST)
							tail_.reset();
					}
				}
			}
		}
		else if (type_ == XT_LIST_NOTAG)   /* new style list w/o tags */
		{
			std::shared_ptr<Rexp> lt = Rexp::shared_from_this();
			int n = 0;
			while (ptr < eod)
			{
				std::shared_ptr<Rexp> h = Rexp::createFromBytes((unsigned int*)ptr, buffer_);
				if (!h) break;
				if (n)
				{
					static_cast<Rlist*>(lt.get())->tail_.reset((Rexp*)new Rlist(type_, h, 0, h->get_next()));
					lt = static_cast<Rlist*>(lt.get())->tail_;
				}
				else
					static_cast<Rlist*>(lt.get())->head_ = h;
				n++;
				ptr = h->get_next();
			}
		}
		else if (type_ == XT_LIST_TAG)   /* new style list with tags */
		{
			std::shared_ptr<Rexp> lt = Rexp::shared_from_this();
			int n = 0;
			while (ptr < eod)
			{
				std::shared_ptr<Rexp> h = Rexp::createFromBytes((unsigned int*)ptr, buffer_);
#ifdef DEBUG_CXX
				std::cout << " LIST_TAG: n=" << n <<", ptr=" << (void*)ptr <<", h=" << h <<"\n";
#endif
				if (!h) break;
				ptr = h->get_next();
				std::shared_ptr<Rexp> t = Rexp::createFromBytes((unsigned int*)ptr, buffer_);

#ifdef DEBUG_CXX
				std::cout << "          tag=" << (void*)t.get() <<" (ptr=" << (void*)ptr <<")\n";
#endif

				if (!t) break;
				if (n)
				{
					static_cast<Rlist*>(lt.get())->tail_.reset((Rexp*)new Rlist(type_, h, t, t->get_next()));
					lt = static_cast<Rlist*>(lt.get())->tail_;
				}
				else
				{
					static_cast<Rlist*>(lt.get())->head_ = h;
					static_cast<Rlist*>(lt.get())->tag_ = t;
				}
				ptr = t->get_next();
				n++;
			}
			next_ = ptr;
		}
#ifdef DEBUG_CXX
		std::cout << " end of list " << (void*) this << ", ptr=" << (void*)ptr << "\n";
#endif
	}

	const std::vector<std::string>& Rvector::strings()
	{
		if (!strs_populated_)
		{
			for (const auto& p : cont_)
			{
				if (p->get_type() == XT_STR)
					strs_.push_back(static_cast<Rstring*>(p.get())->c_str());
			}
			strs_populated_ = true;
		}
		return strs_;
	}

	size_t Rvector::indexOf(const std::shared_ptr<Rexp>& exp) const
	{
		auto it = std::find(cont_.begin(), cont_.end(), exp);
		return it == cont_.end() ? std::string::npos : std::distance(cont_.begin(), it);
	}

	size_t Rvector::indexOfString(const char *str) const
	{
		size_t i = 0;
		for (const auto& p : cont_)
		{
			if (p && p->get_type() == XT_STR && !strcmp(((Rstring*)p.get())->c_str(), str))
				return i;
			++i;
		}
		return std::string::npos;
	}

	int Rstrings::indexOfString(const char *str)
	{
		for (size_t i = 0, n = cont_.size(); i < n; ++i)
			if (!strcmp(cont_[i], str)) return i;
		return -1;
	}


	void Rvector::fix_content()
	{
		char *ptr = data_;
		char *eod = data_ + len_;
		cont_.clear();
		while (ptr < eod)
		{
			cont_.push_back(Rexp::createFromBytes((unsigned int*)ptr, buffer_));
			if (cont_.back())
				ptr = cont_.back()->get_next();
			else
				break;
		}
	}

	std::shared_ptr<Rexp> Rvector::byName_Rexp(const char *name) const
	{
		/* here we are not using IS_LIST_TYPE_() because XT_LIST_NOTAG is guaranteed to not match */
		if (cont_.empty() || !attr_ || (attr_->get_type() != XT_LIST && attr_->get_type() != XT_LIST_TAG))
			return std::shared_ptr<Rexp>();

		std::shared_ptr<Rexp> e = ((Rlist*)attr_.get())->get_head();
		if (((Rlist*)attr_.get())->get_tag())
			e = ((Rlist*)attr_.get())->entryByTagName("names");
		if (!e || (e->get_type() != XT_VECTOR && e->get_type() != XT_ARRAY_STR && e->get_type() != XT_STR))
			return std::shared_ptr<Rexp>();
		if (e->get_type() == XT_VECTOR)
		{
			size_t pos = ((Rvector*)e.get())->indexOfString(name);
			if (pos != std::string::npos && pos < cont_.size()) return cont_[pos];
		}
		else if (e->get_type() == XT_ARRAY_STR)
		{
			size_t pos = ((Rstrings*)e.get())->indexOfString(name);
			if (pos != std::string::npos && pos < cont_.size()) return cont_[pos];
		}
		else
		{
			if (!strcmp(((Rstring*)e.get())->c_str(), name))
				return cont_[0];
		}
		return std::shared_ptr<Rexp>();
	}

	Rconnection::Rconnection(const char *host, int port)
		:
		host_(host),
		port_(port),
		family_((port == -1) ? AF_LOCAL : AF_INET),
		s_(-1),
		auth_(0)
	{
		salt_[0] = '.';
		salt_[1] = '.';
	}

	Rconnection::Rconnection(const Rsession& session)
	{
		const char *sHost = session.host();
		if (!sHost) sHost = "127.0.0.1";
		host_ = sHost;
		port_ = session.port();
		family_ = AF_INET;
		s_ = -1;
		auth_ = 0;
		salt_[0] = '.';
		salt_[1] = '.';
		session_key_.resize(32);
		memcpy(&session_key_[0], session.key(), 32);
	}

	int Rconnection::connect()
	{
#ifdef unix
		struct sockaddr_un sau;
#endif
		SAIN sai;
		char IDstring[33];

		if (family_ == AF_INET)
		{
			memset(&sai, 0, sizeof(sai));
			build_sin(&sai, (char*)host_.c_str(), port_);
		}
		else
		{
#ifdef unix
			memset(&sau, 0, sizeof(sau));
			sau.sun_family = AF_LOCAL;
			strcpy(sau.sun_path, host_.c_str()); // FIXME: possible overflow!
#else
			return -11;  // unsupported
#endif
		}

		IDstring[32] = 0;
		int i;

		s_ = socket(family_, SOCK_STREAM, 0);
		if (family_ == AF_INET)
		{
#ifdef CAN_TCP_NODELAY
			int opt = 1;
			setsockopt(s_, IPPROTO_TCP, TCP_NODELAY, (const char*) &opt, sizeof(opt));
#endif
			i = ::connect(s_, (SA*)&sai, sizeof(sai));
		}
#ifdef unix
		else
			i = ::connect(s_, (SA*)&sau, sizeof(sau));
#endif
		if (i == -1)
		{
			closesocket(s_);
			s_ = -1;
			return -1; // connect failed
		}

		if (!session_key_.empty())   // resume a session
		{
			int n = ::send(s_, &session_key_[0], 32, 0);
			if (n != 32)
			{
				closesocket(s_);
				s_ = -1;
				return -2; // handshake failed (session key send error)
			}
			std::shared_ptr<Rmessage> msg = Rmessage::create();
			int q = msg->read(s_);
			return q;
		}

		int n = recv(s_, IDstring, sizeof(IDstring), 0);
		if (n != 32)
		{
			closesocket(s_);
			s_ = -1;
			return -2; // handshake failed (no IDstring)
		}

		if (strncmp(IDstring, myID, 4))
		{
			closesocket(s_);
			s_ = -1;
			return -3; // invalid IDstring
		}

		if (strncmp(IDstring + 8, myID + 8, 4) || strncmp(IDstring + 4, myID + 4, 4) > 0)
		{
			closesocket(s_);
			s_ = -1;
			return -4; // protocol not supported
		}
	{
		int i = 12;
		while (i < 32)
		{
			if (!strncmp(IDstring + i, "ARuc", 4)) auth_ |= A_required | A_crypt;
			if (!strncmp(IDstring + i, "ARpt", 4)) auth_ |= A_required | A_plain;
			if (IDstring[i] == 'K')
			{
				salt_[0] = IDstring[i + 1];
				salt_[1] = IDstring[i + 2];
			}
			i += 4;
		}
	}
	return 0;
	}

	void Rconnection::disconnect()
	{
		if (s_ > -1)
		{
			closesocket(s_);
			s_ = -1;
		}
	}
	
	int Rconnection::getLastSocketError(char* buffer, int buffer_len, int options) const
	{
		return sockerrorchecks(buffer, buffer_len, options);
	}

	/**--- low-level functions --*/

	int Rconnection::request(Rmessage& msg, int cmd, int len, void *par)
	{
		struct phdr ph;

		if (s_ == -1) return -5; // not connected
		memset(&ph, 0, sizeof(ph));
		ph.len = itop(len);
		ph.cmd = itop(cmd);
		if (send(s_, (char*)&ph, sizeof(ph), 0) != sizeof(ph))
		{
			closesocket(s_);
			s_ = -1;
			return -9;
		}
		if (len > 0 && send(s_, (char*)par, len, 0) != len)
		{
			closesocket(s_);
			s_ = -1;
			return -9;
		}
		return msg.read(s_);
	}

	int Rconnection::request(Rmessage& targetMsg, Rmessage& contents)
	{
		if (s_ == -1) return -5; // not connected
		if (contents.send(s_))
		{
			closesocket(s_);
			s_ = -1;
			return -9; // send error
		}
		int res = targetMsg.read(s_);
		if (res) return res;
		return (targetMsg.get_header().cmd & RESP_ERR) == RESP_ERR ? -20 : 0;
	}

	/** --- high-level functions -- */

	int Rconnection::shutdown(const char *key)
	{
		std::shared_ptr<Rmessage> msg = Rmessage::create();
		std::shared_ptr<Rmessage> cmdMessage = key ? Rmessage::create(CMD_shutdown) : Rmessage::create(CMD_shutdown, key);
		int res = request(*msg, *cmdMessage);
		return res;
	}

	int Rconnection::assign(const char *symbol, const Rexp& exp)
	{
		std::shared_ptr<Rmessage> msg = Rmessage::create();
		std::shared_ptr<Rmessage> cmdMessage = Rmessage::create(CMD_setSEXP);

		int tl = strlen(symbol) + 1;
		if (tl & 3) tl = (tl + 4) & 0xfffc;
		Rsize_t xl = exp.storageSize();
		Rsize_t hl = 4 + tl + 4;
		if (xl > 0x7fffff) hl += 4;
		cmdMessage->alloc_data(hl + xl);
		((unsigned int*)cmdMessage->get_data())[0] = SET_PAR(DT_STRING, tl);
		((unsigned int*)cmdMessage->get_data())[0] = itop(((unsigned int*)cmdMessage->get_data())[0]);
		strcpy(cmdMessage->get_data() + 4, symbol);
		((unsigned int*)(cmdMessage->get_data() + 4 + tl))[0] = SET_PAR((Rsize_t)((xl > 0x7fffff) ? (DT_SEXP | DT_LARGE) : DT_SEXP), (Rsize_t)xl);
		((unsigned int*)(cmdMessage->get_data() + 4 + tl))[0] = itop(((unsigned int*)(cmdMessage->get_data() + 4 + tl))[0]);
		if (xl > 0x7fffff)
			((unsigned int*)(cmdMessage->get_data() + 4 + tl))[1] = itop(xl >> 24);
		exp.store(cmdMessage->get_data() + hl);

		int res = request(*msg, *cmdMessage);
		if (!res)
			res = CMD_STAT(msg->command());
		return res;
	}

	int Rconnection::voidEval(const char *cmd)
	{
		int status = 0;
		eval<Rexp>(cmd, &status, 1);
		return status;
	}

	std::shared_ptr<Rexp> Rconnection::eval_to_Rexp(const char *cmd, int *status, int opt)
	{
		/* opt = 1 -> void eval */
		std::shared_ptr<Rmessage> msg = Rmessage::create();
		std::shared_ptr<Rmessage> cmdMessage = Rmessage::create((opt & 1) ? CMD_voidEval : CMD_eval, cmd);
		int res = request(*msg, *cmdMessage);
		if (status) *status = res;
		if (res || (opt & 1))
			return std::shared_ptr<Rexp>();
		else if (msg->get_par_count() != 1 || (ptoi(msg->get_par(0, 0)) & 0x3f) != DT_SEXP)
		{
			if (status) *status = -12; // returned object is not SEXP
			return std::shared_ptr<Rexp>();
		}
		else
			return Rexp::create(msg);
	}

	/** detached eval (aka detached void eval) initiates eval and detaches the session.
	 *  @param cmd command to evaluate. If NULL equivalent to simple detach()
	 *  @param status optional status to be reported (zero on success)
	 *  @return object describintg he session.
	 *          Note that the caller is responsible for freeing the object if not needed. */
	std::shared_ptr<Rsession> Rconnection::detachedEval(const char *cmd, int *status)
	{
		std::shared_ptr<Rmessage> msg = Rmessage::create();
		std::shared_ptr<Rmessage> cmdMessage = cmd
			? Rmessage::create(CMD_detachedVoidEval, cmd)
			: Rmessage::create(CMD_detachedVoidEval);
		int res = request(*msg, *cmdMessage);
		if (res)
		{
			if (status) *status = res;
			return 0;
		}

		if (
			msg->get_par_count() != 2 ||
			PAR_TYPE(ptoi(msg->get_par(0, 0))) != DT_INT
			|| PAR_LEN(ptoi(msg->get_par(0, 0))) != sizeof(int)
			|| PAR_TYPE(ptoi(msg->get_par(1, 0))) != DT_BYTESTREAM
			|| PAR_LEN(ptoi(msg->get_par(1, 0))) != 32)
		{
			// invalid contents
			if (status) *status = -12;
			return std::shared_ptr<Rsession>();
		}

		std::shared_ptr<Rsession> session = Rsession::create(host_.c_str(), ptoi(msg->get_par(0, 1)), (const char*)(msg->get_par(1) + 1));
		if (status) *status = 0;
		return session;
	}

	std::shared_ptr<Rsession> Rconnection::detach(int *status)
	{
		return detachedEval(0, status);
	}

	int Rconnection::openFile(const char *fn)
	{
		std::shared_ptr<Rmessage> msg = Rmessage::create();
		std::shared_ptr<Rmessage> cmdMessage = Rmessage::create(CMD_openFile, fn);
		int res = request(*msg, *cmdMessage);
		if (!res) res = CMD_STAT(msg->command());
		return res;
	}

	int Rconnection::createFile(const char *fn)
	{
		std::shared_ptr<Rmessage> msg = Rmessage::create();
		std::shared_ptr<Rmessage> cmdMessage = Rmessage::create(CMD_createFile, fn);
		int res = request(*msg, *cmdMessage);
		if (!res) res = CMD_STAT(msg->command());
		return res;
	}

	Rsize_t Rconnection::readFile(char *buf, unsigned int len)
	{
		std::shared_ptr<Rmessage> msg = Rmessage::create();
		std::shared_ptr<Rmessage> cmdMessage = Rmessage::create(CMD_readFile, len);
		int res = request(*msg, *cmdMessage);
		if (!res)
		{
			// FIXME: Rserve up to 0.4-0 actually sends buggy response - it ommits DT_BYTESTREAM header!
			if (msg->get_len() > len)
				// we're in trouble here - techincally we should not get this
				return CERR_malformed_packet;

			if (msg->get_len() > 0)
				memcpy(buf, msg->get_data(), msg->get_len());
			return msg->get_len();
		}
		return CERR_io_error;
	}

	int Rconnection::writeFile(const char *buf, unsigned int len)
	{
		std::shared_ptr<Rmessage> msg = Rmessage::create();
		std::shared_ptr<Rmessage> cmdMessage = Rmessage::create(CMD_writeFile, buf, len);
		int res = request(*msg, *cmdMessage);
		if (!res && msg->command() == RESP_OK)
			return 0;
		// FIXME: this is not really true ...
		return (res == 0) ? CERR_io_error : res;
	}

	int Rconnection::closeFile()
	{
		std::shared_ptr<Rmessage> msg = Rmessage::create();
		std::shared_ptr<Rmessage> cmdMessage = Rmessage::create(CMD_closeFile);
		int res = request(*msg, *cmdMessage);
		if (!res && msg->command() == RESP_OK) return 0;
		return (res == 0) ? CERR_io_error : res; // FIXME was here, "this is not really true"
	}

	int Rconnection::removeFile(const char *fn)
	{
		std::shared_ptr<Rmessage> msg = Rmessage::create();
		std::shared_ptr<Rmessage> cmdMessage = Rmessage::create(CMD_removeFile, fn);
		int res = request(*msg, *cmdMessage);
		if (!res) res = CMD_STAT(msg->command());
		return res;
	}

	int Rconnection::login(const char *user, const char *pwd)
	{
		char *authbuf, *c;
		if (!(auth_&A_required)) return 0;
		std::vector<char> _buff(strlen(user) + strlen(pwd) + 22);
		authbuf = &_buff[0];
		strcpy(authbuf, user);
		c = authbuf + strlen(user);
		*c = '\n';
		c++;
		strcpy(c, pwd);

#ifdef unix
		if (auth_&A_crypt)
			strcpy(c, crypt(pwd, salt_));
#else
		if (!(auth_&A_plain))
		{
			return CERR_auth_unsupported;
		}
#endif

		std::shared_ptr<Rmessage> msg = Rmessage::create();
		std::shared_ptr<Rmessage> cmdMessage = Rmessage::create(CMD_login, authbuf);
		int res = request(*msg, *cmdMessage);
		if (!res) res = CMD_STAT(msg->command());
		return res;
	}

	// [IP] our custom API
	int Rconnection::queryCustomStatus(bool& status)
	{
		std::shared_ptr<Rmessage> msg = Rmessage::create();
		std::shared_ptr<Rmessage> cmdMessage = Rmessage::create(CMD_CustomStatus, "");
		int res = request(*msg, *cmdMessage);
		if (res == 0)
			status = msg->get_header().cmd == RESP_OK;
		return res;
	}

#ifdef CMD_ctrl

	/* server control methods */
	int serverEval(const char *cmd);
	int serverSource(const char *fn);
	int serverShutdown();

	int Rconnection::serverEval(const char *cmd)
	{
		std::shared_ptr<Rmessage> msg = Rmessage::create();
		std::shared_ptr<Rmessage> cmdMessage = Rmessage::create(CMD_ctrlEval, cmd);
		int res = request(*msg, *cmdMessage);
		if (!res) res = CMD_STAT(msg->command());
		return res;
	}

	int Rconnection::serverSource(const char *fn)
	{
		std::shared_ptr<Rmessage> msg = Rmessage::create();
		std::shared_ptr<Rmessage> cmdMessage = Rmessage::create(CMD_ctrlSource, fn);
		int res = request(*msg, *cmdMessage);
		if (!res) res = CMD_STAT(msg->command());
		return res;
	}

	int Rconnection::serverShutdown()
	{
		std::shared_ptr<Rmessage> msg = Rmessage::create();
		std::shared_ptr<Rmessage> cmdMessage = Rmessage::create(CMD_ctrlShutdown);
		int res = request(*msg, *cmdMessage);
		if (!res) res = CMD_STAT(msg->command());
		return res;
	}

} // namespace Rconnection2

#endif
