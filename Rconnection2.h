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
   MAIN     - should be defined in just one file that will contain the fn definitions and variables
              (this is inherited from Rsrv.h and sisocks.h)
*/
#pragma once 

#ifndef __RCONNECTION2_H__
#define __RCONNECTION2_H__

#if defined __GNUC__ && !defined unix && !defined WIN32
#define unix
#endif

#ifdef WIN32
#include <WinSock2.h>
#else
#include <sys/socket.h>
typedef int SOCKET;
#endif
#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>
#include <string>
#include <cstring>
#include <cstdint>
#include "Rsrv.h"

#ifdef WIN32
#if defined(RCONNECTION2_DLL_AS_LIB)
#define RCONNECTION2_API
#else
#if defined(BUILDING_RCONNECTION2_DLL)
#define RCONNECTION2_API __declspec(dllexport)
#else
#define RCONNECTION2_API __declspec(dllimport)
#endif
#endif
#else
#define RCONNECTION2_API
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4251)
#endif

namespace Rconnection2 {

	typedef uint32_t Rsize_t;

	//=== Rconnection error codes

#define CERR_connect_failed    -1
#define CERR_handshake_failed  -2
#define CERR_invalid_id        -3
#define CERR_protocol_not_supp -4
#define CERR_not_connected     -5
#define CERR_peer_closed       -7
#define CERR_malformed_packet  -8
#define CERR_send_error        -9
#define CERR_out_of_mem       -10
#define CERR_not_supported    -11
#define CERR_io_error         -12

	// this one is custom - authentication method required by
	// the server is not supported in this client
#define CERR_auth_unsupported -20


#define A_required 0x001
#define A_crypt    0x002
#define A_plain    0x004

	class RCONNECTION2_API IRconnection
	{
	protected:
		IRconnection() {}
	public:
		virtual ~IRconnection() {}
		virtual int connect() = 0;
		virtual bool disconnect() = 0;
		virtual SOCKET getSocket() const = 0;
	};

	//===================================== Rmessage ---- QAP1 storage

	class RCONNECTION2_API MessageBuffer
	{
	public:
		typedef unsigned int buffer_element_type;
		
		MessageBuffer() 
		:
			bytes_()
		{
		}

		explicit MessageBuffer(size_t nbytes) 
		: 
			bytes_((nbytes / sizeof(buffer_element_type)) + ((nbytes % sizeof(buffer_element_type)) ? 1 : 0)) 
		{
		}

		template<class T> T* get() const { return (T*)(&bytes_[0]); }

	private:
		std::vector<buffer_element_type> bytes_;
	};

	class RCONNECTION2_API Rmessage
	{
	public:
		std::shared_ptr<MessageBuffer> get_buffer() const { return data_; }

	protected:
		struct phdr header_;
		std::shared_ptr<MessageBuffer> data_;
		int complete_;
		Rsize_t len_;

		// the following is avaliable only for parsed messages (max 16 pars)
		std::vector<unsigned int *>par_;

		void alloc_data_only(size_t n)
		{
			if (n == 0) n = 1;
			data_ = std::make_shared<MessageBuffer>(n);
		}

	protected:
		Rmessage();
		Rmessage(int cmd); // 0 data_
		Rmessage(int cmd, const char *txt); // DT_STRING data_
		Rmessage(int cmd, int i); // DT_INT data_ (1 entry)
		Rmessage(int cmd, const void *buf, int len, int raw_data = 0); // raw data_ or DT_BYTESTREAM

	public:
		static std::shared_ptr<Rmessage> create() { return std::shared_ptr<Rmessage>(new Rmessage()); }
		static std::shared_ptr<Rmessage> create(int cmd) { return std::shared_ptr<Rmessage>(new Rmessage(cmd)); } // 0 data_
		static std::shared_ptr<Rmessage> create(int cmd, const char *txt)  { return std::shared_ptr<Rmessage>(new Rmessage(cmd, txt)); } // DT_STRING data_
		static std::shared_ptr<Rmessage> create(int cmd, int i) { return std::shared_ptr<Rmessage>(new Rmessage(cmd, i)); } // DT_INT data_ (1 entry)
		static std::shared_ptr<Rmessage> create(int cmd, const void *buf, int len, int raw_data = 0) { return std::shared_ptr<Rmessage>(new Rmessage(cmd, buf, len, raw_data)); } // raw data_ or DT_BYTESTREAM
		virtual ~Rmessage() {}

