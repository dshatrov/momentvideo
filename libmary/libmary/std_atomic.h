#ifndef LIBMARY__STD_ATOMIC__H__
#define LIBMARY__STD_ATOMIC__H__


#include <libmary/types_base.h>
#include <atomic>


namespace M {

#ifndef LIBMARY_MT_SAFE
template <class T>
class DummyStdAtomic
{
  private:
    T val;

  public:
    bool is_lock_free () const noexcept { return true; }
    void store (T new_val, std::memory_order = std::memory_order_seq_cst)       noexcept { val = new_val; }
    T    load  (           std::memory_order = std::memory_order_seq_cst) const noexcept { return val; }
    operator T () const          noexcept { return val; }
    T exchange (T const new_val) noexcept { T const old_val = val; val = new_val; return old_val; }

    bool compare_exchange_weak (T &expected, T const desired, std::memory_order, std::memory_order) noexcept
        { if (val == expected) { val = desired; return true; } expected = val; return false; }
    bool compare_exchange_weak (T &expected, T const desired, std::memory_order = std::memory_order_seq_cst) noexcept
        { if (val == expected) { val = desired; return true; } expected = val; return false; }

    bool compare_exchange_strong (T &expected, T const desired, std::memory_order, std::memory_order) noexcept
        { if (val == expected) { val = desired; return true; } expected = val; return false; }
    bool compare_exchange_strong (T &expected, T const desired, std::memory_order = std::memory_order_seq_cst) noexcept
        { if (val == expected) { val = desired; return true; } expected = val; return false; }

    T fetch_add (T const arg, std::memory_order = std::memory_order_seq_cst) noexcept { return val += arg; }
    T fetch_sub (T const arg, std::memory_order = std::memory_order_seq_cst) noexcept { return val -= arg; }
    T fetch_and (T const arg, std::memory_order = std::memory_order_seq_cst) noexcept { return val & arg; }
    T fetch_or  (T const arg, std::memory_order = std::memory_order_seq_cst) noexcept { return val | arg; }
    T fetch_xor (T const arg, std::memory_order = std::memory_order_seq_cst) noexcept { return val ^ arg; }

    DummyStdAtomic () noexcept = default;
    constexpr DummyStdAtomic (T const val) noexcept : val (val) {}
    DummyStdAtomic (DummyStdAtomic const &) = delete;
    DummyStdAtomic& operator= (DummyStdAtomic const &) = delete;
    T operator= (T const new_val) noexcept { return val = new_val; }

    T operator++ (int) noexcept { T const old_val = val; ++val; return old_val; }
    T operator-- (int) noexcept { T const old_val = val; --val; return old_val; }
    T operator++ ()    noexcept { return ++val; }
    T operator-- ()    noexcept { return --val; }
    T operator+= (T const arg) noexcept { return val += arg; }
    T operator-= (T const arg) noexcept { return val -= arg; }
    T operator&= (T const arg) noexcept { return val &= arg; }
    T operator|= (T const arg) noexcept { return val |= arg; }
    T operator^= (T const arg) noexcept { return val ^= arg; }
};
#endif

#ifdef LIBMARY_MT_SAFE
template <class T> using Atomic = std::atomic<T>;
#else
template <class T> using Atomic = DummyStdAtomic<T>;
#endif

}


#endif /* LIBMARY__STD_ATOMIC__H__ */

