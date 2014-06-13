#include <iostream>
#include <tuple>
#include <stdexcept>
#include <string>
#include <boost/optional.hpp>
#include <boost/format.hpp>
#include <vector>
#include "../lua-5.2.3/src/lua.hpp"

using std::cout;
using std::endl;
using std::cerr;

namespace TupleHelpers {
	template<std::size_t I, typename... Tp>
	struct forward_comparator
	{
		static const bool value = (I < sizeof...(Tp));
		static const std::size_t start = 0;
		static const std::size_t next = I + 1;
	};

	template<std::size_t I, typename... Tp>
	struct reverse_comparator
	{
		static const std::size_t start = sizeof...(Tp)-1;
		static const std::size_t next = I - 1;
		static const bool value = I >= 0 && I <= start;
	};

	template<
		template<std::size_t, typename...>class Comparator,
		std::size_t I,
		class F,
		typename... Tp
	>
	inline typename std::enable_if<!Comparator<I, Tp...>::value>::type
	citerate(lua_State*, const std::tuple<Tp...>&, F&)
	{ }

	template<
		template<std::size_t, typename...>class Comparator,
		std::size_t I,
		class F,
		typename... Tp
	>
	inline typename std::enable_if<Comparator<I, Tp...>::value>::type
	citerate(lua_State* l, const std::tuple<Tp...>& t, F& f)
	{
		typedef Comparator<I, Tp...> comparator;
		f(l, std::get<I>(t));
		citerate<Comparator, comparator::next, F, Tp...>(l, t, f);
	}

	// Helper for specifying start value
	template<
		template<std::size_t, typename...>class Comparator,
		class F,
		typename... Tp
	>
    inline void citerate(lua_State* l, const std::tuple<Tp...>& t, F& f)
	{
		citerate<Comparator, Comparator<0, Tp...>::start, F, Tp...>(l, t, f);
	}

    template<
        template<std::size_t, typename...>class Comparator,
        class F,
        typename... Tp
    >
    inline void citerate(lua_State* l, const std::tuple<Tp...>& t)
    {
        F f;
        citerate<Comparator, Comparator<0, Tp...>::start, F, Tp...>(l, t, f);
    }


	// ---------------------------------------------------
	/* couldn't get const versions working... easy fix, for now */
	template<
		template<std::size_t, typename...>class Comparator,
		std::size_t I,
		class F,
		typename... Tp
	>
	inline typename std::enable_if<!Comparator<I, Tp...>::value>::type
	iterate(lua_State*, std::tuple<Tp...>&, F&)
	{ }

	template<
		template<std::size_t, typename...>class Comparator,
		std::size_t I,
		class F,
		typename... Tp
	>
	inline typename std::enable_if<Comparator<I, Tp...>::value>::type
	iterate(lua_State* l, std::tuple<Tp...>& t, F& f)
	{
		typedef Comparator<I, Tp...> comparator;
		f(l, std::get<I>(t));
		iterate<Comparator, comparator::next, F, Tp...>(l, t, f);
	}

	// Helper for specifying start value
	template<
		template<std::size_t, typename...>class Comparator,
		class F,
		typename... Tp
	>
    inline void iterate(lua_State* l, std::tuple<Tp...>& t, F& f)
	{
		iterate<Comparator, Comparator<0, Tp...>::start, F, Tp...>(l, t, f);
	}

    template<
        template<std::size_t, typename...>class Comparator,
        class F,
        typename... Tp
    >
    inline void iterate(lua_State* l, std::tuple<Tp...>& t)
    {
        F f;
        iterate<Comparator, Comparator<0, Tp...>::start, F, Tp...>(l, t, f);
    }
};

class LuaException : public std::exception {
	const std::string msg;
public:
	explicit LuaException(const std::string& what)
		: msg("lua_exception: " + what)
	{
	}

	virtual const char* what() const override {
		return msg.c_str();
	}
};

namespace LuaHelpers {

