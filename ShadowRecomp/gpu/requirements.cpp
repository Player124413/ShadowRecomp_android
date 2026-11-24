#ifdef SHADOW_RECOMP_D3D12
#include "requirements.h"

#include <plume_d3d12.h>
#include <os/logger.h>

bool CheckD3D12DeviceRequirements(plume::RenderDevice* device)
{
    ID3D12Device* d3d = static_cast<plume::D3D12Device*>(device)->d3d;
    if (d3d == nullptr)
    {
        return false;
    }

    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_0 };
    if (FAILED(d3d->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel))) ||
        shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_0)
    {
        LOG_ERROR("D3D12: Shader Model 6.0 not supported");
        return false;
    }

    // Some GPUs have Shader Model 6.0 but still crash (tier 1 binding)
    D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
    if (FAILED(d3d->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options))) ||
        options.ResourceBindingTier < D3D12_RESOURCE_BINDING_TIER_2)
    {
        LOG_ERROR("D3D12: Resource binding tier 2 not supported");
        return false;
    }
    return true;
}
#endif
