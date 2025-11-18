#pragma once
#include <string>
#include <chrono>

namespace tools {

using namespace std::literals;

std::string friendly_duration(std::chrono::nanoseconds dur);

}
