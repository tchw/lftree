#include "gtest/gtest.h"
#include "lftree2.h"

using namespace lftree;
using namespace testing;

TEST(ReceiveTest, deliverSetsFuturesCreatedByReceive) {
  Future<int> x = receive<int>();
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver<int>(1));
  EXPECT_TRUE(x.ready());
  EXPECT_EQ(1, x.get());
}

TEST(ReceiveTest, futuresAreShared) {
  Future<int> x = receive<int>();
  Future<int> y = x;
  EXPECT_EQ(1, deliver<int>(1));
  EXPECT_EQ(1, x.get());
  EXPECT_EQ(1, y.get());
}

TEST(ReceiveTest, receiveCreatesUniqueFuture) {
  Future<int> x = receive<int>();
  Future<int> y = receive<int>();
  EXPECT_EQ(2, deliver<int>(1));
  EXPECT_EQ(1, x.get());
  EXPECT_EQ(1, y.get());
}

TEST(ReceiveTest, scope) {
  { Future<int> x = receive<int>(); }

  EXPECT_EQ(0, deliver<int>(0));
}

TEST(ReceiveTest, predicateReceive) {
  const int N = 100000;
  Future<int> x = receive<int>([N](int x) { return x == N; });

  for (int i = 0; i < N; ++i) {
    EXPECT_FALSE(x.ready());
    EXPECT_EQ(1, deliver<int>(i));
  }
  EXPECT_EQ(1, deliver<int>(N));

  EXPECT_TRUE(x.ready());
  EXPECT_EQ(N, x.get());
}

template <std::size_t I>
struct Msg {
  Msg() : val(std::to_string(I)) {}
  template <typename T>
  Msg(const T& x)
      : val(std::to_string(I) + "{" + x.val + "}") {}
  template <typename T, typename U>
  Msg(const T& x, const U& y)
      : val(std::to_string(I) + "{" + x.val + y.val + "}") {}
  std::string val;
};

using A = Msg<0>;
using B = Msg<1>;
using C = Msg<2>;
using D = Msg<3>;
using T = Msg<4>;

TEST(BindTest, bindOneArgument) {
  Future<T> x = bind<T, A>(receive<A>(), [](A a) { return T(A()); });

  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(A()));
  EXPECT_TRUE(x.ready());
  EXPECT_EQ(T(A()).val, x.get().val);
}

TEST(BindTest, bindWithTwoArguments) {
  Future<T> x = bind<T, A, B>(receive<A>(), receive<B>(),
                              [](A a, B b) { return T(a, b); });

  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(A()));
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(B()));
  EXPECT_TRUE(x.ready());
  EXPECT_EQ(T(A(), B()).val, x.get().val);
}

TEST(BindTest, bindWithTwoSameArguments) {
  Future<A> a = receive<A>();
  Future<T> x = bind<T, A, A>(a, a, [](A a, A aa) { return T(a, aa); });

  EXPECT_EQ(1, deliver(A()));
  EXPECT_TRUE(x.ready());
  EXPECT_EQ(T(A(), A()).val, x.get().val);
}

struct AltAltStacked : public Test {
  void SetUp() {
    Future<B> b =
        alt<B, C, D>(receive<C>(), receive<D>(), [](C c) { return B(c); },
                     [](D d) { return B(d); });
    x = alt<T, A, B>(receive<A>(), b, [](A a) { return T(a); },
                     [](B b) { return T(b); });
  }

  Future<T> x;
};

TEST_F(AltAltStacked, case0) {
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(D()));
  EXPECT_TRUE(x.ready());
  EXPECT_EQ(T(B(D())).val, x.get().val);
  EXPECT_EQ(0, deliver(C()));
  EXPECT_EQ(0, deliver(A()));
}

TEST_F(AltAltStacked, case1) {
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(C()));
  EXPECT_TRUE(x.ready());
  EXPECT_EQ(T(B(C())).val, x.get().val);
  EXPECT_EQ(0, deliver(D()));
  EXPECT_EQ(0, deliver(A()));
}

