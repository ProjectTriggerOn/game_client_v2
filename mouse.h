//--------------------------------------------------------------------------------------
// File: mouse.h
//
// �֗��ȃ}�E�X���W���[��
//
//--------------------------------------------------------------------------------------
// 2020/02/11
//     DirectXTK���A�Ȃ񂿂����C����p�ɃV�F�C�v�A�b�v����
//
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
// http://go.microsoft.com/fwlink/?LinkID=615561
//--------------------------------------------------------------------------------------
#ifndef MOUSE_H
#define MOUSE_H
#pragma once


#include <windows.h>
#include <memory>


// �}�E�X���[�h
typedef enum Mouse_PositionMode_tag
{
    MOUSE_POSITION_MODE_ABSOLUTE, // ��΍��W���[�h
    MOUSE_POSITION_MODE_RELATIVE, // ���΍��W���[�h
} Mouse_PositionMode;


// �}�E�X��ԍ\����
typedef struct MouseState_tag
{
    bool leftButton;
    bool middleButton;
    bool rightButton;
    bool xButton1;
    bool xButton2;
    int x;
    int y;
    int scrollWheelValue;
    Mouse_PositionMode positionMode;
} Mouse_State;


// �}�E�X���W���[���̏�����
void Mouse_Initialize(HWND window);

// �}�E�X���W���[���̏I������
void Mouse_Finalize(void);

// �}�E�X�̏�Ԃ�擾����
void Mouse_GetState(Mouse_State* pState);

// �ݐς����}�E�X�X�N���[���z�C�[���l����Z�b�g����
void Mouse_ResetScrollWheelValue(void);

// �}�E�X�̃|�W�V�������[�h��ݒ肷��i�f�t�H���g�͐�΍��W���[�h�j
void Mouse_SetMode(Mouse_PositionMode mode);

// �}�E�X�̐ڑ�����o����
bool Mouse_IsConnected(void);

// �}�E�X�J�[�\�����\������Ă��邩�m�F����
bool Mouse_IsVisible(void);

// �}�E�X�J�[�\���\����ݒ肷��
void Mouse_SetVisible(bool visible);

// �}�E�X����̂��߂̃E�B���h�E���b�Z�[�W�v���V�[�W���t�b�N�֐�
void Mouse_ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);


// �������@
//
// �Ώۂ̃E�B���h�E���������ꂽ�炻�̃E�B���h�E�n���h��������ɏ������֐���Ă�
//
// Mouse_Initialize(hwnd);
//
// �E�B���h�E���b�Z�[�W�v���V�[�W������}�E�X����p�t�b�N�֐���Ăяo��
//
// LResult CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
// {
//     switch (message)
//     {
//     case WM_ACTIVATEAPP:
//     case WM_INPUT:
//     case WM_MOUSEMOVE:
//     case WM_LBUTTONDOWN:
//     case WM_LBUTTONUP:
//     case WM_RBUTTONDOWN:
//     case WM_RBUTTONUP:
//     case WM_MBUTTONDOWN:
//     case WM_MBUTTONUP:
//     case WM_MOUSEWHEEL:
//     case WM_XBUTTONDOWN:
//     case WM_XBUTTONUP:
//     case WM_MOUSEHOVER:
//         Mouse_ProcessMessage(message, wParam, lParam);
//         break;
//
//     }
// }
//

#endif // MOUSE_H
