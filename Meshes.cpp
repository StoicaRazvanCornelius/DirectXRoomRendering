
#include <Windows.h>
#include <mmsystem.h>
#include <d3dx9.h>
#include <dinput.h>
#include <dshow.h>   
#include "Camera.h"

#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "d3dx9.lib")
#pragma comment (lib, "dinput8.lib")
#pragma comment (lib, "dxguid.lib")

//We define an event id 
#define WM_GRAPHNOTIFY  WM_APP + 1   

#define ONE_SECOND 10000000
REFERENCE_TIME rtNow = 0 * ONE_SECOND;

struct meshPosStruct {
    float x;
    float y;
    float z;
};
//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
LPDIRECT3D9             D3D           = NULL; // Used to create the D3DDevice
LPDIRECT3DDEVICE9       d3dDevice     = NULL; // Our rendering device

LPD3DXMESH              Mesh          = NULL; // Our mesh object in sysmem
meshPosStruct           pos           = { 0,-0.05f,0 };
LPD3DXMESH              RoomMesh          = NULL; // Our mesh object in sysmem
D3DMATERIAL9*           MeshMaterials = NULL; // Materials for our mesh
D3DMATERIAL9*           RoomMeshMaterials = NULL; // Materials for our mesh
LPDIRECT3DTEXTURE9*     MeshTextures  = NULL; // Textures for our mesh
LPDIRECT3DTEXTURE9*     RoomMeshTextures  = NULL; // Textures for our mesh
DWORD                   NumMaterials = 0L;   // Number of mesh materials
DWORD                   NumRoomMaterials = 0L;   // Number of mesh materials
CXCamera *camera; 


LPDIRECTINPUT8			g_pDin;							// the pointer to our DirectInput interface
LPDIRECTINPUTDEVICE8	g_pDinKeyboard;					// the pointer to the keyboard device

POINT					g_SurfacePosition;				// The position of the surface
BYTE					g_Keystate[256];				// the storage for the key-information

LPDIRECTINPUTDEVICE8	g_pDinmouse;					// the pointer to the mouse device
DIMOUSESTATE			g_pMousestate;					// the storage for the mouse-information



IGraphBuilder* graphBuilder = NULL;
//Help us to start/stop the play
IMediaControl* mediaControl = NULL;
//We receie events in case something happened - during playing, stoping, errors etc..
IMediaEventEx* mediaEvent = NULL;
//We can use to fast forward, revert etc..
IMediaSeeking* mediaSeeking = NULL;
HWND hWnd;
HDC hdc;


#pragma region d3dStuff

//-----------------------------------------------------------------------------
// Name: InitD3D()
// Desc: Initializes Direct3D
//-----------------------------------------------------------------------------
HRESULT InitD3D(HWND hWnd)
{
    // Create the D3D object.
    if (NULL == (D3D = Direct3DCreate9(D3D_SDK_VERSION)))
        return E_FAIL;

    // Set up the structure used to create the D3DDevice. Since we are now
    // using more complex geometry, we will create a device with a zbuffer.
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

    // Create the D3DDevice
    if (FAILED(D3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &d3dpp, &d3dDevice)))
    {
        if (FAILED(D3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, hWnd,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING,
            &d3dpp, &d3dDevice)))
            return E_FAIL;
    }

    // Turn on the zbuffer
    d3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);

    // Turn on ambient lighting 
    d3dDevice->SetRenderState(D3DRS_AMBIENT, 0x002C2C2C);


    return S_OK;
}


void InitiateCamera()
{
    camera = new CXCamera(d3dDevice);

    // Set up our view matrix. A view matrix can be defined given an eye point,
    // a point to lookat, and a direction for which way is up. Here, we set the
    // eye five units back along the z-axis and up three units, look at the 
    // origin, and define "up" to be in the y-direction.
    D3DXVECTOR3 vEyePt(0.0f, 1.0f, -3.0f);
    D3DXVECTOR3 vLookatPt(0.0f, 0.5f, 0.0f);
    D3DXVECTOR3 vUpVec(0.0f, 1.0f, 0.0f);
    D3DXMATRIXA16 matView;

    camera->LookAtPos(&vEyePt, &vLookatPt, &vUpVec);
}