	// ------------------------------------------------------------------------------
	//
	template <typename T>
	typename std::enable_if<std::is_arithmetic<T>::value, const char*>::type
		ctype_name()
	{
		return "number";
	}
	template <> const char* ctype_name<const bool>() { return "boolean"; }

	template <typename T>
	typename std::enable_if<std::is_same<typename std::decay<T>::type, std::string>::value, const char*>::type
		ctype_name()
	{
		return "string";
	}
	//
	// ------------------------------------------------------------------------------
	//
	// Reference to any Lua type, stored within the Lua VM
	struct LuaAnyRef {
		lua_State* L;
		int ref;

		LuaAnyRef()
			: L(nullptr), ref(LUA_NOREF)
		{}

		// Non-copyable
		LuaAnyRef(const LuaAnyRef&) = delete;
		LuaAnyRef& operator=(const LuaAnyRef&) = delete;

		LuaAnyRef(LuaAnyRef&& other)
			: L(std::move(other.L)), ref(std::move(other.ref))
		{
			other.L = 0; other.ref = 0;
		}

		LuaAnyRef& operator=(LuaAnyRef&& other) {
			L = std::move(other.L);
			ref = std::move(other.ref);
			other.L = 0;
			other.ref = 0;
			return *this;
		}
		
		~LuaAnyRef() {
			if (L) {
				luaL_unref(L, LUA_REGISTRYINDEX, ref);
			}
		}

		void push(lua_State* L) const {
			assert(L == nullptr || L == this->L);
			lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
		}

		void pop(lua_State* L) {
			if (this->L)
				luaL_unref(this->L, LUA_REGISTRYINDEX, ref);
			this->L = L;			
			ref = luaL_ref(L, LUA_REGISTRYINDEX);
		}
	};

	//
	// ------------------------------------------------------------------------------
	//

	struct LuaNil {};

	struct Push {
		template <typename T>
		typename std::enable_if<std::is_arithmetic<T>::value, void>::type
			operator()(lua_State* L, const T& x)
		{
			lua_pushnumber(L, x);
		}

		void operator()(lua_State* L, const LuaAnyRef& x) {
			x.push(L);
		}

		void operator()(lua_State* L, const LuaNil&) {
			lua_pushnil(L);
		}

		void operator()(lua_State* L, const char* x) {
			lua_pushstring(L, x);
		}
		void operator()(lua_State* L, const std::string& x) {
			lua_pushstring(L, x.c_str());
		}
	};

    template <> void Push::operator()<bool>(lua_State* L, const bool& x) {
        lua_pushboolean(L, x);
    }

	struct Pop {
		enum class e_mode {
			kArg,
			kRet
		};

		int n;
		e_mode mode;
		bool err;

		Pop(int n, e_mode mode)
			: n(n), mode(mode), err(false)
		{}

		template <typename Y>
		void raise_error(lua_State* L, int got) {
			if (mode == e_mode::kArg) {
				auto f = boost::format("expected %s, got %s") % ctype_name<Y>() % lua_typename(L, got);
				luaL_argerror(L, n, f.str().c_str());
			} else {
				err = true;
			}
		}

		void pop(lua_State* L) {
			lua_pop(L, 1);
			n--;
		}

		template <typename Y>
		void pop_number_or_bool(lua_State* L, Y& x) {
			const char* str;
			char* end;
			int ltype = lua_type(L, -1);
			switch (ltype) {
			case LUA_TNUMBER:
				x = static_cast<Y>(lua_tonumber(L, -1));
				break;
			case LUA_TBOOLEAN: 
				x = static_cast<Y>(lua_toboolean(L, -1));
				break;
			case LUA_TSTRING:
				str = lua_tostring(L, -1);
				x = static_cast<Y>(strtol(str, &end, 10));
				if (*end != '\0')
					raise_error<Y>(L, ltype);
				break;
			default:
				raise_error<Y>(L, ltype);
				break;
			}
			pop(L);
		}

		template <typename T>
		typename std::enable_if<std::is_arithmetic<T>::value, void>::type 
			operator()(lua_State* L, T& x)
		{			
			pop_number_or_bool(L, x);
		}

