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

template <typename... ResultTypes>
class LuaCaller {
private:
	static const size_t nresults = sizeof...(ResultTypes);

	lua_State* l;

public:

	LuaCaller() {
		l = luaL_newstate();
		if (luaL_dostring(l, "							function test_func(a, b, c) \
							 									return a, b, c, 9 \
														end"))
		{
			std::string error = lua_tostring(l, -1);
			lua_pop(l, 1);

			throw std::exception(error.c_str());
		}
	}
		
	//LuaCaller(function_handle);

	template <typename T>
	struct Push {
		void operator()(lua_State* l, bool x) {
			lua_pushboolean(l, x);
		}
		void operator()(lua_State* l, int x) {
			lua_pushnumber(l, x);
		}
		void operator()(lua_State* l, double x) {
			lua_pushnumber(l, x);
		}
		void operator()(lua_State* l, const char* x) {
			lua_pushstring(l, x);
		}
		void operator()(lua_State* l, const std::string& x) {
			lua_pushstring(l, x.c_str());
		}
	};

	template <typename T>
	struct Pop {
		/*void operator()(lua_State* l, T& x) {
			x = static_cast<T>(lua_tonumber(l, -1));
			lua_pop(l, 1);
		}*/

		void operator()(lua_State* l, double& x) {
			x = static_cast<double>(lua_tonumber(l, -1));
			lua_pop(l, 1);
		}

		void operator()(lua_State* l, int& x) {
			x = static_cast<int>(lua_tonumber(l, -1));
			lua_pop(l, 1);
		}

		void operator()(lua_State* l, float& x) {
			x = static_cast<float>(lua_tonumber(l, -1));
			lua_pop(l, 1);
		}

		template <typename Y>
		void operator()(lua_State* l, boost::optional<Y>& x) {
			cout << "nilable" << endl;
			if (lua_type(l, -1) == LUA_TNIL) {
				//x.set = false;
				lua_pop(l, 1);
			} else {
				Y val;
				operator()(l, val);
				x = val;
			}
		}
	};

	template <typename... ArgTypes> 
	std::tuple<ResultTypes...> call(const std::string& func, const std::tuple<ArgTypes...>& args) {
		int top = lua_gettop(l);
		lua_getglobal(l, func.c_str());
		if (lua_type(l, -1) == LUA_TNIL) throw std::exception("no func...");

		size_t nargs = sizeof...(ArgTypes);
		size_t nresults = sizeof...(ResultTypes);
		
		TupleHelpers::citerate<TupleHelpers::forward_comparator, Push>(l, args);

		if (lua_pcall(l, nargs, nresults, 0))
		{
			std::string error = lua_tostring(l, -1);
			lua_pop(l, 1);
			throw std::exception(error.c_str());
		}

		std::tuple<ResultTypes...> results;
		TupleHelpers::iterate<TupleHelpers::reverse_comparator, Pop>(l, results);
		return results;
	}
};

int main() {
	try {
		int i1;
		double d1;
		float f1;
		boost::optional<int> optInt;
		LuaCaller<int, double, float, boost::optional<int>> c;
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