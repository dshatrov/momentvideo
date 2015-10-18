#include <libmary/libmary.h>


namespace libMary_Test {

using namespace M;

class A : public Referenced
{
  public:
    void f () {}
};

class B : public A
{
    bool *dtor_called;

  public:
    B (bool * const dtor_called = NULL)
        : dtor_called (dtor_called)
    {}

    ~B ()
    {
        if (dtor_called)
            *dtor_called = true;
    }
};

class D : public Object
{
  public:
    D (EmbedContainer * const embed_container,
       int /* param */)
        : Object (embed_container)
    {}
};

class E : public Referenced
{
  public:
    E (int /* param */) {}
};

Ref<A> pass (A * const a)
{
    return a;
}

Ref<B> pass (B * const b)
{
    return b;
}

void testRef ()
{
    Ref<A> const null_ref;
    assert (!null_ref);
    assert (null_ref.ptr() == NULL);
    assert (null_ref.isNull());

    Ref<B> b = grabNew <B> ();
    assert (b->getRefCount() == 1);

    Ref<B> const b2 = b;
    assert (b->getRefCount() == 2);
    assert (b2 == b.ptr());

    Ref<A> a = b;
    assert (a->getRefCount() == 3);
    assert (a.ptr() == b2);

    a = b2;
    assert (a->getRefCount() == 3);
    assert (a.ptr() == b);

    Ref<B> const b_null = NULL;
    a = NULL;
    assert (a.ptr() == b_null);
    assert (b->getRefCount() == 2);
    a = b;

    {
        A * const a_ptr = b2;
        assert (a_ptr->getRefCount() == 3);
        assert (a_ptr == b);
    }

    b->f ();
    a->f ();

    B * const b_ptr = &*b;
    A * const a_ptr = &*b2;
    assert (a_ptr == b_ptr);

    if (a /* implicit conversion to bool */) {
        assert (b);
    }

    bool dtor_called = false;
    {
        B * const obj = new (std::nothrow) B (&dtor_called);
        assert (obj);
        assert (obj->getRefCount() == 1);
        a.setNoRef (obj);
        assert (a.ptr() == obj);
        assert (a->getRefCount() == 1);
        assert (b->getRefCount() == 2);
    }

    assert (!a.isNull());
    a.setNoRef (NULL);
    assert (dtor_called);
    assert (a.isNull());

    b.setNoUnref (NULL);
    assert (b2->getRefCount() == 2);
    b.setNoRef (b2);
    assert (b->getRefCount() == 2);

    b.setNoRefUnref (NULL);
    assert (b == (B const *) NULL);
    assert (b2->getRefCount() == 2);
    b.setNoRefUnref (b2);
    assert (b2 == b.ptr());
    assert (b2->getRefCount() == 2);

    a = b;
    assert (b->getRefCount() == 3);
    a = b;
    assert (b->getRefCount() == 3);
    b = b;
    assert (b->getRefCount() == 3);

    assert (a.ptr() == b.ptr());
    assert (a.ptr() == a);

    Ref<A> a2 = pass (a.ptr());
    assert (b->getRefCount() == 4);
    a2 = pass (b.ptr());
    assert (a2->getRefCount() == 4);

    B * const c_ptr = new (std::nothrow) B;
    assert (c_ptr);
    assert (c_ptr->getRefCount() == 1);
    Ref<A> const c = Ref<A>::createNoRef (c_ptr);
    assert (c.ptr() == c_ptr);
    assert (c->getRefCount() == 1);

    B &b_ref = b.der();
    assert (&b_ref == b);

    Ref<D> const d = grabNewObject <D> (/*param=*/ 1);
    assert (d);
    assert (d->getRefCount() == 1);

    Ref<E> const e = grabNew <E> (/*param=*/ 2);
    assert (e);
    assert (e->getRefCount() == 1);

    Ref<E> const e2 = grab (new (std::nothrow) E (/*param=*/ 3));
    assert (e2);
    assert (e2->getRefCount() == 1);

    UintPtr const ptr_val = uintPtr (d);
    UintPtr const ptr_val2 = (UintPtr) d.ptr();
    assert (ptr_val == ptr_val2);
}

}

