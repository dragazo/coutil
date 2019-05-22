#include <iostream>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <cassert>
#include <experimental/coroutine>

#include "coutil.h"

using namespace coutil;

// given a macro name, converts its value into a string.
// e.g. STR(__LINE__) would expand to "15" on line 15.
#define STR(x) _STR(x)
#define _STR(x) #x

// asserts that expr throws an exception of type ex
#define assert_throws(expr, ex) try { (expr); std::cerr << "LINE " STR(__LINE__) " DID NOT THROW: " #expr "\n"; std::terminate(); } \
                                catch (const ex&) {} \
                                catch (const std::exception &e) { std::cerr << "LINE " STR(__LINE__) " THREW WRONG TYPE " #ex ": " #expr "\nCAUSE: " << e.what() << '\n'; std::terminate(); } \
                                catch (...) { std::cerr << "LINE " STR(__LINE__) " THREW WRONG TYPE " #ex ": " #expr "\n"; std::terminate(); }
// asserts that expr throws an exception (of any type)
#define assert_throws_any(expr) try { (expr); std::cerr << "LINE " STR(__LINE__) " DID NOT THROW: " #expr "\n"; std::terminate(); } \
                                catch (...) {}
// asserts that expr does not throw an exception
#define assert_nothrow(expr) try { (expr); } \
                             catch (const std::exception &e) { std::cerr << "LINE " STR(__LINE__) " THREW: " #expr "\nCAUSE: " << e.what() << '\n'; std::terminate(); } \
                             catch (...) { std::cerr << "LINE " STR(__LINE__) " THREW: " #expr "\n"; std::terminate(); }

int main() try
{
	static_assert(std::is_same_v<task<void>, task<const void>>);
	static_assert(std::is_same_v<task<void>, task<volatile void>>);
	static_assert(std::is_same_v<task<void>, task<const volatile void>>);

	static_assert(std::is_same_v<lazy_task<void>, lazy_task<const void>>);
	static_assert(std::is_same_v<lazy_task<void>, lazy_task<volatile void>>);
	static_assert(std::is_same_v<lazy_task<void>, lazy_task<const volatile void>>);

	{
		int val = 0;
		auto a = [&]() -> task<>
		{
			assert(val == 0);
			val = 14;
			co_await std::experimental::suspend_always{};
			assert(val == 65);
			val = -56;
			co_await std::experimental::suspend_always{};
			assert(val == -128);
			val = 365;
			co_await std::experimental::suspend_always{};
			assert(val == 12);
			val = 19;
		}();
		auto b = [&]() -> task<>
		{
			assert(val == 14);
			val = 65;
			co_await std::experimental::suspend_always{};
			assert(val == -56);
			val = -128;
			co_await std::experimental::suspend_always{};
			assert(val == 365);
			val = 12;
			co_await std::experimental::suspend_always{};
			assert(val == 19);
			val = 1777;
		}();

		wait_all(a, b);
		assert(val == 1777);
	}

	{
		int p = 4;
		task<> co = [](int &p) -> task<> { p = 44; co_return; }(p);
		assert(co.done() && p == 44);
		co.wait();
		assert(co.done() && p == 44);
	}
	{
		int p = 6;
		lazy_task<> co = [](int &p) -> lazy_task<> { p = 77; co_return; }(p);
		assert(!co.done() && p == 6);
		co.wait();
		assert(co.done() && p == 77);
	}

	assert([](int a, int b) -> task<int> { co_return a + b; }(6, 7).wait() == 13);
	
	assert_throws([]() -> task<int> { throw 6; co_return 77; }().wait(), int);
	assert_throws([]() -> lazy_task<int> { throw 6; co_return 77; }().wait(), int);
	assert_throws([]() -> task<> { throw 6; co_return; }().wait(), int);
	assert_throws([]() -> lazy_task<> { throw 6; co_return; }().wait(), int);

	{
		generator<int> gen123 = []() -> generator<int> { co_yield 1; co_yield 2; co_yield 3; }();
		auto beg = gen123.begin();
		auto end = gen123.end();

		assert(beg != end);
		assert(*beg == 1);
		assert_nothrow(++beg);

		assert(beg != end);
		assert(*beg == 2);
		assert_nothrow(++beg);

		assert(beg != end);
		assert(*beg == 3);
		assert_nothrow(++beg);
		assert(beg == end);
		assert_throws(++beg, std::invalid_argument);
		assert_throws(++end, std::invalid_argument);
	}
	{
		generator<int> gen_1_stop = []() -> generator<int> { co_yield 7; co_return; co_yield 8; }();
		auto end = gen_1_stop.end();
		auto beg = gen_1_stop.begin();
		auto beg2 = gen_1_stop.begin();
		
		assert(beg2 == end);

		assert(beg != end);
		assert(*beg == 7);
		assert_nothrow(++beg);

		assert(beg == end);
		assert(beg == beg2);
		assert_throws(++beg, std::invalid_argument);
		assert_throws(++end, std::invalid_argument);
		assert_throws(++beg2, std::invalid_argument);
	}
	{
		generator<int> empty = []() -> generator<int> { co_return; }();
		assert(empty.begin() == empty.end());
	}
	{
		generator<int> my_iota = []() -> generator<int> { for (int i = 0;;) co_yield i++; }();
		auto beg = my_iota.begin();
		auto end = my_iota.end();
		
		for (int i = 0; i < 20; ++i)
		{
			assert(*beg == i);
			assert_nothrow(++beg);
		}
	}
	{
		generator<int> my_iota = []() -> generator<int> { for (int i = 0;;) co_yield i++; }();

		int i = 0;
		for (int v : my_iota)
		{
			assert(i == v);
			if (++i == 20) break;
		}
	}

	std::cout << "all tests completed\n";

	return 0;
}
catch (const std::exception &ex)
{
	std::cerr << "UNHANDLED EXCEPTION: " << ex.what() << '\n';
}
catch (...)
{
	std::cerr << "UNHANDLED EXCEPTION\n";
}