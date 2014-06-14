#include <iostream>
#include <tuple>
#include <stdexcept>
#include <string>
#include <boost/optional.hpp>
#include <boost/format.hpp>
#include <vector>
#include "lua-bindings.hpp"

using std::cout;
using std::endl;
using std::cerr;

int test(lua_State* L) {
	int t;
	boost::optional<std::string> b;
	bool b1;
	boost::optional<unsigned long> opt_ul;
	std::tie(t, b, b1, opt_ul) = lua::callback::getArguments<int, boost::optional<std::string>, bool, decltype(opt_ul)>(L, __FUNCTION__);
	cout << t << " " << *b << " " << (b1 ? "true" : "false") << " " << *opt_ul << endl;
	return lua::callback::pushReturns(L, std::make_tuple("some string", 5, 6, lua::types::Nil()));
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
		lua::Caller<std::string, double, float, lua::types::AnyRef, boost::optional<int>> c(state);
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
