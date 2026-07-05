#include "midnight/platform/SdlContext.hpp"

#include <SDL3/SDL.h>

#include <stdexcept>
#include <string>

namespace midnight {

SdlContext::SdlContext()
{
    SDL_SetAppMetadata("Midnight", "0.1.0", "dev.midnight.game");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
    }
}

SdlContext::~SdlContext()
{
    SDL_Quit();
}

}
