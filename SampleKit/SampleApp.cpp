#include "SampleApp.h"
#include "SampleWindowProc.h"

using namespace izanagi;
using namespace sample;

CSampleApp::CSampleApp()
{
    m_Device = IZ_NULL;
    m_Allocator = IZ_NULL;

    m_DebugFont = IZ_NULL;
    m_Pad = IZ_NULL;

    m_ScreenWidth = 1;
    m_ScreenHeight = 1;

    m_Flags.onLBtn = IZ_FALSE;
    m_Flags.onRBtn = IZ_FALSE;
}

CSampleApp::~CSampleApp()
{
    // 解放されているかチェック
    IZ_ASSERT(m_Device == IZ_NULL);
}

// 初期化
IZ_BOOL CSampleApp::Init(
    const izanagi::sys::WindowHandle& handle,
    const SSampleAppParams& params)
{
    IZ_BOOL ret = IZ_TRUE;

    m_ScreenWidth = params.screenWidth;
    m_ScreenHeight= params.screenHeight;

    void* nativeWndHandle = IZ_NULL;

    m_Allocator = params.allocator;
    VGOTO(ret = (m_Allocator != IZ_NULL), __EXIT__);

    // グラフィックスデバイス作成
    {
        IZ_ASSERT(params.allocatorForGraph != IZ_NULL);

        m_Device = izanagi::graph::CGraphicsDevice::CreateGraphicsDevice(
                        params.allocatorForGraph);
        VGOTO(ret = (m_Device != IZ_NULL), __EXIT__);
    }

    // プラットフォームごとのウインドウハンドルを取得
    nativeWndHandle = izanagi::sys::CSysWindow::GetNativeWindowHandle(handle);

    // グラフィックスデバイス設定
    izanagi::graph::SGraphicsDeviceInitParams gfxDevParams;
#ifdef __IZ_DX9__
    {
        gfxDevParams.hFocusWindow = (HWND)nativeWndHandle;
        gfxDevParams.hDeviceWindow = (HWND)nativeWndHandle;
        
        gfxDevParams.Windowed = IZ_TRUE;                        // 画面モード(ウインドウモード)

        gfxDevParams.BackBufferWidth = params.screenWidth;            // バックバッファの幅
        gfxDevParams.BackBufferHeight = params.screenHeight;        // バックバッファの高さ

        gfxDevParams.MultiSampleType = D3DMULTISAMPLE_NONE;    // マルチ・サンプリングの種類

        gfxDevParams.Adapter = D3DADAPTER_DEFAULT;
        gfxDevParams.DeviceType = D3DDEVTYPE_HAL;
        gfxDevParams.BehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;

        gfxDevParams.DepthStencilFormat = D3DFMT_D24S8;

        //gfxDevParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
        gfxDevParams.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    }
#elif __IZ_GLES2__
    {
        gfxDevParams.display = (EGLNativeDisplayType)izanagi::sys::CSysWindow::GetNativeDisplayHandle(handle);
        gfxDevParams.window = (EGLNativeWindowType)nativeWndHandle;

        gfxDevParams.screenWidth = params.screenWidth;
        gfxDevParams.screenHeight = params.screenHeight;

        gfxDevParams.rgba[0] = 8;
        gfxDevParams.rgba[1] = 8;
        gfxDevParams.rgba[2] = 8;
        gfxDevParams.rgba[3] = 8;

        gfxDevParams.depth = 16;
        gfxDevParams.stencil = 8;

        gfxDevParams.enableMultiSample = GL_FALSE;
    }
#elif __IZ_GLUT__
    {
        gfxDevParams.screenWidth = params.screenWidth;
        gfxDevParams.screenHeight = params.screenHeight;

        gfxDevParams.rgba[0] = 8;
        gfxDevParams.rgba[1] = 8;
        gfxDevParams.rgba[2] = 8;
        gfxDevParams.rgba[3] = 8;

        gfxDevParams.depth = 16;
        gfxDevParams.stencil = 8;

        gfxDevParams.enableMultiSample = GL_FALSE;
    }
#else
    IZ_C_ASSERT(IZ_FALSE);
#endif

    // デバイスリセット
    ret = m_Device->Reset((const void*)&gfxDevParams);
    VGOTO(ret, __EXIT__);

    // デバッグフォント初期化
    {
        m_DebugFont = izanagi::CDebugFont::CreateDebugFont(
                        m_Allocator,
                        m_Device);
        VGOTO(ret = (m_DebugFont != IZ_NULL), __EXIT__);
    }

#if defined(__IZ_DX9__)
    // 入力初期化
    {
        // パッド作成
        {
            izanagi::sys::SInputDeviceInitParam padInitParam =
            {
                params.instanceHandle,
                nativeWndHandle,
            };

#if 0
            m_Pad = izanagi::CPad::CreatePad(
                m_Allocator,
                &padInitParam,
                0.15f);
#else
            m_Pad = izanagi::sys::CPad::CreatePad(
                m_Allocator,
                IZ_NULL,
                0.15f);
#endif
        }
    }
#endif  // #if defined(__IZ_DX9__)

    // TODO
    // リセット用コールバックセット

    ret = InitInternal(
            m_Allocator,
            m_Device,
            m_Camera);
    VGOTO(ret, __EXIT__);

__EXIT__:
    if (!ret) {
        Release();
    }

    return ret;
}

