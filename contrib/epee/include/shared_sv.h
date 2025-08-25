#pragma once
#include <memory>
#include <string>
#include <string_view>

namespace epee {

struct shared_sv {
  std::shared_ptr<std::string> ptr;
  std::string_view view;
  shared_sv() = default;
  explicit shared_sv(std::shared_ptr<std::string> src_ptr) : ptr{std::move(src_ptr)}, view{*ptr}  {}
  explicit shared_sv(std::string&& str) : shared_sv{std::make_shared<std::string>(std::move(str))} {}
  shared_sv(std::shared_ptr<std::string> src_ptr, std::string_view view) : ptr{std::move(src_ptr)}, view{view} {}

  auto size() const { return view.size(); }
  auto data() const { return view.data(); }

  shared_sv extract_prefix(size_t size) {
    auto prefix_view = view.substr(0, size);
    view.remove_prefix(prefix_view.size());
    return {ptr, prefix_view};
  }

};

} // namespace epee