TEST_F(AltAltStacked, case2) {
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(A()));
  EXPECT_TRUE(x.ready());
  EXPECT_EQ(T(A()).val, x.get().val);
  EXPECT_EQ(0, deliver(C()));
  EXPECT_EQ(0, deliver(D()));
}

struct AltBindStacked : public Test {
  void SetUp() {
    Future<B> b = bind<B, C, D>(receive<C>(), receive<D>(),
                                [](C c, D d) { return B(c, d); });
    x = alt<T, A, B>(receive<A>(), b, [](A a) { return T(a); },
                     [](B b) { return T(b); });
  }

  Future<T> x;
};

TEST_F(AltBindStacked, case0) {
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(A()));
  EXPECT_TRUE(x.ready());
  EXPECT_EQ(T(A()).val, x.get().val);
  EXPECT_EQ(0, deliver(C()));
  EXPECT_EQ(0, deliver(D()));
}

TEST_F(AltBindStacked, case1) {
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(C()));
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(D()));
  EXPECT_TRUE(x.ready());
  EXPECT_EQ(T(B(C(), D())).val, x.get().val);
  EXPECT_EQ(0, deliver(A()));
}

TEST_F(AltBindStacked, case2) {
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(C()));
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(A()));
  EXPECT_TRUE(x.ready());
  EXPECT_EQ(T(A()).val, x.get().val);
  EXPECT_EQ(0, deliver(D()));
}

struct BindAltStacked : public Test {
  void SetUp() {
    Future<B> b =
        alt<B, C, D>(receive<C>(), receive<D>(), [](C c) { return B(c); },
                     [](D d) { return B(d); });
    x = bind<T, A, B>(receive<A>(), b, [](A a, B b) { return T(a, b); });
  }

  Future<T> x;
};

TEST_F(BindAltStacked, case0) {
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(A()));
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(C()));
  EXPECT_TRUE(x.ready());
  EXPECT_EQ(T(A(), B(C())).val, x.get().val);
  EXPECT_EQ(0, deliver(D()));
}

TEST_F(BindAltStacked, case1) {
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(A()));
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(D()));
  EXPECT_TRUE(x.ready());
  EXPECT_EQ(T(A(), B(D())).val, x.get().val);
  EXPECT_EQ(0, deliver(C()));
}

TEST_F(BindAltStacked, case2) {
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(C()));
  EXPECT_EQ(0, deliver(D()));
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(A()));
  EXPECT_TRUE(x.ready());
  EXPECT_EQ(T(A(), B(C())).val, x.get().val);
}

TEST_F(BindAltStacked, case3) {
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(D()));
  EXPECT_EQ(0, deliver(C()));
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(A()));
  EXPECT_TRUE(x.ready());
  EXPECT_EQ(T(A(), B(D())).val, x.get().val);
}

struct BindBindStacked : public Test {
  void SetUp() {
    Future<B> b = bind<B, C, D>(receive<C>(), receive<D>(),
                                [](C c, D d) { return B(c, d); });
    x = bind<T, A, B>(receive<A>(), b, [](A a, B b) { return T(a, b); });
  }

  Future<T> x;

  template <typename M1, typename M2, typename M3>
  void test() {
    EXPECT_FALSE(x.ready());
    EXPECT_EQ(1, deliver(M1()));
    EXPECT_FALSE(x.ready());
    EXPECT_EQ(1, deliver(M2()));
    EXPECT_FALSE(x.ready());
    EXPECT_EQ(1, deliver(M3()));
    EXPECT_TRUE(x.ready());
    EXPECT_EQ(T(A(), B(C(), D())).val, x.get().val);
  }
};

TEST_F(BindBindStacked, case0) { test<A, C, D>(); }
TEST_F(BindBindStacked, case1) { test<A, D, C>(); }
TEST_F(BindBindStacked, case2) { test<C, A, D>(); }
TEST_F(BindBindStacked, case3) { test<C, D, A>(); }
TEST_F(BindBindStacked, case4) { test<D, A, C>(); }
TEST_F(BindBindStacked, case5) { test<D, C, A>(); }