		int command() { return complete_ ? header_.cmd : -1; }
		Rsize_t length() { return complete_ ? header_.len : -1; }
		int is_complete() { return complete_; }
		const unsigned int* get_par(int index) const { return par_[index]; }
		unsigned get_par(int index1, int index2) { return par_[index1][index2]; }
		size_t get_par_count() const { return par_.size(); }
		Rsize_t get_len() const { return len_; }
		char* get_data() { return data_ ? data_->get<char>() : NULL; }
		const char* get_data() const { return data_ ? data_->get<char>() : NULL; }
		void alloc_data(size_t n)
		{
			alloc_data_only(n);
			len_ = header_.len = n;
		}

		const struct phdr& get_header() const { return header_; }

		int read(IRconnection& conn);
		void parse();

		int send(IRconnection& conn);
	};

	//===================================== Rexp --- basis for all SEXPs

	class RCONNECTION2_API Rexp : public std::enable_shared_from_this < Rexp >
	{
	private:
		explicit Rexp(const Rexp&);
		Rexp& operator=(const Rexp&);
		
	protected:
		Rsize_t len_;
		int type_;
		char* data_;
		char* next_;

		// the next_ two are only cached if requested, no direct access allowed
		std::vector<char> databuf_;
		mutable std::vector<std::string> attrnames_;
		std::shared_ptr<Rexp> attr_;
		std::shared_ptr<MessageBuffer> buffer_;

	protected:
		explicit Rexp(const std::shared_ptr<Rmessage>& msg);
		Rexp(const unsigned int *pos, const std::shared_ptr<MessageBuffer>& buffer);
		Rexp(int type, const char *data = 0, int len = 0, std::shared_ptr<Rexp> attr = std::shared_ptr<Rexp>());
		virtual void fix_content() {}
		char *parseBytes(const unsigned int *pos);
		static std::shared_ptr<Rexp> createFromBytes(const unsigned int *d, const std::shared_ptr<MessageBuffer>& buffer);

	public:
		static std::shared_ptr<Rexp> create(const std::shared_ptr<Rmessage>& msg);

		static std::shared_ptr<Rexp> create(const unsigned int *pos, 
			std::shared_ptr<Rmessage> msg = std::shared_ptr<Rmessage>())
		{
			return std::shared_ptr<Rexp>(new Rexp(pos, msg ? msg->get_buffer() 
				: std::shared_ptr<MessageBuffer>()));
		}

		static std::shared_ptr<Rexp> create(int type, const char *data = 0, int len = 0, 
			std::shared_ptr<Rexp> attr = std::shared_ptr<Rexp>())
		{
			return std::shared_ptr<Rexp>(new Rexp(type, data, len, attr));
		}

		virtual ~Rexp() {}

		int get_type() const { return type_; }

		virtual Rsize_t storageSize() const { return len_ + ((len_ > 0x7fffff) ? 8 : 4); }

		virtual void store(char *buf) const;
		std::shared_ptr<Rexp> attribute(const char *name) const;
		const std::vector<std::string>& attributeNames() const;
		char* get_next() const { return next_; }

		virtual Rsize_t length() { return len_; }

		friend std::ostream& operator<< (std::ostream& os, const Rexp& exp)
		{
			return ((Rexp&)exp).os_print(os);
		}

		friend std::ostream& operator<< (std::ostream& os, const Rexp* exp)
		{
			return ((Rexp*)exp)->os_print(os);
		}

		virtual std::ostream& os_print(std::ostream& os)
		{
			return os << "Rexp[type=" << type_ << ",len=" << len_ << "]";
		}
	};

	//===================================== Rint --- XT_INT/XT_ARRAY_INT

	class RCONNECTION2_API Rinteger : public Rexp {
	protected:
		Rinteger(const std::shared_ptr<Rmessage>& msg) : Rexp(msg) { }
		Rinteger(const unsigned int *ipos, const std::shared_ptr<MessageBuffer>& buffer) : Rexp(ipos, buffer) { }
		Rinteger(const int *array, int count) : Rexp(XT_ARRAY_INT, (char*)array, count*sizeof(int)) { }
		Rinteger(const std::vector<int>& array) : Rexp(XT_ARRAY_INT, (char*)&array[0], array.size()*sizeof(int)) {}
		Rinteger(const std::vector<unsigned>& array) : Rexp(XT_ARRAY_INT, (char*)&array[0], array.size()*sizeof(unsigned)) {}
		virtual void fix_content();

