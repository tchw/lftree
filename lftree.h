#pragma once

#include <vector>
#include <memory>
#include <tuple>
#include <array>

namespace lftree {

namespace detail {

template <typename T>
struct Observer;

template <typename T>
struct Future;

template <typename T>
struct Node {
  virtual ~Node() {}

  void propagate(Future<T> x) {
    if (x.ready()) {
      for (auto child : children)
        if (auto p = child.lock()) p->set(x.get());
    } else {
      x.getParent()->children = std::move(this->children);
      for (auto child : x.getParent()->children) {
        if (auto p = child.lock()) p->parent = x.getParent();
      }
    }
  }

  void connect(std::weak_ptr<Observer<T>> child) { children.push_back(child); }

  template <typename NodeT, typename... Args, std::size_t... Is>
  static void connect(std::index_sequence<Is...>, std::shared_ptr<NodeT> node,
                      std::shared_ptr<Node<Args>>... args) {
    using Expand = int[];
    std::tuple<std::shared_ptr<Observer<Args>>...> observers;
    Expand{0, (std::get<Is>(observers) =
                   std::make_shared<ArgObserver<NodeT, Args, Is>>(args, node),
               args->connect(std::get<Is>(observers)),
               node->parents.push_back(std::get<Is>(observers)), 0)...};
  }

 protected:
  template <typename NodeT, typename Arg, std::size_t I>
  struct ArgObserver : public Observer<Arg> {
    ArgObserver(std::shared_ptr<Node<Arg>> parent, std::weak_ptr<NodeT> node) {
      this->parent = parent;
      this->node = node;
    }

    void set(const Arg& x) {
      if (auto p = node.lock()) p->template set<Arg, I>(x);
    }

    std::weak_ptr<NodeT> node;
  };

  std::vector<std::shared_ptr<void>> parents;
  std::vector<std::weak_ptr<Observer<T>>> children;
};

template <typename T>
struct Observer {
  virtual ~Observer() {}
  virtual void set(const T& x) = 0;

  std::shared_ptr<Node<T>> parent;
};

template <typename T>
struct Future {
  Future() : p(std::make_shared<Data>()) {}
  Future(const T& x) : p(std::make_shared<Data>(x)) {}

  static Future<T> create(std::shared_ptr<Node<T>> node) {
    Future<T> x;
    x.p->parent = node;
    node->connect(x.p);
    return x;
  }

  bool ready() { return p->ready; }
  T get() { return p->value; }

  std::shared_ptr<Node<T>> getParent() { return p->parent; }

 private:
  struct Data : public Observer<T> {
    Data() : ready(false) {}
    Data(const T& x) : ready(true), value(x) {}

    bool ready;
    T value;

    void set(const T& x) {
      ready = true;
      value = x;
      this->parent.reset();
    }
  };

  std::shared_ptr<Data> p;
};

template <typename T, typename... Args>
struct Bind : public Node<T> {
  static constexpr int N = sizeof...(Args);
  using BindT = Bind<T, Args...>;

  Bind(std::function<Future<T>(Args...)> f) : f(f) {
    for (bool& b : ready) b = false;
  }

  static Future<T> create(Future<Args>... args,
                          std::function<Future<T>(Args...)> f) {
    std::shared_ptr<BindT> bind = std::make_shared<BindT>(f);
    Node<T>::connect(std::make_index_sequence<N>(), bind, args.getParent()...);
    return Future<T>::create(bind);
  }

  template <typename Arg, std::size_t I>
  void set(const Arg& x) {
    std::get<I>(args) = x;
    ready[I] = true;
    this->parents[I].reset();

    for (bool x : ready)
      if (!x) return;

    Future<T> result = invoke(std::make_index_sequence<N>());
    this->propagate(result);
  }

 private:
  template <std::size_t... Is>
  Future<T> invoke(std::index_sequence<Is...>) {
    return f(std::get<Is>(args)...);
  }

  std::array<bool, N> ready;
  std::tuple<Args...> args;
  std::function<Future<T>(Args...)> f;
};

template <typename T, typename... Args>
struct Alt : public Node<T> {
  static constexpr int N = sizeof...(Args);
  using AltT = Alt<T, Args...>;

  Alt(std::function<Future<T>(Args)>... fs) : fs(fs...) {}

  static Future<T> create(Future<Args>... args,
                          std::function<Future<T>(Args)>... fs) {
    std::shared_ptr<AltT> alt = std::make_shared<AltT>(fs...);
    Node<T>::connect(std::make_index_sequence<N>(), alt, args.getParent()...);
    return Future<T>::create(alt);
  }

  template <typename Arg, std::size_t I>
  void set(const Arg& x) {
    for (auto& parent : this->parents) parent.reset();
    Future<T> result = std::get<I>(fs)(x);
    this->propagate(result);
  }

 private:
  std::tuple<std::function<Future<T>(Args)>...> fs;
};

template <typename T>
struct Receive : public Node<T> {
  static Future<T> create() {
    std::shared_ptr<Receive<T>> receive = std::make_shared<Receive<T>>();
    instances().push_back(receive);
    return Future<T>::create(receive);
  }

  static std::size_t deliver(const T& x) {
    std::vector<std::weak_ptr<Receive<T>>> is = std::move(instances());
    int n = 0;
    for (auto i : is)
      if (auto p = i.lock()) {
        p->propagate(x);
        ++n;
      }
    return n;
  }

 private:
  static std::vector<std::weak_ptr<Receive<T>>>& instances() {
    static std::vector<std::weak_ptr<Receive<T>>> x;
    return x;
  }
};
}

using detail::Future;

template <typename T>
Future<T> receive() {
  return detail::Receive<T>::create();
}

template <typename T>
std::size_t deliver(const T& x) {
  return detail::Receive<T>::deliver(x);
}

template <typename T, typename... Args>
Future<T> bind(Future<Args>... args, std::function<Future<T>(Args...)> f) {
  return detail::Bind<T, Args...>::create(args..., f);
}

template <typename T, typename... Args>
Future<T> alt(Future<Args>... args, std::function<Future<T>(Args)>... fs) {
  return detail::Alt<T, Args...>::create(args..., fs...);
}

template <typename T, typename F>
Future<T> receive(F pred) {
  return bind<T, T>(receive<T>(), [pred](const T& x) -> Future<T> {
    if (pred(x))
      return x;
    else
      return receive<T, F>(pred);
  });
}
}
