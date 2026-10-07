#pragma once
#include <cmath>
#include <iostream>
#include <stdexcept>
#define CHECK(...) do { if(!(__VA_ARGS__)) throw std::runtime_error(std::string(__FILE__)+":"+std::to_string(__LINE__)+": " #__VA_ARGS__); } while(false)
template<class F> void rejects(F f) { bool caught=false; try { f(); } catch(const std::invalid_argument&) { caught=true; } CHECK(caught); }