	public:
		static std::shared_ptr<Rinteger> create(const std::shared_ptr<Rmessage>& msg)
		{
			auto p = std::shared_ptr<Rinteger>(new Rinteger(msg));
			p->fix_content();
			return p;
		}

		static std::shared_ptr<Rinteger> create(const unsigned int *ipos, const std::shared_ptr<MessageBuffer>& buffer)
		{
			auto p = std::shared_ptr<Rinteger>(new Rinteger(ipos, buffer));
			p->fix_content();
			return p;
		}

		static std::shared_ptr<Rinteger> create(const int *array, int count)
		{
			auto p = std::shared_ptr<Rinteger>(new Rinteger(array, count));
			p->fix_content();
			return p;
		}

		static std::shared_ptr<Rinteger> create(const std::vector<int>& array)
		{
			auto p = std::shared_ptr<Rinteger>(new Rinteger(array));
			p->fix_content();
			return p;
		}

		static std::shared_ptr<Rinteger> create(const std::vector<unsigned>& array)
		{
			auto p = std::shared_ptr<Rinteger>(new Rinteger(array));
			p->fix_content();
			return p;
		}

		virtual ~Rinteger() {}

		int *intArray() { return (int*)data_; }
		int intAt(int pos) { return (pos >= 0 && (unsigned)pos < len_ / 4) ? ((int*)data_)[pos] : 0; }
		virtual Rsize_t length() { return len_ / 4; }

		virtual std::ostream& os_print(std::ostream& os)
		{
			return os << "Rinteger[" << (len_ / 4) << "]";
		}

	};

	//===================================== Rdouble --- XT_DOUBLE/XT_ARRAY_DOUBLE

	class RCONNECTION2_API Rdouble : public Rexp {
	protected:
		Rdouble(const std::shared_ptr<Rmessage>& msg) : Rexp(msg) { }
		Rdouble(const unsigned int *ipos, const std::shared_ptr<MessageBuffer>& buffer) : Rexp(ipos, buffer) {}
		Rdouble(const double *array, int count) : Rexp(XT_ARRAY_DOUBLE, (char*)array, count*sizeof(double)) {}
		Rdouble(const std::vector<double>& array) : Rexp(XT_ARRAY_DOUBLE, (char*)&array[0], array.size()*sizeof(double)) {}
		virtual void fix_content();

	public:
		static std::shared_ptr<Rdouble> create(const std::shared_ptr<Rmessage>& msg)
		{
			auto p = std::shared_ptr<Rdouble>(new Rdouble(msg));
			p->fix_content();
			return p;
		}

		static std::shared_ptr<Rdouble> create(const unsigned int *ipos, const std::shared_ptr<MessageBuffer>& buffer)
		{
			auto p = std::shared_ptr<Rdouble>(new Rdouble(ipos, buffer));
			p->fix_content();
			return p;
		}

		static std::shared_ptr<Rdouble> create(const double *array, int count)
		{
			auto p = std::shared_ptr<Rdouble>(new Rdouble(array, count));
			p->fix_content();
			return p;
		}

		static std::shared_ptr<Rdouble> create(const std::vector<double>& array)
		{
			auto p = std::shared_ptr<Rdouble>(new Rdouble(array));
			p->fix_content();
			return p;
		}

		virtual ~Rdouble() {}

		double *doubleArray() { return (double*)data_; }
		double doubleAt(int pos) { return (pos >= 0 && (unsigned)pos < len_ / 8) ? ((double*)data_)[pos] : 0; }
		virtual Rsize_t length() { return len_ / 8; }

		virtual std::ostream& os_print(std::ostream& os)
		{
			return os << "Rdouble[" << (len_ / 8) << "]";
		}
	};

	//===================================== Rsymbol --- XT_SYM

	class RCONNECTION2_API Rsymbol : public Rexp
	{
	protected:
		std::string name_;
		Rsymbol(const std::shared_ptr<Rmessage>& msg) : Rexp(msg), name_() {}
		Rsymbol(const unsigned int *ipos, const std::shared_ptr<MessageBuffer>& buffer) : Rexp(ipos, buffer), name_() {}
		virtual void fix_content();

