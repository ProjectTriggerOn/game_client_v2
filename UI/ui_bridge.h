#pragma once

namespace ultralight { class View; }

namespace UI {
namespace Bridge {

// Bind the `game.*` API into the View's JS context.
// Must be called from OnDOMReady (the JSContext resets on every navigation,
// so ui_manager's load listener re-invokes this after each reload).
// API contract: docs/ultralight_integration.md §8.1.
void Register(ultralight::View* view);

}  // namespace Bridge
}  // namespace UI
