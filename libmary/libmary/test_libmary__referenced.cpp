#include <libmary/libmary.h>


#warning Too many tlocal destructors called?


namespace libMary_Test {

using namespace M;

namespace {
class Derived : public Referenced
{
    bool *dtor_called;

  public:
    Derived (bool * const dtor_called)
        : dtor_called (dtor_called)
    {}

    ~Derived ()
    {
        *dtor_called = true;
    }
};
}

namespace {
class DerivedVirt : public Referenced,
                    public virtual VirtReferenced
{};
}

#ifdef LIBMARY_MT_SAFE
  static void testReferenced_MtSafe ();
#endif

void testReferenced ()
{
    Referenced * const obj = new (std::nothrow) Referenced;
    assert (obj);
    assert (obj->getRefCount() == 1);

    obj->ref ();
    assert (obj->getRefCount() == 2);

    obj->unref ();
    assert (obj->getRefCount() == 1);

    obj->ref ();
    obj->ref ();
    assert (obj->getRefCount() == 3);

    obj->libMary_unref ();
    assert (obj->getRefCount() == 2);
    obj->libMary_ref ();
    assert (obj->getRefCount() == 3);

    obj->virt_ref ();
    assert (obj->getRefCount() == 4);
    obj->virt_unref ();
    assert (obj->getRefCount() == 3);

    VirtReferenced * const vr = static_cast <VirtReferenced*> (obj);
    vr->virt_ref ();
    assert (obj->getRefCount() == 4);
    vr->virt_unref ();
    assert (obj->getRefCount() == 3);

    delete obj;

    {
        bool dtor_called = false;
        Derived * const derived = new (std::nothrow) Derived (&dtor_called);
        assert (derived);
        Referenced * const referenced = static_cast <Referenced*> (derived);
        delete referenced;
        assert (dtor_called);
    }

    {
        bool dtor_called = false;
        Derived * const derived = new (std::nothrow) Derived (&dtor_called);
        assert (derived);
        VirtReferenced * const virt_referenced = static_cast <VirtReferenced*> (derived);
        delete virt_referenced;
        assert (dtor_called);
    }

    {
        bool dtor_called = false;
        Derived * const derived = new (std::nothrow) Derived (&dtor_called);
        assert (derived);
        assert (derived->getRefCount() == 1);
        Referenced * const referenced = static_cast <Referenced*> (derived);
        referenced->unref ();
        assert (dtor_called);
    }

    {
        DerivedVirt * const derived_virt = new (std::nothrow) DerivedVirt;
        assert (derived_virt);
        VirtReferenced * const virt_referenced_one = static_cast <VirtReferenced*> (derived_virt);
        Referenced * const referenced = static_cast <Referenced*> (derived_virt);
        VirtReferenced * const virt_referenced_two = static_cast <VirtReferenced*> (referenced);
        assert (virt_referenced_one == virt_referenced_two);
        virt_referenced_one->virt_ref ();
        assert (derived_virt->getRefCount() == 2);
        virt_referenced_two->virt_unref ();
        assert (derived_virt->getRefCount() == 1);
    }

  #ifdef LIBMARY_MT_SAFE
    testReferenced_MtSafe ();
  #endif
}

}


#ifdef LIBMARY_MT_SAFE
#include <atomic>


namespace libMary_Test {

static std::atomic<bool> stop (false);

static void threadFunc (void*);

static void testReferenced_MtSafe ()
{
    Count const num_threads = 10;

    Referenced * const obj = new (std::nothrow) Referenced;
    assert (obj);

    Ref<Thread> threads [num_threads];
    for (Count i = 0; i < num_threads; ++i) {
        threads [i] = grabNewObject <Thread> (CbDesc<Thread::ThreadFunc> (threadFunc, obj, NULL));
        threads [i]->spawn (/*joinable=*/ true);
    }

    sleepSeconds (3);

    stop.store (true, std::memory_order_relaxed);
    for (Count i = 0; i < num_threads; ++i) {
        threads [i]->join ();
    }

    assert (obj->getRefCount() == 1);
    obj->unref ();
}

static void threadFunc (void * const _obj)
{
    Referenced * const obj = static_cast <Referenced*> (_obj);

    while (!stop.load (std::memory_order_relaxed)) {
        unsigned const n = (unsigned) (randomUint32 () % 32);

        for (unsigned i = 0; i < n; ++i)
            obj->ref ();

        for (unsigned i = 0; i < n; ++i)
            obj->unref ();
    }
}

}
#endif // LIBMARY_MT_SAFE