// 開放
void CSampleApp::Release()
{
    ReleaseInternal();

    izanagi::IMemoryAllocator* allocator = IZ_NULL;
    if (m_Device) {
        allocator = m_Device->GetDeviceAllocator();
    }
    
    SAFE_RELEASE(m_DebugFont);
    SAFE_RELEASE(m_Pad);

    m_Device->Terminate();

    SAFE_RELEASE(m_Device);
}

// 更新.
void CSampleApp::Update()
{
    UpdateInternal(m_Device);

    m_Camera.Update();
}

// 描画.
void CSampleApp::Render()
{
    IZ_ASSERT(m_Device != IZ_NULL);

    IZ_COLOR bgColor = GetBgColor();

    m_Device->BeginRender(
        izanagi::graph::E_GRAPH_CLEAR_FLAG_ALL,
        bgColor, 1.0f, 0);
    {
        RenderInternal(m_Device);

        if (m_isDispFramerate) {
            IZ_ASSERT(m_DebugFont != IZ_NULL);

            // 時間表示
            if (m_Device->Begin2D()) {
                m_DebugFont->Begin(m_Device);

                {
                    IZ_FLOAT time = GetTimer(0).GetTime();
                    IZ_FLOAT fps = 1000.0f / time;

                    m_DebugFont->DBPrint(
                        m_Device,
                        "%.2f[ms] %.2f[fps]\n",
                        time, fps);
                }
                {
                    IZ_FLOAT time = GetTimer(1).GetTime();
                    IZ_FLOAT fps = 1000.0f / time;

                    m_DebugFont->DBPrint(
                        m_Device,
                        "%.2f[ms] %.2f[fps]\n",
                        time, fps);
                }

                m_DebugFont->End();

                m_Device->End2D();
            }
        }
    }
    m_Device->EndRender();
}

// V同期.
void CSampleApp::Present(void* nativeParam)
{
    IZ_ASSERT(m_Device != IZ_NULL);
    m_Device->Present(nativeParam);
}

void CSampleApp::Idle(void* nativeParam)
{
    GetTimer(0).Begin();
    GetTimer(1).Begin();

    Update();
    Render();

    GetTimer(1).End();

    Present(nativeParam);

    GetTimer(0).End();
}

namespace {
    inline IZ_FLOAT _NormalizeHorizontal(
        CSampleApp* app,
        IZ_INT x)
    {
        IZ_UINT width = app->GetScreenWidth();
        IZ_FLOAT ret = (2.0f * x - width) / (IZ_FLOAT)width;
        return ret;
    }

    inline IZ_FLOAT _NormalizeVertical(
        CSampleApp* app,
        IZ_INT y)
    {
        IZ_UINT height = app->GetScreenHeight();
        IZ_FLOAT ret = (height - 2.0f * y) / (IZ_FLOAT)height;
        return ret;
    }
}

void CSampleApp::OnMouseMove(const izanagi::CIntPoint& point)
{
    if (m_isEnableCamera) {
        if (m_Flags.onLBtn) {
            if (m_PrevPoint.x != point.x || m_PrevPoint.y != point.y)
            {
                izanagi::CFloatPoint p1(
                    _NormalizeHorizontal(this, m_PrevPoint.x),
                    _NormalizeVertical(this, m_PrevPoint.y));

                izanagi::CFloatPoint p2(
                    _NormalizeHorizontal(this, point.x),
                    _NormalizeVertical(this, point.y));

                GetCamera().Rotate(p1, p2);
            }
        }
        else if (m_Flags.onRBtn) {
            float fOffsetX = (float)(m_PrevPoint.x - point.x);
            fOffsetX *= 0.5f;

            float fOffsetY = (float)(m_PrevPoint.y - point.y);
            fOffsetY *= 0.5f;

            GetCamera().Move(fOffsetX, fOffsetY);
        }
    }

    OnMouseMoveInternal(point);

    m_PrevPoint = point;
}

// スクリーン幅取得
IZ_UINT CSampleApp::GetScreenWidth() const
{
    IZ_ASSERT(m_Device != IZ_NULL);
    return m_ScreenWidth;
}

// スクリーン高さ取得
IZ_UINT CSampleApp::GetScreenHeight() const
{
    IZ_ASSERT(m_Device != IZ_NULL);
    return m_ScreenHeight;
}

// タイマ取得.
izanagi::sys::CTimer& CSampleApp::GetTimer(IZ_UINT idx)
{
    IZ_ASSERT(idx < SAMPLE_TIMER_NUM);
    return m_Timer[idx];
}

// カメラ取得.
CSampleCamera& CSampleApp::GetCamera()
{
    return m_Camera;
}

// 背景色取得.
IZ_COLOR CSampleApp::GetBgColor() const
{
    static const IZ_COLOR bgColor = IZ_COLOR_RGBA(0, 128, 255, 255);
    return bgColor;
}