	public:
		static std::shared_ptr<Rsymbol> create(const std::shared_ptr<Rmessage>& msg)
		{
			auto p = std::shared_ptr<Rsymbol>(new Rsymbol(msg));
			p->fix_content();
			return p;
		}

		static std::shared_ptr<Rsymbol> create(const unsigned int *ipos, const std::shared_ptr<MessageBuffer>& buffer)
		{
			auto p = std::shared_ptr<Rsymbol>(new Rsymbol(ipos, buffer));
			p->fix_content();
			return p;
		}

		virtual ~Rsymbol() {}

		const char *symbolName() { return name_.c_str(); }

		virtual std::ostream& os_print(std::ostream& os)
		{
			return os << "Rsymbol[" << symbolName() << "]";
		}
	};

	//===================================== Rstrings --- XT_ARRAY_STR
	// NOTE: XT_ARRAY_STR is new in 0103 and ths class is just a
	//       very crude implementation. It replaces Rstring because
	//       XT_STR has been deprecated.
	// FIXME: it should be a subclass of Rvector!
	class RCONNECTION2_API Rstrings : public Rexp
	{
	protected:
		std::vector<const char*> cont_;

		Rstrings(const std::shared_ptr<Rmessage>& msg) : Rexp(msg), cont_() {}
		Rstrings(const unsigned int *ipos, const std::shared_ptr<MessageBuffer>& buffer) : Rexp(ipos, buffer), cont_() 
{}
		virtual void fix_content();

	public:
		static std::shared_ptr<Rstrings> create(const std::shared_ptr<Rmessage>& msg)
		{
			auto p = std::shared_ptr<Rstrings>(new Rstrings(msg));
			p->fix_content();
			return p;
		}

		static std::shared_ptr<Rstrings> create(const unsigned int *ipos, const std::shared_ptr<MessageBuffer>& buffer)
		{
			auto p = std::shared_ptr<Rstrings>(new Rstrings(ipos, buffer));
			p->fix_content();
			return p;
		}

		static std::shared_ptr<Rstrings> create(const std::vector<std::string>& sv);

		virtual ~Rstrings() {}

		const std::vector<const char*>& strings() const { return cont_; }
		std::string str(size_t i = 0) const { return cont_.at(i); }

		unsigned int count() { return cont_.size(); }
		int indexOfString(const char *str);

		virtual std::ostream& os_print(std::ostream& os)
		{
			return os << "char*[" << cont_.size() << "]\"" << str() << "\"..";
		}
	};

	//===================================== Rstring --- XT_STR

	class RCONNECTION2_API Rstring : public Rexp
	{
	protected:
		Rstring(const std::shared_ptr<Rmessage>& msg) : Rexp(msg) {}
		Rstring(const unsigned int *ipos, const std::shared_ptr<MessageBuffer>& buffer) : Rexp(ipos, buffer) {}
		Rstring(const char *str) : Rexp(XT_STR, str, strlen(str) + 1) {}

	public:
		static std::shared_ptr<Rstring> create(const std::shared_ptr<Rmessage>& msg)
		{
			auto p = std::shared_ptr<Rstring>(new Rstring(msg));
			p->fix_content();
			return p;
		}

		static std::shared_ptr<Rstring> create(const unsigned int *ipos, const std::shared_ptr<MessageBuffer>& buffer)
		{
			auto p = std::shared_ptr<Rstring>(new Rstring(ipos, buffer));
			p->fix_content();
			return p;
		}

		static std::shared_ptr<Rstring> create(const char *str)
		{
			auto p = std::shared_ptr<Rstring>(new Rstring(str));
			p->fix_content();
			return p;
		}

		virtual ~Rstring() {}


		const char* c_str() { return (char*)data_; }

		virtual std::ostream& os_print(std::ostream& os)
		{
			return os << "\"" << c_str() << "\"";
		}
	};



	//===================================== Rlist --- XT_LIST (CONS lists)

	class RCONNECTION2_API Rlist : public Rexp
	{
	protected:
		std::shared_ptr<Rexp> head_, tag_, tail_;

