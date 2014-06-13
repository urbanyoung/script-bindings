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
	inline void citerate(lua_State* l, const std::tuple<Tp...>& t, F& f = F())
	{
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
	inline void iterate(lua_State* l, std::tuple<Tp...>& t, F& f = F())
	{
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
	
	struct Push {
		void operator()(lua_State* L, bool x) {
			lua_pushboolean(L, x);
		}
		void operator()(lua_State* L, int x) {
			lua_pushnumber(L, x);
		}
		void operator()(lua_State* L, double x) {
			lua_pushnumber(L, x);
		}
		void operator()(lua_State* L, const char* x) {
			lua_pushstring(L, x);
		}
		void operator()(lua_State* L, const std::string& x) {
			lua_pushstring(L, x.c_str());
		}
	};

	struct Pop {
		void operator()(lua_State* L, double& x) {
			x = static_cast<double>(lua_tonumber(L, -1));
			lua_pop(L, 1);
		}

		void operator()(lua_State* L, int& x) {
			x = static_cast<int>(lua_tonumber(L, -1));
			lua_pop(L, 1);
		}

		void operator()(lua_State* L, float& x) {
			x = static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);
		}

		template <typename Y>
		void operator()(lua_State* L, boost::optional<Y>& x) {
			if (lua_type(L, -1) == LUA_TNIL) {
				lua_pop(L, 1);
			} else {
				Y val;
				operator()(L, val);
				x = val;
			}
		}
	};

	struct PopChecked {
		int narg;

		PopChecked()
			: narg(0)
		{}

		template <typename Y>
		void raise_error(lua_State* L, int expected) {
			auto f = boost::format("expected %s, got %s") % ctype_name<Y>() % lua_typename(L, expected);
			luaL_argerror(L, narg, f.str().c_str());
		}

		template <typename Y>
		void pop_number_or_bool(lua_State* L, Y& x) {
			bool b;
			int ltype = lua_type(L, -1);
			switch (ltype) {
			case LUA_TNUMBER:
				x = static_cast<Y>(lua_tonumber(L, -1));
				break;
			case LUA_TBOOLEAN: 
				b = lua_toboolean(L, -1) == 1 ? true : false;
				x = static_cast<Y>(b ? 1 : 0);
				break;
			default:
				raise_error<Y>(L, ltype);
				break;
			}
			lua_pop(L, 1);
		}

		void operator()(lua_State* L, double& x) {
			pop_number_or_bool(L, x);
		}

		void operator()(lua_State* L, int& x) {
			pop_number_or_bool(L, x);
		}

		void operator()(lua_State* L, float& x) {
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
			default:
				raise_error<std::string>(L, ltype);

			}
		}

		template <typename Y>
		void operator()(lua_State* L, boost::optional<Y>& x) {
			if (lua_type(L, -1) == LUA_TNIL) {
				lua_pop(L, 1);
			} else {
				Y val;
				operator()(L, val);
				x = val;
			}
		}
	};
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

public:

	LuaCaller(LuaState& L)
		: L(L)
	{}

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
		TupleHelpers::iterate<TupleHelpers::reverse_comparator, LuaHelpers::Pop>(L, results);
		return results;
	}
};

namespace LuaCallback {
	typedef void(*prototype)(lua_State*);
	struct CFunc {
		const char* name;
		prototype func;
	};

	static int invokeCFunction(lua_State* L) {
		prototype f = static_cast<prototype>(lua_touserdata(L, lua_upvalueindex(1)));
		f(L);
		return 0;
	}

	template <class Itr>
	static void registerFunctions(lua_State* L, Itr itr, const Itr end) {
		for (; itr != end; ++itr) {
			lua_pushlightuserdata(L, itr->func);
			lua_pushcclosure(L, &invokeCFunction, 1);
			lua_setglobal(L, itr->name);
		}
	}
	
	// Counts number of non-boost::optional<> parameters.
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
	size_t count_optional(const std::tuple<T...>& x,
		typename std::enable_if<more_than_zero<N>::value>::type* = 0) {
		return do_count(std::get<N-1>(x)) + count_optional<N - 1, T...>(x);
	}

	template <std::size_t N, typename... T>
	size_t count_optional(const std::tuple<T...>& x,
		typename std::enable_if<!more_than_zero<N>::value>::type* = 0) {
		return 0;
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
			luaL_error(L, "'%s' expects at most %d argument(s) (got %d)", funcname, nrequired, nargs);
		}

		LuaHelpers::PopChecked p;
		TupleHelpers::iterate<TupleHelpers::reverse_comparator, LuaHelpers::PopChecked>(L, args, p);
		return args;
	}
};

void test(lua_State* L) {
	int t;
	boost::optional<int> b;
	std::tie(t, b) = LuaCallback::getArguments<int, boost::optional<int>>(L, __FUNCTION__);
	cout << "test: " << t << endl;
}
std::vector<LuaCallback::CFunc> funcTable = {{"test", &test}, {"test1", &test}};


int main() {
	//boost::optional<int> t;
	//size_t n = LuaCallback::count_non_optional(std::make_tuple(4, 5, 6, t));
	//cout << "non-optional: " << n << endl;

	try {
		int i1;
		double d1;
		float f1;
		boost::optional<int> optInt;
//		cout << "non-optional: " << LuaCallback::countNonOptional(std::make_tuple(4, 5, 6, boost::optional<int>(7))) << endl;

		LuaState state;
		LuaCallback::registerFunctions(state, funcTable.cbegin(), funcTable.cend());

		state.doFile("../test.lua");
		//state.setGlobal("phasor_version", 202);
		LuaCaller<int, double, float, boost::optional<int>> c(state);
		std::tie(i1, d1, f1, optInt) = c.call("test_func", std::make_tuple(1, 2, 3));
		cout << i1 << " " << d1 << " " << f1 << endl;
		if (optInt) cout << "opt: " << *optInt << endl;
	} catch (std::exception& e) {
		cout << e.what() << endl;
	}
	//std::tuple<int, int> a;
	//std::get<0>(a) = 4;
	//std::tuple_element<0, std::tuple<int, int>>::type b = "15";
	//cout << std::get<1>(a);	
}