		void operator()(lua_State* L, std::string& x) {
			int ltype = lua_type(L, -1);
			switch (ltype) {
			case LUA_TBOOLEAN:
				x = (lua_toboolean(L, -1) == 1) ? "true" : "false";
				break;
			case LUA_TSTRING:
			case LUA_TNUMBER:
				x = lua_tostring(L, -1);
				break;
			case LUA_TNIL:
				x = "nil";
				break;
			default:
				raise_error<std::string>(L, ltype);
			}
			pop(L);
		}

		void operator()(lua_State* L, LuaAnyRef& r) {
			r.pop(L);
			n--;
		}

		template <typename Y>
		void operator()(lua_State* L, boost::optional<Y>& x) {
			if (lua_type(L, -1) == LUA_TNIL) {
				pop(L);
			} else {
				Y val;
				operator()(L, val);
				x = val;
			}
		}
	};

    template <> void Pop::operator()<bool>(lua_State* L, bool& x) {
        int x1;
        pop_number_or_bool(L, x1);
        x = (x1 == 1);
    }

}

struct LuaState  {
	lua_State* L;

	LuaState() {
		L = luaL_newstate();
		if (!L) // only NULL if memory error
			throw std::bad_alloc();
		luaL_openlibs(L);
	}

	~LuaState() {
		lua_close(L);
	}

	operator lua_State*() { return L; }

	void doFile(const std::string& file) {
		if (luaL_dofile(L, file.c_str())) {
			std::string error = lua_tostring(L, -1);
			lua_pop(L, 1);
			throw LuaException(error);
		}
	}

	void pcall(int nargs, int nresults) {
		if (lua_pcall(L, nargs, nresults, 0)) {
			std::string error = lua_tostring(L, -1);
			lua_pop(L, 1);
			throw LuaException(error);
		}
	}

	template <typename T>
	void setGlobal(const std::string& name, const T& value) {
		LuaHelpers::Push p;
		p(L, value);
		lua_setglobal(L, name.c_str());
	}

	template <typename T>
	boost::optional<T> getGlobal(const std::string& name) {
		boost::optional<T> result;
		LuaHelpers::Pop p;
		p(L, result);
		return result;
	}
};

template <typename... ResultTypes>
class LuaCaller {
private:
	static const size_t nresults = sizeof...(ResultTypes);
	LuaState& L;
	bool err;

public:

	LuaCaller(LuaState& L)
		: L(L), err(false)
	{}

	// whether or not an error occurred popping return values
	bool hasError() const {
		return err;
	}

	template <typename... ArgTypes> 
	std::tuple<ResultTypes...> call(const std::string& func, const std::tuple<ArgTypes...>& args) {
		lua_getglobal(L, func.c_str());
		if (lua_type(L, -1) != LUA_TFUNCTION) {
			lua_pop(L, 1);
			throw LuaException("attempt to call non-function entity");
		}

		size_t nargs = sizeof...(ArgTypes);
		size_t nresults = sizeof...(ResultTypes);
		
		TupleHelpers::citerate<TupleHelpers::forward_comparator, LuaHelpers::Push>(L, args);

		L.pcall(nargs, nresults);

		std::tuple<ResultTypes...> results;
		LuaHelpers::Pop p(nresults, LuaHelpers::Pop::e_mode::kRet);
		TupleHelpers::iterate<TupleHelpers::reverse_comparator, LuaHelpers::Pop>(L, results, p);
		err = p.err;
		return results;
	}
};

namespace LuaCallback {
	struct CFunc {
		const char* name;
		lua_CFunction func;
	};

	template <class Itr>
	static void registerFunctions(lua_State* L, Itr itr, const Itr end) {
		for (; itr != end; ++itr) {
			lua_register(L, itr->name, itr->func);
		}
	}
	
	// Counts number of boost::optional<> parameters (starting from end).
	// -------------------------------------------------------------------------------------
	//
	template <typename Y>
	size_t do_count(boost::optional<Y>) { return 1; }
	template <typename T>
	size_t do_count(T&) { return 0; }
	template <size_t N>
	struct more_than_zero {
		static const bool value = N > 0;
	};

