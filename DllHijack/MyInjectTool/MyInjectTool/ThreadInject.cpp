// ThreadInject.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "MyInjectTool.h"
#include "ThreadInject.h"
#include "afxdialogex.h"

//777A01D5    68 78563412     push    12345678
//777A01DA    9C              pushfd
//777A01DB    60              pushad

//777A01EB    61              popad
//777A01EC    9D              popfd
//777A01ED    C3              retn
//�ṹ�����ֽڶ���1
#pragma pack(1)  
typedef struct _INJECT_CODE
{
	BYTE  byPUSH;
	DWORD dwPUSH_VALUE;
	BYTE  byPUSHFD;
	BYTE  byPUSHAD;
	BYTE  byMOV_EAX;          //mov eax, addr szDllpath
	DWORD dwMOV_EAX_VALUE;
	BYTE  byPUSH_EAX;         //push eax
	BYTE  byMOV_ECX;          //mov ecx, LoadLibrary
	DWORD dwMOV_ECX_VALUE;
	WORD  wCALL_ECX;          //call ecx
	BYTE  byPOPAD;
	BYTE  byPOPFD;
	BYTE  byRETN;
	CHAR  szDllPath[MAX_PATH];
}INJECT_CODE, *PINJECT_CODE;
#pragma pack()  


// ThreadInject �Ի���

IMPLEMENT_DYNAMIC(ThreadInject, CDialogEx)

ThreadInject::ThreadInject(CWnd* pParent /*=NULL*/)
	: CDialogEx(ThreadInject::IDD, pParent)
	, m_dwPid(0)
	, m_strDllPath(_T(""))
{

}

ThreadInject::~ThreadInject()
{
}

void ThreadInject::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT1, m_dwPid);
	DDX_Text(pDX, IDC_EDIT4, m_strDllPath);
}


BEGIN_MESSAGE_MAP(ThreadInject, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON2, &ThreadInject::OnBnClickedButton2)
	ON_BN_CLICKED(IDC_BUTTON3, &ThreadInject::OnBnClickedButton3)
	ON_BN_CLICKED(IDC_BUTTON4, &ThreadInject::OnBnClickedButton4)
END_MESSAGE_MAP()


// ThreadInject ��Ϣ��������


void ThreadInject::OnBnClickedButton2()
{
	// TODO:  �ڴ����ӿؼ�֪ͨ�����������
	char szFilter[] = "��̬���ӿ�|*.dll";
	CFileDialog fileDlg(TRUE, "dll", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, szFilter);
	UpdateData(TRUE);
	if (fileDlg.DoModal() == IDOK)
	{
		m_strDllPath = fileDlg.GetPathName();
	}
	UpdateData(FALSE);

}

HANDLE g_hProcess = 0;
LPVOID g_lpBuffer = NULL;
void ThreadInject::OnBnClickedButton3()
{
	// TODO:  �ڴ����ӿؼ�֪ͨ�����������
	//��Ŀ�����
	BOOL   bRet = 0;
	
	INJECT_CODE ic = { 0 };
	THREADENTRY32 te32 = { 0 };
	HANDLE hThreadSnap = NULL;
	DWORD dwThreadId = 0;
	HANDLE hThread = NULL;
	CONTEXT oldContext = { 0 };
	CONTEXT newContext = { 0 };
	DWORD dwOldEip = 0;
	//�򿪽���
	g_hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_dwPid);
	
	if (!g_hProcess)
	{
		MessageBox("OpenProcess ʧ��");
		return;
	}


	g_lpBuffer = VirtualAllocEx(g_hProcess, NULL, 0x1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (!g_lpBuffer)
	{
		MessageBox("VirtualAllocEx ʧ��");
		return;
	}

	//��ShellCode�ṹ�帳ֵ
	ic.byPUSH			= 0x68;
	ic.dwPUSH_VALUE		= 0x12345678;
	ic.byPUSHFD			= 0x9C;
	ic.byPUSHAD			= 0x60;
	ic.byMOV_EAX		= 0xB8;
	ic.dwMOV_EAX_VALUE = (DWORD)g_lpBuffer + offsetof(INJECT_CODE, szDllPath);
	ic.byPUSH_EAX		= 0x50;
	ic.byMOV_ECX		= 0xB9;
	ic.dwMOV_ECX_VALUE  = (DWORD)&LoadLibrary;
	ic.wCALL_ECX		= 0xD1FF;
	ic.byPOPAD			= 0x61;
	ic.byPOPFD			= 0x9D;
	ic.byRETN			= 0xC3;
	memcpy(ic.szDllPath, m_strDllPath.GetBuffer(0), m_strDllPath.GetLength());

	//д��ShellCode
	bRet = WriteProcessMemory(g_hProcess, g_lpBuffer, &ic, sizeof(ic), NULL);
	if (!bRet)
	{
		MessageBox("д���ڴ�ʧ��");
		return;
	}

	//�����߳̿��ղ���Ŀ��������߳�
	te32.dwSize = sizeof(te32);
	hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap == INVALID_HANDLE_VALUE)
	{
		MessageBox("CreateToolhelp32Snapshot ʧ��");
		return;
	}

	//������ѯĿ��������߳�ID
	if (Thread32First(hThreadSnap, &te32))
	{
		do
		{
			if (m_dwPid == te32.th32OwnerProcessID)
			{
				dwThreadId = te32.th32ThreadID;
				break;
			}
		} while (Thread32Next(hThreadSnap, &te32));
	}
	
	//��Ŀ�����߳�
	hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, dwThreadId);

	if (!OpenThread)
	{
		MessageBox("OpenThread ʧ��");
		return;
	}

	//����Ŀ�����߳�
	bRet = SuspendThread(hThread);

	if (bRet == -1)
	{
		MessageBox("SuspendThread ʧ��");
		return;
	}

	oldContext.ContextFlags = CONTEXT_FULL;
	bRet = GetThreadContext(hThread, &oldContext);
	if (!bRet)
	{
		MessageBox("GetThreadContext ʧ��");
		return;
	}
	newContext = oldContext;


#ifdef _WIN64
	newContext.Rip = (DWORD)g_lpBuffer;
	dwOldEip = newContext.Rip;
#else 
	newContext.Eip = (DWORD)g_lpBuffer;
	dwOldEip = newContext.Eip;
#endif

	//;��ָ��ָ��ShellCode��һ��push 12345678h�еĵ�ַ,д�뷵�ص�ַ
	bRet = WriteProcessMemory(g_hProcess, ((char*)g_lpBuffer) + 1, &dwOldEip, sizeof(DWORD), NULL);
	if (!bRet)
	{
		MessageBox("д���ڴ�ʧ��");
		return;
	}

	bRet = SetThreadContext(hThread, &newContext);

	if (!bRet)
	{
		MessageBox("SetThreadContext ʧ��");
		return;
	}

	//Ȼ������߳�������
	bRet = ResumeThread(hThread);
	
	if (bRet == -1)
	{
		MessageBox("ResumeThread ʧ��");
		return;
	}


}


void ThreadInject::OnBnClickedButton4()
{
	// TODO:  �ڴ����ӿؼ�֪ͨ�����������
	if (!VirtualFreeEx(g_hProcess, g_lpBuffer, 0, MEM_RELEASE))
	{
		MessageBox("VirtualFreeEx ʧ��");
		return;
	}

	MessageBox("�ͷŶԷ��ռ�ɹ�");
}