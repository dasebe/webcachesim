#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <stdarg.h>

using namespace std;

 class A;

 class CacheFactory {
 public:
  CacheFactory() {}
  virtual unique_ptr<A> create_unique() = 0;
};

 class A {
 public:
  A() {}
  static void registerType(string name, CacheFactory *factory) {
    get_factory_instance()[name] = factory;
  }
  static unique_ptr<A> create_unique(string name) {
    unique_ptr<A> A_instance =
        move(get_factory_instance()[name]->create_unique());
    return A_instance;
  }

   virtual void setPar(int count, ...) {}
   virtual ~A() {}

 protected:
   static map<string, CacheFactory *> &get_factory_instance() {
     static map<string, CacheFactory *> map_instance;
     return map_instance;
  }
};


template<class T>
 class Factory : public CacheFactory {
 public:
  Factory(string name) { A::registerType(name, this); }
  unique_ptr<A> create_unique() {
    unique_ptr<A> newT(new T);
    return newT;
  }
};

class B : public A {
 public:
  B() {}
  void I_am() { cout << "I am B " << xx << "\n"; }
  virtual void setPar(int count, ...) {
    va_list args;
    // assert count==1
    va_start(args, count);
    xx=va_arg(args, int); }
  ~B() {}

protected:
  int xx;
};
static Factory<B> factoryB("B");

class C : public A {
 public:
  C() {}
  void I_am() { cout << "I am C \n"; }
  ~C() {}
};
static Factory<C> factoryC("C");

 void caller() {}

 int main() {
  unique_ptr<A> b1 = move(A::create_unique("B"));
  unique_ptr<A> b2 = move(A::create_unique("C"));
  b1->setPar(1,3);
  b1->I_am();
  b2->I_am();

  return 0;
}
