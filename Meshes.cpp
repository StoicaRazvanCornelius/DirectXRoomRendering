
#include <Windows.h>
#include <mmsystem.h>
#include <d3dx9.h>
#include <dinput.h>
#include <dshow.h> 
#include "directxmath.h"  
#include "Camera.h"

#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "d3dx9.lib")
#pragma comment (lib, "dinput8.lib")
#pragma comment (lib, "dxguid.lib")

#define WM_GRAPHNOTIFY  WM_APP + 1   

#define ONE_SECOND 10000000
REFERENCE_TIME rtNow = 0 * ONE_SECOND;

struct meshPosStruct {
    float x;
    float y;
    float z;
};
struct meshRotStruct {
    float x;
    float y;
    float z;
};

using namespace DirectX;
LPDIRECT3D9             D3D           = NULL; 
LPDIRECT3DDEVICE9       d3dDevice     = NULL; 

LPD3DXMESH              Mesh          = NULL; 
meshPosStruct           pos           = { 0,-0.05f,0 };
meshPosStruct           rot           = { 0,0,0 };
LPD3DXMESH              RoomMesh          = NULL; 
D3DMATERIAL9*           MeshMaterials = NULL; 
D3DMATERIAL9*           RoomMeshMaterials = NULL; 
LPDIRECT3DTEXTURE9*     MeshTextures  = NULL; 
LPDIRECT3DTEXTURE9*     RoomMeshTextures  = NULL; 
DWORD                   NumMaterials = 0L;   
DWORD                   NumRoomMaterials = 0L;  
CXCamera *camera; 


LPDIRECTINPUT8			g_pDin;							
LPDIRECTINPUTDEVICE8	g_pDinKeyboard;					

POINT					g_SurfacePosition;				
BYTE					g_Keystate[256];				

LPDIRECTINPUTDEVICE8	g_pDinmouse;					
DIMOUSESTATE			g_pMousestate;					



IGraphBuilder* graphBuilder = NULL;
IMediaControl* mediaControl = NULL;
IMediaEventEx* mediaEvent = NULL;
IMediaSeeking* mediaSeeking = NULL;
HWND hWnd;
HDC hdc;


#pragma region d3dStuff

HRESULT InitD3D(HWND hWnd)
{
    if (NULL == (D3D = Direct3DCreate9(D3D_SDK_VERSION)))
        return E_FAIL;

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    if (FAILED(D3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &d3dpp, &d3dDevice)))
    {
        if (FAILED(D3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, hWnd,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &d3dpp, &d3dDevice)))
            return E_FAIL;
    }

    d3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
    d3dDevice->SetRenderState(D3DRS_AMBIENT, 0x002C2C2C);

    return S_OK;
}


void InitiateCamera()
{
    camera = new CXCamera(d3dDevice);
    D3DXVECTOR3 vEyePt(0.0f, 1.0f, -3.0f);
    D3DXVECTOR3 vLookatPt(0.0f, 0.5f, 0.0f);
    D3DXVECTOR3 vUpVec(0.0f, 1.0f, 0.0f);
    D3DXMATRIXA16 matView;

    camera->LookAtPos(&vEyePt, &vLookatPt, &vUpVec);
}

