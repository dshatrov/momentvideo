#include <new>
#include <cstdio>


struct EmbedContainer
{
  private:
    /* mt_const */ EmbedContainer *embed_container;

    unsigned embed_container_refcnt;

    EmbedContainer* getEmbedContainer () { return embed_container ? embed_container : static_cast <EmbedContainer*> (this); }

    void embedContainerRef ()
    {
        printf ("0x%lx embedContainerRef, embed_container_refcnt: %u\n", (unsigned long) this, embed_container_refcnt);
        ++embed_container_refcnt;
    }

    void embedContainerUnref ()
    {
        printf ("0x%lx embedContainerUnref, embed_container_refcnt: %u\n", (unsigned long) this, embed_container_refcnt);
        if (! --embed_container_refcnt)
            delete this;
    }

  protected:
    void do_delete ()
    {
        if (embed_container) {
            this->~EmbedContainer ();
            embed_container->embedContainerUnref ();
        } else {
            embedContainerUnref ();
        }
    }

    EmbedContainer (EmbedContainer * const embed_container)
        : embed_container_refcnt (1)
    {
        if (embed_container) {
            this->embed_container = embed_container->getEmbedContainer ();
            this->embed_container->embedContainerRef ();
        } else {
            this->embed_container = NULL;
        }
    }

  public:
    virtual ~EmbedContainer () {}
};

template <class T>
class Embed
{
public:
    // alignas is supported in g++-4.8 and higher. We'll switch to that after
    // we stop using g++-4.6.
    // alignas (T) unsigned char data [sizeof (T)];
    __attribute__ ((aligned (alignof (T)))) unsigned char data [sizeof (T)];

    T const * operator-> () const { return reinterpret_cast <T const *> (data); }
    T       * operator-> ()       { return reinterpret_cast <T       *> (data); }

    operator T const *   () const { return reinterpret_cast <T const *> (data); }
    operator T       *   ()       { return reinterpret_cast <T       *> (data); }

    Embed (EmbedContainer * const embed_container)
    {
        new (reinterpret_cast <T*> (data)) T (embed_container);
    }

    ~Embed ()
    {
        reinterpret_cast <T*> (data)->unref ();
    }
};

struct Object : public EmbedContainer
{
    unsigned object_refcnt;

    void ref   () { ++object_refcnt; }
    void unref () { if (! --object_refcnt) do_delete (); }

    Object (EmbedContainer * const embed_container)
        : EmbedContainer (embed_container),
          object_refcnt (1)
    {}
};

struct A : public Object
{
    A (EmbedContainer * const embed_container)
        : Object (embed_container)
    {
        printf ("0x%lx A()\n", (unsigned long) this);
    }

    ~A ()
    {
        printf ("0x%lx ~A()\n", (unsigned long) this);
    }
};

struct B : public Object
{
    B (EmbedContainer * const embed_container)
        : Object (embed_container)
    {
        printf ("0x%lx B()\n", (unsigned long) this);
    }

    ~B ()
    {
        printf ("0x%lx ~B()\n", (unsigned long) this);
    }
};

struct C : public Object
{
    Embed<A> a;
    Embed<B> b;

    C (EmbedContainer * const embed_container)
        : Object (embed_container),
          a (this /* embed_container */),
          b (this /* embed_container */)
    {}
};

int main (void)
{
    C c (NULL /* embed_container */);
}

