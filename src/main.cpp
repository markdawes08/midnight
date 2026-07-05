#include "midnight/core/Application.hpp"

#include <exception>
#include <iostream>

int main()
{
    try {
        midnight::Application app;
        return app.run();
    } catch (const std::exception& error) {
        std::cerr << "[Midnight] Fatal error: " << error.what() << '\n';
        return 1;
    }
}
