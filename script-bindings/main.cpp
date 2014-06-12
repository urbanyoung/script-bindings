#include <iostream>
#include <tuple>
#include <stdexcept>
#include <string>
#include <boost/optional.hpp>
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
		template<typename>class F,
		typename... Tp
	>
	inline typename std::enable_if<!Comparator<I, Tp...>::value>::type
	citerate(lua_State*, const std::tuple<Tp...>&)
	{ }

	template<
		template<std::size_t, typename...>class Comparator,
		std::size_t I,
		template<typename>class F,
		typename... Tp
	>
	inline typename std::enable_if<Comparator<I, Tp...>::value>::type
	citerate(lua_State* l, const std::tuple<Tp...>& t)
	{
		typedef std::tuple_element<I, std::tuple<Tp...>>::type elem_type;
		typedef Comparator<I, Tp...> comparator;
		F<elem_type> f;
		f(l, std::get<I>(t));
		citerate<Comparator, comparator::next, F, Tp...>(l, t);
	}

	// Helper for specifying start value
	template<
		template<std::size_t, typename...>class Comparator,
		template<typename>class F,
		typename... Tp
	>
	inline void citerate(lua_State* l, const std::tuple<Tp...>& t)
	{
		citerate<Comparator, Comparator<0, Tp...>::start, F, Tp...>(l, t);
	}

	// ---------------------------------------------------
	/* couldn't get const versions working... easy fix, for now */
	template<
		template<std::size_t, typename...>class Comparator,
		std::size_t I,
		template<typename>class F,
		typename... Tp
	>
	inline typename std::enable_if<!Comparator<I, Tp...>::value>::type
	iterate(lua_State*, std::tuple<Tp...>&)
	{ }

	template<
		template<std::size_t, typename...>class Comparator,
		std::size_t I,
		template<typename>class F,
		typename... Tp
	>
	inline typename std::enable_if<Comparator<I, Tp...>::value>::type
	iterate(lua_State* l, std::tuple<Tp...>& t)
	{
		typedef std::tuple_element<I, std::tuple<Tp...>>::type elem_type;
		typedef Comparator<I, Tp...> comparator;
		F<elem_type> f;
		f(l, std::get<I>(t));
		iterate<Comparator, comparator::next, F, Tp...>(l, t);
	}

	// Helper for specifying start value
	template<
		template<std::size_t, typename...>class Comparator,
		template<typename>class F,
		typename... Tp
	>
	inline void iterate(lua_State* l, std::tuple<Tp...>& t)
	{
		iterate<Comparator, Comparator<0, Tp...>::start, F, Tp...>(l, t);
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
	template <typename T>
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

	template <typename T>
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
		if (lua_pcall(L, nargs, nresults, 0))
		{
			std::string error = lua_tostring(L, -1);
			lua_pop(L, 1);
			throw LuaException(error);
		}
	}

	template <typename T>
	void setGlobal(const std::string& name, const T& value) {
		LuaHelpers::Push<T> p;
		p(L, value);
		lua_setglobal(L, name.c_str());
	}

	template <typename T>
	boost::optional<T> getGlobal(const std::string& name) {
		boost::optional<T> result;
		LuaHelpers::Pop<T> p;
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

int main() {
	try {
		int i1;
		double d1;
		float f1;
		boost::optional<int> optInt;

		LuaState state;
		state.doFile("../test.lua");
		state.setGlobal("phasor_version", 202);
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