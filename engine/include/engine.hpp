#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <Tracy.hpp>
#include <entt/locator/locator.hpp>

#include <string>

namespace vme {

struct Version {
  uint8_t major, minor, patch;
};

class Application {
public:
  Application(const std::string &name, const Version &version) : name_(name), version_(version) {
    TracyAppInfo(name.c_str(), name.size());
  }

  const std::string &getName() const noexcept { return name_; }
  const Version &getVersion() const noexcept { return version_; }

  void run(unsigned update_freq);

private:
  std::string name_;
  Version version_;

  virtual bool shouldClose() = 0;
  virtual void onInit() = 0;
  virtual void onTerminate() = 0;
  virtual void onUpdate(double delta) = 0;
  virtual void onRender(double alpha) = 0;
};

class Engine {
public:
  static void init();
  static void terminate();
  template <typename Service> static Service &get() noexcept {
    return entt::locator<Service>::value();
  }
};
} // namespace vme

#endif
