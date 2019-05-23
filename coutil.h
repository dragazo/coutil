#ifndef DRAGAZO_COUTIL_H
#define DRAGAZO_COUTIL_H

#include <utility>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <variant>
#include <iterator>
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

	template<typename, typename> class basic_task;

	namespace detail
	{
		template<typename T, typename InitialSuspend>
		struct _basic_task_promise_type
		{
			// this holds the state information about this coroutine (ret or exception)
			std::variant<std::exception_ptr, T> stat;

			auto get_return_object() { return basic_task<T, InitialSuspend>{ std::experimental::coroutine_handle<_basic_task_promise_type>::from_promise(*this) }; }

			auto initial_suspend() const noexcept { return InitialSuspend{}; }
			auto final_suspend() const noexcept { return std::experimental::suspend_always{}; }

			template<typename U>
			void return_value(U &&u) { stat.emplace<1>(std::forward<U>(u)); }

			void unhandled_exception() { stat.emplace<0>(std::current_exception()); }
		};
		template<typename T, typename InitialSuspend>
		struct _basic_task_promise_type<T&, InitialSuspend>
		{
			// this holds the state information about this coroutine (ret or exception)
			std::variant<std::exception_ptr, T*> stat;

			auto get_return_object() { return basic_task<T&, InitialSuspend>{ std::experimental::coroutine_handle<_basic_task_promise_type>::from_promise(*this) }; }

			auto initial_suspend() const noexcept { return InitialSuspend{}; }
			auto final_suspend() const noexcept { return std::experimental::suspend_always{}; }

			void return_value(T &p) { stat.emplace<1>(&p); }

			void unhandled_exception() { stat.emplace<0>(std::current_exception()); }
		};
		template<typename T, typename InitialSuspend>
		struct _basic_task_promise_type<T&&, InitialSuspend>
		{
			// this holds the state information about this coroutine (ret or exception)
			std::variant<std::exception_ptr, T*> stat;

			auto get_return_object() { return basic_task<T&, InitialSuspend>{ std::experimental::coroutine_handle<_basic_task_promise_type>::from_promise(*this) }; }

			auto initial_suspend() const noexcept { return InitialSuspend{}; }
			auto final_suspend() const noexcept { return std::experimental::suspend_always{}; }

			void return_value(T &&p) { stat.emplace<1>(&p); }

			void unhandled_exception() { stat.emplace<0>(std::current_exception()); }
		};
		template<typename InitialSuspend>
		struct _basic_task_promise_type<void, InitialSuspend>
		{
			// the exception thrown during coroutine execution (if any)
			std::exception_ptr ex;

			auto get_return_object() { return basic_task<void, InitialSuspend>{ std::experimental::coroutine_handle<_basic_task_promise_type>::from_promise(*this) }; }

			auto initial_suspend() const noexcept { return InitialSuspend{}; }
			auto final_suspend() const noexcept { return std::experimental::suspend_always{}; }

			void return_void() {}

			void unhandled_exception() { ex = std::current_exception(); }
		};
	}

	// basic_task represents a coroutine that co_returns a (single) value of type T.
	// if an exception is thrown, it is caught and rethrown upon awaiting the result.
	// if T is void, this represents a void-returning coroutine.
	// if T is a reference type, represents a reference-returning coroutine (the reference category is preserved).
	//     in this case the return value is stored as a pointer - it is the responsibility of the coroutine writer to guarantee the object outlives the coroutine.
	// otherwise this represents a (regular) T-returning coroutine.
	// T              - the return type of the coroutine (must not be cv-qualified).
	// InitialSuspend - the type to return for initial_suspend() (empty brace initialized) (must not be cv-qualified).
	template<typename T, typename InitialSuspend>
	class basic_task
	{
	public: // -- promise -- //

		static_assert(std::is_same_v<T, std::remove_cv_t<T>>, "T must not be cv-qualified");
		static_assert(std::is_same_v<InitialSuspend, std::remove_cv_t<InitialSuspend>>, "InitialSuspend must not be cv-qualified");

		typedef detail::_basic_task_promise_type<T, InitialSuspend> promise_type;
		friend struct promise_type;

	private: // -- private utility info -- //

		typedef std::experimental::coroutine_handle<promise_type> handle;

		// constexpr control flags for performing compile-time branching in management functions
		static inline constexpr bool void_mode = std::is_void_v<T>;
		static inline constexpr bool ref_mode = std::is_reference_v<T>;

	private: // -- data -- //

		handle co = nullptr; // the raw coroutine handle

	private: // -- private util -- //

		explicit basic_task(handle h) : co(std::move(h)) {}

	public: // -- ctor / dtor / asgn -- //

		// constructs an empty basic_task (does not refer to any existing coroutine)
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

	public: // -- coroutine control -- //

		// returns true if the coroutine has completed execution (successfully or due to exception).
		// if the basic_task is currently empty, throws bad_coroutine_access.
		bool done() const
		{
			if (empty()) throw bad_coroutine_access("Accessing empty couroutine manager");
			return co.done();
		}

		// if the coroutine is not finished, resumes it, otherwise does nothing.
		// if the basic_task is currently empty, throws bad_coroutine_access.
		void resume()
		{
			if (empty()) throw bad_coroutine_access("Accessing empty couroutine manager");
			if (!co.done()) co.resume();
		}

		// blocks until completion of the coroutine and gets the returned value.
		// if the basic_task is currently empty, throws bad_coroutine_access.
		// if the coroutine ended due to exception, rethrows the exception.
		// after this operation (regardless of success) the basic_task is in the empty state (the coroutine is destroyed).
		// T is void      - returns void.
		// T is reference - returns the reference (preserving reference category).
		// otherwise      - returns the stored object by value (move-constructed).
		decltype(auto) wait()
		{
			if (empty()) throw bad_coroutine_access("Accessing empty couroutine manager");
			while (!co.done()) co.resume();

			// create a sentry object that will set us to the empty state regardless of success (i.e. even if an exception is thrown)
			struct _
			{
				handle &h;
				_(handle &_h) : h(_h) {}
				~_() { h.destroy(); h = nullptr; }
			} sentry(co);
			
			if constexpr (void_mode)
			{
				if (co.promise().ex) std::rethrow_exception(co.promise().ex);
			}
			else if constexpr (ref_mode)
			{
				if (std::remove_reference_t<T> **p = std::get_if<1>(&co.promise().stat)) return static_cast<T>(**p); // cast is to preserve rvalue reference return type (otherwise would deduce to lvalue reference)
				else std::rethrow_exception(std::get<0>(co.promise().stat));
			}
			else
			{
				if (T *ret = std::get_if<1>(&co.promise().stat)) return static_cast<T>(std::move(*ret)); // the cast is to return a move-constructed value
				else std::rethrow_exception(std::get<0>(co.promise().stat));
			}
		}

	public: // -- await interface -- //

		bool           await_ready() { return done(); }
		void           await_suspend(std::experimental::coroutine_handle<>) {}
		decltype(auto) await_resume() { return wait(); }
	};

	// a task is a basic_task which starts immediately and suspends
	template<typename T = void>
	using task = basic_task<std::remove_cv_t<T>, std::experimental::suspend_never>;

	// a lazy_task is a basic_task which suspends immediately upon creation
	template<typename T = void>
	using lazy_task = basic_task<std::remove_cv_t<T>, std::experimental::suspend_always>;

	namespace detail
	{
		// gets if type T is any kind of basic_task
		template<typename T>
		struct _is_task : std::false_type {};
		template<typename T, typename InitialSuspend>
		struct _is_task<basic_task<T, InitialSuspend>> : std::true_type {};
	}

	// gets if type T is any kind of (potentially cv-qualified) basic_task
	template<typename T>
	using is_task = detail::_is_task<std::remove_cv_t<T>>;
	template<typename T>
	inline constexpr bool is_task_v = is_task<T>::value;
	
	// given one or more tasks, resume()s them in a loop until all of them are done().
	// wait() is not actually called on any task, so the results are not extracted.
	template<typename ...T, std::enable_if_t<std::conjunction_v<is_task<T>...>, int> = 0>
	void wait_all(T &...tasks)
	{
		while ((... | (tasks.resume(), !tasks.done())));
	}
	// given one or more tasks, resume()s them in a loop until at least one of them is done().
	// wait() is not actually called on any task, so the results are not extracted.
	template<typename ...T, std::enable_if_t<std::conjunction_v<is_task<T>...>, int> = 0>
	void wait_any(T &...tasks)
	{
		while (!(... | (tasks.resume(), tasks.done())));
	}

	// ---------------- //

	// -- generators -- //

	// ---------------- //

	// basic_generator represents a coroutine that co_yields zero or more values T and optionally co_returns (nothing) to end execution.
	// has the ability to iterate over the generated sequence of values as a (single-pass) input iterator.
	// execution of the coroutine body does not begin until a yield value is requested.
	template<typename T>
	class basic_generator
	{
	public: // -- promise -- //

		static_assert(!std::is_reference_v<T>, "T in basic_generator<T> cannot be of reference type");

		struct promise_type;
		typedef std::experimental::coroutine_handle<promise_type> handle;

		struct promise_type
		{
			std::variant<std::exception_ptr, T> stat; // holds the state information about this coroutine (ret or exception)
			bool yield_flag = false;                  // flag used to mark when a yield value is obtained

			auto get_return_object() { return basic_generator{ handle::from_promise(*this) }; }

			auto initial_suspend() const noexcept { return std::experimental::suspend_always{}; }
			auto final_suspend() const noexcept { return std::experimental::suspend_always{}; }

			template<typename U>
			auto yield_value(U &&u)
			{
				// if we already have a T object, move assign it - otherwise move construct it
				if (T *p = std::get_if<1>(&stat)) *p = std::forward<U>(u);
				else stat.emplace<1>(std::forward<U>(u));

				// mark that we now have a yield value
				yield_flag = true;

				// suspend so that we can use it
				return std::experimental::suspend_always{};
			}

			void unhandled_exception()
			{
				stat.emplace<0>(std::current_exception());
			}
		};

	private: // -- data -- //

		handle co = nullptr; // the raw coroutine handle

	private: // -- private util -- //

		basic_generator(handle h) : co(std::move(h)) {}

	public: // -- ctor / dtor / asgn -- //

		// constructs an empty basic_generator (does not refer to any existing coroutine)
		basic_generator() = default;

		// destroys the currently-held coroutine handle (if any)
		~basic_generator() { if (co) co.destroy(); }

		basic_generator(const basic_generator&) = delete;
		basic_generator &operator=(const basic_generator&) = delete;

		// steals the handle of other - other is left in the empty state
		basic_generator(basic_generator &&other) : co(std::exchange(other.co, nullptr)) {}
		// discards the current coroutine handle and steals other's - other is left in the empty state.
		// on self assignment, does nothing.
		basic_generator &operator=(basic_generator &&other) { co = std::exchange(other.co, nullptr); return *this; }

	public: // -- state information -- //

		// returns true if the basic_generator is currently in the empty state.
		bool empty() const { return !co; }

		// returns true if the basic_generator is non-empty
		explicit operator bool() const { return !empty(); }
		// returns true if the basic_generator is empty
		bool operator!() const { return empty(); }

	public: // -- iterator -- //

		class iterator
		{
		public: // -- pseudo-iterator -- //

			class pseudo_iterator
			{
			private: // -- data -- //

				iterator *it;

			private: // -- private util -- //

				friend class iterator;

				// constructs a pseudo_iterator to advance the given iterator.
				// the provided iterator must have a valid (non-null) coroutine handle.
				explicit pseudo_iterator(iterator &i) : it(&i)
				{
					// clear the yield flag and resume execution of the coroutine
					it->co.promise().yield_flag = false;
					it->co.resume();
				}

				// returns true if the increment process has completed
				bool done()
				{
					// if the coroutine has finished (no yield value) or we got a yield value, we're done with the increment process
					return it->co.done() || it->co.promise().yield_flag;
				}
				// blocks until completion of the increment process and updates the iterator source.
				// after everything is completed, sets this pseudo-iterator to the empty state.
				void wait()
				{
					// wait for completion of the increment process
					while (!done()) it->co.resume();
					
					// if the coroutine has finished execution (no yield value) destroy the coroutine and null it.
					// this effectively sets the iterator it was sourced from to the end iterator state.
					if (it->co.done()) { it->co.destroy(); it->co = nullptr; }

					// we're done now - no longer need the coroutine handle.
					// null the raw pointer we have so that the destructor won't try to wait() again
					it = nullptr;
				}

			public: // -- ctor / dtor / asgn -- //

				// blocks and advances the stored iterator to the next yield value
				~pseudo_iterator()
				{
					if (it) wait();
				}

				pseudo_iterator(const pseudo_iterator&) = delete;
				pseudo_iterator &operator=(const pseudo_iterator&) = delete;

				// passes on the responsibility for advancing the stored iterator to a new pseudo_iterator.
				pseudo_iterator(pseudo_iterator &&other) : it(std::exchange(other.it, nullptr)) {}
				pseudo_iterator &operator=(pseudo_iterator&&) = delete;

			public: // -- await interface -- //

				bool await_ready() { return done(); }
				void await_suspend(std::experimental::coroutine_handle<>) {}
				void await_result() { wait(); }
			};

		public: // -- iterator traits -- //

			typedef std::input_iterator_tag iterator_category;
			typedef T                       value_type;
			typedef std::ptrdiff_t          difference_type;
			typedef T                      *pointer_type;
			typedef T                      &reference_type;

		private: // -- data -- //

			handle co; // the raw coroutine handle (from the generator)

		private: // -- private util -- //

			friend class basic_generator;

			explicit iterator(handle h) : co(std::move(h))
			{
				// if we have a good coroutine, advance once to get the first value
				if (co) ++*this;
			}

		public: // -- ctor / dtor / asgn -- //

			iterator() = default;

			~iterator() { if (co) co.destroy(); }

			iterator(const iterator&) = delete;
			iterator &operator=(const iterator&) = delete;

			// steals the iteration handle from other - other is "empty" after this.
			iterator(iterator &&other) : co(std::exchange(other.co, nullptr)) {}
			// discards the current iterator handle and steals other's - other is "empty" after this.
			// self assignment is no-op.
			iterator &operator=(iterator &&other) { co = std::exchange(other.co, nullptr); return *this; }

		public: // -- iterator interface -- //

			// gets the currently-stored (most-recently-yielded) value.
			// if an exception caused the coroutine to end prematurely, rethrows the exception.
			const T &operator*() const&
			{
				// if we have an exception, rethrow it
				if (auto ex = std::get_if<0>(&co.promise().stat); ex && *ex) std::rethrow_exception(*ex);

				return std::get<1>(co.promise().stat);
			}
			T operator*() &&
			{
				// if we have an exception, rethrow it
				if (auto ex = std::get_if<0>(&co.promise().stat); ex && *ex) std::rethrow_exception(*ex);

				return std::move(std::get<1>(co.promise().stat));
			}

			// advances to the next yield value.
			// this can be performed asyncronously with co_await.
			// otherwise the destruction of the pseudo_iterator performs the operation synchronously.
			// exceptions from this action are propagated through the dereferencing operator.
			pseudo_iterator operator++() &
			{
				// make sure we have a valid (non-null) coroutine handle
				if (!co) throw std::invalid_argument("Attempt to increment past end iterator");
				return pseudo_iterator{*this};
			}
			pseudo_iterator operator++() && = delete;

			friend bool operator==(const iterator &a, const iterator &b)
			{
				// iterator is a uniquely-owning type, so the only way they can be equal is if they're both invalid (end) iterators
				return !a.co && !b.co;
			}
			friend bool operator!=(const iterator &a, const iterator &b) { return !(a == b); }
		};

		// gets the begin iterator of this generator.
		// this operation steals the coroutine handle from this object - i.e. the generator is empty after this operation.
		// if the generator is currently empty, returns the end iterator.
		iterator begin() { return iterator{ std::exchange(co, nullptr) }; }
		// returns the end iterator for this generator - this generator is not modified by this action.
		iterator end() const { return iterator{}; }
	};

	template<typename T>
	using generator = basic_generator<T>;
}

#endif
