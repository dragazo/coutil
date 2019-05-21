#include <iostream>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <cassert>
#include <experimental/coroutine>

#include "coutil.h"

using namespace coutil;

// asserts that expr throws an exception of type ex
#define assert_throws(expr, ex) try { (expr); std::cerr << "DID NOT THROW: " #expr "\n"; std::terminate(); } \
                                catch (const ex&) {} \
                                catch (const std::exception &e) { std::cerr << "THREW WRONG TYPE " #ex ": " #expr "\nCAUSE: " << e.what() << '\n'; std::terminate(); } \
                                catch (...) { std::cerr << "THREW WRONG TYPE " #ex ": " #expr "\n"; std::terminate(); }
// asserts that expr throws an exception (of any type)
#define assert_throws_any(expr) try { (expr); std::cerr << "DID NOT THROW: " #expr "\n"; std::terminate(); } \
                                catch (...) {}
// asserts that expr does not throw an exception
#define assert_nothrow(expr) try { (expr); } \
                             catch (const std::exception &e) { std::cerr << "THREW: " #expr "\nCAUSE: " << e.what() << '\n'; std::terminate(); } \
                             catch (...) { std::cerr << "THREW: " #expr "\n"; std::terminate(); }

int main() try
{
	static_assert(std::is_same_v<task<void>, task<const void>>);
	static_assert(std::is_same_v<task<void>, task<volatile void>>);
	static_assert(std::is_same_v<task<void>, task<const volatile void>>);

	static_assert(std::is_same_v<lazy_task<void>, lazy_task<const void>>);
	static_assert(std::is_same_v<lazy_task<void>, lazy_task<volatile void>>);
	static_assert(std::is_same_v<lazy_task<void>, lazy_task<const volatile void>>);

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