HRESULT InitGeometry()
{
    LPD3DXBUFFER pD3DXMtrlBuffer, roomBuffer;

    if (FAILED(D3DXLoadMeshFromX("manequin.x", D3DXMESH_SYSTEMMEM,
        d3dDevice, NULL,
        &pD3DXMtrlBuffer, NULL, &NumMaterials,
        &Mesh)))
    {
        if (FAILED(D3DXLoadMeshFromX("..\\manequin.x", D3DXMESH_SYSTEMMEM,
            d3dDevice, NULL,
            &pD3DXMtrlBuffer, NULL, &NumMaterials,
            &Mesh)))
        {
            MessageBox(NULL, "Could not find manequin.x", "Meshes.exe", MB_OK);
            return E_FAIL;
        }
    }

    if (FAILED(D3DXLoadMeshFromX("room.x", D3DXMESH_SYSTEMMEM,
        d3dDevice, NULL,
        &roomBuffer, NULL, &NumRoomMaterials,
        &RoomMesh)))
    {
       if (FAILED(D3DXLoadMeshFromX("..\\room.x", D3DXMESH_SYSTEMMEM,
            d3dDevice, NULL,
            &roomBuffer, NULL, &NumRoomMaterials,
            &RoomMesh)))
       {
            MessageBox(NULL, "Could not find room.x", "Meshes.exe", MB_OK);
            return E_FAIL;
       }
    }

    D3DXMATERIAL* d3dxMaterials = (D3DXMATERIAL*)pD3DXMtrlBuffer->GetBufferPointer();
    MeshMaterials = new D3DMATERIAL9[NumMaterials];
    MeshTextures = new LPDIRECT3DTEXTURE9[NumMaterials];

    for (DWORD i = 0; i < NumMaterials; i++)
    {
        MeshMaterials[i] = d3dxMaterials[i].MatD3D;
        MeshMaterials[i].Ambient = MeshMaterials[i].Diffuse;
        MeshTextures[i] = NULL;
        if (d3dxMaterials[i].pTextureFilename != NULL &&
            lstrlen(d3dxMaterials[i].pTextureFilename) > 0)
        {
            if (FAILED(D3DXCreateTextureFromFile(d3dDevice,
                d3dxMaterials[i].pTextureFilename,
                &MeshTextures[i])))
            {
                const TCHAR* strPrefix = TEXT("..\\");
                const int lenPrefix = lstrlen(strPrefix);
                TCHAR strTexture[MAX_PATH];
                lstrcpyn(strTexture, strPrefix, MAX_PATH);
                lstrcpyn(strTexture + lenPrefix, d3dxMaterials[i].pTextureFilename, MAX_PATH - lenPrefix);
                if (FAILED(D3DXCreateTextureFromFile(d3dDevice,
                    strTexture,
                    &MeshTextures[i])))
                {
                    MessageBox(NULL, "Could not find texture map", "Meshes.exe", MB_OK);
                }
            }
        }


    }

    D3DXMATERIAL* roomMaterials = (D3DXMATERIAL*)roomBuffer->GetBufferPointer();
    RoomMeshMaterials = new D3DMATERIAL9[NumRoomMaterials];
    RoomMeshTextures = new LPDIRECT3DTEXTURE9[NumRoomMaterials];

    for (DWORD i = 0; i < NumRoomMaterials; i++)
    {
        RoomMeshMaterials[i] = roomMaterials[i].MatD3D;

        RoomMeshMaterials[i].Ambient = RoomMeshMaterials[i].Diffuse;

        RoomMeshTextures[i] = NULL;
        if (roomMaterials[i].pTextureFilename != NULL &&
            lstrlen(roomMaterials[i].pTextureFilename) > 0)
        {
            if (FAILED(D3DXCreateTextureFromFile(d3dDevice,
                roomMaterials[i].pTextureFilename,
                &RoomMeshTextures[i])))
            {
                const TCHAR* strPrefix = TEXT("..\\");
                const int lenPrefix = lstrlen(strPrefix);
                TCHAR strTexture[MAX_PATH];
                lstrcpyn(strTexture, strPrefix, MAX_PATH);
                lstrcpyn(strTexture + lenPrefix, roomMaterials[i].pTextureFilename, MAX_PATH - lenPrefix);
                if (FAILED(D3DXCreateTextureFromFile(d3dDevice,
                    strTexture,
                    &RoomMeshTextures[i])))
                {
                    MessageBox(NULL, "Could not find texture map", "Meshes.exe", MB_OK);
                }
            }
        }
    }

    pD3DXMtrlBuffer->Release();
    roomBuffer->Release();
    
    LPDIRECT3DVERTEXBUFFER9 VertexBuffer = NULL;
    LPDIRECT3DVERTEXBUFFER9 RoomVertexBuffer = NULL;

    BYTE* Vertices = NULL;
    BYTE* RoomVertices = NULL;

    DWORD FVFVertexSize = D3DXGetFVFVertexSize(Mesh->GetFVF());
    DWORD RoomFVFVertexSize = D3DXGetFVFVertexSize(RoomMesh->GetFVF());

    Mesh->GetVertexBuffer(&VertexBuffer);
    RoomMesh->GetVertexBuffer(&RoomVertexBuffer);

    VertexBuffer->Lock(0, 0, (VOID**)&Vertices, D3DLOCK_DISCARD);
    RoomVertexBuffer->Lock(0, 0, (VOID**)&RoomVertices, D3DLOCK_DISCARD);

    VertexBuffer->Unlock();
    RoomVertexBuffer->Unlock();

    VertexBuffer->Release();
    RoomVertexBuffer->Release();

    InitiateCamera();

    return S_OK;
}