		Rlist(const std::shared_ptr<Rmessage>& msg) : Rexp(msg), head_(), tag_(), tail_() {}

		Rlist(const unsigned int *ipos, const std::shared_ptr<MessageBuffer>& buffer) 
		: 
			Rexp(ipos, buffer), 
			head_(), tag_(), tail_()
		{
		}

		/* this is a sort of special constructor that allows to create a Rlist
		   based solely on its content. This is necessary since 0.5 because
		   each LISTSXP is no longer represented by its own encoded SEXP
		   but they are packed in one content list instead */
		Rlist(int type, const std::shared_ptr<Rexp>& head, const std::shared_ptr<Rexp>& tag,
			char *next)
			:
			Rexp(type, 0, 0),
			head_(head),
			tag_(tag),
			tail_()
		{
			next_ = next;
		}

	public:
		static std::shared_ptr<Rlist> create(const std::shared_ptr<Rmessage>& msg)
		{
			auto p = std::shared_ptr<Rlist>(new Rlist(msg));
			p->fix_content();
			return p;
		}

		static std::shared_ptr<Rlist> create(const unsigned int *ipos, const std::shared_ptr<MessageBuffer>& buffer)
		{
			auto p = std::shared_ptr<Rlist>(new Rlist(ipos, buffer));
			p->fix_content();
			return p;
		}

		static std::shared_ptr<Rlist> create(int type, const std::shared_ptr<Rexp>& head, const std::shared_ptr<Rexp>& tag,
			char *next)
		{
			auto p = std::shared_ptr<Rlist>(new Rlist(type, head, tag, next));
			p->fix_content();
			return p;
		}

		virtual ~Rlist() {}

		std::shared_ptr<Rexp> get_head() const { return head_; }
		std::shared_ptr<Rexp> get_tail() const { return tail_; }
		std::shared_ptr<Rexp> get_tag() const { return tag_; }

		std::shared_ptr<Rexp> entryByTagName(const char *tagName)
		{
			if (tag_ && (tag_->get_type() == XT_SYM || tag_->get_type() == XT_SYMNAME)
				&& !strcmp((static_cast<Rsymbol*>(tag_.get()))->symbolName(), tagName))
				return head_;
			else if (tail_)
				return static_cast<Rlist*>(tail_.get())->entryByTagName(tagName);
			else
				return std::shared_ptr<Rexp>();
		}

		virtual std::ostream& os_print(std::ostream& os)
		{
			os << "Rlist[tag=";
			if (tag_) os << *tag_; else os << "<none>";
			os << ",head_=";
			if (head_) os << *head_; else os << "<none>";
			if (tail_) os << ",tail=" << *tail_;
			return os << "]";
		}

		virtual void fix_content();
	};

	//===================================== Rvector --- XT_VECTOR (general lists)

	class RCONNECTION2_API Rvector : public Rexp
	{
	protected:
		mutable std::vector< std::shared_ptr<Rexp> >cont_;

		// cached
		std::vector<std::string> strs_;
		bool strs_populated_;

		Rvector(const std::shared_ptr<Rmessage>& msg)
			:
			Rexp(msg),
			cont_(),
			strs_(),
			strs_populated_(false)
		{
		}

		Rvector(const unsigned int *ipos, const std::shared_ptr<MessageBuffer>& buffer)
			:
			Rexp(ipos, buffer),
			cont_(),
			strs_(),
			strs_populated_(false)
		{
		}

	public:
		static std::shared_ptr<Rvector> create(const std::shared_ptr<Rmessage>& msg)
		{
			auto p = std::shared_ptr<Rvector>(new Rvector(msg));
			p->fix_content();
			return p;
		}

		static std::shared_ptr<Rvector> create(const unsigned int *ipos, const std::shared_ptr<MessageBuffer>& buffer)
		{
			auto p = std::shared_ptr<Rvector>(new Rvector(ipos, buffer));
			p->fix_content();
			return p;
		}

		virtual ~Rvector() {}

		const std::vector<std::string>& strings();
		size_t indexOf(const std::shared_ptr<Rexp>& exp) const;
		size_t indexOfString(const char *str) const;
		virtual Rsize_t length() { return (Rsize_t)cont_.size(); }

		const char *stringAt(size_t i)
		{
			if (i >= cont_.size() || !cont_[i] || cont_[i]->get_type() != XT_STR) 
				return 0;
			else
				return ((Rstring*)cont_[i].get())->c_str();
		}

