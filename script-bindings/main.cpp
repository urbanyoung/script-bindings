#include <iostream>
#include <tuple>
#include <stdexcept>
#include <string>
#include <vector>
#include "lua-bindings.hpp"

using std::cout;
using std::endl;
using std::cerr;

struct some_random_type {};
struct MyPop : public lua::Pop {
	
	// msvc has no inheriting ctors
	MyPop(lua_State* L, int nargs, lua::Pop::e_mode mode)
		: Pop(L, nargs, mode)
	{}

	using lua::Pop::operator();

	void operator()(some_random_type&) {
		cout << "Pop::some_random_type" << endl;
	}
	void operator()(boost::optional<some_random_type>& x) {
		cout << "void*" << endl;
	}

	template <typename Y>
	void operator()(boost::optional<Y>& x) {
		if (lua_type(L, -1) == LUA_TNIL) {
			pop(L);
		} else {
			Y val;
			operator()(val);
			x = val;
		}
	}
};

struct MyPush : public lua::Push {
	MyPush(lua_State* L)
		: lua::Push(L)
	{}

	using lua::Push::operator();

	template <typename T>
	void operator()(const boost::optional<T>& x) {
		if (!x) operator()(types::Nil());
		else operator()(*x);
	}

};

int test(lua_State* L) {
	int t;
	boost::optional<std::string> b;
	bool b1;
	boost::optional<unsigned long> opt_ul;
	//std::tie(t, b, b1, opt_ul) = lua::callback::getArguments<int, boost::optional<std::string>, bool, decltype(opt_ul)>(L, __FUNCTION__);
	some_random_type x;
	std::tie(x,t,t,t) = lua::callback::getArguments<MyPop, some_random_type,int,int,int>(L, "");
	//lua::callback::getArguments<int>(L, "");
	//	cout << t << " " << *b << " " << (b1 ? "true" : "false") << " " << *opt_ul << endl;
	return lua::callback::pushReturns<MyPush>(L, std::make_tuple("some string", 5, 6, lua::types::Nil()));
	
}

std::vector<lua::callback::CFunc> funcTable = {{"test", &test}, {"test1", &test}};

int main() {
	try {
		lua::State state;
		lua::callback::registerFunctions(state, funcTable.cbegin(), funcTable.cend());

		std::string i1;
		double d1;
		float f1;
		boost::optional<int> optInt;
		lua::types::AnyRef r;

		state.doFile("../test.lua");
		//state.setGlobal("phasor_version", 202);
		lua::Caller<std::string, double, float, lua::types::AnyRef, boost::optional<some_random_type>> c(state);
		boost::optional<some_random_type> vo;
		std::tie(i1, d1, f1, r, vo) = c.call<lua::Push, MyPop>("test_func", std::make_tuple(1, 2, 3));
		if (!c.hasError()) {
			cout << i1 << " " << d1 << " " << f1 << endl;
			if (optInt) cout << "opt: " << *optInt << endl;

			c.call<lua::Push, MyPop>("test_func1", std::make_tuple(std::ref(r)));
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
