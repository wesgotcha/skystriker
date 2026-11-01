#include "App.hpp"

int main(int argc, char** argv) {
    sky::App& a = sky::App::I();
    if (!a.init(argc, argv)) return 1;
    a.run();
    a.shutdown();
    return 0;
}
