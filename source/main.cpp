#define _CRT_SECURE_NO_WARNINGS

#include <string>
#include "AddonData.h"
#include "injector\injector.hpp"

AddonData gAddonData;

void (*CPostFX__Draw)() = nullptr;
static void OnInitEffectRuntime(reshade::api::effect_runtime *runtime);
static void OnDestroyEffectRuntime(reshade::api::effect_runtime *runtime);
static void OnInitSwapchain(reshade::api::swapchain *swapchain);
static void OnDestroyDevice(reshade::api::device *device);
static void OnInitCommandList(reshade::api::command_list *commandList);

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID)
{
    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            if (!reshade::register_addon(hinstDLL))
            {
                return false;
            }

            gAddonData.mAddonHandle = hinstDLL;

            reshade::register_event<reshade::addon_event::init_effect_runtime>(&OnInitEffectRuntime);
            reshade::register_event<reshade::addon_event::destroy_effect_runtime>(&OnDestroyEffectRuntime);
            reshade::register_event<reshade::addon_event::init_swapchain>(&OnInitSwapchain);
            reshade::register_event<reshade::addon_event::destroy_device>(&OnDestroyDevice);
            reshade::register_event<reshade::addon_event::init_command_list>(&OnInitCommandList);
        break;
        
        case DLL_PROCESS_DETACH:
            reshade::unregister_addon(hinstDLL);
        break;
    }

    return true;
}


void __declspec(naked) DrawComposite()
{
    _asm
    {
        push ebp
        mov ebp, esp
        sub esp, __LOCAL_SIZE
        pushad

        push [ebp+0x10]
        push [ebp+0xC]
        push [ebp+0x8]
        call CPostFX__Draw
    }

    gAddonData.mEffectRuntime->set_effects_state(true);
    gAddonData.mEffectRuntime->render_effects(gAddonData.mCommandList, gAddonData.mRenderTarget, gAddonData.mRenderTargetSrgb);

    _asm
    {
        popad
        mov esp, ebp
        pop ebp
        ret 0xC
    }
}

static void OnInitEffectRuntime(reshade::api::effect_runtime *runtime)
{
    runtime->set_effects_state(false);

    gAddonData.mEffectRuntime = runtime;
}

static void OnDestroyEffectRuntime(reshade::api::effect_runtime *runtime)
{
    if(gAddonData.mEffectRuntime == runtime)
    {
        gAddonData.mEffectRuntime = nullptr;
    }
}

static void OnInitSwapchain(reshade::api::swapchain *swapchain)
{
    //1070 crashes if we create a resource view from the first device swapchain
    static bool firstDevice = true;
    uint32_t baseAddress = (uint32_t)GetModuleHandle(L"GTAIV.exe");;
    if(firstDevice || !baseAddress)
    {
        firstDevice = false;
        return;
    }

    reshade::api::device *device = swapchain->get_device();

    if(!gAddonData.mRenderTarget.handle)
    {
        gAddonData.mRenderTarget.handle = swapchain->get_back_buffer(0).handle;

        reshade::api::resource_view_desc desc {};
        desc = device->get_resource_view_desc(gAddonData.mRenderTarget);
        desc.format = reshade::api::format_to_default_typed(desc.format, 1);

        device->create_resource_view(swapchain->get_back_buffer(0), reshade::api::resource_usage::render_target, desc, &gAddonData.mRenderTargetSrgb);
    }

    if(!CPostFX__Draw)
    {
        uint32_t signature = (baseAddress - 0x400000) + 0x608C34;
        switch(*(uint32_t*)signature)
        {
            //1040
            case 0x110FF300:
                injector::MakeCALL(baseAddress + 0x3C1634, DrawComposite, true);
                CPostFX__Draw = (void(*)())(baseAddress + 0x3BFE50);
            break;

            //1070
            case 0x1006E857:
                injector::MakeCALL(baseAddress + 0x483BF8, DrawComposite, true);
                CPostFX__Draw = (void(*)())(baseAddress + 0x482250);
            break;

            //1080  
            case 0x404B100F:
                injector::MakeCALL(baseAddress + 0x48E038, DrawComposite, true);
                CPostFX__Draw = (void(*)())(baseAddress + 0x48C690);
            break;

            default:
                std::string errorMessage = "Unsupported version. ";
                errorMessage += std::to_string(signature);

                MessageBoxA(0, errorMessage.c_str(), "EffectsBeforeUI Error", MB_OK);
                OnDestroyDevice(device);
                reshade::unregister_addon(gAddonData.mAddonHandle);
            break;
        }
    }
}

static void OnDestroyDevice(reshade::api::device *device)
{
    if(gAddonData.mRenderTarget.handle)
    {
        device->destroy_resource_view(gAddonData.mRenderTarget);
        gAddonData.mRenderTarget.handle = 0;
    }

    if(gAddonData.mRenderTargetSrgb.handle)
    {
        device->destroy_resource_view(gAddonData.mRenderTargetSrgb);
        gAddonData.mRenderTargetSrgb.handle = 0;
    }

}

static void OnInitCommandList(reshade::api::command_list *commandList)
{
    gAddonData.mCommandList = commandList;
}