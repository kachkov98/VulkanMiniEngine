#ifndef FSM_HPP
#define FSM_HPP

#include <optional>
#include <utility>
#include <variant>

namespace vme {

template <typename Derived, typename... States> class StateMachine {
public:
  using State = std::variant<States...>;

  State &getState() { return state_; }
  const State &getState() const { return state_; }

  template <typename Event> void dispatch(Event &&event) {
    auto child = static_cast<Derived &>(*this);
    auto new_state = std::visit(
        [&](auto &state) -> std::optional<State> {
          return child.onEvent(state, std::forward<Event>(event));
        },
        state_);
    if (new_state)
      state_ = std::move(*new_state);
  }

private:
  State state_;
};
} // namespace vme

#endif