VOID Cleanup()
{
    if (MeshMaterials != NULL)
        delete[] MeshMaterials;

    if (RoomMeshMaterials != NULL)
        delete[] RoomMeshMaterials;

    if (MeshTextures)
    {
        for (DWORD i = 0; i < NumMaterials; i++)
        {
            if (MeshTextures[i])
                MeshTextures[i]->Release();
        }
        delete[] MeshTextures;
    }
    if (RoomMeshTextures)
    {
        for (DWORD i = 0; i < NumRoomMaterials; i++)
        {
            if (RoomMeshTextures[i])
                RoomMeshTextures[i]->Release();
        }
        delete[] RoomMeshTextures;
    }

    if (Mesh != NULL)
        Mesh->Release();
    if (RoomMesh != NULL)
        RoomMesh->Release();

    if (d3dDevice != NULL)
        d3dDevice->Release();

    if (D3D != NULL)
        D3D->Release();
}


VOID SetupWorldMatrix()
{
    D3DXMATRIXA16 matWorld;
    D3DXMatrixIdentity(&matWorld);
    d3dDevice->SetTransform(D3DTS_WORLD, &matWorld);
}

float distance;
int direction = 0;
int maximumStepDirection = 300;
int currentStep = 0;
void SetupViewMatrix()
{
    distance = 0.01 * direction;
    currentStep = currentStep + direction;
    if (currentStep > maximumStepDirection)
    {
        direction = -1;
    }
    if (currentStep < 0)
        direction = 1;
    camera->MoveForward(distance);
    camera->Update();
}


void SetupProjectionMatrix()
{
    D3DXMATRIXA16 matProj;
    D3DXMatrixPerspectiveFovLH(&matProj, D3DX_PI / 4, 1.0f, 1.0f, 100.0f);
    d3dDevice->SetTransform(D3DTS_PROJECTION, &matProj);
}

VOID SetupMatrices()
{

    SetupWorldMatrix();

    SetupViewMatrix();

    SetupProjectionMatrix();

}

VOID Render()
{
    d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        D3DCOLOR_XRGB(255, 255, 255), 1.0f, 0);

    if (SUCCEEDED(d3dDevice->BeginScene()))
    {
        SetupMatrices();
        D3DXMATRIXA16 matWorld;
        D3DXMatrixIdentity(&matWorld);
        D3DXMatrixTranslation(&matWorld, pos.x, pos.y, pos.z);
        FLOAT theta_x = XMConvertToRadians(rot.x);
        FLOAT theta_y = XMConvertToRadians(rot.y);
        FLOAT theta_z = XMConvertToRadians(rot.z);
        D3DXMATRIX g_Transform_Rotate_x;
        D3DXMATRIX g_Transform_Rotate_y;
        D3DXMATRIX g_Transform_Rotate_z;
        D3DXMatrixIdentity(&g_Transform_Rotate_x);
        D3DXMatrixIdentity(&g_Transform_Rotate_y);
        D3DXMatrixIdentity(&g_Transform_Rotate_z);

        D3DXMatrixRotationX(&g_Transform_Rotate_x, theta_x);
        D3DXMatrixRotationY(&g_Transform_Rotate_y, theta_y);
        D3DXMatrixRotationZ(&g_Transform_Rotate_z, theta_z);

        matWorld = g_Transform_Rotate_y * g_Transform_Rotate_z * g_Transform_Rotate_x * matWorld;
        d3dDevice->SetTransform(D3DTS_WORLD, &matWorld);
        for (DWORD i = 0; i < NumMaterials; i++)
        {
            d3dDevice->SetMaterial(&MeshMaterials[i]);
            d3dDevice->SetTexture(0, MeshTextures[i]);

            Mesh->DrawSubset(i);
        }
        
        D3DXMatrixIdentity(&matWorld);
        D3DXMatrixTranslation(&matWorld, -1, -0.75f, -1.5);
        d3dDevice->SetTransform(D3DTS_WORLD, &matWorld);
        
        for (DWORD i = 0; i < NumRoomMaterials; i++)
        {
            d3dDevice->SetMaterial(&RoomMeshMaterials[i]);
            d3dDevice->SetTexture(0, RoomMeshTextures[i]);

            RoomMesh->DrawSubset(i);
        }
        d3dDevice->EndScene();
    }

    d3dDevice->Present(NULL, NULL, NULL, NULL);
}
#pragma endregion



