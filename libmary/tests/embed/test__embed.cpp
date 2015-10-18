#include <libmary/libmary.h>


using namespace M;

class A : public Object
{
  public:
    A (EmbedContainer * const embed_container)
        : Object (embed_container)
    {
        logD_ (_this_func_);
    }

    ~A ()
    {
        logD_ (_this_func_);
    }
};

class B : public Object
{
    Embed<A> a;

  public:
    B (EmbedContainer * const embed_container)
        : Object (embed_container),
          a      (this /* embed_container */)
    {
        logD_ (_this_func_);
    }

    ~B ()
    {
        logD_ (_this_func_);
    }
};

int main (void)
{
    libMaryInit ();

    Ref<B> const b = grab (new (std::nothrow) B (NULL /* embed_container */));

    logD_ (_func, "done");
    return 0;
}

