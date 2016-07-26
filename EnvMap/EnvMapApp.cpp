#include <vector>
#include "EnvMapApp.h"

CEnvMapApp::CEnvMapApp()
{
    m_Cube = IZ_NULL;
    m_Img = IZ_NULL;

    izanagi::math::SMatrix44::SetUnit(m_L2W);
}

CEnvMapApp::~CEnvMapApp()
{
}

// 初期化.
IZ_BOOL CEnvMapApp::InitInternal(
    izanagi::IMemoryAllocator* allocator,
    izanagi::graph::CGraphicsDevice* device,
    izanagi::sample::CSampleCamera& camera)
{
    IZ_BOOL result = IZ_TRUE;

    IZ_UINT flag = izanagi::E_DEBUG_MESH_VTX_FORM_POS;

    // Cube
    {
        m_Cube = izanagi::sample::CSampleEnvBox::CreateSampleEnvBox(
            allocator,
            device);
        VGOTO(result = (m_Cube != IZ_NULL), __EXIT__);
    }

    // テクスチャ
    {
        izanagi::CFileInputStream in;
        VGOTO(result = in.Open("data/EnvMap.img"), __EXIT__);

        m_Img = izanagi::CImage::CreateImage(
                    allocator,
                    device,
                    &in);
        VGOTO(result = (m_Img != IZ_NULL), __EXIT__);
    }

    // シェーダ
    {
        izanagi::CFileInputStream in;
        VGOTO(result = in.Open("shader/vs_equirect.glsl"), __EXIT__);

        std::vector<uint8_t> buf(in.GetSize() + 1);
        in.Read(&buf[0], 0, in.GetSize());

        m_vs = device->CreateVertexShader(&buf[0]);

        VGOTO(result = (m_vs != IZ_NULL), __EXIT__);
    }
    {
        izanagi::CFileInputStream in;
        VGOTO(result = in.Open("shader/fs_equirect.glsl"), __EXIT__);

        std::vector<uint8_t> buf(in.GetSize() + 1);
        in.Read(&buf[0], 0, in.GetSize());

        m_fs = device->CreatePixelShader(&buf[0]);

        VGOTO(result = (m_fs != IZ_NULL), __EXIT__);
    }
    {
        m_shd = device->CreateShaderProgram();
        m_shd->AttachVertexShader(m_vs);
        m_shd->AttachPixelShader(m_fs);
    }

    // カメラ
    camera.Init(
        izanagi::math::CVector4(0.0f, 0.0f,  0.0f, 1.0f),
        izanagi::math::CVector4(0.0f, 0.0f, -1.0f, 1.0f),
        izanagi::math::CVector4(0.0f, 1.0f,  0.0f, 1.0f),
        1.0f,
        500.0f,
        izanagi::math::CMath::Deg2Rad(60.0f),
        (IZ_FLOAT)device->GetBackBufferWidth() / device->GetBackBufferHeight());
    camera.Update();

__EXIT__:
    if (!result) {
        ReleaseInternal();
    }

    return IZ_TRUE;
}

// 解放.
void CEnvMapApp::ReleaseInternal()
{
    SAFE_RELEASE(m_Cube);
    SAFE_RELEASE(m_Img);

    SAFE_RELEASE(m_shd);
    SAFE_RELEASE(m_vs);
    SAFE_RELEASE(m_fs);
}

// 更新.
void CEnvMapApp::UpdateInternal(izanagi::graph::CGraphicsDevice* device)
{
    GetCamera().Update();

    izanagi::math::SMatrix44::SetScale(m_L2W, 100.0f, 100.0f, 100.0f);

    // カメラの位置にあわせて移動する
    izanagi::math::SMatrix44::Trans(
        m_L2W,
        m_L2W,
        GetCamera().GetParam().pos);
}

namespace {
    inline void _SetShaderParam(
        izanagi::shader::CShaderBasic* shader,
        const char* name,
        const void* value,
        IZ_UINT bytes)
    {
        izanagi::shader::IZ_SHADER_HANDLE handle = shader->GetParameterByName(name);
        IZ_ASSERT(handle != 0);

        shader->SetParamValue(
            handle,
            value,
            bytes);
    }
}

// 描画.
void CEnvMapApp::RenderInternal(izanagi::graph::CGraphicsDevice* device)
{
    izanagi::sample::CSampleCamera& camera = GetCamera();

    static IZ_INT texIdx[] = {
        1,
        3,
        4,
    };

    static IZ_CHAR* name[] = {
        "Cube",
        "Latitude-Longitude",
        "Angular",
    };

    device->SetRenderState(
        izanagi::graph::E_GRAPH_RS_CULLMODE,
        izanagi::graph::E_GRAPH_CULL_NONE);

    device->SetTexture(0, m_Img->GetTexture(texIdx[m_Idx]));

    device->SetShaderProgram(m_shd);

    auto mtxW2C = camera.GetParam().mtxW2C;

    auto hL2W = m_shd->GetHandleByName("g_mL2W");
    m_shd->SetMatrix(device, hL2W, m_L2W);

    auto hW2C = m_shd->GetHandleByName("g_mW2C");
    m_shd->SetMatrix(device, hW2C, mtxW2C);

    auto hEye = m_shd->GetHandleByName("g_vEye");
    m_shd->SetVector(device, hEye, camera.GetParam().pos);

    m_Cube->Render(device);

    if (device->Begin2D()) {
        izanagi::CDebugFont* debugFont = GetDebugFont();

        debugFont->Begin(device, 0, izanagi::CDebugFont::FONT_SIZE * 2);

        debugFont->DBPrint(
            device, 
            "%s\n",
            name[m_Idx]);

        debugFont->End();

        device->End2D();
    }
}

IZ_BOOL CEnvMapApp::OnKeyDown(izanagi::sys::E_KEYBOARD_BUTTON key)
{
    return IZ_TRUE;
}