#pragma region input
HRESULT InitDInput(HINSTANCE hInstance, HWND hWnd)
{
    DirectInput8Create(hInstance,    
        DIRECTINPUT_VERSION,   
        IID_IDirectInput8,   
        (void**)&g_pDin,  
        NULL);   

    
    g_pDin->CreateDevice(GUID_SysKeyboard,   
        &g_pDinKeyboard,   
        NULL);   

    g_pDin->CreateDevice(GUID_SysMouse,
        &g_pDinmouse,  
        NULL); 

    g_pDinKeyboard->SetDataFormat(&c_dfDIKeyboard);

    g_pDinmouse->SetDataFormat(&c_dfDIMouse);

    g_pDinKeyboard->SetCooperativeLevel(hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);

    g_pDinmouse->SetCooperativeLevel(hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);

    return S_OK;
}
VOID DetectInput()
{
    g_pDinKeyboard->Acquire();
    g_pDinmouse->Acquire();

    g_pDinKeyboard->GetDeviceState(256, (LPVOID)g_Keystate);
    g_pDinmouse->GetDeviceState(sizeof(DIMOUSESTATE), (LPVOID)&g_pMousestate);

}

#pragma endregion


#pragma region directshow
HRESULT InitDirectShow(HWND hWnd)
{
    HRESULT hr = CoCreateInstance(CLSID_FilterGraph, NULL,
        CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&graphBuilder);

    hr = graphBuilder->QueryInterface(IID_IMediaControl, (void**)&mediaControl);
    hr = graphBuilder->QueryInterface(IID_IMediaEventEx, (void**)&mediaEvent);
    hr = graphBuilder->QueryInterface(IID_IMediaSeeking, (void**)&mediaSeeking);

    hr = graphBuilder->RenderFile(L"drag.wav", NULL);

    mediaEvent->SetNotifyWindow((OAHWND)hWnd, WM_GRAPHNOTIFY, 0);

    REFERENCE_TIME stop;
    mediaSeeking->GetDuration(&stop);
    mediaSeeking->SetPositions(0, AM_SEEKING_AbsolutePositioning, &stop, AM_SEEKING_AbsolutePositioning);
    mediaControl->Run();

    return S_OK;
}

void HandleGraphEvent()
{
    long evCode;
    LONG_PTR param1, param2;

    while (SUCCEEDED(mediaEvent->GetEvent(&evCode, &param1, &param2, 0)))
    {
        mediaEvent->FreeEventParams(evCode, param1, param2);
        switch (evCode)
        {
        case EC_COMPLETE:  
        case EC_USERABORT:
        case EC_ERRORABORT:
            PostQuitMessage(0);
            return;
        }
    }
}

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        Cleanup();
        PostQuitMessage(0);
        return 0;

    case WM_GRAPHNOTIFY:
        REFERENCE_TIME stop;
        mediaSeeking->GetDuration(&stop);
        mediaSeeking->SetPositions(0, AM_SEEKING_AbsolutePositioning, &stop, AM_SEEKING_AbsolutePositioning);
        mediaControl->Run();
        return 0;
        HandleGraphEvent();
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}


#pragma endregion