		std::shared_ptr<Rexp> elementAt(int i) { return cont_[i]; }
		
		template<class V> std::shared_ptr<V> byName(const char *name) const
		{
			std::shared_ptr<Rexp> p = byName_Rexp(name);
			return std::shared_ptr<V>(p, static_cast<V*>(p.get()));
		}
		
		std::shared_ptr<Rexp> byName_Rexp(const char *name) const;

		virtual std::ostream& os_print(std::ostream& os)
		{
			os << "Rvector[count=" << cont_.size() << ":";
			int i = 0;
			for (const auto& p : cont_)
			{
				if (i) os << ",";
				if (p) os << *cont_[i]; else os << "NULL";
				i++;
			}
			return os << "]";
		}

		virtual void fix_content();
	};

	//===================================== Rconnection ---- Rserve interface class

	class Rconnection;

	class RCONNECTION2_API Rsession
	{
	protected:
		std::string host_;
		int port_;
		char key_[32];

		Rsession(const char *host, int port, const char key[32])
		:
			host_(host),
			port_ (port)
		{
			memcpy(key_, key, 32);
		}

	public:
		static std::shared_ptr<Rsession> create(const char *host, int port, const char key[32])
		{
			return std::shared_ptr<Rsession>(new Rsession(host, port, key));
		}

		~Rsession() {}

		const char *host() const { return host_.c_str(); }
		int port() const { return port_; }
		const char *key() const { return key_; }
	};

	class RCONNECTION2_API Rconnection: public IRconnection
	{
	protected:
		std::string host_;
		int  port_;
		int  family_;
		SOCKET s_;
		int auth_;
		char salt_[2];
		std::vector<char> session_key_;

		/** host - either host name or unix socket path
			port - either TCP port or -1 if unix sockets should be used */
		explicit Rconnection(const char *host = "127.0.0.1", int port = default_Rsrv_port);
		explicit Rconnection(const Rsession& session);

	public:
		static std::shared_ptr<Rconnection> create(const char *host = "127.0.0.1", int port = default_Rsrv_port)
		{
			return std::shared_ptr<Rconnection>(new Rconnection(host, port));
		}

		static std::shared_ptr<Rconnection> create(const Rsession& session)
		{
			return std::shared_ptr<Rconnection>(new Rconnection(session));
		}

		virtual ~Rconnection()
		{
			disconnect();
		}

		virtual int connect();
		virtual bool disconnect();
		virtual SOCKET getSocket() const { return s_; }
		
		int getLastSocketError(char* buffer, int buffer_len, int options) const;

		/** --- high-level functions --- */

		int assign(const char *symbol, const Rexp& exp);
		int voidEval(const char *cmd);
		template<class V> std::shared_ptr<V> eval(const char *cmd, int *status = 0, int opt = 0)
		{
			std::shared_ptr<Rexp> p = eval_to_Rexp(cmd, status, opt);
			return std::shared_ptr<V>(p, static_cast<V*>(p.get()));
		}

		std::shared_ptr<Rexp> eval_to_Rexp(const char *cmd, int *status, int opt);

		int login(const char *user, const char *pwd);
		int shutdown(const char *key);

		/*      ( I/O functions )     */
		int openFile(const char *fn);
		int createFile(const char *fn);
		Rsize_t readFile(char *buf, unsigned int len);
		int writeFile(const char *buf, unsigned int len);
		int closeFile();
		int removeFile(const char *fn);

		/* session methods - results of detach [if not NULL] must be deleted by the caller when no longer needed! */
		std::shared_ptr<Rsession> detachedEval(const char *cmd, int *status = 0);
		std::shared_ptr<Rsession> detach(int *status = 0);
		// sessions are resumed using resume() method of the Rsession object

		int queryCustomStatus(bool& status); // [IP] our custom API


#ifdef CMD_ctrl
		/* server control functions (need Rserve 0.6-0 or higher) */
		int serverEval(const char *cmd);
		int serverSource(const char *fn);
		int serverShutdown();
#endif

	protected:

		int request(Rmessage& msg, int cmd, int len = 0, void *par = 0);
		int request(Rmessage& targetMsg, Rmessage& contents);

	};

} // namespace Rconnection2

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