struct AltAltNested : public Test {
  void SetUp() {
    x = alt<T, A, B>(receive<A>(), receive<B>(),
                     [](A a) {
                       return alt<T, A, C>(receive<A>(), receive<C>(),
                                           [a](A aa) { return T(a, aa); },
                                           [a](C c) { return T(a, c); });
                     },
                     [](B b) {
                       return alt<T, C, D>(receive<C>(), receive<D>(),
                                           [b](C c) { return T(b, c); },
                                           [b](D d) { return T(b, d); });
                     });
  }

  Future<T> x;
};

TEST_F(AltAltNested, case0) {
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(A()));
  EXPECT_EQ(0, deliver(B()));
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(A()));
  EXPECT_EQ(0, deliver(C()));
  EXPECT_EQ(T(A(), A()).val, x.get().val);
}

TEST_F(AltAltNested, case1) {
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(A()));
  EXPECT_EQ(0, deliver(B()));
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(C()));
  EXPECT_EQ(0, deliver(A()));
  EXPECT_EQ(T(A(), C()).val, x.get().val);
}

TEST_F(AltAltNested, case2) {
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(B()));
  EXPECT_EQ(0, deliver(A()));
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(C()));
  EXPECT_EQ(0, deliver(D()));
  EXPECT_EQ(T(B(), C()).val, x.get().val);
}

TEST_F(AltAltNested, case3) {
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(B()));
  EXPECT_EQ(0, deliver(A()));
  EXPECT_FALSE(x.ready());
  EXPECT_EQ(1, deliver(D()));
  EXPECT_EQ(0, deliver(C()));
  EXPECT_EQ(T(B(), D()).val, x.get().val);
}

struct AltBindMixedTest : public ::testing::Test {
  void SetUp() {
    x = bind<T, int>(receive<int>(), [](int i) -> Future<T> {
      if (i == 0) {
        Future<A> futureA = bind<A, B, C>(receive<B>(), receive<C>(),
                                          [](B b, C c) { return A(b, c); });
        return alt<T, A, A>(futureA, receive<A>(), [](A a) { return T(a); },
                            [](A a) { return T(a); });
      } else {
        Future<B> futureB =
            alt<B, A, C>(receive<A>(), receive<C>(), [](A a) { return B(a); },
                         [](C c) { return B(c); });

        return bind<T, B, A>(futureB, receive<A>(),
                             [i](B b, A a) { return T(b, a); });
      }
    });
  }

  Future<T> x;
};

TEST_F(AltBindMixedTest, branch0_finishedBind) {
  EXPECT_EQ(0, deliver(A()));
  EXPECT_EQ(0, deliver(B()));
  EXPECT_EQ(0, deliver(C()));
  EXPECT_EQ(1, deliver<int>(0));
  EXPECT_EQ(1, deliver(B()));
  EXPECT_EQ(1, deliver(C()));
  EXPECT_EQ(0, deliver(A()));
  EXPECT_EQ(T(A(B(), C())).val, x.get().val);
}

TEST_F(AltBindMixedTest, branch0_interruptedBind) {
  EXPECT_EQ(1, deliver<int>(0));
  EXPECT_EQ(1, deliver(B()));
  EXPECT_EQ(1, deliver(A()));
  EXPECT_EQ(0, deliver(C()));
  EXPECT_EQ(T(A()).val, x.get().val);
}

TEST_F(AltBindMixedTest, branch0_notStartedBind) {
  EXPECT_EQ(1, deliver<int>(0));
  EXPECT_EQ(1, deliver(A()));
  EXPECT_EQ(0, deliver(B()));
  EXPECT_EQ(0, deliver(C()));
  EXPECT_EQ(T(A()).val, x.get().val);
}

TEST_F(AltBindMixedTest, branch1_order0) {
  deliver<int>(1);
  EXPECT_EQ(2, deliver(A()));
  EXPECT_EQ(T(B(A()), A()).val, x.get().val);
}

TEST_F(AltBindMixedTest, branch1_order1) {
  deliver<int>(1);
  EXPECT_EQ(1, deliver(C()));
  EXPECT_EQ(1, deliver(A()));
  EXPECT_EQ(T(B(C()), A()).val, x.get().val);
}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