INT WINAPI WinMain( HINSTANCE hInst, HINSTANCE, LPSTR, INT )
{
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L, 
                      GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                      "D3D", NULL };
    RegisterClassEx( &wc );

    HWND hWnd = CreateWindow( "D3D", "D3D", 
                              WS_OVERLAPPEDWINDOW, 460, 40, 1000, 1000,
                              GetDesktopWindow(), NULL, wc.hInstance, NULL );

    HRESULT hr = CoInitialize(NULL);

    hdc = GetDC(hWnd);

    if( SUCCEEDED( InitD3D( hWnd ) ) )
    { 
       if( SUCCEEDED( InitGeometry() ) )
        {
            InitDInput(hInst, hWnd);

            ShowWindow( hWnd, SW_SHOWDEFAULT );
            UpdateWindow( hWnd );

            MSG msg; 
            ZeroMemory( &msg, sizeof(msg) );

            while( msg.message!=WM_QUIT )
            {
                if( PeekMessage( &msg, NULL, 0U, 0U, PM_REMOVE ) )
                {
                    TranslateMessage( &msg );
                    DispatchMessage( &msg );
                }
                else
                {
                    
                    DetectInput();    
                    Render();

                    #pragma region cameraInput
                    if (g_Keystate[DIK_ESCAPE] & 0x80) {
                        PostMessage(hWnd, WM_DESTROY, 0, 0);
                    }
                    if (g_Keystate[DIK_UP] & 0x80) {
                        camera->MoveForward(0.1f);
                        Render();
                    }
                    if (g_Keystate[DIK_DOWN] & 0x80) {
                        camera->MoveForward(-0.1f);
                        Render();
                    }
                    if (g_Keystate[DIK_RIGHT] & 0x80) {
                        camera->MoveRight(0.1f);
                        Render();
                    }
                    if (g_Keystate[DIK_LEFT] & 0x80) {
                        camera->MoveRight(-0.1f);
                        Render();
                    }

                    if (g_pMousestate.rgbButtons[1]) {
                        camera->RotateRight(g_pMousestate.lX / 100.0);
                        camera->RotateDown(g_pMousestate.lY / 100.0);
                        Render();
                    }
                    #pragma endregion

                    if (g_Keystate[DIK_ESCAPE] & 0x80) {
                        PostMessage(hWnd, WM_DESTROY, 0, 0);
                    }

                    #pragma region meshInput
                    if (g_Keystate[DIK_W] & 0x80) {
                        if (FAILED(InitDirectShow(hWnd)))
                            return 0;
                        pos
                            .z += 0.1f;
                        Render();
                    }
                    if (g_Keystate[DIK_S] & 0x80) {
                        if (FAILED(InitDirectShow(hWnd)))
                            return 0;
                        pos.z -= 0.1f;
                        Render();
                    }
                    if (g_Keystate[DIK_D] & 0x80) {
                        if (FAILED(InitDirectShow(hWnd)))
                            return 0;
                        pos.x += 0.1f;
                        Render();
                    }
                    if (g_Keystate[DIK_A] & 0x80) {
                        if (FAILED(InitDirectShow(hWnd)))
                            return 0;
                        pos.x -= 0.1f;
                        Render();
                    }
                    if (g_Keystate[DIK_Q] & 0x80) {
                        if (FAILED(InitDirectShow(hWnd)))
                            return 0;
                        pos.y += 0.1f;
                        Render();
                    }
                    if (g_Keystate[DIK_E] & 0x80) {
                        if (FAILED(InitDirectShow(hWnd)))
                            return 0;
                        pos.y -= 0.1f;
                        Render();
                    }
                    if (g_Keystate[DIK_NUMPAD8] & 0x80) {
                        rot.x += 10;
                        Render();
                    }
                    if (g_Keystate[DIK_NUMPAD2] & 0x80) {
                        rot.x -= 10;
                        Render();
                    }
                    if (g_Keystate[DIK_NUMPAD6] & 0x80) {
                        rot.y += 10;
                        Render();
                    }
                    if (g_Keystate[DIK_NUMPAD4] & 0x80) {
                        rot.y -= 10;
                        Render();
                    }
                    if (g_Keystate[DIK_NUMPAD7] & 0x80) {
                        rot.z -= 10;
                        Render();
                    }
                    if (g_Keystate[DIK_NUMPAD9] & 0x80) {
                        rot.z += 10;
                        Render();
                    }
                    #pragma endregion
                    
                }
            }
        }
    }

    CoUninitialize();

    UnregisterClass( "D3D", wc.hInstance );
    return 0;
}
