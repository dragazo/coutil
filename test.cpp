#include <iostream>
#include <experimental/coroutine>

#include "coutil.h"

using namespace coutil;

task<int> async_add(int a, int b) { co_return a + b; }
lazy_task<int> foo()
{
	std::cerr << "computing\n";
	int v = co_await async_add(4, 5);
	return v;
}
lazy_task<> lazy_print(const char *str)
{
	std::cerr << "lazy print: " << str << '\n';
	co_return;
}

int main()
{
	auto msg = lazy_print("hello world");

	auto f = foo();
	std::cerr << "fetching value\n";
	int v = f.get();
	std::cerr << "got: " << v << '\n';

	msg.wait();

	std::cin.get();
	return 0;
}