//-----------------------------------------------------------------------------
// Name: InitGeometry()
// Desc: Load the mesh and build the material and texture arrays
//-----------------------------------------------------------------------------
HRESULT InitGeometry()
{
    LPD3DXBUFFER pD3DXMtrlBuffer, roomBuffer;

    // Load the mesh from the specified file
    if (FAILED(D3DXLoadMeshFromX("manequin.x", D3DXMESH_SYSTEMMEM,
        d3dDevice, NULL,
        &pD3DXMtrlBuffer, NULL, &NumMaterials,
        &Mesh)))
    {
        // If model is not in current folder, try parent folder
        if (FAILED(D3DXLoadMeshFromX("..\\manequin.x", D3DXMESH_SYSTEMMEM,
            d3dDevice, NULL,
            &pD3DXMtrlBuffer, NULL, &NumMaterials,
            &Mesh)))
        {
            MessageBox(NULL, "Could not find manequin.x", "Meshes.exe", MB_OK);
            return E_FAIL;
        }
    }

    // Load the mesh from the specified file
    if (FAILED(D3DXLoadMeshFromX("room.x", D3DXMESH_SYSTEMMEM,
        d3dDevice, NULL,
        &roomBuffer, NULL, &NumRoomMaterials,
        &RoomMesh)))
    {
        // If model is not in current folder, try parent folder
        if (FAILED(D3DXLoadMeshFromX("..\\room.x", D3DXMESH_SYSTEMMEM,
            d3dDevice, NULL,
            &roomBuffer, NULL, &NumRoomMaterials,
            &RoomMesh)))
        {
            MessageBox(NULL, "Could not find room.x", "Meshes.exe", MB_OK);
            return E_FAIL;
        }
    }

    // We need to extract the material properties and texture names from the 
        // pD3DXMtrlBuffer
    D3DXMATERIAL* d3dxMaterials = (D3DXMATERIAL*)pD3DXMtrlBuffer->GetBufferPointer();
    MeshMaterials = new D3DMATERIAL9[NumMaterials];
    MeshTextures = new LPDIRECT3DTEXTURE9[NumMaterials];

    for (DWORD i = 0; i < NumMaterials; i++)
    {
        // Copy the material
        MeshMaterials[i] = d3dxMaterials[i].MatD3D;

        // Set the ambient color for the material (D3DX does not do this)
        MeshMaterials[i].Ambient = MeshMaterials[i].Diffuse;

        //MeshMaterials[i].Emissive.b += 0.6;
        //MeshMaterials[i].Emissive.g += 0.6;
        //MeshMaterials[i].Emissive.r += 0.6;
        MeshTextures[i] = NULL;
        if (d3dxMaterials[i].pTextureFilename != NULL &&
            lstrlen(d3dxMaterials[i].pTextureFilename) > 0)
        {
            // Create the texture
            if (FAILED(D3DXCreateTextureFromFile(d3dDevice,
                d3dxMaterials[i].pTextureFilename,
                &MeshTextures[i])))
            {
                // If texture is not in current folder, try parent folder
                const TCHAR* strPrefix = TEXT("..\\");
                const int lenPrefix = lstrlen(strPrefix);
                TCHAR strTexture[MAX_PATH];
                lstrcpyn(strTexture, strPrefix, MAX_PATH);
                lstrcpyn(strTexture + lenPrefix, d3dxMaterials[i].pTextureFilename, MAX_PATH - lenPrefix);
                // If texture is not in current folder, try parent folder
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
        // Copy the material
        RoomMeshMaterials[i] = roomMaterials[i].MatD3D;

        // Set the ambient color for the material (D3DX does not do this)
        RoomMeshMaterials[i].Ambient = RoomMeshMaterials[i].Diffuse;

        // RoomMeshMaterials[i].Emissive.r -= 0.1;
        // RoomMeshMaterials[i].Emissive.g -= 0.1;
        // RoomMeshMaterials[i].Emissive.b -= 0.1;

        RoomMeshTextures[i] = NULL;
        if (roomMaterials[i].pTextureFilename != NULL &&
            lstrlen(roomMaterials[i].pTextureFilename) > 0)
        {
            // Create the texture
            if (FAILED(D3DXCreateTextureFromFile(d3dDevice,
                roomMaterials[i].pTextureFilename,
                &RoomMeshTextures[i])))
            {
                // If texture is not in current folder, try parent folder
                const TCHAR* strPrefix = TEXT("..\\");
                const int lenPrefix = lstrlen(strPrefix);
                TCHAR strTexture[MAX_PATH];
                lstrcpyn(strTexture, strPrefix, MAX_PATH);
                lstrcpyn(strTexture + lenPrefix, roomMaterials[i].pTextureFilename, MAX_PATH - lenPrefix);
                // If texture is not in current folder, try parent folder
                if (FAILED(D3DXCreateTextureFromFile(d3dDevice,
                    strTexture,
                    &RoomMeshTextures[i])))
                {
                    MessageBox(NULL, "Could not find texture map", "Meshes.exe", MB_OK);
                }
            }
        }
    }

    // Done with the material buffer
    pD3DXMtrlBuffer->Release();
    roomBuffer->Release();
    //Declare object of type VertexBuffer
    LPDIRECT3DVERTEXBUFFER9 VertexBuffer = NULL;
    LPDIRECT3DVERTEXBUFFER9 RoomVertexBuffer = NULL;

    //The pointer to the current element in the array in VertexBuffer
    BYTE* Vertices = NULL;
    BYTE* RoomVertices = NULL;

    //Get the size of one element in the array of VertexBuffer 
    DWORD FVFVertexSize = D3DXGetFVFVertexSize(Mesh->GetFVF());
    DWORD RoomFVFVertexSize = D3DXGetFVFVertexSize(RoomMesh->GetFVF());

    //Get the VertexBuffer
    Mesh->GetVertexBuffer(&VertexBuffer);
    RoomMesh->GetVertexBuffer(&RoomVertexBuffer);

    //Get the address of the first element in the array of VertexBuffer
    VertexBuffer->Lock(0, 0, (VOID**)&Vertices, D3DLOCK_DISCARD);
    RoomVertexBuffer->Lock(0, 0, (VOID**)&RoomVertices, D3DLOCK_DISCARD);
    /*
        Avem access
    */

    VertexBuffer->Unlock();
    RoomVertexBuffer->Unlock();

    VertexBuffer->Release();
    RoomVertexBuffer->Release();

    InitiateCamera();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Cleanup()
// Desc: Releases all previously initialized objects
//-----------------------------------------------------------------------------
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
    // For our world matrix, we will just leave it as the identity
    D3DXMATRIXA16 matWorld;
    D3DXMatrixIdentity(&matWorld);
    //this rotates
    //D3DXMatrixRotationY(&matWorld, timeGetTime() / 1000.0f);
    d3dDevice->SetTransform(D3DTS_WORLD, &matWorld);
}

float distance;
int direction = 0;//1 forward, -1 backward
int maximumStepDirection = 300;
int currentStep = 0;
void SetupViewMatrix()
{

    //Below is a simulation of mooving forward - backwards. This is done by calling the camera method;
    distance = 0.01 * direction;
    currentStep = currentStep + direction;
    if (currentStep > maximumStepDirection)
    {
        direction = -1;
    }
    if (currentStep < 0)
        direction = 1;
    camera->MoveForward(distance);

    //Each time you render you must call update camera
    //By calling camera update the View Matrix is set;
    camera->Update();
}


void SetupProjectionMatrix()
{
    D3DXMATRIXA16 matProj;
    D3DXMatrixPerspectiveFovLH(&matProj, D3DX_PI / 4, 1.0f, 1.0f, 100.0f);
    d3dDevice->SetTransform(D3DTS_PROJECTION, &matProj);
}

//-----------------------------------------------------------------------------
// Name: SetupMatrices()
// Desc: Sets up the world, view, and projection transform matrices.
//-----------------------------------------------------------------------------
VOID SetupMatrices()
{

    SetupWorldMatrix();

    SetupViewMatrix();

    SetupProjectionMatrix();

}

//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Draws the scene
//-----------------------------------------------------------------------------
VOID Render()
{
    // Clear the backbuffer and the zbuffer
    d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
        D3DCOLOR_XRGB(255, 255, 255), 1.0f, 0);

    // Begin the scene
    if (SUCCEEDED(d3dDevice->BeginScene()))
    {
        // Setup the world, view, and projection matrices
        SetupMatrices();

        D3DXMATRIXA16 matWorld;
        D3DXMatrixIdentity(&matWorld);
        D3DXMatrixTranslation(&matWorld, pos.x, pos.y, pos.z);
        d3dDevice->SetTransform(D3DTS_WORLD, &matWorld);


        for (DWORD i = 0; i < NumMaterials; i++)
        {
            // Set the material and texture for this subset
            d3dDevice->SetMaterial(&MeshMaterials[i]);
            d3dDevice->SetTexture(0, MeshTextures[i]);

            // Draw the mesh subset
            Mesh->DrawSubset(i);
        }


        //Shift world to render the room properly
        D3DXMatrixIdentity(&matWorld);
        D3DXMatrixTranslation(&matWorld, -1, -0.75f, -1.5);
        d3dDevice->SetTransform(D3DTS_WORLD, &matWorld);

        for (DWORD i = 0; i < NumRoomMaterials; i++)
        {
            // Set the material and texture for this subset
            d3dDevice->SetMaterial(&RoomMeshMaterials[i]);
            d3dDevice->SetTexture(0, RoomMeshTextures[i]);

            // Draw the mesh subset
            RoomMesh->DrawSubset(i);
        }
        // End the scene
        d3dDevice->EndScene();
    }

    // Present the backbuffer contents to the display
    d3dDevice->Present(NULL, NULL, NULL, NULL);
}
#pragma endregion



#pragma region input
HRESULT InitDInput(HINSTANCE hInstance, HWND hWnd)
{
    // create the DirectInput interface
    DirectInput8Create(hInstance,    // the handle to the application
        DIRECTINPUT_VERSION,    // the compatible version
        IID_IDirectInput8,    // the DirectInput interface version
        (void**)&g_pDin,    // the pointer to the interface
        NULL);    // COM stuff, so we'll set it to NULL

    // create the keyboard device
    g_pDin->CreateDevice(GUID_SysKeyboard,    // the default keyboard ID being used
        &g_pDinKeyboard,    // the pointer to the device interface
        NULL);    // COM stuff, so we'll set it to NULL

    // create the mouse device
    g_pDin->CreateDevice(GUID_SysMouse,
        &g_pDinmouse,  // the pointer to the device interface
        NULL); // COM stuff, so we'll set it to NULL

    // set the data format to keyboard format
    g_pDinKeyboard->SetDataFormat(&c_dfDIKeyboard);

    // set the data format to mouse format
    g_pDinmouse->SetDataFormat(&c_dfDIMouse);

    // set the control we will have over the keyboard
    g_pDinKeyboard->SetCooperativeLevel(hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);

    // set the control we will have over the mouse
    g_pDinmouse->SetCooperativeLevel(hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);

    return S_OK;
}
VOID DetectInput()
{
    // get access if we don't have it already
    g_pDinKeyboard->Acquire();
    g_pDinmouse->Acquire();

    // get the input data
    g_pDinKeyboard->GetDeviceState(256, (LPVOID)g_Keystate);
    g_pDinmouse->GetDeviceState(sizeof(DIMOUSESTATE), (LPVOID)&g_pMousestate);


}

#pragma endregion


#pragma region directshow
HRESULT InitDirectShow(HWND hWnd)
{
    //Create Filter Graph   
    HRESULT hr = CoCreateInstance(CLSID_FilterGraph, NULL,
        CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&graphBuilder);

    //Create Media Control and Events   
    hr = graphBuilder->QueryInterface(IID_IMediaControl, (void**)&mediaControl);
    hr = graphBuilder->QueryInterface(IID_IMediaEventEx, (void**)&mediaEvent);
    hr = graphBuilder->QueryInterface(IID_IMediaSeeking, (void**)&mediaSeeking);

    //Load a file   
    hr = graphBuilder->RenderFile(L"drag.wav", NULL);

    //Set window for events  - basically we tell our event in case you raise an event use the following event id.
    mediaEvent->SetNotifyWindow((OAHWND)hWnd, WM_GRAPHNOTIFY, 0);

    //Rewind+Play media control   
    REFERENCE_TIME stop;
    mediaSeeking->GetDuration(&stop);
    mediaSeeking->SetPositions(0, AM_SEEKING_AbsolutePositioning, &stop, AM_SEEKING_AbsolutePositioning);
    mediaControl->Run();


    return S_OK;
}

void HandleGraphEvent()
{
    // Get all the events   
    long evCode;
    LONG_PTR param1, param2;

    while (SUCCEEDED(mediaEvent->GetEvent(&evCode, &param1, &param2, 0)))
    {
        mediaEvent->FreeEventParams(evCode, param1, param2);
        switch (evCode)
        {
        case EC_COMPLETE:  // Fall through.   
        case EC_USERABORT: // Fall through.   
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
        //Rewind+Play media control   
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
//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: The application's entry point
//-----------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE hInst, HINSTANCE, LPSTR, INT )
{
    // Register the window class
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, MsgProc, 0L, 0L, 
                      GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
                      "D3D", NULL };
    RegisterClassEx( &wc );

    // Create the application's window
    HWND hWnd = CreateWindow( "D3D", "D3D", 
                              WS_OVERLAPPEDWINDOW, 460, 40, 1000, 1000,
                              GetDesktopWindow(), NULL, wc.hInstance, NULL );

    HRESULT hr = CoInitialize(NULL);

    hdc = GetDC(hWnd);

    // Initialize Direct3D
    if( SUCCEEDED( InitD3D( hWnd ) ) )
    { 
       // Create the scene geometry
        if( SUCCEEDED( InitGeometry() ) )
        {
            InitDInput(hInst, hWnd);

            // Show the window
            ShowWindow( hWnd, SW_SHOWDEFAULT );
            UpdateWindow( hWnd );

            // Enter the message loop
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
                    
                    //No message to process?
                    // Then do your game stuff here
                    DetectInput();    // update the input data before rendering
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
                        pos.z += 0.1f;
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
                    #pragma endregion
                    
                    
                    


                }
            }
        }
    }

    CoUninitialize();

    UnregisterClass( "D3D", wc.hInstance );
    return 0;
}



