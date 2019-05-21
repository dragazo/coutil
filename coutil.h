#ifndef DRAGAZO_COUTIL_H
#define DRAGAZO_COUTIL_H

#include <utility>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <experimental/coroutine>

namespace coutil
{
	// --------------------- //

	// -- exception types -- //

	// --------------------- //

	// exception type that denotes accessing an empty couroutine management object.
	struct bad_coroutine_access : std::runtime_error { using std::runtime_error::runtime_error; };

	// ----------- //

	// -- tasks -- //

	// ----------- //

	// a basic_task represents a coroutine that co_returns a (single) value of type T.
	// InitialSuspend is the type to return for initial_suspend().
	template<typename T, typename InitialSuspend>
	class basic_task
	{
	public: // -- promise -- //

		//static_assert(!std::is_reference_v<T>, "T in basic_task<T> cannot be of reference type");

		struct promise_type;
		typedef std::experimental::coroutine_handle<promise_type> handle;

		struct promise_type
		{
			alignas(T) char buffer[sizeof(T)];   // buffer to hold the return value
			bool            constructed = false; // marks if the buffer object is constructed

			~promise_type()
			{
				if (constructed) reinterpret_cast<T*>(buffer)->~T();
			}

			basic_task get_return_object() { return basic_task{ handle::from_promise(*this) }; }

			auto initial_suspend() const noexcept { return InitialSuspend{}; }
			auto final_suspend() const noexcept { return std::experimental::suspend_always{}; }

			template<typename U>
			void return_value(U &&u)
			{
				new (buffer) T(std::forward<U>(u));
				constructed = true;
			}
		};

	private: // -- data -- //

		handle co; // the raw coroutine handle

	private: // -- private util -- //

		basic_task(handle h) : co(h) {}

	public: // -- ctor / dtor / asgn -- //

		// constructs an empty basic_task (does not refer to the return value of any coroutine)
		basic_task() = default;

		// destroys the currently-held coroutine handle (if any)
		~basic_task() { if (co) co.destroy(); }

		basic_task(const basic_task&) = delete;
		basic_task &operator=(const basic_task&) = delete;

		// steals the handle of other - other is left in the empty state
		basic_task(basic_task &&other) : co(std::exchange(other.co, nullptr)) {}
		// discards the current coroutine handle and steals other's - other is left in the empty state.
		// on self assignment, does nothing.
		basic_task &operator=(basic_task &&other) { co = std::exchange(other.co, nullptr); return *this; }

	public: // -- state information -- //

		// returns true if the basic_task is currently in the empty state.
		bool empty() const { return !co; }

		// returns true if the basic_task is non-empty
		explicit operator bool() const { return !empty(); }
		// returns true if the basic_task is empty
		bool operator!() const { return empty(); }

		// returns true if the coroutine has completed execution.
		// if the basic_task is currently empty, throws bad_coroutine_access.
		bool done() const
		{
			if (empty()) throw bad_coroutine_access("Accessing empty couroutine manager");
			return co.done();
		}

	public: // -- coroutine control -- //

		// if the coroutine is not finished, resumes it, otherwise does nothing.
		// if the basic_task is currently empty, throws bad_coroutine_access.
		void resume()
		{
			if (empty()) throw bad_coroutine_access("Accessing empty couroutine manager");
			if (co.done()) co.resume();
		}

	public: // -- value access -- //

		// blocks until completion of the coroutine - returns *this.
		// if the basic_task is currently empty, throws bad_coroutine_access.
		void wait() const&
		{
			if (empty()) throw bad_coroutine_access("Accessing empty couroutine manager");
			while (!co.done()) co.resume();
		}
		void wait() && = delete;

		// blocks until completion of the coroutine and gets the returned value.
		// if the basic_task is currently empty, throws bad_coroutine_access.
		const T &get() const&
		{
			if (empty()) throw bad_coroutine_access("Accessing empty couroutine manager");
			wait();
			return reinterpret_cast<T&>(co.promise().buffer);
		}
		T get() &&
		{
			if (empty()) throw bad_coroutine_access("Accessing empty couroutine manager");
			wait();
			return reinterpret_cast<T&&>(co.promise().buffer);
		}

	public: // -- await interface -- //

		bool await_ready() { return done(); }
		void await_suspend(std::experimental::coroutine_handle<>) {}
		T    await_resume() { return get(); }
	};
	template<typename InitialSuspend>
	class basic_task<void, InitialSuspend>
	{
	public: // -- promise -- //

		struct promise_type;
		typedef std::experimental::coroutine_handle<promise_type> handle;

		struct promise_type
		{
			basic_task get_return_object() { return basic_task{ handle::from_promise(*this) }; }

			auto initial_suspend() const noexcept { return InitialSuspend{}; }
			auto final_suspend() const noexcept { return std::experimental::suspend_always{}; }

			void return_void() {}
		};

	private: // -- data -- //

		handle co; // the raw coroutine handle

	private: // -- private util -- //

		basic_task(handle h) : co(h) {}

	public: // -- ctor / dtor / asgn -- //

		// constructs an empty basic_task (does not refer to the return value of any coroutine)
		basic_task() = default;

		// destroys the currently-held coroutine handle (if any)
		~basic_task() { if (co) co.destroy(); }

		basic_task(const basic_task&) = delete;
		basic_task &operator=(const basic_task&) = delete;

		// steals the handle of other - other is left in the empty state
		basic_task(basic_task &&other) : co(std::exchange(other.co, nullptr)) {}
		// discards the current coroutine handle and steals other's - other is left in the empty state.
		// on self assignment, does nothing.
		basic_task &operator=(basic_task &&other) { co = std::exchange(other.co, nullptr); return *this; }

	public: // -- state information -- //

		// returns true if the basic_task is currently in the empty state.
		bool empty() const { return !co; }

		// returns true if the basic_task is non-empty
		explicit operator bool() const { return !empty(); }
		// returns true if the basic_task is empty
		bool operator!() const { return empty(); }

		// returns true if the coroutine has completed execution.
		// if the basic_task is currently empty, throws bad_coroutine_access.
		bool done() const
		{
			if (empty()) throw bad_coroutine_access("Accessing empty couroutine manager");
			return co.done();
		}

	public: // -- coroutine control -- //

		// if the coroutine is not finished, resumes it, otherwise does nothing.
		// if the basic_task is currently empty, throws bad_coroutine_access.
		void resume()
		{
			if (empty()) throw bad_coroutine_access("Accessing empty couroutine manager");
			if (co.done()) co.resume();
		}

	public: // -- value access -- //

		// blocks until completion of the coroutine - returns *this.
		// if the basic_task is currently empty, throws bad_coroutine_access.
		void wait() const&
		{
			if (empty()) throw bad_coroutine_access("Accessing empty couroutine manager");
			while (!co.done()) co.resume();
		}
		void wait() && = delete;

	public: // -- await interface -- //

		bool await_ready() { return done(); }
		void await_suspend(std::experimental::coroutine_handle<>) {}
		void await_resume() { return wait(); }
	};

	// a task is a basic_task which starts immediately and suspends
	template<typename T = void>
	using task = basic_task<T, std::experimental::suspend_never>;

	// a lazy_task is a basic_task which suspends immediately upon creation
	template<typename T = void>
	using lazy_task = basic_task<T, std::experimental::suspend_always>;
}

#endif
