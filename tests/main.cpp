#include <boost/ut.hpp>

int main(int argc, const char** argv) {
  return static_cast<int>(
    boost::ut::cfg<boost::ut::override>.run(boost::ut::run_cfg{.report_errors = true, .argc = argc, .argv = argv}));
}
