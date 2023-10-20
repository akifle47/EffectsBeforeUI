#pragma once
#include "reshade\reshade.hpp"

struct AddonData
{
    reshade::api::effect_runtime *mEffectRuntime = nullptr;
    reshade::api::command_list *mCommandList = nullptr;
    reshade::api::resource_view mRenderTarget {};
    reshade::api::resource_view mRenderTargetSrgb {};
    HINSTANCE mAddonHandle = 0;
};