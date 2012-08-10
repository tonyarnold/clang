// RUN: %clang_cc1 -analyze -analyzer-checker=core,debug.ExprInspection -analyzer-ipa=inlining -verify %s

void clang_analyzer_eval(bool);
void clang_analyzer_checkInlined(bool);

class A {
public:
  int getZero() { return 0; }
  virtual int getNum() { return 0; }
};

void test(A &a) {
  clang_analyzer_eval(a.getZero() == 0); // expected-warning{{TRUE}}
  clang_analyzer_eval(a.getNum() == 0); // expected-warning{{UNKNOWN}}

  A copy(a);
  clang_analyzer_eval(copy.getZero() == 0); // expected-warning{{TRUE}}
  clang_analyzer_eval(copy.getNum() == 0); // expected-warning{{TRUE}}
}


class One : public A {
public:
  virtual int getNum() { return 1; }
};

void testPathSensitivity(int x) {
  A a;
  One b;

  A *ptr;
  switch (x) {
  case 0:
    ptr = &a;
    break;
  case 1:
    ptr = &b;
    break;
  default:
    return;
  }

  // This should be true on both branches.
  clang_analyzer_eval(ptr->getNum() == x); // expected-warning {{TRUE}}
}


namespace PureVirtualParent {
  class Parent {
  public:
    virtual int pureVirtual() const = 0;
    int callVirtual() const {
      return pureVirtual();
    }
  };

  class Child : public Parent {
  public:
    virtual int pureVirtual() const {
      clang_analyzer_checkInlined(true); // expected-warning{{TRUE}}
      return 42;
    }
  };

  void testVirtual() {
    Child x;

    clang_analyzer_eval(x.pureVirtual() == 42); // expected-warning{{TRUE}}
    clang_analyzer_eval(x.callVirtual() == 42); // expected-warning{{TRUE}}
  }
}


