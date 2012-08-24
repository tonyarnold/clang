//===- unittest/Tooling/ASTMatchersTest.cpp - AST matcher unit tests ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "ASTMatchersTest.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Tooling/Tooling.h"
#include "gtest/gtest.h"

namespace clang {
namespace ast_matchers {

#if GTEST_HAS_DEATH_TEST
TEST(HasNameDeathTest, DiesOnEmptyName) {
  ASSERT_DEBUG_DEATH({
    DeclarationMatcher HasEmptyName = recordDecl(hasName(""));
    EXPECT_TRUE(notMatches("class X {};", HasEmptyName));
  }, "");
}

TEST(HasNameDeathTest, DiesOnEmptyPattern) {
  ASSERT_DEBUG_DEATH({
      DeclarationMatcher HasEmptyName = recordDecl(matchesName(""));
      EXPECT_TRUE(notMatches("class X {};", HasEmptyName));
    }, "");
}

TEST(IsDerivedFromDeathTest, DiesOnEmptyBaseName) {
  ASSERT_DEBUG_DEATH({
    DeclarationMatcher IsDerivedFromEmpty = recordDecl(isDerivedFrom(""));
    EXPECT_TRUE(notMatches("class X {};", IsDerivedFromEmpty));
  }, "");
}
#endif

TEST(Decl, MatchesDeclarations) {
  EXPECT_TRUE(notMatches("", decl(usingDecl())));
  EXPECT_TRUE(matches("namespace x { class X {}; } using x::X;",
                      decl(usingDecl())));
}

TEST(NameableDeclaration, MatchesVariousDecls) {
  DeclarationMatcher NamedX = namedDecl(hasName("X"));
  EXPECT_TRUE(matches("typedef int X;", NamedX));
  EXPECT_TRUE(matches("int X;", NamedX));
  EXPECT_TRUE(matches("class foo { virtual void X(); };", NamedX));
  EXPECT_TRUE(matches("void foo() try { } catch(int X) { }", NamedX));
  EXPECT_TRUE(matches("void foo() { int X; }", NamedX));
  EXPECT_TRUE(matches("namespace X { }", NamedX));
  EXPECT_TRUE(matches("enum X { A, B, C };", NamedX));

  EXPECT_TRUE(notMatches("#define X 1", NamedX));
}

TEST(NameableDeclaration, REMatchesVariousDecls) {
  DeclarationMatcher NamedX = namedDecl(matchesName("::X"));
  EXPECT_TRUE(matches("typedef int Xa;", NamedX));
  EXPECT_TRUE(matches("int Xb;", NamedX));
  EXPECT_TRUE(matches("class foo { virtual void Xc(); };", NamedX));
  EXPECT_TRUE(matches("void foo() try { } catch(int Xdef) { }", NamedX));
  EXPECT_TRUE(matches("void foo() { int Xgh; }", NamedX));
  EXPECT_TRUE(matches("namespace Xij { }", NamedX));
  EXPECT_TRUE(matches("enum X { A, B, C };", NamedX));

  EXPECT_TRUE(notMatches("#define Xkl 1", NamedX));

  DeclarationMatcher StartsWithNo = namedDecl(matchesName("::no"));
  EXPECT_TRUE(matches("int no_foo;", StartsWithNo));
  EXPECT_TRUE(matches("class foo { virtual void nobody(); };", StartsWithNo));

  DeclarationMatcher Abc = namedDecl(matchesName("a.*b.*c"));
  EXPECT_TRUE(matches("int abc;", Abc));
  EXPECT_TRUE(matches("int aFOObBARc;", Abc));
  EXPECT_TRUE(notMatches("int cab;", Abc));
  EXPECT_TRUE(matches("int cabc;", Abc));
}

TEST(DeclarationMatcher, MatchClass) {
  DeclarationMatcher ClassMatcher(recordDecl());
#if !defined(_MSC_VER)
  EXPECT_FALSE(matches("", ClassMatcher));
#else
  // Matches class type_info.
  EXPECT_TRUE(matches("", ClassMatcher));
#endif

  DeclarationMatcher ClassX = recordDecl(recordDecl(hasName("X")));
  EXPECT_TRUE(matches("class X;", ClassX));
  EXPECT_TRUE(matches("class X {};", ClassX));
  EXPECT_TRUE(matches("template<class T> class X {};", ClassX));
  EXPECT_TRUE(notMatches("", ClassX));
}

TEST(DeclarationMatcher, ClassIsDerived) {
  DeclarationMatcher IsDerivedFromX = recordDecl(isDerivedFrom("X"));

  EXPECT_TRUE(matches("class X {}; class Y : public X {};", IsDerivedFromX));
  EXPECT_TRUE(matches("class X {}; class Y : public X {};", IsDerivedFromX));
  EXPECT_TRUE(matches("class X {};", IsDerivedFromX));
  EXPECT_TRUE(matches("class X;", IsDerivedFromX));
  EXPECT_TRUE(notMatches("class Y;", IsDerivedFromX));
  EXPECT_TRUE(notMatches("", IsDerivedFromX));

  DeclarationMatcher ZIsDerivedFromX =
      recordDecl(hasName("Z"), isDerivedFrom("X"));
  EXPECT_TRUE(
      matches("class X {}; class Y : public X {}; class Z : public Y {};",
              ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("class X {};"
              "template<class T> class Y : public X {};"
              "class Z : public Y<int> {};", ZIsDerivedFromX));
  EXPECT_TRUE(matches("class X {}; template<class T> class Z : public X {};",
                      ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("template<class T> class X {}; "
              "template<class T> class Z : public X<T> {};",
              ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("template<class T, class U=T> class X {}; "
              "template<class T> class Z : public X<T> {};",
              ZIsDerivedFromX));
  EXPECT_TRUE(
      notMatches("template<class X> class A { class Z : public X {}; };",
                 ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("template<class X> class A { public: class Z : public X {}; }; "
              "class X{}; void y() { A<X>::Z z; }", ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("template <class T> class X {}; "
              "template<class Y> class A { class Z : public X<Y> {}; };",
              ZIsDerivedFromX));
  EXPECT_TRUE(
      notMatches("template<template<class T> class X> class A { "
                 "  class Z : public X<int> {}; };", ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("template<template<class T> class X> class A { "
              "  public: class Z : public X<int> {}; }; "
              "template<class T> class X {}; void y() { A<X>::Z z; }",
              ZIsDerivedFromX));
  EXPECT_TRUE(
      notMatches("template<class X> class A { class Z : public X::D {}; };",
                 ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("template<class X> class A { public: "
              "  class Z : public X::D {}; }; "
              "class Y { public: class X {}; typedef X D; }; "
              "void y() { A<Y>::Z z; }", ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("class X {}; typedef X Y; class Z : public Y {};",
              ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("template<class T> class Y { typedef typename T::U X; "
              "  class Z : public X {}; };", ZIsDerivedFromX));
  EXPECT_TRUE(matches("class X {}; class Z : public ::X {};",
                      ZIsDerivedFromX));
  EXPECT_TRUE(
      notMatches("template<class T> class X {}; "
                "template<class T> class A { class Z : public X<T>::D {}; };",
                ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("template<class T> class X { public: typedef X<T> D; }; "
              "template<class T> class A { public: "
              "  class Z : public X<T>::D {}; }; void y() { A<int>::Z z; }",
              ZIsDerivedFromX));
  EXPECT_TRUE(
      notMatches("template<class X> class A { class Z : public X::D::E {}; };",
                 ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("class X {}; typedef X V; typedef V W; class Z : public W {};",
              ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("class X {}; class Y : public X {}; "
              "typedef Y V; typedef V W; class Z : public W {};",
              ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("template<class T, class U> class X {}; "
              "template<class T> class A { class Z : public X<T, int> {}; };",
              ZIsDerivedFromX));
  EXPECT_TRUE(
      notMatches("template<class X> class D { typedef X A; typedef A B; "
                 "  typedef B C; class Z : public C {}; };",
                 ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("class X {}; typedef X A; typedef A B; "
              "class Z : public B {};", ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("class X {}; typedef X A; typedef A B; typedef B C; "
              "class Z : public C {};", ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("class U {}; typedef U X; typedef X V; "
              "class Z : public V {};", ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("class Base {}; typedef Base X; "
              "class Z : public Base {};", ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("class Base {}; typedef Base Base2; typedef Base2 X; "
              "class Z : public Base {};", ZIsDerivedFromX));
  EXPECT_TRUE(
      notMatches("class Base {}; class Base2 {}; typedef Base2 X; "
                 "class Z : public Base {};", ZIsDerivedFromX));
  EXPECT_TRUE(
      matches("class A {}; typedef A X; typedef A Y; "
              "class Z : public Y {};", ZIsDerivedFromX));
  EXPECT_TRUE(
      notMatches("template <typename T> class Z;"
                 "template <> class Z<void> {};"
                 "template <typename T> class Z : public Z<void> {};",
                 IsDerivedFromX));
  EXPECT_TRUE(
      matches("template <typename T> class X;"
              "template <> class X<void> {};"
              "template <typename T> class X : public X<void> {};",
              IsDerivedFromX));
  EXPECT_TRUE(matches(
      "class X {};"
      "template <typename T> class Z;"
      "template <> class Z<void> {};"
      "template <typename T> class Z : public Z<void>, public X {};",
      ZIsDerivedFromX));

  // FIXME: Once we have better matchers for template type matching,
  // get rid of the Variable(...) matching and match the right template
  // declarations directly.
  const char *RecursiveTemplateOneParameter =
      "class Base1 {}; class Base2 {};"
      "template <typename T> class Z;"
      "template <> class Z<void> : public Base1 {};"
      "template <> class Z<int> : public Base2 {};"
      "template <> class Z<float> : public Z<void> {};"
      "template <> class Z<double> : public Z<int> {};"
      "template <typename T> class Z : public Z<float>, public Z<double> {};"
      "void f() { Z<float> z_float; Z<double> z_double; Z<char> z_char; }";
  EXPECT_TRUE(matches(
      RecursiveTemplateOneParameter,
      varDecl(hasName("z_float"),
              hasInitializer(hasType(recordDecl(isDerivedFrom("Base1")))))));
  EXPECT_TRUE(notMatches(
      RecursiveTemplateOneParameter,
      varDecl(hasName("z_float"),
              hasInitializer(hasType(recordDecl(isDerivedFrom("Base2")))))));
  EXPECT_TRUE(matches(
      RecursiveTemplateOneParameter,
      varDecl(hasName("z_char"),
              hasInitializer(hasType(recordDecl(isDerivedFrom("Base1"),
                                                isDerivedFrom("Base2")))))));

  const char *RecursiveTemplateTwoParameters =
      "class Base1 {}; class Base2 {};"
      "template <typename T1, typename T2> class Z;"
      "template <typename T> class Z<void, T> : public Base1 {};"
      "template <typename T> class Z<int, T> : public Base2 {};"
      "template <typename T> class Z<float, T> : public Z<void, T> {};"
      "template <typename T> class Z<double, T> : public Z<int, T> {};"
      "template <typename T1, typename T2> class Z : "
      "    public Z<float, T2>, public Z<double, T2> {};"
      "void f() { Z<float, void> z_float; Z<double, void> z_double; "
      "           Z<char, void> z_char; }";
  EXPECT_TRUE(matches(
      RecursiveTemplateTwoParameters,
      varDecl(hasName("z_float"),
              hasInitializer(hasType(recordDecl(isDerivedFrom("Base1")))))));
  EXPECT_TRUE(notMatches(
      RecursiveTemplateTwoParameters,
      varDecl(hasName("z_float"),
              hasInitializer(hasType(recordDecl(isDerivedFrom("Base2")))))));
  EXPECT_TRUE(matches(
      RecursiveTemplateTwoParameters,
      varDecl(hasName("z_char"),
              hasInitializer(hasType(recordDecl(isDerivedFrom("Base1"),
                                                isDerivedFrom("Base2")))))));
  EXPECT_TRUE(matches(
      "namespace ns { class X {}; class Y : public X {}; }",
      recordDecl(isDerivedFrom("::ns::X"))));
  EXPECT_TRUE(notMatches(
      "class X {}; class Y : public X {};",
      recordDecl(isDerivedFrom("::ns::X"))));

  EXPECT_TRUE(matches(
      "class X {}; class Y : public X {};",
      recordDecl(isDerivedFrom(recordDecl(hasName("X")).bind("test")))));
}

TEST(ClassTemplate, DoesNotMatchClass) {
  DeclarationMatcher ClassX = classTemplateDecl(hasName("X"));
  EXPECT_TRUE(notMatches("class X;", ClassX));
  EXPECT_TRUE(notMatches("class X {};", ClassX));
}

TEST(ClassTemplate, MatchesClassTemplate) {
  DeclarationMatcher ClassX = classTemplateDecl(hasName("X"));
  EXPECT_TRUE(matches("template<typename T> class X {};", ClassX));
  EXPECT_TRUE(matches("class Z { template<class T> class X {}; };", ClassX));
}

TEST(ClassTemplate, DoesNotMatchClassTemplateExplicitSpecialization) {
  EXPECT_TRUE(notMatches("template<typename T> class X { };"
                         "template<> class X<int> { int a; };",
              classTemplateDecl(hasName("X"),
                                hasDescendant(fieldDecl(hasName("a"))))));
}

TEST(ClassTemplate, DoesNotMatchClassTemplatePartialSpecialization) {
  EXPECT_TRUE(notMatches("template<typename T, typename U> class X { };"
                         "template<typename T> class X<T, int> { int a; };",
              classTemplateDecl(hasName("X"),
                                hasDescendant(fieldDecl(hasName("a"))))));
}

TEST(AllOf, AllOverloadsWork) {
  const char Program[] =
      "struct T { }; int f(int, T*); void g(int x) { T t; f(x, &t); }";
  EXPECT_TRUE(matches(Program,
      callExpr(allOf(callee(functionDecl(hasName("f"))),
                     hasArgument(0, declRefExpr(to(varDecl())))))));
  EXPECT_TRUE(matches(Program,
      callExpr(allOf(callee(functionDecl(hasName("f"))),
                     hasArgument(0, declRefExpr(to(varDecl()))),
                     hasArgument(1, hasType(pointsTo(
                                        recordDecl(hasName("T")))))))));
}

TEST(DeclarationMatcher, MatchAnyOf) {
  DeclarationMatcher YOrZDerivedFromX =
      recordDecl(anyOf(hasName("Y"), allOf(isDerivedFrom("X"), hasName("Z"))));
  EXPECT_TRUE(
      matches("class X {}; class Z : public X {};", YOrZDerivedFromX));
  EXPECT_TRUE(matches("class Y {};", YOrZDerivedFromX));
  EXPECT_TRUE(
      notMatches("class X {}; class W : public X {};", YOrZDerivedFromX));
  EXPECT_TRUE(notMatches("class Z {};", YOrZDerivedFromX));

  DeclarationMatcher XOrYOrZOrU =
      recordDecl(anyOf(hasName("X"), hasName("Y"), hasName("Z"), hasName("U")));
  EXPECT_TRUE(matches("class X {};", XOrYOrZOrU));
  EXPECT_TRUE(notMatches("class V {};", XOrYOrZOrU));

  DeclarationMatcher XOrYOrZOrUOrV =
      recordDecl(anyOf(hasName("X"), hasName("Y"), hasName("Z"), hasName("U"),
                       hasName("V")));
  EXPECT_TRUE(matches("class X {};", XOrYOrZOrUOrV));
  EXPECT_TRUE(matches("class Y {};", XOrYOrZOrUOrV));
  EXPECT_TRUE(matches("class Z {};", XOrYOrZOrUOrV));
  EXPECT_TRUE(matches("class U {};", XOrYOrZOrUOrV));
  EXPECT_TRUE(matches("class V {};", XOrYOrZOrUOrV));
  EXPECT_TRUE(notMatches("class A {};", XOrYOrZOrUOrV));
}

TEST(DeclarationMatcher, MatchHas) {
  DeclarationMatcher HasClassX = recordDecl(has(recordDecl(hasName("X"))));
  EXPECT_TRUE(matches("class Y { class X {}; };", HasClassX));
  EXPECT_TRUE(matches("class X {};", HasClassX));

  DeclarationMatcher YHasClassX =
      recordDecl(hasName("Y"), has(recordDecl(hasName("X"))));
  EXPECT_TRUE(matches("class Y { class X {}; };", YHasClassX));
  EXPECT_TRUE(notMatches("class X {};", YHasClassX));
  EXPECT_TRUE(
      notMatches("class Y { class Z { class X {}; }; };", YHasClassX));
}

TEST(DeclarationMatcher, MatchHasRecursiveAllOf) {
  DeclarationMatcher Recursive =
    recordDecl(
      has(recordDecl(
        has(recordDecl(hasName("X"))),
        has(recordDecl(hasName("Y"))),
        hasName("Z"))),
      has(recordDecl(
        has(recordDecl(hasName("A"))),
        has(recordDecl(hasName("B"))),
        hasName("C"))),
      hasName("F"));

  EXPECT_TRUE(matches(
      "class F {"
      "  class Z {"
      "    class X {};"
      "    class Y {};"
      "  };"
      "  class C {"
      "    class A {};"
      "    class B {};"
      "  };"
      "};", Recursive));

  EXPECT_TRUE(matches(
      "class F {"
      "  class Z {"
      "    class A {};"
      "    class X {};"
      "    class Y {};"
      "  };"
      "  class C {"
      "    class X {};"
      "    class A {};"
      "    class B {};"
      "  };"
      "};", Recursive));

  EXPECT_TRUE(matches(
      "class O1 {"
      "  class O2 {"
      "    class F {"
      "      class Z {"
      "        class A {};"
      "        class X {};"
      "        class Y {};"
      "      };"
      "      class C {"
      "        class X {};"
      "        class A {};"
      "        class B {};"
      "      };"
      "    };"
      "  };"
      "};", Recursive));
}

TEST(DeclarationMatcher, MatchHasRecursiveAnyOf) {
  DeclarationMatcher Recursive =
      recordDecl(
          anyOf(
              has(recordDecl(
                  anyOf(
                      has(recordDecl(
                          hasName("X"))),
                      has(recordDecl(
                          hasName("Y"))),
                      hasName("Z")))),
              has(recordDecl(
                  anyOf(
                      hasName("C"),
                      has(recordDecl(
                          hasName("A"))),
                      has(recordDecl(
                          hasName("B")))))),
              hasName("F")));

  EXPECT_TRUE(matches("class F {};", Recursive));
  EXPECT_TRUE(matches("class Z {};", Recursive));
  EXPECT_TRUE(matches("class C {};", Recursive));
  EXPECT_TRUE(matches("class M { class N { class X {}; }; };", Recursive));
  EXPECT_TRUE(matches("class M { class N { class B {}; }; };", Recursive));
  EXPECT_TRUE(
      matches("class O1 { class O2 {"
              "  class M { class N { class B {}; }; }; "
              "}; };", Recursive));
}

TEST(DeclarationMatcher, MatchNot) {
  DeclarationMatcher NotClassX =
      recordDecl(
          isDerivedFrom("Y"),
          unless(hasName("Y")),
          unless(hasName("X")));
  EXPECT_TRUE(notMatches("", NotClassX));
  EXPECT_TRUE(notMatches("class Y {};", NotClassX));
  EXPECT_TRUE(matches("class Y {}; class Z : public Y {};", NotClassX));
  EXPECT_TRUE(notMatches("class Y {}; class X : public Y {};", NotClassX));
  EXPECT_TRUE(
      notMatches("class Y {}; class Z {}; class X : public Y {};",
                 NotClassX));

  DeclarationMatcher ClassXHasNotClassY =
      recordDecl(
          hasName("X"),
          has(recordDecl(hasName("Z"))),
          unless(
              has(recordDecl(hasName("Y")))));
  EXPECT_TRUE(matches("class X { class Z {}; };", ClassXHasNotClassY));
  EXPECT_TRUE(notMatches("class X { class Y {}; class Z {}; };",
                         ClassXHasNotClassY));
}

TEST(DeclarationMatcher, HasDescendant) {
  DeclarationMatcher ZDescendantClassX =
      recordDecl(
          hasDescendant(recordDecl(hasName("X"))),
          hasName("Z"));
  EXPECT_TRUE(matches("class Z { class X {}; };", ZDescendantClassX));
  EXPECT_TRUE(
      matches("class Z { class Y { class X {}; }; };", ZDescendantClassX));
  EXPECT_TRUE(
      matches("class Z { class A { class Y { class X {}; }; }; };",
              ZDescendantClassX));
  EXPECT_TRUE(
      matches("class Z { class A { class B { class Y { class X {}; }; }; }; };",
              ZDescendantClassX));
  EXPECT_TRUE(notMatches("class Z {};", ZDescendantClassX));

  DeclarationMatcher ZDescendantClassXHasClassY =
      recordDecl(
          hasDescendant(recordDecl(has(recordDecl(hasName("Y"))),
                              hasName("X"))),
          hasName("Z"));
  EXPECT_TRUE(matches("class Z { class X { class Y {}; }; };",
              ZDescendantClassXHasClassY));
  EXPECT_TRUE(
      matches("class Z { class A { class B { class X { class Y {}; }; }; }; };",
              ZDescendantClassXHasClassY));
  EXPECT_TRUE(notMatches(
      "class Z {"
      "  class A {"
      "    class B {"
      "      class X {"
      "        class C {"
      "          class Y {};"
      "        };"
      "      };"
      "    }; "
      "  };"
      "};", ZDescendantClassXHasClassY));

  DeclarationMatcher ZDescendantClassXDescendantClassY =
      recordDecl(
          hasDescendant(recordDecl(hasDescendant(recordDecl(hasName("Y"))),
                                   hasName("X"))),
          hasName("Z"));
  EXPECT_TRUE(
      matches("class Z { class A { class X { class B { class Y {}; }; }; }; };",
              ZDescendantClassXDescendantClassY));
  EXPECT_TRUE(matches(
      "class Z {"
      "  class A {"
      "    class X {"
      "      class B {"
      "        class Y {};"
      "      };"
      "      class Y {};"
      "    };"
      "  };"
      "};", ZDescendantClassXDescendantClassY));
}

TEST(Enum, DoesNotMatchClasses) {
  EXPECT_TRUE(notMatches("class X {};", enumDecl(hasName("X"))));
}

TEST(Enum, MatchesEnums) {
  EXPECT_TRUE(matches("enum X {};", enumDecl(hasName("X"))));
}

TEST(EnumConstant, Matches) {
  DeclarationMatcher Matcher = enumConstantDecl(hasName("A"));
  EXPECT_TRUE(matches("enum X{ A };", Matcher));
  EXPECT_TRUE(notMatches("enum X{ B };", Matcher));
  EXPECT_TRUE(notMatches("enum X {};", Matcher));
}

TEST(StatementMatcher, Has) {
  StatementMatcher HasVariableI =
      expr(hasType(pointsTo(recordDecl(hasName("X")))),
           has(declRefExpr(to(varDecl(hasName("i"))))));

  EXPECT_TRUE(matches(
      "class X; X *x(int); void c() { int i; x(i); }", HasVariableI));
  EXPECT_TRUE(notMatches(
      "class X; X *x(int); void c() { int i; x(42); }", HasVariableI));
}

TEST(StatementMatcher, HasDescendant) {
  StatementMatcher HasDescendantVariableI =
      expr(hasType(pointsTo(recordDecl(hasName("X")))),
           hasDescendant(declRefExpr(to(varDecl(hasName("i"))))));

  EXPECT_TRUE(matches(
      "class X; X *x(bool); bool b(int); void c() { int i; x(b(i)); }",
      HasDescendantVariableI));
  EXPECT_TRUE(notMatches(
      "class X; X *x(bool); bool b(int); void c() { int i; x(b(42)); }",
      HasDescendantVariableI));
}

TEST(TypeMatcher, MatchesClassType) {
  TypeMatcher TypeA = hasDeclaration(recordDecl(hasName("A")));

  EXPECT_TRUE(matches("class A { public: A *a; };", TypeA));
  EXPECT_TRUE(notMatches("class A {};", TypeA));

  TypeMatcher TypeDerivedFromA = hasDeclaration(recordDecl(isDerivedFrom("A")));

  EXPECT_TRUE(matches("class A {}; class B : public A { public: B *b; };",
              TypeDerivedFromA));
  EXPECT_TRUE(notMatches("class A {};", TypeA));

  TypeMatcher TypeAHasClassB = hasDeclaration(
      recordDecl(hasName("A"), has(recordDecl(hasName("B")))));

  EXPECT_TRUE(
      matches("class A { public: A *a; class B {}; };", TypeAHasClassB));
}

// Returns from Run whether 'bound_nodes' contain a Decl bound to 'Id', which
// can be dynamically casted to T.
// Optionally checks that the check succeeded a specific number of times.
template <typename T>
class VerifyIdIsBoundToDecl : public BoundNodesCallback {
public:
  // Create an object that checks that a node of type 'T' was bound to 'Id'.
  // Does not check for a certain number of matches.
  explicit VerifyIdIsBoundToDecl(const std::string& Id)
    : Id(Id), ExpectedCount(-1), Count(0) {}

  // Create an object that checks that a node of type 'T' was bound to 'Id'.
  // Checks that there were exactly 'ExpectedCount' matches.
  explicit VerifyIdIsBoundToDecl(const std::string& Id, int ExpectedCount)
    : Id(Id), ExpectedCount(ExpectedCount), Count(0) {}

  ~VerifyIdIsBoundToDecl() {
    if (ExpectedCount != -1) {
      EXPECT_EQ(ExpectedCount, Count);
    }
  }

  virtual bool run(const BoundNodes *Nodes) {
    if (Nodes->getDeclAs<T>(Id) != NULL) {
      ++Count;
      return true;
    }
    return false;
  }

private:
  const std::string Id;
  const int ExpectedCount;
  int Count;
};
template <typename T>
class VerifyIdIsBoundToStmt : public BoundNodesCallback {
public:
  explicit VerifyIdIsBoundToStmt(const std::string &Id) : Id(Id) {}
  virtual bool run(const BoundNodes *Nodes) {
    const T *Node = Nodes->getStmtAs<T>(Id);
    return Node != NULL;
  }
private:
  const std::string Id;
};

TEST(Matcher, BindMatchedNodes) {
  DeclarationMatcher ClassX = has(recordDecl(hasName("::X")).bind("x"));

  EXPECT_TRUE(matchAndVerifyResultTrue("class X {};",
      ClassX, new VerifyIdIsBoundToDecl<CXXRecordDecl>("x")));

  EXPECT_TRUE(matchAndVerifyResultFalse("class X {};",
      ClassX, new VerifyIdIsBoundToDecl<CXXRecordDecl>("other-id")));

  TypeMatcher TypeAHasClassB = hasDeclaration(
      recordDecl(hasName("A"), has(recordDecl(hasName("B")).bind("b"))));

  EXPECT_TRUE(matchAndVerifyResultTrue("class A { public: A *a; class B {}; };",
      TypeAHasClassB,
      new VerifyIdIsBoundToDecl<Decl>("b")));

  StatementMatcher MethodX =
      callExpr(callee(methodDecl(hasName("x")))).bind("x");

  EXPECT_TRUE(matchAndVerifyResultTrue("class A { void x() { x(); } };",
      MethodX,
      new VerifyIdIsBoundToStmt<CXXMemberCallExpr>("x")));
}

TEST(Matcher, BindTheSameNameInAlternatives) {
  StatementMatcher matcher = anyOf(
      binaryOperator(hasOperatorName("+"),
                     hasLHS(expr().bind("x")),
                     hasRHS(integerLiteral(equals(0)))),
      binaryOperator(hasOperatorName("+"),
                     hasLHS(integerLiteral(equals(0))),
                     hasRHS(expr().bind("x"))));

  EXPECT_TRUE(matchAndVerifyResultTrue(
      // The first branch of the matcher binds x to 0 but then fails.
      // The second branch binds x to f() and succeeds.
      "int f() { return 0 + f(); }",
      matcher,
      new VerifyIdIsBoundToStmt<CallExpr>("x")));
}

TEST(HasType, TakesQualTypeMatcherAndMatchesExpr) {
  TypeMatcher ClassX = hasDeclaration(recordDecl(hasName("X")));
  EXPECT_TRUE(
      matches("class X {}; void y(X &x) { x; }", expr(hasType(ClassX))));
  EXPECT_TRUE(
      notMatches("class X {}; void y(X *x) { x; }",
                 expr(hasType(ClassX))));
  EXPECT_TRUE(
      matches("class X {}; void y(X *x) { x; }",
              expr(hasType(pointsTo(ClassX)))));
}

TEST(HasType, TakesQualTypeMatcherAndMatchesValueDecl) {
  TypeMatcher ClassX = hasDeclaration(recordDecl(hasName("X")));
  EXPECT_TRUE(
      matches("class X {}; void y() { X x; }", varDecl(hasType(ClassX))));
  EXPECT_TRUE(
      notMatches("class X {}; void y() { X *x; }", varDecl(hasType(ClassX))));
  EXPECT_TRUE(
      matches("class X {}; void y() { X *x; }",
              varDecl(hasType(pointsTo(ClassX)))));
}

TEST(HasType, TakesDeclMatcherAndMatchesExpr) {
  DeclarationMatcher ClassX = recordDecl(hasName("X"));
  EXPECT_TRUE(
      matches("class X {}; void y(X &x) { x; }", expr(hasType(ClassX))));
  EXPECT_TRUE(
      notMatches("class X {}; void y(X *x) { x; }",
                 expr(hasType(ClassX))));
}

TEST(HasType, TakesDeclMatcherAndMatchesValueDecl) {
  DeclarationMatcher ClassX = recordDecl(hasName("X"));
  EXPECT_TRUE(
      matches("class X {}; void y() { X x; }", varDecl(hasType(ClassX))));
  EXPECT_TRUE(
      notMatches("class X {}; void y() { X *x; }", varDecl(hasType(ClassX))));
}

TEST(Matcher, Call) {
  // FIXME: Do we want to overload Call() to directly take
  // Matcher<Decl>, too?
  StatementMatcher MethodX = callExpr(hasDeclaration(methodDecl(hasName("x"))));

  EXPECT_TRUE(matches("class Y { void x() { x(); } };", MethodX));
  EXPECT_TRUE(notMatches("class Y { void x() {} };", MethodX));

  StatementMatcher MethodOnY =
      memberCallExpr(on(hasType(recordDecl(hasName("Y")))));

  EXPECT_TRUE(
      matches("class Y { public: void x(); }; void z() { Y y; y.x(); }",
              MethodOnY));
  EXPECT_TRUE(
      matches("class Y { public: void x(); }; void z(Y &y) { y.x(); }",
              MethodOnY));
  EXPECT_TRUE(
      notMatches("class Y { public: void x(); }; void z(Y *&y) { y->x(); }",
                 MethodOnY));
  EXPECT_TRUE(
      notMatches("class Y { public: void x(); }; void z(Y y[]) { y->x(); }",
                 MethodOnY));
  EXPECT_TRUE(
      notMatches("class Y { public: void x(); }; void z() { Y *y; y->x(); }",
                 MethodOnY));

  StatementMatcher MethodOnYPointer =
      memberCallExpr(on(hasType(pointsTo(recordDecl(hasName("Y"))))));

  EXPECT_TRUE(
      matches("class Y { public: void x(); }; void z() { Y *y; y->x(); }",
              MethodOnYPointer));
  EXPECT_TRUE(
      matches("class Y { public: void x(); }; void z(Y *&y) { y->x(); }",
              MethodOnYPointer));
  EXPECT_TRUE(
      matches("class Y { public: void x(); }; void z(Y y[]) { y->x(); }",
              MethodOnYPointer));
  EXPECT_TRUE(
      notMatches("class Y { public: void x(); }; void z() { Y y; y.x(); }",
                 MethodOnYPointer));
  EXPECT_TRUE(
      notMatches("class Y { public: void x(); }; void z(Y &y) { y.x(); }",
                 MethodOnYPointer));
}

TEST(HasType, MatchesAsString) {
  EXPECT_TRUE(
      matches("class Y { public: void x(); }; void z() {Y* y; y->x(); }",
              memberCallExpr(on(hasType(asString("class Y *"))))));
  EXPECT_TRUE(matches("class X { void x(int x) {} };",
      methodDecl(hasParameter(0, hasType(asString("int"))))));
  EXPECT_TRUE(matches("namespace ns { struct A {}; }  struct B { ns::A a; };",
      fieldDecl(hasType(asString("ns::A")))));
  EXPECT_TRUE(matches("namespace { struct A {}; }  struct B { A a; };",
      fieldDecl(hasType(asString("struct <anonymous>::A")))));
}

TEST(Matcher, OverloadedOperatorCall) {
  StatementMatcher OpCall = operatorCallExpr();
  // Unary operator
  EXPECT_TRUE(matches("class Y { }; "
              "bool operator!(Y x) { return false; }; "
              "Y y; bool c = !y;", OpCall));
  // No match -- special operators like "new", "delete"
  // FIXME: operator new takes size_t, for which we need stddef.h, for which
  // we need to figure out include paths in the test.
  // EXPECT_TRUE(NotMatches("#include <stddef.h>\n"
  //             "class Y { }; "
  //             "void *operator new(size_t size) { return 0; } "
  //             "Y *y = new Y;", OpCall));
  EXPECT_TRUE(notMatches("class Y { }; "
              "void operator delete(void *p) { } "
              "void a() {Y *y = new Y; delete y;}", OpCall));
  // Binary operator
  EXPECT_TRUE(matches("class Y { }; "
              "bool operator&&(Y x, Y y) { return true; }; "
              "Y a; Y b; bool c = a && b;",
              OpCall));
  // No match -- normal operator, not an overloaded one.
  EXPECT_TRUE(notMatches("bool x = true, y = true; bool t = x && y;", OpCall));
  EXPECT_TRUE(notMatches("int t = 5 << 2;", OpCall));
}

TEST(Matcher, HasOperatorNameForOverloadedOperatorCall) {
  StatementMatcher OpCallAndAnd =
      operatorCallExpr(hasOverloadedOperatorName("&&"));
  EXPECT_TRUE(matches("class Y { }; "
              "bool operator&&(Y x, Y y) { return true; }; "
              "Y a; Y b; bool c = a && b;", OpCallAndAnd));
  StatementMatcher OpCallLessLess =
      operatorCallExpr(hasOverloadedOperatorName("<<"));
  EXPECT_TRUE(notMatches("class Y { }; "
              "bool operator&&(Y x, Y y) { return true; }; "
              "Y a; Y b; bool c = a && b;",
              OpCallLessLess));
}

TEST(Matcher, ThisPointerType) {
  StatementMatcher MethodOnY =
    memberCallExpr(thisPointerType(recordDecl(hasName("Y"))));

  EXPECT_TRUE(
      matches("class Y { public: void x(); }; void z() { Y y; y.x(); }",
              MethodOnY));
  EXPECT_TRUE(
      matches("class Y { public: void x(); }; void z(Y &y) { y.x(); }",
              MethodOnY));
  EXPECT_TRUE(
      matches("class Y { public: void x(); }; void z(Y *&y) { y->x(); }",
              MethodOnY));
  EXPECT_TRUE(
      matches("class Y { public: void x(); }; void z(Y y[]) { y->x(); }",
              MethodOnY));
  EXPECT_TRUE(
      matches("class Y { public: void x(); }; void z() { Y *y; y->x(); }",
              MethodOnY));

  EXPECT_TRUE(matches(
      "class Y {"
      "  public: virtual void x();"
      "};"
      "class X : public Y {"
      "  public: virtual void x();"
      "};"
      "void z() { X *x; x->Y::x(); }", MethodOnY));
}

TEST(Matcher, VariableUsage) {
  StatementMatcher Reference =
      declRefExpr(to(
          varDecl(hasInitializer(
              memberCallExpr(thisPointerType(recordDecl(hasName("Y"))))))));

  EXPECT_TRUE(matches(
      "class Y {"
      " public:"
      "  bool x() const;"
      "};"
      "void z(const Y &y) {"
      "  bool b = y.x();"
      "  if (b) {}"
      "}", Reference));

  EXPECT_TRUE(notMatches(
      "class Y {"
      " public:"
      "  bool x() const;"
      "};"
      "void z(const Y &y) {"
      "  bool b = y.x();"
      "}", Reference));
}

TEST(Matcher, FindsVarDeclInFuncitonParameter) {
  EXPECT_TRUE(matches(
      "void f(int i) {}",
      varDecl(hasName("i"))));
}

TEST(Matcher, CalledVariable) {
  StatementMatcher CallOnVariableY = expr(
      memberCallExpr(on(declRefExpr(to(varDecl(hasName("y")))))));

  EXPECT_TRUE(matches(
      "class Y { public: void x() { Y y; y.x(); } };", CallOnVariableY));
  EXPECT_TRUE(matches(
      "class Y { public: void x() const { Y y; y.x(); } };", CallOnVariableY));
  EXPECT_TRUE(matches(
      "class Y { public: void x(); };"
      "class X : public Y { void z() { X y; y.x(); } };", CallOnVariableY));
  EXPECT_TRUE(matches(
      "class Y { public: void x(); };"
      "class X : public Y { void z() { X *y; y->x(); } };", CallOnVariableY));
  EXPECT_TRUE(notMatches(
      "class Y { public: void x(); };"
      "class X : public Y { void z() { unsigned long y; ((X*)y)->x(); } };",
      CallOnVariableY));
}

TEST(UnaryExprOrTypeTraitExpr, MatchesSizeOfAndAlignOf) {
  EXPECT_TRUE(matches("void x() { int a = sizeof(a); }",
                      unaryExprOrTypeTraitExpr()));
  EXPECT_TRUE(notMatches("void x() { int a = sizeof(a); }",
                         alignOfExpr(anything())));
  // FIXME: Uncomment once alignof is enabled.
  // EXPECT_TRUE(matches("void x() { int a = alignof(a); }",
  //                     unaryExprOrTypeTraitExpr()));
  // EXPECT_TRUE(notMatches("void x() { int a = alignof(a); }",
  //                        sizeOfExpr()));
}

TEST(UnaryExpressionOrTypeTraitExpression, MatchesCorrectType) {
  EXPECT_TRUE(matches("void x() { int a = sizeof(a); }", sizeOfExpr(
      hasArgumentOfType(asString("int")))));
  EXPECT_TRUE(notMatches("void x() { int a = sizeof(a); }", sizeOfExpr(
      hasArgumentOfType(asString("float")))));
  EXPECT_TRUE(matches(
      "struct A {}; void x() { A a; int b = sizeof(a); }",
      sizeOfExpr(hasArgumentOfType(hasDeclaration(recordDecl(hasName("A")))))));
  EXPECT_TRUE(notMatches("void x() { int a = sizeof(a); }", sizeOfExpr(
      hasArgumentOfType(hasDeclaration(recordDecl(hasName("string")))))));
}

TEST(MemberExpression, DoesNotMatchClasses) {
  EXPECT_TRUE(notMatches("class Y { void x() {} };", memberExpr()));
}

TEST(MemberExpression, MatchesMemberFunctionCall) {
  EXPECT_TRUE(matches("class Y { void x() { x(); } };", memberExpr()));
}

TEST(MemberExpression, MatchesVariable) {
  EXPECT_TRUE(
      matches("class Y { void x() { this->y; } int y; };", memberExpr()));
  EXPECT_TRUE(
      matches("class Y { void x() { y; } int y; };", memberExpr()));
  EXPECT_TRUE(
      matches("class Y { void x() { Y y; y.y; } int y; };", memberExpr()));
}

TEST(MemberExpression, MatchesStaticVariable) {
  EXPECT_TRUE(matches("class Y { void x() { this->y; } static int y; };",
              memberExpr()));
  EXPECT_TRUE(notMatches("class Y { void x() { y; } static int y; };",
              memberExpr()));
  EXPECT_TRUE(notMatches("class Y { void x() { Y::y; } static int y; };",
              memberExpr()));
}

TEST(IsInteger, MatchesIntegers) {
  EXPECT_TRUE(matches("int i = 0;", varDecl(hasType(isInteger()))));
  EXPECT_TRUE(matches(
      "long long i = 0; void f(long long) { }; void g() {f(i);}",
      callExpr(hasArgument(0, declRefExpr(
                                  to(varDecl(hasType(isInteger()))))))));
}

TEST(IsInteger, ReportsNoFalsePositives) {
  EXPECT_TRUE(notMatches("int *i;", varDecl(hasType(isInteger()))));
  EXPECT_TRUE(notMatches("struct T {}; T t; void f(T *) { }; void g() {f(&t);}",
                      callExpr(hasArgument(0, declRefExpr(
                          to(varDecl(hasType(isInteger()))))))));
}

TEST(IsArrow, MatchesMemberVariablesViaArrow) {
  EXPECT_TRUE(matches("class Y { void x() { this->y; } int y; };",
              memberExpr(isArrow())));
  EXPECT_TRUE(matches("class Y { void x() { y; } int y; };",
              memberExpr(isArrow())));
  EXPECT_TRUE(notMatches("class Y { void x() { (*this).y; } int y; };",
              memberExpr(isArrow())));
}

TEST(IsArrow, MatchesStaticMemberVariablesViaArrow) {
  EXPECT_TRUE(matches("class Y { void x() { this->y; } static int y; };",
              memberExpr(isArrow())));
  EXPECT_TRUE(notMatches("class Y { void x() { y; } static int y; };",
              memberExpr(isArrow())));
  EXPECT_TRUE(notMatches("class Y { void x() { (*this).y; } static int y; };",
              memberExpr(isArrow())));
}

TEST(IsArrow, MatchesMemberCallsViaArrow) {
  EXPECT_TRUE(matches("class Y { void x() { this->x(); } };",
              memberExpr(isArrow())));
  EXPECT_TRUE(matches("class Y { void x() { x(); } };",
              memberExpr(isArrow())));
  EXPECT_TRUE(notMatches("class Y { void x() { Y y; y.x(); } };",
              memberExpr(isArrow())));
}

TEST(Callee, MatchesDeclarations) {
  StatementMatcher CallMethodX = callExpr(callee(methodDecl(hasName("x"))));

  EXPECT_TRUE(matches("class Y { void x() { x(); } };", CallMethodX));
  EXPECT_TRUE(notMatches("class Y { void x() {} };", CallMethodX));
}

TEST(Callee, MatchesMemberExpressions) {
  EXPECT_TRUE(matches("class Y { void x() { this->x(); } };",
              callExpr(callee(memberExpr()))));
  EXPECT_TRUE(
      notMatches("class Y { void x() { this->x(); } };", callExpr(callee(callExpr()))));
}

TEST(Function, MatchesFunctionDeclarations) {
  StatementMatcher CallFunctionF = callExpr(callee(functionDecl(hasName("f"))));

  EXPECT_TRUE(matches("void f() { f(); }", CallFunctionF));
  EXPECT_TRUE(notMatches("void f() { }", CallFunctionF));

#if !defined(_MSC_VER)
  // FIXME: Make this work for MSVC.
  // Dependent contexts, but a non-dependent call.
  EXPECT_TRUE(matches("void f(); template <int N> void g() { f(); }",
                      CallFunctionF));
  EXPECT_TRUE(
      matches("void f(); template <int N> struct S { void g() { f(); } };",
              CallFunctionF));
#endif

  // Depedent calls don't match.
  EXPECT_TRUE(
      notMatches("void f(int); template <typename T> void g(T t) { f(t); }",
                 CallFunctionF));
  EXPECT_TRUE(
      notMatches("void f(int);"
                 "template <typename T> struct S { void g(T t) { f(t); } };",
                 CallFunctionF));
}

TEST(FunctionTemplate, MatchesFunctionTemplateDeclarations) {
  EXPECT_TRUE(
      matches("template <typename T> void f(T t) {}",
      functionTemplateDecl(hasName("f"))));
}

TEST(FunctionTemplate, DoesNotMatchFunctionDeclarations) {
  EXPECT_TRUE(
      notMatches("void f(double d); void f(int t) {}",
      functionTemplateDecl(hasName("f"))));
}

TEST(FunctionTemplate, DoesNotMatchFunctionTemplateSpecializations) {
  EXPECT_TRUE(
      notMatches("void g(); template <typename T> void f(T t) {}"
                 "template <> void f(int t) { g(); }",
      functionTemplateDecl(hasName("f"),
                           hasDescendant(declRefExpr(to(
                               functionDecl(hasName("g"))))))));
}

TEST(Matcher, Argument) {
  StatementMatcher CallArgumentY = expr(callExpr(
      hasArgument(0, declRefExpr(to(varDecl(hasName("y")))))));

  EXPECT_TRUE(matches("void x(int) { int y; x(y); }", CallArgumentY));
  EXPECT_TRUE(
      matches("class X { void x(int) { int y; x(y); } };", CallArgumentY));
  EXPECT_TRUE(notMatches("void x(int) { int z; x(z); }", CallArgumentY));

  StatementMatcher WrongIndex = expr(callExpr(
      hasArgument(42, declRefExpr(to(varDecl(hasName("y")))))));
  EXPECT_TRUE(notMatches("void x(int) { int y; x(y); }", WrongIndex));
}

TEST(Matcher, AnyArgument) {
  StatementMatcher CallArgumentY = expr(callExpr(
      hasAnyArgument(declRefExpr(to(varDecl(hasName("y")))))));
  EXPECT_TRUE(matches("void x(int, int) { int y; x(1, y); }", CallArgumentY));
  EXPECT_TRUE(matches("void x(int, int) { int y; x(y, 42); }", CallArgumentY));
  EXPECT_TRUE(notMatches("void x(int, int) { x(1, 2); }", CallArgumentY));
}

TEST(Matcher, ArgumentCount) {
  StatementMatcher Call1Arg = expr(callExpr(argumentCountIs(1)));

  EXPECT_TRUE(matches("void x(int) { x(0); }", Call1Arg));
  EXPECT_TRUE(matches("class X { void x(int) { x(0); } };", Call1Arg));
  EXPECT_TRUE(notMatches("void x(int, int) { x(0, 0); }", Call1Arg));
}

TEST(Matcher, References) {
  DeclarationMatcher ReferenceClassX = varDecl(
      hasType(references(recordDecl(hasName("X")))));
  EXPECT_TRUE(matches("class X {}; void y(X y) { X &x = y; }",
                      ReferenceClassX));
  EXPECT_TRUE(
      matches("class X {}; void y(X y) { const X &x = y; }", ReferenceClassX));
  EXPECT_TRUE(
      notMatches("class X {}; void y(X y) { X x = y; }", ReferenceClassX));
  EXPECT_TRUE(
      notMatches("class X {}; void y(X *y) { X *&x = y; }", ReferenceClassX));
}

TEST(HasParameter, CallsInnerMatcher) {
  EXPECT_TRUE(matches("class X { void x(int) {} };",
      methodDecl(hasParameter(0, varDecl()))));
  EXPECT_TRUE(notMatches("class X { void x(int) {} };",
      methodDecl(hasParameter(0, hasName("x")))));
}

TEST(HasParameter, DoesNotMatchIfIndexOutOfBounds) {
  EXPECT_TRUE(notMatches("class X { void x(int) {} };",
      methodDecl(hasParameter(42, varDecl()))));
}

TEST(HasType, MatchesParameterVariableTypesStrictly) {
  EXPECT_TRUE(matches("class X { void x(X x) {} };",
      methodDecl(hasParameter(0, hasType(recordDecl(hasName("X")))))));
  EXPECT_TRUE(notMatches("class X { void x(const X &x) {} };",
      methodDecl(hasParameter(0, hasType(recordDecl(hasName("X")))))));
  EXPECT_TRUE(matches("class X { void x(const X *x) {} };",
      methodDecl(hasParameter(0, 
                              hasType(pointsTo(recordDecl(hasName("X"))))))));
  EXPECT_TRUE(matches("class X { void x(const X &x) {} };",
      methodDecl(hasParameter(0,
                              hasType(references(recordDecl(hasName("X"))))))));
}

TEST(HasAnyParameter, MatchesIndependentlyOfPosition) {
  EXPECT_TRUE(matches("class Y {}; class X { void x(X x, Y y) {} };",
      methodDecl(hasAnyParameter(hasType(recordDecl(hasName("X")))))));
  EXPECT_TRUE(matches("class Y {}; class X { void x(Y y, X x) {} };",
      methodDecl(hasAnyParameter(hasType(recordDecl(hasName("X")))))));
}

TEST(Returns, MatchesReturnTypes) {
  EXPECT_TRUE(matches("class Y { int f() { return 1; } };",
                      functionDecl(returns(asString("int")))));
  EXPECT_TRUE(notMatches("class Y { int f() { return 1; } };",
                         functionDecl(returns(asString("float")))));
  EXPECT_TRUE(matches("class Y { Y getMe() { return *this; } };",
                      functionDecl(returns(hasDeclaration(
                          recordDecl(hasName("Y")))))));
}

TEST(IsExternC, MatchesExternCFunctionDeclarations) {
  EXPECT_TRUE(matches("extern \"C\" void f() {}", functionDecl(isExternC())));
  EXPECT_TRUE(matches("extern \"C\" { void f() {} }",
              functionDecl(isExternC())));
  EXPECT_TRUE(notMatches("void f() {}", functionDecl(isExternC())));
}

TEST(HasAnyParameter, DoesntMatchIfInnerMatcherDoesntMatch) {
  EXPECT_TRUE(notMatches("class Y {}; class X { void x(int) {} };",
      methodDecl(hasAnyParameter(hasType(recordDecl(hasName("X")))))));
}

TEST(HasAnyParameter, DoesNotMatchThisPointer) {
  EXPECT_TRUE(notMatches("class Y {}; class X { void x() {} };",
      methodDecl(hasAnyParameter(hasType(pointsTo(
          recordDecl(hasName("X"))))))));
}

TEST(HasName, MatchesParameterVariableDeclartions) {
  EXPECT_TRUE(matches("class Y {}; class X { void x(int x) {} };",
      methodDecl(hasAnyParameter(hasName("x")))));
  EXPECT_TRUE(notMatches("class Y {}; class X { void x(int) {} };",
      methodDecl(hasAnyParameter(hasName("x")))));
}

TEST(Matcher, MatchesClassTemplateSpecialization) {
  EXPECT_TRUE(matches("template<typename T> struct A {};"
                      "template<> struct A<int> {};",
                      classTemplateSpecializationDecl()));
  EXPECT_TRUE(matches("template<typename T> struct A {}; A<int> a;",
                      classTemplateSpecializationDecl()));
  EXPECT_TRUE(notMatches("template<typename T> struct A {};",
                         classTemplateSpecializationDecl()));
}

TEST(Matcher, MatchesTypeTemplateArgument) {
  EXPECT_TRUE(matches(
      "template<typename T> struct B {};"
      "B<int> b;",
      classTemplateSpecializationDecl(hasAnyTemplateArgument(refersToType(
          asString("int"))))));
}

TEST(Matcher, MatchesDeclarationReferenceTemplateArgument) {
  EXPECT_TRUE(matches(
      "struct B { int next; };"
      "template<int(B::*next_ptr)> struct A {};"
      "A<&B::next> a;",
      classTemplateSpecializationDecl(hasAnyTemplateArgument(
          refersToDeclaration(fieldDecl(hasName("next")))))));
}

TEST(Matcher, MatchesSpecificArgument) {
  EXPECT_TRUE(matches(
      "template<typename T, typename U> class A {};"
      "A<bool, int> a;",
      classTemplateSpecializationDecl(hasTemplateArgument(
          1, refersToType(asString("int"))))));
  EXPECT_TRUE(notMatches(
      "template<typename T, typename U> class A {};"
      "A<int, bool> a;",
      classTemplateSpecializationDecl(hasTemplateArgument(
          1, refersToType(asString("int"))))));
}

TEST(Matcher, ConstructorCall) {
  StatementMatcher Constructor = expr(constructExpr());

  EXPECT_TRUE(
      matches("class X { public: X(); }; void x() { X x; }", Constructor));
  EXPECT_TRUE(
      matches("class X { public: X(); }; void x() { X x = X(); }",
              Constructor));
  EXPECT_TRUE(
      matches("class X { public: X(int); }; void x() { X x = 0; }",
              Constructor));
  EXPECT_TRUE(matches("class X {}; void x(int) { X x; }", Constructor));
}

TEST(Matcher, ConstructorArgument) {
  StatementMatcher Constructor = expr(constructExpr(
      hasArgument(0, declRefExpr(to(varDecl(hasName("y")))))));

  EXPECT_TRUE(
      matches("class X { public: X(int); }; void x() { int y; X x(y); }",
              Constructor));
  EXPECT_TRUE(
      matches("class X { public: X(int); }; void x() { int y; X x = X(y); }",
              Constructor));
  EXPECT_TRUE(
      matches("class X { public: X(int); }; void x() { int y; X x = y; }",
              Constructor));
  EXPECT_TRUE(
      notMatches("class X { public: X(int); }; void x() { int z; X x(z); }",
                 Constructor));

  StatementMatcher WrongIndex = expr(constructExpr(
      hasArgument(42, declRefExpr(to(varDecl(hasName("y")))))));
  EXPECT_TRUE(
      notMatches("class X { public: X(int); }; void x() { int y; X x(y); }",
                 WrongIndex));
}

TEST(Matcher, ConstructorArgumentCount) {
  StatementMatcher Constructor1Arg =
      expr(constructExpr(argumentCountIs(1)));

  EXPECT_TRUE(
      matches("class X { public: X(int); }; void x() { X x(0); }",
              Constructor1Arg));
  EXPECT_TRUE(
      matches("class X { public: X(int); }; void x() { X x = X(0); }",
              Constructor1Arg));
  EXPECT_TRUE(
      matches("class X { public: X(int); }; void x() { X x = 0; }",
              Constructor1Arg));
  EXPECT_TRUE(
      notMatches("class X { public: X(int, int); }; void x() { X x(0, 0); }",
                 Constructor1Arg));
}

TEST(Matcher, BindTemporaryExpression) {
  StatementMatcher TempExpression = expr(bindTemporaryExpr());

  std::string ClassString = "class string { public: string(); ~string(); }; ";

  EXPECT_TRUE(
      matches(ClassString +
              "string GetStringByValue();"
              "void FunctionTakesString(string s);"
              "void run() { FunctionTakesString(GetStringByValue()); }",
              TempExpression));

  EXPECT_TRUE(
      notMatches(ClassString +
                 "string* GetStringPointer(); "
                 "void FunctionTakesStringPtr(string* s);"
                 "void run() {"
                 "  string* s = GetStringPointer();"
                 "  FunctionTakesStringPtr(GetStringPointer());"
                 "  FunctionTakesStringPtr(s);"
                 "}",
                 TempExpression));

  EXPECT_TRUE(
      notMatches("class no_dtor {};"
                 "no_dtor GetObjByValue();"
                 "void ConsumeObj(no_dtor param);"
                 "void run() { ConsumeObj(GetObjByValue()); }",
                 TempExpression));
}

TEST(MaterializeTemporaryExpr, MatchesTemporary) {
  std::string ClassString =
      "class string { public: string(); int length(); }; ";

  EXPECT_TRUE(
      matches(ClassString +
              "string GetStringByValue();"
              "void FunctionTakesString(string s);"
              "void run() { FunctionTakesString(GetStringByValue()); }",
              materializeTemporaryExpr()));

  EXPECT_TRUE(
      notMatches(ClassString +
                 "string* GetStringPointer(); "
                 "void FunctionTakesStringPtr(string* s);"
                 "void run() {"
                 "  string* s = GetStringPointer();"
                 "  FunctionTakesStringPtr(GetStringPointer());"
                 "  FunctionTakesStringPtr(s);"
                 "}",
                 materializeTemporaryExpr()));

  EXPECT_TRUE(
      notMatches(ClassString +
                 "string GetStringByValue();"
                 "void run() { int k = GetStringByValue().length(); }",
                 materializeTemporaryExpr()));

  EXPECT_TRUE(
      notMatches(ClassString +
                 "string GetStringByValue();"
                 "void run() { GetStringByValue(); }",
                 materializeTemporaryExpr()));
}

TEST(ConstructorDeclaration, SimpleCase) {
  EXPECT_TRUE(matches("class Foo { Foo(int i); };",
                      constructorDecl(ofClass(hasName("Foo")))));
  EXPECT_TRUE(notMatches("class Foo { Foo(int i); };",
                         constructorDecl(ofClass(hasName("Bar")))));
}

TEST(ConstructorDeclaration, IsImplicit) {
  // This one doesn't match because the constructor is not added by the
  // compiler (it is not needed).
  EXPECT_TRUE(notMatches("class Foo { };",
                         constructorDecl(isImplicit())));
  // The compiler added the implicit default constructor.
  EXPECT_TRUE(matches("class Foo { }; Foo* f = new Foo();",
                      constructorDecl(isImplicit())));
  EXPECT_TRUE(matches("class Foo { Foo(){} };",
                      constructorDecl(unless(isImplicit()))));
}

TEST(DestructorDeclaration, MatchesVirtualDestructor) {
  EXPECT_TRUE(matches("class Foo { virtual ~Foo(); };",
                      destructorDecl(ofClass(hasName("Foo")))));
}

TEST(DestructorDeclaration, DoesNotMatchImplicitDestructor) {
  EXPECT_TRUE(notMatches("class Foo {};",
                         destructorDecl(ofClass(hasName("Foo")))));
}

TEST(HasAnyConstructorInitializer, SimpleCase) {
  EXPECT_TRUE(notMatches(
      "class Foo { Foo() { } };",
      constructorDecl(hasAnyConstructorInitializer(anything()))));
  EXPECT_TRUE(matches(
      "class Foo {"
      "  Foo() : foo_() { }"
      "  int foo_;"
      "};",
      constructorDecl(hasAnyConstructorInitializer(anything()))));
}

TEST(HasAnyConstructorInitializer, ForField) {
  static const char Code[] =
      "class Baz { };"
      "class Foo {"
      "  Foo() : foo_() { }"
      "  Baz foo_;"
      "  Baz bar_;"
      "};";
  EXPECT_TRUE(matches(Code, constructorDecl(hasAnyConstructorInitializer(
      forField(hasType(recordDecl(hasName("Baz"))))))));
  EXPECT_TRUE(matches(Code, constructorDecl(hasAnyConstructorInitializer(
      forField(hasName("foo_"))))));
  EXPECT_TRUE(notMatches(Code, constructorDecl(hasAnyConstructorInitializer(
      forField(hasType(recordDecl(hasName("Bar"))))))));
}

TEST(HasAnyConstructorInitializer, WithInitializer) {
  static const char Code[] =
      "class Foo {"
      "  Foo() : foo_(0) { }"
      "  int foo_;"
      "};";
  EXPECT_TRUE(matches(Code, constructorDecl(hasAnyConstructorInitializer(
      withInitializer(integerLiteral(equals(0)))))));
  EXPECT_TRUE(notMatches(Code, constructorDecl(hasAnyConstructorInitializer(
      withInitializer(integerLiteral(equals(1)))))));
}

TEST(HasAnyConstructorInitializer, IsWritten) {
  static const char Code[] =
      "struct Bar { Bar(){} };"
      "class Foo {"
      "  Foo() : foo_() { }"
      "  Bar foo_;"
      "  Bar bar_;"
      "};";
  EXPECT_TRUE(matches(Code, constructorDecl(hasAnyConstructorInitializer(
      allOf(forField(hasName("foo_")), isWritten())))));
  EXPECT_TRUE(notMatches(Code, constructorDecl(hasAnyConstructorInitializer(
      allOf(forField(hasName("bar_")), isWritten())))));
  EXPECT_TRUE(matches(Code, constructorDecl(hasAnyConstructorInitializer(
      allOf(forField(hasName("bar_")), unless(isWritten()))))));
}

TEST(Matcher, NewExpression) {
  StatementMatcher New = expr(newExpr());

  EXPECT_TRUE(matches("class X { public: X(); }; void x() { new X; }", New));
  EXPECT_TRUE(
      matches("class X { public: X(); }; void x() { new X(); }", New));
  EXPECT_TRUE(
      matches("class X { public: X(int); }; void x() { new X(0); }", New));
  EXPECT_TRUE(matches("class X {}; void x(int) { new X; }", New));
}

TEST(Matcher, NewExpressionArgument) {
  StatementMatcher New = expr(constructExpr(
      hasArgument(0, declRefExpr(to(varDecl(hasName("y")))))));

  EXPECT_TRUE(
      matches("class X { public: X(int); }; void x() { int y; new X(y); }",
              New));
  EXPECT_TRUE(
      matches("class X { public: X(int); }; void x() { int y; new X(y); }",
              New));
  EXPECT_TRUE(
      notMatches("class X { public: X(int); }; void x() { int z; new X(z); }",
                 New));

  StatementMatcher WrongIndex = expr(constructExpr(
      hasArgument(42, declRefExpr(to(varDecl(hasName("y")))))));
  EXPECT_TRUE(
      notMatches("class X { public: X(int); }; void x() { int y; new X(y); }",
                 WrongIndex));
}

TEST(Matcher, NewExpressionArgumentCount) {
  StatementMatcher New = constructExpr(argumentCountIs(1));

  EXPECT_TRUE(
      matches("class X { public: X(int); }; void x() { new X(0); }", New));
  EXPECT_TRUE(
      notMatches("class X { public: X(int, int); }; void x() { new X(0, 0); }",
                 New));
}

TEST(Matcher, DeleteExpression) {
  EXPECT_TRUE(matches("struct A {}; void f(A* a) { delete a; }",
                      deleteExpr()));
}

TEST(Matcher, DefaultArgument) {
  StatementMatcher Arg = defaultArgExpr();

  EXPECT_TRUE(matches("void x(int, int = 0) { int y; x(y); }", Arg));
  EXPECT_TRUE(
      matches("class X { void x(int, int = 0) { int y; x(y); } };", Arg));
  EXPECT_TRUE(notMatches("void x(int, int = 0) { int y; x(y, 0); }", Arg));
}

TEST(Matcher, StringLiterals) {
  StatementMatcher Literal = expr(stringLiteral());
  EXPECT_TRUE(matches("const char *s = \"string\";", Literal));
  // wide string
  EXPECT_TRUE(matches("const wchar_t *s = L\"string\";", Literal));
  // with escaped characters
  EXPECT_TRUE(matches("const char *s = \"\x05five\";", Literal));
  // no matching -- though the data type is the same, there is no string literal
  EXPECT_TRUE(notMatches("const char s[1] = {'a'};", Literal));
}

TEST(Matcher, CharacterLiterals) {
  StatementMatcher CharLiteral = expr(characterLiteral());
  EXPECT_TRUE(matches("const char c = 'c';", CharLiteral));
  // wide character
  EXPECT_TRUE(matches("const char c = L'c';", CharLiteral));
  // wide character, Hex encoded, NOT MATCHED!
  EXPECT_TRUE(notMatches("const wchar_t c = 0x2126;", CharLiteral));
  EXPECT_TRUE(notMatches("const char c = 0x1;", CharLiteral));
}

TEST(Matcher, IntegerLiterals) {
  StatementMatcher HasIntLiteral = expr(integerLiteral());
  EXPECT_TRUE(matches("int i = 10;", HasIntLiteral));
  EXPECT_TRUE(matches("int i = 0x1AB;", HasIntLiteral));
  EXPECT_TRUE(matches("int i = 10L;", HasIntLiteral));
  EXPECT_TRUE(matches("int i = 10U;", HasIntLiteral));

  // Non-matching cases (character literals, float and double)
  EXPECT_TRUE(notMatches("int i = L'a';",
                HasIntLiteral));  // this is actually a character
                                  // literal cast to int
  EXPECT_TRUE(notMatches("int i = 'a';", HasIntLiteral));
  EXPECT_TRUE(notMatches("int i = 1e10;", HasIntLiteral));
  EXPECT_TRUE(notMatches("int i = 10.0;", HasIntLiteral));
}

TEST(Matcher, Conditions) {
  StatementMatcher Condition = ifStmt(hasCondition(boolLiteral(equals(true))));

  EXPECT_TRUE(matches("void x() { if (true) {} }", Condition));
  EXPECT_TRUE(notMatches("void x() { if (false) {} }", Condition));
  EXPECT_TRUE(notMatches("void x() { bool a = true; if (a) {} }", Condition));
  EXPECT_TRUE(notMatches("void x() { if (true || false) {} }", Condition));
  EXPECT_TRUE(notMatches("void x() { if (1) {} }", Condition));
}

TEST(MatchBinaryOperator, HasOperatorName) {
  StatementMatcher OperatorOr = binaryOperator(hasOperatorName("||"));

  EXPECT_TRUE(matches("void x() { true || false; }", OperatorOr));
  EXPECT_TRUE(notMatches("void x() { true && false; }", OperatorOr));
}

TEST(MatchBinaryOperator, HasLHSAndHasRHS) {
  StatementMatcher OperatorTrueFalse =
      binaryOperator(hasLHS(boolLiteral(equals(true))),
                     hasRHS(boolLiteral(equals(false))));

  EXPECT_TRUE(matches("void x() { true || false; }", OperatorTrueFalse));
  EXPECT_TRUE(matches("void x() { true && false; }", OperatorTrueFalse));
  EXPECT_TRUE(notMatches("void x() { false || true; }", OperatorTrueFalse));
}

TEST(MatchBinaryOperator, HasEitherOperand) {
  StatementMatcher HasOperand =
      binaryOperator(hasEitherOperand(boolLiteral(equals(false))));

  EXPECT_TRUE(matches("void x() { true || false; }", HasOperand));
  EXPECT_TRUE(matches("void x() { false && true; }", HasOperand));
  EXPECT_TRUE(notMatches("void x() { true || true; }", HasOperand));
}

TEST(Matcher, BinaryOperatorTypes) {
  // Integration test that verifies the AST provides all binary operators in
  // a way we expect.
  // FIXME: Operator ','
  EXPECT_TRUE(
      matches("void x() { 3, 4; }", binaryOperator(hasOperatorName(","))));
  EXPECT_TRUE(
      matches("bool b; bool c = (b = true);",
              binaryOperator(hasOperatorName("="))));
  EXPECT_TRUE(
      matches("bool b = 1 != 2;", binaryOperator(hasOperatorName("!="))));
  EXPECT_TRUE(
      matches("bool b = 1 == 2;", binaryOperator(hasOperatorName("=="))));
  EXPECT_TRUE(matches("bool b = 1 < 2;", binaryOperator(hasOperatorName("<"))));
  EXPECT_TRUE(
      matches("bool b = 1 <= 2;", binaryOperator(hasOperatorName("<="))));
  EXPECT_TRUE(
      matches("int i = 1 << 2;", binaryOperator(hasOperatorName("<<"))));
  EXPECT_TRUE(
      matches("int i = 1; int j = (i <<= 2);",
              binaryOperator(hasOperatorName("<<="))));
  EXPECT_TRUE(matches("bool b = 1 > 2;", binaryOperator(hasOperatorName(">"))));
  EXPECT_TRUE(
      matches("bool b = 1 >= 2;", binaryOperator(hasOperatorName(">="))));
  EXPECT_TRUE(
      matches("int i = 1 >> 2;", binaryOperator(hasOperatorName(">>"))));
  EXPECT_TRUE(
      matches("int i = 1; int j = (i >>= 2);",
              binaryOperator(hasOperatorName(">>="))));
  EXPECT_TRUE(
      matches("int i = 42 ^ 23;", binaryOperator(hasOperatorName("^"))));
  EXPECT_TRUE(
      matches("int i = 42; int j = (i ^= 42);",
              binaryOperator(hasOperatorName("^="))));
  EXPECT_TRUE(
      matches("int i = 42 % 23;", binaryOperator(hasOperatorName("%"))));
  EXPECT_TRUE(
      matches("int i = 42; int j = (i %= 42);",
              binaryOperator(hasOperatorName("%="))));
  EXPECT_TRUE(
      matches("bool b = 42  &23;", binaryOperator(hasOperatorName("&"))));
  EXPECT_TRUE(
      matches("bool b = true && false;",
              binaryOperator(hasOperatorName("&&"))));
  EXPECT_TRUE(
      matches("bool b = true; bool c = (b &= false);",
              binaryOperator(hasOperatorName("&="))));
  EXPECT_TRUE(
      matches("bool b = 42 | 23;", binaryOperator(hasOperatorName("|"))));
  EXPECT_TRUE(
      matches("bool b = true || false;",
              binaryOperator(hasOperatorName("||"))));
  EXPECT_TRUE(
      matches("bool b = true; bool c = (b |= false);",
              binaryOperator(hasOperatorName("|="))));
  EXPECT_TRUE(
      matches("int i = 42  *23;", binaryOperator(hasOperatorName("*"))));
  EXPECT_TRUE(
      matches("int i = 42; int j = (i *= 23);",
              binaryOperator(hasOperatorName("*="))));
  EXPECT_TRUE(
      matches("int i = 42 / 23;", binaryOperator(hasOperatorName("/"))));
  EXPECT_TRUE(
      matches("int i = 42; int j = (i /= 23);",
              binaryOperator(hasOperatorName("/="))));
  EXPECT_TRUE(
      matches("int i = 42 + 23;", binaryOperator(hasOperatorName("+"))));
  EXPECT_TRUE(
      matches("int i = 42; int j = (i += 23);",
              binaryOperator(hasOperatorName("+="))));
  EXPECT_TRUE(
      matches("int i = 42 - 23;", binaryOperator(hasOperatorName("-"))));
  EXPECT_TRUE(
      matches("int i = 42; int j = (i -= 23);",
              binaryOperator(hasOperatorName("-="))));
  EXPECT_TRUE(
      matches("struct A { void x() { void (A::*a)(); (this->*a)(); } };",
              binaryOperator(hasOperatorName("->*"))));
  EXPECT_TRUE(
      matches("struct A { void x() { void (A::*a)(); ((*this).*a)(); } };",
              binaryOperator(hasOperatorName(".*"))));

  // Member expressions as operators are not supported in matches.
  EXPECT_TRUE(
      notMatches("struct A { void x(A *a) { a->x(this); } };",
                 binaryOperator(hasOperatorName("->"))));

  // Initializer assignments are not represented as operator equals.
  EXPECT_TRUE(
      notMatches("bool b = true;", binaryOperator(hasOperatorName("="))));

  // Array indexing is not represented as operator.
  EXPECT_TRUE(notMatches("int a[42]; void x() { a[23]; }", unaryOperator()));

  // Overloaded operators do not match at all.
  EXPECT_TRUE(notMatches(
      "struct A { bool operator&&(const A &a) const { return false; } };"
      "void x() { A a, b; a && b; }",
      binaryOperator()));
}

TEST(MatchUnaryOperator, HasOperatorName) {
  StatementMatcher OperatorNot = unaryOperator(hasOperatorName("!"));

  EXPECT_TRUE(matches("void x() { !true; } ", OperatorNot));
  EXPECT_TRUE(notMatches("void x() { true; } ", OperatorNot));
}

TEST(MatchUnaryOperator, HasUnaryOperand) {
  StatementMatcher OperatorOnFalse =
      unaryOperator(hasUnaryOperand(boolLiteral(equals(false))));

  EXPECT_TRUE(matches("void x() { !false; }", OperatorOnFalse));
  EXPECT_TRUE(notMatches("void x() { !true; }", OperatorOnFalse));
}

TEST(Matcher, UnaryOperatorTypes) {
  // Integration test that verifies the AST provides all unary operators in
  // a way we expect.
  EXPECT_TRUE(matches("bool b = !true;", unaryOperator(hasOperatorName("!"))));
  EXPECT_TRUE(
      matches("bool b; bool *p = &b;", unaryOperator(hasOperatorName("&"))));
  EXPECT_TRUE(matches("int i = ~ 1;", unaryOperator(hasOperatorName("~"))));
  EXPECT_TRUE(
      matches("bool *p; bool b = *p;", unaryOperator(hasOperatorName("*"))));
  EXPECT_TRUE(
      matches("int i; int j = +i;", unaryOperator(hasOperatorName("+"))));
  EXPECT_TRUE(
      matches("int i; int j = -i;", unaryOperator(hasOperatorName("-"))));
  EXPECT_TRUE(
      matches("int i; int j = ++i;", unaryOperator(hasOperatorName("++"))));
  EXPECT_TRUE(
      matches("int i; int j = i++;", unaryOperator(hasOperatorName("++"))));
  EXPECT_TRUE(
      matches("int i; int j = --i;", unaryOperator(hasOperatorName("--"))));
  EXPECT_TRUE(
      matches("int i; int j = i--;", unaryOperator(hasOperatorName("--"))));

  // We don't match conversion operators.
  EXPECT_TRUE(notMatches("int i; double d = (double)i;", unaryOperator()));

  // Function calls are not represented as operator.
  EXPECT_TRUE(notMatches("void f(); void x() { f(); }", unaryOperator()));

  // Overloaded operators do not match at all.
  // FIXME: We probably want to add that.
  EXPECT_TRUE(notMatches(
      "struct A { bool operator!() const { return false; } };"
      "void x() { A a; !a; }", unaryOperator(hasOperatorName("!"))));
}

TEST(Matcher, ConditionalOperator) {
  StatementMatcher Conditional = conditionalOperator(
      hasCondition(boolLiteral(equals(true))),
      hasTrueExpression(boolLiteral(equals(false))));

  EXPECT_TRUE(matches("void x() { true ? false : true; }", Conditional));
  EXPECT_TRUE(notMatches("void x() { false ? false : true; }", Conditional));
  EXPECT_TRUE(notMatches("void x() { true ? true : false; }", Conditional));

  StatementMatcher ConditionalFalse = conditionalOperator(
      hasFalseExpression(boolLiteral(equals(false))));

  EXPECT_TRUE(matches("void x() { true ? true : false; }", ConditionalFalse));
  EXPECT_TRUE(
      notMatches("void x() { true ? false : true; }", ConditionalFalse));
}

TEST(ArraySubscriptMatchers, ArraySubscripts) {
  EXPECT_TRUE(matches("int i[2]; void f() { i[1] = 1; }",
                      arraySubscriptExpr()));
  EXPECT_TRUE(notMatches("int i; void f() { i = 1; }",
                         arraySubscriptExpr()));
}

TEST(ArraySubscriptMatchers, ArrayIndex) {
  EXPECT_TRUE(matches(
      "int i[2]; void f() { i[1] = 1; }",
      arraySubscriptExpr(hasIndex(integerLiteral(equals(1))))));
  EXPECT_TRUE(matches(
      "int i[2]; void f() { 1[i] = 1; }",
      arraySubscriptExpr(hasIndex(integerLiteral(equals(1))))));
  EXPECT_TRUE(notMatches(
      "int i[2]; void f() { i[1] = 1; }",
      arraySubscriptExpr(hasIndex(integerLiteral(equals(0))))));
}

TEST(ArraySubscriptMatchers, MatchesArrayBase) {
  EXPECT_TRUE(matches(
      "int i[2]; void f() { i[1] = 2; }",
      arraySubscriptExpr(hasBase(implicitCastExpr(
          hasSourceExpression(declRefExpr()))))));
}

TEST(Matcher, HasNameSupportsNamespaces) {
  EXPECT_TRUE(matches("namespace a { namespace b { class C; } }",
              recordDecl(hasName("a::b::C"))));
  EXPECT_TRUE(matches("namespace a { namespace b { class C; } }",
              recordDecl(hasName("::a::b::C"))));
  EXPECT_TRUE(matches("namespace a { namespace b { class C; } }",
              recordDecl(hasName("b::C"))));
  EXPECT_TRUE(matches("namespace a { namespace b { class C; } }",
              recordDecl(hasName("C"))));
  EXPECT_TRUE(notMatches("namespace a { namespace b { class C; } }",
              recordDecl(hasName("c::b::C"))));
  EXPECT_TRUE(notMatches("namespace a { namespace b { class C; } }",
              recordDecl(hasName("a::c::C"))));
  EXPECT_TRUE(notMatches("namespace a { namespace b { class C; } }",
              recordDecl(hasName("a::b::A"))));
  EXPECT_TRUE(notMatches("namespace a { namespace b { class C; } }",
              recordDecl(hasName("::C"))));
  EXPECT_TRUE(notMatches("namespace a { namespace b { class C; } }",
              recordDecl(hasName("::b::C"))));
  EXPECT_TRUE(notMatches("namespace a { namespace b { class C; } }",
              recordDecl(hasName("z::a::b::C"))));
  EXPECT_TRUE(notMatches("namespace a { namespace b { class C; } }",
              recordDecl(hasName("a+b::C"))));
  EXPECT_TRUE(notMatches("namespace a { namespace b { class AC; } }",
              recordDecl(hasName("C"))));
}

TEST(Matcher, HasNameSupportsOuterClasses) {
  EXPECT_TRUE(
      matches("class A { class B { class C; }; };",
              recordDecl(hasName("A::B::C"))));
  EXPECT_TRUE(
      matches("class A { class B { class C; }; };",
              recordDecl(hasName("::A::B::C"))));
  EXPECT_TRUE(
      matches("class A { class B { class C; }; };",
              recordDecl(hasName("B::C"))));
  EXPECT_TRUE(
      matches("class A { class B { class C; }; };",
              recordDecl(hasName("C"))));
  EXPECT_TRUE(
      notMatches("class A { class B { class C; }; };",
                 recordDecl(hasName("c::B::C"))));
  EXPECT_TRUE(
      notMatches("class A { class B { class C; }; };",
                 recordDecl(hasName("A::c::C"))));
  EXPECT_TRUE(
      notMatches("class A { class B { class C; }; };",
                 recordDecl(hasName("A::B::A"))));
  EXPECT_TRUE(
      notMatches("class A { class B { class C; }; };",
                 recordDecl(hasName("::C"))));
  EXPECT_TRUE(
      notMatches("class A { class B { class C; }; };",
                 recordDecl(hasName("::B::C"))));
  EXPECT_TRUE(notMatches("class A { class B { class C; }; };",
              recordDecl(hasName("z::A::B::C"))));
  EXPECT_TRUE(
      notMatches("class A { class B { class C; }; };",
                 recordDecl(hasName("A+B::C"))));
}

TEST(Matcher, IsDefinition) {
  DeclarationMatcher DefinitionOfClassA =
      recordDecl(hasName("A"), isDefinition());
  EXPECT_TRUE(matches("class A {};", DefinitionOfClassA));
  EXPECT_TRUE(notMatches("class A;", DefinitionOfClassA));

  DeclarationMatcher DefinitionOfVariableA =
      varDecl(hasName("a"), isDefinition());
  EXPECT_TRUE(matches("int a;", DefinitionOfVariableA));
  EXPECT_TRUE(notMatches("extern int a;", DefinitionOfVariableA));

  DeclarationMatcher DefinitionOfMethodA =
      methodDecl(hasName("a"), isDefinition());
  EXPECT_TRUE(matches("class A { void a() {} };", DefinitionOfMethodA));
  EXPECT_TRUE(notMatches("class A { void a(); };", DefinitionOfMethodA));
}

TEST(Matcher, OfClass) {
  StatementMatcher Constructor = constructExpr(hasDeclaration(methodDecl(
      ofClass(hasName("X")))));

  EXPECT_TRUE(
      matches("class X { public: X(); }; void x(int) { X x; }", Constructor));
  EXPECT_TRUE(
      matches("class X { public: X(); }; void x(int) { X x = X(); }",
              Constructor));
  EXPECT_TRUE(
      notMatches("class Y { public: Y(); }; void x(int) { Y y; }",
                 Constructor));
}

TEST(Matcher, VisitsTemplateInstantiations) {
  EXPECT_TRUE(matches(
      "class A { public: void x(); };"
      "template <typename T> class B { public: void y() { T t; t.x(); } };"
      "void f() { B<A> b; b.y(); }",
      callExpr(callee(methodDecl(hasName("x"))))));

  EXPECT_TRUE(matches(
      "class A { public: void x(); };"
      "class C {"
      " public:"
      "  template <typename T> class B { public: void y() { T t; t.x(); } };"
      "};"
      "void f() {"
      "  C::B<A> b; b.y();"
      "}",
      recordDecl(hasName("C"),
                 hasDescendant(callExpr(callee(methodDecl(hasName("x"))))))));
}

TEST(Matcher, HandlesNullQualTypes) {
  // FIXME: Add a Type matcher so we can replace uses of this
  // variable with Type(True())
  const TypeMatcher AnyType = anything();

  // We don't really care whether this matcher succeeds; we're testing that
  // it completes without crashing.
  EXPECT_TRUE(matches(
      "struct A { };"
      "template <typename T>"
      "void f(T t) {"
      "  T local_t(t /* this becomes a null QualType in the AST */);"
      "}"
      "void g() {"
      "  f(0);"
      "}",
      expr(hasType(TypeMatcher(
          anyOf(
              TypeMatcher(hasDeclaration(anything())),
              pointsTo(AnyType),
              references(AnyType)
              // Other QualType matchers should go here.
                ))))));
}

// For testing AST_MATCHER_P().
AST_MATCHER_P(Decl, just, internal::Matcher<Decl>, AMatcher) {
  // Make sure all special variables are used: node, match_finder,
  // bound_nodes_builder, and the parameter named 'AMatcher'.
  return AMatcher.matches(Node, Finder, Builder);
}

TEST(AstMatcherPMacro, Works) {
  DeclarationMatcher HasClassB = just(has(recordDecl(hasName("B")).bind("b")));

  EXPECT_TRUE(matchAndVerifyResultTrue("class A { class B {}; };",
      HasClassB, new VerifyIdIsBoundToDecl<Decl>("b")));

  EXPECT_TRUE(matchAndVerifyResultFalse("class A { class B {}; };",
      HasClassB, new VerifyIdIsBoundToDecl<Decl>("a")));

  EXPECT_TRUE(matchAndVerifyResultFalse("class A { class C {}; };",
      HasClassB, new VerifyIdIsBoundToDecl<Decl>("b")));
}

AST_POLYMORPHIC_MATCHER_P(
    polymorphicHas, internal::Matcher<Decl>, AMatcher) {
  TOOLING_COMPILE_ASSERT((llvm::is_same<NodeType, Decl>::value) ||
                         (llvm::is_same<NodeType, Stmt>::value),
                         assert_node_type_is_accessible);
  internal::TypedBaseMatcher<Decl> ChildMatcher(AMatcher);
  return Finder->matchesChildOf(
      Node, ChildMatcher, Builder,
      ASTMatchFinder::TK_IgnoreImplicitCastsAndParentheses,
      ASTMatchFinder::BK_First);
}

TEST(AstPolymorphicMatcherPMacro, Works) {
  DeclarationMatcher HasClassB =
      polymorphicHas(recordDecl(hasName("B")).bind("b"));

  EXPECT_TRUE(matchAndVerifyResultTrue("class A { class B {}; };",
      HasClassB, new VerifyIdIsBoundToDecl<Decl>("b")));

  EXPECT_TRUE(matchAndVerifyResultFalse("class A { class B {}; };",
      HasClassB, new VerifyIdIsBoundToDecl<Decl>("a")));

  EXPECT_TRUE(matchAndVerifyResultFalse("class A { class C {}; };",
      HasClassB, new VerifyIdIsBoundToDecl<Decl>("b")));

  StatementMatcher StatementHasClassB =
      polymorphicHas(recordDecl(hasName("B")));

  EXPECT_TRUE(matches("void x() { class B {}; }", StatementHasClassB));
}

TEST(For, FindsForLoops) {
  EXPECT_TRUE(matches("void f() { for(;;); }", forStmt()));
  EXPECT_TRUE(matches("void f() { if(true) for(;;); }", forStmt()));
}

TEST(For, ForLoopInternals) {
  EXPECT_TRUE(matches("void f(){ int i; for (; i < 3 ; ); }",
                      forStmt(hasCondition(anything()))));
  EXPECT_TRUE(matches("void f() { for (int i = 0; ;); }",
                      forStmt(hasLoopInit(anything()))));
}

TEST(For, NegativeForLoopInternals) {
  EXPECT_TRUE(notMatches("void f(){ for (int i = 0; ; ++i); }",
                         forStmt(hasCondition(expr()))));
  EXPECT_TRUE(notMatches("void f() {int i; for (; i < 4; ++i) {} }",
                         forStmt(hasLoopInit(anything()))));
}

TEST(For, ReportsNoFalsePositives) {
  EXPECT_TRUE(notMatches("void f() { ; }", forStmt()));
  EXPECT_TRUE(notMatches("void f() { if(true); }", forStmt()));
}

TEST(CompoundStatement, HandlesSimpleCases) {
  EXPECT_TRUE(notMatches("void f();", compoundStmt()));
  EXPECT_TRUE(matches("void f() {}", compoundStmt()));
  EXPECT_TRUE(matches("void f() {{}}", compoundStmt()));
}

TEST(CompoundStatement, DoesNotMatchEmptyStruct) {
  // It's not a compound statement just because there's "{}" in the source
  // text. This is an AST search, not grep.
  EXPECT_TRUE(notMatches("namespace n { struct S {}; }",
              compoundStmt()));
  EXPECT_TRUE(matches("namespace n { struct S { void f() {{}} }; }",
              compoundStmt()));
}

TEST(HasBody, FindsBodyOfForWhileDoLoops) {
  EXPECT_TRUE(matches("void f() { for(;;) {} }",
              forStmt(hasBody(compoundStmt()))));
  EXPECT_TRUE(notMatches("void f() { for(;;); }",
              forStmt(hasBody(compoundStmt()))));
  EXPECT_TRUE(matches("void f() { while(true) {} }",
              whileStmt(hasBody(compoundStmt()))));
  EXPECT_TRUE(matches("void f() { do {} while(true); }",
              doStmt(hasBody(compoundStmt()))));
}

TEST(HasAnySubstatement, MatchesForTopLevelCompoundStatement) {
  // The simplest case: every compound statement is in a function
  // definition, and the function body itself must be a compound
  // statement.
  EXPECT_TRUE(matches("void f() { for (;;); }",
              compoundStmt(hasAnySubstatement(forStmt()))));
}

TEST(HasAnySubstatement, IsNotRecursive) {
  // It's really "has any immediate substatement".
  EXPECT_TRUE(notMatches("void f() { if (true) for (;;); }",
              compoundStmt(hasAnySubstatement(forStmt()))));
}

TEST(HasAnySubstatement, MatchesInNestedCompoundStatements) {
  EXPECT_TRUE(matches("void f() { if (true) { for (;;); } }",
              compoundStmt(hasAnySubstatement(forStmt()))));
}

TEST(HasAnySubstatement, FindsSubstatementBetweenOthers) {
  EXPECT_TRUE(matches("void f() { 1; 2; 3; for (;;); 4; 5; 6; }",
              compoundStmt(hasAnySubstatement(forStmt()))));
}

TEST(StatementCountIs, FindsNoStatementsInAnEmptyCompoundStatement) {
  EXPECT_TRUE(matches("void f() { }",
              compoundStmt(statementCountIs(0))));
  EXPECT_TRUE(notMatches("void f() {}",
              compoundStmt(statementCountIs(1))));
}

TEST(StatementCountIs, AppearsToMatchOnlyOneCount) {
  EXPECT_TRUE(matches("void f() { 1; }",
              compoundStmt(statementCountIs(1))));
  EXPECT_TRUE(notMatches("void f() { 1; }",
              compoundStmt(statementCountIs(0))));
  EXPECT_TRUE(notMatches("void f() { 1; }",
              compoundStmt(statementCountIs(2))));
}

TEST(StatementCountIs, WorksWithMultipleStatements) {
  EXPECT_TRUE(matches("void f() { 1; 2; 3; }",
              compoundStmt(statementCountIs(3))));
}

TEST(StatementCountIs, WorksWithNestedCompoundStatements) {
  EXPECT_TRUE(matches("void f() { { 1; } { 1; 2; 3; 4; } }",
              compoundStmt(statementCountIs(1))));
  EXPECT_TRUE(matches("void f() { { 1; } { 1; 2; 3; 4; } }",
              compoundStmt(statementCountIs(2))));
  EXPECT_TRUE(notMatches("void f() { { 1; } { 1; 2; 3; 4; } }",
              compoundStmt(statementCountIs(3))));
  EXPECT_TRUE(matches("void f() { { 1; } { 1; 2; 3; 4; } }",
              compoundStmt(statementCountIs(4))));
}

TEST(Member, WorksInSimplestCase) {
  EXPECT_TRUE(matches("struct { int first; } s; int i(s.first);",
                      memberExpr(member(hasName("first")))));
}

TEST(Member, DoesNotMatchTheBaseExpression) {
  // Don't pick out the wrong part of the member expression, this should
  // be checking the member (name) only.
  EXPECT_TRUE(notMatches("struct { int i; } first; int i(first.i);",
                         memberExpr(member(hasName("first")))));
}

TEST(Member, MatchesInMemberFunctionCall) {
  EXPECT_TRUE(matches("void f() {"
                      "  struct { void first() {}; } s;"
                      "  s.first();"
                      "};",
                      memberExpr(member(hasName("first")))));
}

TEST(Member, MatchesMemberAllocationFunction) {
  EXPECT_TRUE(matches("namespace std { typedef typeof(sizeof(int)) size_t; }"
                      "class X { void *operator new(std::size_t); };",
                      methodDecl(ofClass(hasName("X")))));

  EXPECT_TRUE(matches("class X { void operator delete(void*); };",
                      methodDecl(ofClass(hasName("X")))));

  EXPECT_TRUE(matches(
      "namespace std { typedef typeof(sizeof(int)) size_t; }"
      "class X { void operator delete[](void*, std::size_t); };",
      methodDecl(ofClass(hasName("X")))));
}

TEST(HasObjectExpression, DoesNotMatchMember) {
  EXPECT_TRUE(notMatches(
      "class X {}; struct Z { X m; }; void f(Z z) { z.m; }",
      memberExpr(hasObjectExpression(hasType(recordDecl(hasName("X")))))));
}

TEST(HasObjectExpression, MatchesBaseOfVariable) {
  EXPECT_TRUE(matches(
      "struct X { int m; }; void f(X x) { x.m; }",
      memberExpr(hasObjectExpression(hasType(recordDecl(hasName("X")))))));
  EXPECT_TRUE(matches(
      "struct X { int m; }; void f(X* x) { x->m; }",
      memberExpr(hasObjectExpression(
          hasType(pointsTo(recordDecl(hasName("X"))))))));
}

TEST(HasObjectExpression,
     MatchesObjectExpressionOfImplicitlyFormedMemberExpression) {
  EXPECT_TRUE(matches(
      "class X {}; struct S { X m; void f() { this->m; } };",
      memberExpr(hasObjectExpression(
          hasType(pointsTo(recordDecl(hasName("S"))))))));
  EXPECT_TRUE(matches(
      "class X {}; struct S { X m; void f() { m; } };",
      memberExpr(hasObjectExpression(
          hasType(pointsTo(recordDecl(hasName("S"))))))));
}

TEST(Field, DoesNotMatchNonFieldMembers) {
  EXPECT_TRUE(notMatches("class X { void m(); };", fieldDecl(hasName("m"))));
  EXPECT_TRUE(notMatches("class X { class m {}; };", fieldDecl(hasName("m"))));
  EXPECT_TRUE(notMatches("class X { enum { m }; };", fieldDecl(hasName("m"))));
  EXPECT_TRUE(notMatches("class X { enum m {}; };", fieldDecl(hasName("m"))));
}

TEST(Field, MatchesField) {
  EXPECT_TRUE(matches("class X { int m; };", fieldDecl(hasName("m"))));
}

TEST(IsConstQualified, MatchesConstInt) {
  EXPECT_TRUE(matches("const int i = 42;",
                      varDecl(hasType(isConstQualified()))));
}

TEST(IsConstQualified, MatchesConstPointer) {
  EXPECT_TRUE(matches("int i = 42; int* const p(&i);",
                      varDecl(hasType(isConstQualified()))));
}

TEST(IsConstQualified, MatchesThroughTypedef) {
  EXPECT_TRUE(matches("typedef const int const_int; const_int i = 42;",
                      varDecl(hasType(isConstQualified()))));
  EXPECT_TRUE(matches("typedef int* int_ptr; const int_ptr p(0);",
                      varDecl(hasType(isConstQualified()))));
}

TEST(IsConstQualified, DoesNotMatchInappropriately) {
  EXPECT_TRUE(notMatches("typedef int nonconst_int; nonconst_int i = 42;",
                         varDecl(hasType(isConstQualified()))));
  EXPECT_TRUE(notMatches("int const* p;",
                         varDecl(hasType(isConstQualified()))));
}

TEST(CastExpression, MatchesExplicitCasts) {
  EXPECT_TRUE(matches("char *p = reinterpret_cast<char *>(&p);",
                      expr(castExpr())));
  EXPECT_TRUE(matches("void *p = (void *)(&p);", expr(castExpr())));
  EXPECT_TRUE(matches("char q, *p = const_cast<char *>(&q);",
                      expr(castExpr())));
  EXPECT_TRUE(matches("char c = char(0);", expr(castExpr())));
}
TEST(CastExpression, MatchesImplicitCasts) {
  // This test creates an implicit cast from int to char.
  EXPECT_TRUE(matches("char c = 0;", expr(castExpr())));
  // This test creates an implicit cast from lvalue to rvalue.
  EXPECT_TRUE(matches("char c = 0, d = c;", expr(castExpr())));
}

TEST(CastExpression, DoesNotMatchNonCasts) {
  EXPECT_TRUE(notMatches("char c = '0';", expr(castExpr())));
  EXPECT_TRUE(notMatches("char c, &q = c;", expr(castExpr())));
  EXPECT_TRUE(notMatches("int i = (0);", expr(castExpr())));
  EXPECT_TRUE(notMatches("int i = 0;", expr(castExpr())));
}

TEST(ReinterpretCast, MatchesSimpleCase) {
  EXPECT_TRUE(matches("char* p = reinterpret_cast<char*>(&p);",
                      expr(reinterpretCastExpr())));
}

TEST(ReinterpretCast, DoesNotMatchOtherCasts) {
  EXPECT_TRUE(notMatches("char* p = (char*)(&p);",
                         expr(reinterpretCastExpr())));
  EXPECT_TRUE(notMatches("char q, *p = const_cast<char*>(&q);",
                         expr(reinterpretCastExpr())));
  EXPECT_TRUE(notMatches("void* p = static_cast<void*>(&p);",
                         expr(reinterpretCastExpr())));
  EXPECT_TRUE(notMatches("struct B { virtual ~B() {} }; struct D : B {};"
                         "B b;"
                         "D* p = dynamic_cast<D*>(&b);",
                         expr(reinterpretCastExpr())));
}

TEST(FunctionalCast, MatchesSimpleCase) {
  std::string foo_class = "class Foo { public: Foo(char*); };";
  EXPECT_TRUE(matches(foo_class + "void r() { Foo f = Foo(\"hello world\"); }",
                      expr(functionalCastExpr())));
}

TEST(FunctionalCast, DoesNotMatchOtherCasts) {
  std::string FooClass = "class Foo { public: Foo(char*); };";
  EXPECT_TRUE(
      notMatches(FooClass + "void r() { Foo f = (Foo) \"hello world\"; }",
                 expr(functionalCastExpr())));
  EXPECT_TRUE(
      notMatches(FooClass + "void r() { Foo f = \"hello world\"; }",
                 expr(functionalCastExpr())));
}

TEST(DynamicCast, MatchesSimpleCase) {
  EXPECT_TRUE(matches("struct B { virtual ~B() {} }; struct D : B {};"
                      "B b;"
                      "D* p = dynamic_cast<D*>(&b);",
                      expr(dynamicCastExpr())));
}

TEST(StaticCast, MatchesSimpleCase) {
  EXPECT_TRUE(matches("void* p(static_cast<void*>(&p));",
                      expr(staticCastExpr())));
}

TEST(StaticCast, DoesNotMatchOtherCasts) {
  EXPECT_TRUE(notMatches("char* p = (char*)(&p);",
                         expr(staticCastExpr())));
  EXPECT_TRUE(notMatches("char q, *p = const_cast<char*>(&q);",
                         expr(staticCastExpr())));
  EXPECT_TRUE(notMatches("void* p = reinterpret_cast<char*>(&p);",
                         expr(staticCastExpr())));
  EXPECT_TRUE(notMatches("struct B { virtual ~B() {} }; struct D : B {};"
                         "B b;"
                         "D* p = dynamic_cast<D*>(&b);",
                         expr(staticCastExpr())));
}

TEST(HasDestinationType, MatchesSimpleCase) {
  EXPECT_TRUE(matches("char* p = static_cast<char*>(0);",
                      expr(staticCastExpr(hasDestinationType(
                               pointsTo(TypeMatcher(anything())))))));
}

TEST(HasImplicitDestinationType, MatchesSimpleCase) {
  // This test creates an implicit const cast.
  EXPECT_TRUE(matches("int x; const int i = x;",
                      expr(implicitCastExpr(
                          hasImplicitDestinationType(isInteger())))));
  // This test creates an implicit array-to-pointer cast.
  EXPECT_TRUE(matches("int arr[3]; int *p = arr;",
                      expr(implicitCastExpr(hasImplicitDestinationType(
                          pointsTo(TypeMatcher(anything())))))));
}

TEST(HasImplicitDestinationType, DoesNotMatchIncorrectly) {
  // This test creates an implicit cast from int to char.
  EXPECT_TRUE(notMatches("char c = 0;",
                      expr(implicitCastExpr(hasImplicitDestinationType(
                          unless(anything()))))));
  // This test creates an implicit array-to-pointer cast.
  EXPECT_TRUE(notMatches("int arr[3]; int *p = arr;",
                      expr(implicitCastExpr(hasImplicitDestinationType(
                          unless(anything()))))));
}

TEST(ImplicitCast, MatchesSimpleCase) {
  // This test creates an implicit const cast.
  EXPECT_TRUE(matches("int x = 0; const int y = x;",
                      varDecl(hasInitializer(implicitCastExpr()))));
  // This test creates an implicit cast from int to char.
  EXPECT_TRUE(matches("char c = 0;",
                      varDecl(hasInitializer(implicitCastExpr()))));
  // This test creates an implicit array-to-pointer cast.
  EXPECT_TRUE(matches("int arr[6]; int *p = arr;",
                      varDecl(hasInitializer(implicitCastExpr()))));
}

TEST(ImplicitCast, DoesNotMatchIncorrectly) {
  // This test verifies that implicitCastExpr() matches exactly when implicit casts
  // are present, and that it ignores explicit and paren casts.

  // These two test cases have no casts.
  EXPECT_TRUE(notMatches("int x = 0;",
                         varDecl(hasInitializer(implicitCastExpr()))));
  EXPECT_TRUE(notMatches("int x = 0, &y = x;",
                         varDecl(hasInitializer(implicitCastExpr()))));

  EXPECT_TRUE(notMatches("int x = 0; double d = (double) x;",
                         varDecl(hasInitializer(implicitCastExpr()))));
  EXPECT_TRUE(notMatches("const int *p; int *q = const_cast<int *>(p);",
                         varDecl(hasInitializer(implicitCastExpr()))));

  EXPECT_TRUE(notMatches("int x = (0);",
                         varDecl(hasInitializer(implicitCastExpr()))));
}

TEST(IgnoringImpCasts, MatchesImpCasts) {
  // This test checks that ignoringImpCasts matches when implicit casts are
  // present and its inner matcher alone does not match.
  // Note that this test creates an implicit const cast.
  EXPECT_TRUE(matches("int x = 0; const int y = x;",
                      varDecl(hasInitializer(ignoringImpCasts(
                          declRefExpr(to(varDecl(hasName("x")))))))));
  // This test creates an implict cast from int to char.
  EXPECT_TRUE(matches("char x = 0;",
                      varDecl(hasInitializer(ignoringImpCasts(
                          integerLiteral(equals(0)))))));
}

TEST(IgnoringImpCasts, DoesNotMatchIncorrectly) {
  // These tests verify that ignoringImpCasts does not match if the inner
  // matcher does not match.
  // Note that the first test creates an implicit const cast.
  EXPECT_TRUE(notMatches("int x; const int y = x;",
                         varDecl(hasInitializer(ignoringImpCasts(
                             unless(anything()))))));
  EXPECT_TRUE(notMatches("int x; int y = x;",
                         varDecl(hasInitializer(ignoringImpCasts(
                             unless(anything()))))));

  // These tests verify that ignoringImplictCasts does not look through explicit
  // casts or parentheses.
  EXPECT_TRUE(notMatches("char* p = static_cast<char*>(0);",
                         varDecl(hasInitializer(ignoringImpCasts(
                             integerLiteral())))));
  EXPECT_TRUE(notMatches("int i = (0);",
                         varDecl(hasInitializer(ignoringImpCasts(
                             integerLiteral())))));
  EXPECT_TRUE(notMatches("float i = (float)0;",
                         varDecl(hasInitializer(ignoringImpCasts(
                             integerLiteral())))));
  EXPECT_TRUE(notMatches("float i = float(0);",
                         varDecl(hasInitializer(ignoringImpCasts(
                             integerLiteral())))));
}

TEST(IgnoringImpCasts, MatchesWithoutImpCasts) {
  // This test verifies that expressions that do not have implicit casts
  // still match the inner matcher.
  EXPECT_TRUE(matches("int x = 0; int &y = x;",
                      varDecl(hasInitializer(ignoringImpCasts(
                          declRefExpr(to(varDecl(hasName("x")))))))));
}

TEST(IgnoringParenCasts, MatchesParenCasts) {
  // This test checks that ignoringParenCasts matches when parentheses and/or
  // casts are present and its inner matcher alone does not match.
  EXPECT_TRUE(matches("int x = (0);",
                      varDecl(hasInitializer(ignoringParenCasts(
                          integerLiteral(equals(0)))))));
  EXPECT_TRUE(matches("int x = (((((0)))));",
                      varDecl(hasInitializer(ignoringParenCasts(
                          integerLiteral(equals(0)))))));

  // This test creates an implict cast from int to char in addition to the
  // parentheses.
  EXPECT_TRUE(matches("char x = (0);",
                      varDecl(hasInitializer(ignoringParenCasts(
                          integerLiteral(equals(0)))))));

  EXPECT_TRUE(matches("char x = (char)0;",
                      varDecl(hasInitializer(ignoringParenCasts(
                          integerLiteral(equals(0)))))));
  EXPECT_TRUE(matches("char* p = static_cast<char*>(0);",
                      varDecl(hasInitializer(ignoringParenCasts(
                          integerLiteral(equals(0)))))));
}

TEST(IgnoringParenCasts, MatchesWithoutParenCasts) {
  // This test verifies that expressions that do not have any casts still match.
  EXPECT_TRUE(matches("int x = 0;",
                      varDecl(hasInitializer(ignoringParenCasts(
                          integerLiteral(equals(0)))))));
}

TEST(IgnoringParenCasts, DoesNotMatchIncorrectly) {
  // These tests verify that ignoringImpCasts does not match if the inner
  // matcher does not match.
  EXPECT_TRUE(notMatches("int x = ((0));",
                         varDecl(hasInitializer(ignoringParenCasts(
                             unless(anything()))))));

  // This test creates an implicit cast from int to char in addition to the
  // parentheses.
  EXPECT_TRUE(notMatches("char x = ((0));",
                         varDecl(hasInitializer(ignoringParenCasts(
                             unless(anything()))))));

  EXPECT_TRUE(notMatches("char *x = static_cast<char *>((0));",
                         varDecl(hasInitializer(ignoringParenCasts(
                             unless(anything()))))));
}

TEST(IgnoringParenAndImpCasts, MatchesParenImpCasts) {
  // This test checks that ignoringParenAndImpCasts matches when
  // parentheses and/or implicit casts are present and its inner matcher alone
  // does not match.
  // Note that this test creates an implicit const cast.
  EXPECT_TRUE(matches("int x = 0; const int y = x;",
                      varDecl(hasInitializer(ignoringParenImpCasts(
                          declRefExpr(to(varDecl(hasName("x")))))))));
  // This test creates an implicit cast from int to char.
  EXPECT_TRUE(matches("const char x = (0);",
                      varDecl(hasInitializer(ignoringParenImpCasts(
                          integerLiteral(equals(0)))))));
}

TEST(IgnoringParenAndImpCasts, MatchesWithoutParenImpCasts) {
  // This test verifies that expressions that do not have parentheses or
  // implicit casts still match.
  EXPECT_TRUE(matches("int x = 0; int &y = x;",
                      varDecl(hasInitializer(ignoringParenImpCasts(
                          declRefExpr(to(varDecl(hasName("x")))))))));
  EXPECT_TRUE(matches("int x = 0;",
                      varDecl(hasInitializer(ignoringParenImpCasts(
                          integerLiteral(equals(0)))))));
}

TEST(IgnoringParenAndImpCasts, DoesNotMatchIncorrectly) {
  // These tests verify that ignoringParenImpCasts does not match if
  // the inner matcher does not match.
  // This test creates an implicit cast.
  EXPECT_TRUE(notMatches("char c = ((3));",
                         varDecl(hasInitializer(ignoringParenImpCasts(
                             unless(anything()))))));
  // These tests verify that ignoringParenAndImplictCasts does not look
  // through explicit casts.
  EXPECT_TRUE(notMatches("float y = (float(0));",
                         varDecl(hasInitializer(ignoringParenImpCasts(
                             integerLiteral())))));
  EXPECT_TRUE(notMatches("float y = (float)0;",
                         varDecl(hasInitializer(ignoringParenImpCasts(
                             integerLiteral())))));
  EXPECT_TRUE(notMatches("char* p = static_cast<char*>(0);",
                         varDecl(hasInitializer(ignoringParenImpCasts(
                             integerLiteral())))));
}

TEST(HasSourceExpression, MatchesImplicitCasts) {
  EXPECT_TRUE(matches("class string {}; class URL { public: URL(string s); };"
                      "void r() {string a_string; URL url = a_string; }",
                      expr(implicitCastExpr(
                          hasSourceExpression(constructExpr())))));
}

TEST(HasSourceExpression, MatchesExplicitCasts) {
  EXPECT_TRUE(matches("float x = static_cast<float>(42);",
                      expr(explicitCastExpr(
                          hasSourceExpression(hasDescendant(
                              expr(integerLiteral())))))));
}

TEST(Statement, DoesNotMatchDeclarations) {
  EXPECT_TRUE(notMatches("class X {};", stmt()));
}

TEST(Statement, MatchesCompoundStatments) {
  EXPECT_TRUE(matches("void x() {}", stmt()));
}

TEST(DeclarationStatement, DoesNotMatchCompoundStatements) {
  EXPECT_TRUE(notMatches("void x() {}", declStmt()));
}

TEST(DeclarationStatement, MatchesVariableDeclarationStatements) {
  EXPECT_TRUE(matches("void x() { int a; }", declStmt()));
}

TEST(InitListExpression, MatchesInitListExpression) {
  EXPECT_TRUE(matches("int a[] = { 1, 2 };",
                      initListExpr(hasType(asString("int [2]")))));
  EXPECT_TRUE(matches("struct B { int x, y; }; B b = { 5, 6 };",
                      initListExpr(hasType(recordDecl(hasName("B"))))));
}

TEST(UsingDeclaration, MatchesUsingDeclarations) {
  EXPECT_TRUE(matches("namespace X { int x; } using X::x;",
                      usingDecl()));
}

TEST(UsingDeclaration, MatchesShadowUsingDelcarations) {
  EXPECT_TRUE(matches("namespace f { int a; } using f::a;",
                      usingDecl(hasAnyUsingShadowDecl(hasName("a")))));
}

TEST(UsingDeclaration, MatchesSpecificTarget) {
  EXPECT_TRUE(matches("namespace f { int a; void b(); } using f::b;",
                      usingDecl(hasAnyUsingShadowDecl(
                          hasTargetDecl(functionDecl())))));
  EXPECT_TRUE(notMatches("namespace f { int a; void b(); } using f::a;",
                         usingDecl(hasAnyUsingShadowDecl(
                             hasTargetDecl(functionDecl())))));
}

TEST(UsingDeclaration, ThroughUsingDeclaration) {
  EXPECT_TRUE(matches(
      "namespace a { void f(); } using a::f; void g() { f(); }",
      declRefExpr(throughUsingDecl(anything()))));
  EXPECT_TRUE(notMatches(
      "namespace a { void f(); } using a::f; void g() { a::f(); }",
      declRefExpr(throughUsingDecl(anything()))));
}

TEST(SingleDecl, IsSingleDecl) {
  StatementMatcher SingleDeclStmt =
      declStmt(hasSingleDecl(varDecl(hasInitializer(anything()))));
  EXPECT_TRUE(matches("void f() {int a = 4;}", SingleDeclStmt));
  EXPECT_TRUE(notMatches("void f() {int a;}", SingleDeclStmt));
  EXPECT_TRUE(notMatches("void f() {int a = 4, b = 3;}",
                          SingleDeclStmt));
}

TEST(DeclStmt, ContainsDeclaration) {
  DeclarationMatcher MatchesInit = varDecl(hasInitializer(anything()));

  EXPECT_TRUE(matches("void f() {int a = 4;}",
                      declStmt(containsDeclaration(0, MatchesInit))));
  EXPECT_TRUE(matches("void f() {int a = 4, b = 3;}",
                      declStmt(containsDeclaration(0, MatchesInit),
                               containsDeclaration(1, MatchesInit))));
  unsigned WrongIndex = 42;
  EXPECT_TRUE(notMatches("void f() {int a = 4, b = 3;}",
                         declStmt(containsDeclaration(WrongIndex,
                                                      MatchesInit))));
}

TEST(DeclCount, DeclCountIsCorrect) {
  EXPECT_TRUE(matches("void f() {int i,j;}",
                      declStmt(declCountIs(2))));
  EXPECT_TRUE(notMatches("void f() {int i,j; int k;}",
                         declStmt(declCountIs(3))));
  EXPECT_TRUE(notMatches("void f() {int i,j, k, l;}",
                         declStmt(declCountIs(3))));
}

TEST(While, MatchesWhileLoops) {
  EXPECT_TRUE(notMatches("void x() {}", whileStmt()));
  EXPECT_TRUE(matches("void x() { while(true); }", whileStmt()));
  EXPECT_TRUE(notMatches("void x() { do {} while(true); }", whileStmt()));
}

TEST(Do, MatchesDoLoops) {
  EXPECT_TRUE(matches("void x() { do {} while(true); }", doStmt()));
  EXPECT_TRUE(matches("void x() { do ; while(false); }", doStmt()));
}

TEST(Do, DoesNotMatchWhileLoops) {
  EXPECT_TRUE(notMatches("void x() { while(true) {} }", doStmt()));
}

TEST(SwitchCase, MatchesCase) {
  EXPECT_TRUE(matches("void x() { switch(42) { case 42:; } }", switchCase()));
  EXPECT_TRUE(matches("void x() { switch(42) { default:; } }", switchCase()));
  EXPECT_TRUE(matches("void x() { switch(42) default:; }", switchCase()));
  EXPECT_TRUE(notMatches("void x() { switch(42) {} }", switchCase()));
}

TEST(HasConditionVariableStatement, DoesNotMatchCondition) {
  EXPECT_TRUE(notMatches(
      "void x() { if(true) {} }",
      ifStmt(hasConditionVariableStatement(declStmt()))));
  EXPECT_TRUE(notMatches(
      "void x() { int x; if((x = 42)) {} }",
      ifStmt(hasConditionVariableStatement(declStmt()))));
}

TEST(HasConditionVariableStatement, MatchesConditionVariables) {
  EXPECT_TRUE(matches(
      "void x() { if(int* a = 0) {} }",
      ifStmt(hasConditionVariableStatement(declStmt()))));
}

TEST(ForEach, BindsOneNode) {
  EXPECT_TRUE(matchAndVerifyResultTrue("class C { int x; };",
      recordDecl(hasName("C"), forEach(fieldDecl(hasName("x")).bind("x"))),
      new VerifyIdIsBoundToDecl<FieldDecl>("x", 1)));
}

TEST(ForEach, BindsMultipleNodes) {
  EXPECT_TRUE(matchAndVerifyResultTrue("class C { int x; int y; int z; };",
      recordDecl(hasName("C"), forEach(fieldDecl().bind("f"))),
      new VerifyIdIsBoundToDecl<FieldDecl>("f", 3)));
}

TEST(ForEach, BindsRecursiveCombinations) {
  EXPECT_TRUE(matchAndVerifyResultTrue(
      "class C { class D { int x; int y; }; class E { int y; int z; }; };",
      recordDecl(hasName("C"),
                 forEach(recordDecl(forEach(fieldDecl().bind("f"))))),
      new VerifyIdIsBoundToDecl<FieldDecl>("f", 4)));
}

TEST(ForEachDescendant, BindsOneNode) {
  EXPECT_TRUE(matchAndVerifyResultTrue("class C { class D { int x; }; };",
      recordDecl(hasName("C"),
                 forEachDescendant(fieldDecl(hasName("x")).bind("x"))),
      new VerifyIdIsBoundToDecl<FieldDecl>("x", 1)));
}

TEST(ForEachDescendant, BindsMultipleNodes) {
  EXPECT_TRUE(matchAndVerifyResultTrue(
      "class C { class D { int x; int y; }; "
      "          class E { class F { int y; int z; }; }; };",
      recordDecl(hasName("C"), forEachDescendant(fieldDecl().bind("f"))),
      new VerifyIdIsBoundToDecl<FieldDecl>("f", 4)));
}

TEST(ForEachDescendant, BindsRecursiveCombinations) {
  EXPECT_TRUE(matchAndVerifyResultTrue(
      "class C { class D { "
      "          class E { class F { class G { int y; int z; }; }; }; }; };",
      recordDecl(hasName("C"), forEachDescendant(recordDecl(
          forEachDescendant(fieldDecl().bind("f"))))),
      new VerifyIdIsBoundToDecl<FieldDecl>("f", 8)));
}


TEST(IsTemplateInstantiation, MatchesImplicitClassTemplateInstantiation) {
  // Make sure that we can both match the class by name (::X) and by the type
  // the template was instantiated with (via a field).

  EXPECT_TRUE(matches(
      "template <typename T> class X {}; class A {}; X<A> x;",
      recordDecl(hasName("::X"), isTemplateInstantiation())));

  EXPECT_TRUE(matches(
      "template <typename T> class X { T t; }; class A {}; X<A> x;",
      recordDecl(isTemplateInstantiation(), hasDescendant(
          fieldDecl(hasType(recordDecl(hasName("A"))))))));
}

TEST(IsTemplateInstantiation, MatchesImplicitFunctionTemplateInstantiation) {
  EXPECT_TRUE(matches(
      "template <typename T> void f(T t) {} class A {}; void g() { f(A()); }",
      functionDecl(hasParameter(0, hasType(recordDecl(hasName("A")))),
               isTemplateInstantiation())));
}

TEST(IsTemplateInstantiation, MatchesExplicitClassTemplateInstantiation) {
  EXPECT_TRUE(matches(
      "template <typename T> class X { T t; }; class A {};"
      "template class X<A>;",
      recordDecl(isTemplateInstantiation(), hasDescendant(
          fieldDecl(hasType(recordDecl(hasName("A"))))))));
}

TEST(IsTemplateInstantiation,
     MatchesInstantiationOfPartiallySpecializedClassTemplate) {
  EXPECT_TRUE(matches(
      "template <typename T> class X {};"
      "template <typename T> class X<T*> {}; class A {}; X<A*> x;",
      recordDecl(hasName("::X"), isTemplateInstantiation())));
}

TEST(IsTemplateInstantiation,
     MatchesInstantiationOfClassTemplateNestedInNonTemplate) {
  EXPECT_TRUE(matches(
      "class A {};"
      "class X {"
      "  template <typename U> class Y { U u; };"
      "  Y<A> y;"
      "};",
      recordDecl(hasName("::X::Y"), isTemplateInstantiation())));
}

TEST(IsTemplateInstantiation, DoesNotMatchInstantiationsInsideOfInstantiation) {
  // FIXME: Figure out whether this makes sense. It doesn't affect the
  // normal use case as long as the uppermost instantiation always is marked
  // as template instantiation, but it might be confusing as a predicate.
  EXPECT_TRUE(matches(
      "class A {};"
      "template <typename T> class X {"
      "  template <typename U> class Y { U u; };"
      "  Y<T> y;"
      "}; X<A> x;",
      recordDecl(hasName("::X<A>::Y"), unless(isTemplateInstantiation()))));
}

TEST(IsTemplateInstantiation, DoesNotMatchExplicitClassTemplateSpecialization) {
  EXPECT_TRUE(notMatches(
      "template <typename T> class X {}; class A {};"
      "template <> class X<A> {}; X<A> x;",
      recordDecl(hasName("::X"), isTemplateInstantiation())));
}

TEST(IsTemplateInstantiation, DoesNotMatchNonTemplate) {
  EXPECT_TRUE(notMatches(
      "class A {}; class Y { A a; };",
      recordDecl(isTemplateInstantiation())));
}

TEST(IsExplicitTemplateSpecialization,
     DoesNotMatchPrimaryTemplate) {
  EXPECT_TRUE(notMatches(
      "template <typename T> class X {};",
      recordDecl(isExplicitTemplateSpecialization())));
  EXPECT_TRUE(notMatches(
      "template <typename T> void f(T t);",
      functionDecl(isExplicitTemplateSpecialization())));
}

TEST(IsExplicitTemplateSpecialization,
     DoesNotMatchExplicitTemplateInstantiations) {
  EXPECT_TRUE(notMatches(
      "template <typename T> class X {};"
      "template class X<int>; extern template class X<long>;",
      recordDecl(isExplicitTemplateSpecialization())));
  EXPECT_TRUE(notMatches(
      "template <typename T> void f(T t) {}"
      "template void f(int t); extern template void f(long t);",
      functionDecl(isExplicitTemplateSpecialization())));
}

TEST(IsExplicitTemplateSpecialization,
     DoesNotMatchImplicitTemplateInstantiations) {
  EXPECT_TRUE(notMatches(
      "template <typename T> class X {}; X<int> x;",
      recordDecl(isExplicitTemplateSpecialization())));
  EXPECT_TRUE(notMatches(
      "template <typename T> void f(T t); void g() { f(10); }",
      functionDecl(isExplicitTemplateSpecialization())));
}

TEST(IsExplicitTemplateSpecialization,
     MatchesExplicitTemplateSpecializations) {
  EXPECT_TRUE(matches(
      "template <typename T> class X {};"
      "template<> class X<int> {};",
      recordDecl(isExplicitTemplateSpecialization())));
  EXPECT_TRUE(matches(
      "template <typename T> void f(T t) {}"
      "template<> void f(int t) {}",
      functionDecl(isExplicitTemplateSpecialization())));
}

} // end namespace ast_matchers
} // end namespace clang
