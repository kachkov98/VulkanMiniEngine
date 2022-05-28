#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <entt/locator/locator.hpp>
#include <string>

namespace vme {

struct Version {
  uint8_t major, minor, patch;
};

class Application {
public:
  Application(const std::string &name, const Version &version) : name_(name), version_(version) {}

  const std::string &getName() const noexcept { return name_; }
  const Version &getVersion() const noexcept { return version_; }

  virtual bool shouldClose() = 0;
  virtual void onInit() = 0;
  virtual void onTerminate() = 0;
  virtual void onUpdate(double delta) = 0;
  virtual void onRender(double alpha) = 0;

private:
  std::string name_;
  Version version_;
};

class Engine {
public:
  static void init();
  static void terminate();
  template <typename Service> static Service &get() noexcept {
    return entt::locator<Service>::value();
  }
  static void run(Application &app, unsigned update_freq);
};
} // namespace vme

#endif