    template <std::size_t N, typename... T>
    size_t count_optional(const std::tuple<T...>&,
        typename std::enable_if<!more_than_zero<N>::value>::type* = 0) {
        return 0;
    }

	template <std::size_t N, typename... T>
	size_t count_optional(const std::tuple<T...>& x,
		typename std::enable_if<more_than_zero<N>::value>::type* = 0) {
		auto c = do_count(std::get<N-1>(x));
		if (c == 0) return 0;
		return 1 + count_optional<N - 1, T...>(x);
	}

	template <typename... T>
	size_t count_optional(const std::tuple<T...>& x) {
		return count_optional<sizeof...(T), T...>(x);
	}
	//
	// -------------------------------------------------------------------------------------
	
	template <typename... ResultTypes>
	std::tuple<ResultTypes...> getArguments(lua_State* L, const char* funcname) {
		std::tuple<ResultTypes...> args;
		static const size_t noptional = count_optional(args);
		static const size_t nrequired = sizeof...(ResultTypes) - noptional;

		size_t nargs = lua_gettop(L);
		if (nargs < nrequired) {
			luaL_error(L, "'%s' expects at least %d argument(s) (got %d)", funcname, nrequired, nargs);
		} else if (nargs > nrequired + noptional) {
			luaL_error(L, "'%s' expects at most %d argument(s) (got %d)", funcname, noptional + nrequired, nargs);
		} else {
			size_t diff = noptional - (nargs - nrequired);
			for (size_t x = 0; x < diff; x++)
				lua_pushnil(L);
		}
		nargs = nrequired + noptional;

		LuaHelpers::Pop p(nargs, LuaHelpers::Pop::e_mode::kArg);
		TupleHelpers::iterate<TupleHelpers::reverse_comparator, LuaHelpers::Pop>(L, args, p);
		return args;
	}

	template <typename... Types>
	int pushReturns(lua_State* L, const std::tuple<Types...>& t) {
		TupleHelpers::citerate<TupleHelpers::forward_comparator, LuaHelpers::Push>(L, t);
		return sizeof...(Types);
	}
};

int test(lua_State* L) {
	int t;
	boost::optional<std::string> b;
	bool b1;
	boost::optional<unsigned long> opt_ul;
	std::tie(t, b, b1, opt_ul) = LuaCallback::getArguments<int, boost::optional<std::string>, bool, decltype(opt_ul)>(L, __FUNCTION__);
	cout << t << " " << *b << " " << (b1 ? "true" : "false") << " " << *opt_ul << endl;
	return LuaCallback::pushReturns(L, std::make_tuple("some string", 5, 6, LuaHelpers::LuaNil()));
}

std::vector<LuaCallback::CFunc> funcTable = {{"test", &test}, {"test1", &test}};

int main() {

	try {
		LuaState state;
		LuaCallback::registerFunctions(state, funcTable.cbegin(), funcTable.cend());

		std::string i1;
		double d1;
		float f1;
		boost::optional<int> optInt;
		LuaHelpers::LuaAnyRef r;

		state.doFile("../test.lua");
		//state.setGlobal("phasor_version", 202);
		LuaCaller<std::string, double, float, LuaHelpers::LuaAnyRef, boost::optional<int>> c(state);
		std::tie(i1, d1, f1, r, optInt) = c.call("test_func", std::make_tuple(1, 2, 3));
		if (!c.hasError()) {
			cout << i1 << " " << d1 << " " << f1 << endl;
			if (optInt) cout << "opt: " << *optInt << endl;

			c.call("test_func1", std::make_tuple(std::ref(r)));
		} else {
			cout << "return values ignored" << endl;
		}
	} catch (std::exception& e) {
		cout << e.what() << endl;
	}
	//std::tuple<int, int> a;
	//std::get<0>(a) = 4;
	//std::tuple_element<0, std::tuple<int, int>>::type b = "15";
	//cout << std::get<1>(a);	
}
