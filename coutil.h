#ifndef DRAGAZO_COUTIL_H
#define DRAGAZO_COUTIL_H

#include <utility>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <variant>
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

	// basic_task represents a coroutine that co_returns a (single) value of type T.
	// if an exception is thrown, it is caught and rethrown upon awaiting the result.
	// T              - the return type of the coroutine.
	// InitialSuspend - the type to return for initial_suspend() (empty brace initialized).
	template<typename T, typename InitialSuspend>
	class basic_task
	{
	public: // -- promise -- //

		static_assert(!std::is_reference_v<T>, "T in basic_task<T> cannot be of reference type");

		struct promise_type;
		typedef std::experimental::coroutine_handle<promise_type> handle;

		struct promise_type
		{
			// this holds the state information about this coroutine (nothing, ret, or exception)
			std::variant<std::exception_ptr, T> stat;

			auto get_return_object() { return basic_task{ handle::from_promise(*this) }; }

			auto initial_suspend() const noexcept { return InitialSuspend{}; }
			auto final_suspend() const noexcept { return std::experimental::suspend_always{}; }

			template<typename U>
			void return_value(U &&u)
			{
				stat.emplace<1>(std::forward<U>(u));
			}

			void unhandled_exception()
			{
				stat.emplace<0>(std::current_exception());
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

		// returns true if the coroutine has completed execution (successfully or due to exception).
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
			if (!co.done()) co.resume();
		}

	public: // -- value access -- //

		// blocks until completion of the coroutine and gets the returned value.
		// if the basic_task is currently empty, throws bad_coroutine_access.
		// if the coroutine ended due to exception, rethrows the exception.
		const T &wait() const&
		{
			if (empty()) throw bad_coroutine_access("Accessing empty couroutine manager");
			while (!co.done()) co.resume();

			if (auto ret = std::get_if<1>(&co.promise().stat)) return *ret;
			else std::rethrow_exception(std::get<0>(co.promise().stat));
		}
		T wait() &&
		{
			if (empty()) throw bad_coroutine_access("Accessing empty couroutine manager");
			while (!co.done()) co.resume();

			if (auto ret = std::get_if<1>(&co.promise().stat)) return std::move(*ret);
			else std::rethrow_exception(std::get<0>(co.promise().stat));
		}

	public: // -- await interface -- //

		bool await_ready() { return done(); }
		void await_suspend(std::experimental::coroutine_handle<>) {}
		T    await_resume() { return wait(); }
	};
	template<typename InitialSuspend>
	class basic_task<void, InitialSuspend>
	{
	public: // -- promise -- //

		struct promise_type;
		typedef std::experimental::coroutine_handle<promise_type> handle;

		struct promise_type
		{
			// the exception thrown during coroutine execution (if any)
			std::exception_ptr ex;

			basic_task get_return_object() { return basic_task{ handle::from_promise(*this) }; }

			auto initial_suspend() const noexcept { return InitialSuspend{}; }
			auto final_suspend() const noexcept { return std::experimental::suspend_always{}; }

			void return_void() {}

			void unhandled_exception() { ex = std::current_exception(); }
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

		// returns true if the coroutine has completed execution (successfully or due to exception).
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
			if (!co.done()) co.resume();
		}

	public: // -- value access -- //

		// blocks until completion of the coroutine - returns *this.
		// if the basic_task is currently empty, throws bad_coroutine_access.
		// if the coroutine ended due to exception, rethrows the exception.
		void wait() const
		{
			if (empty()) throw bad_coroutine_access("Accessing empty couroutine manager");
			while (!co.done()) co.resume();

			if (co.promise().ex) std::rethrow_exception(co.promise().ex);
		}

	public: // -- await interface -- //

		bool await_ready() { return done(); }
		void await_suspend(std::experimental::coroutine_handle<>) {}
		void await_resume() { wait(); }
	};

	// a task is a basic_task which starts immediately and suspends
	template<typename T = void>
	using task = basic_task<std::remove_cv_t<T>, std::experimental::suspend_never>;

	// a lazy_task is a basic_task which suspends immediately upon creation
	template<typename T = void>
	using lazy_task = basic_task<std::remove_cv_t<T>, std::experimental::suspend_always>;
}

#endif
