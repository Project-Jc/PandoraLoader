#include "stdafx.h"
#include <Windows.h>
#include <vcclr.h>
#include "PandoraLoader.h"


#define CREATEAPPDOMAIN_EXEASM (WM_USER + 0xFFF)
#define LOADASM_METHODINVOKE (CREATEAPPDOMAIN_EXEASM + 1)


#include <stdio.h>
#include <iostream>
using std::cout;


#define DEBUGG

void LoadConsole() {
#ifdef DEBUG
	if (AllocConsole()) {
		FILE* file;
		freopen_s(&file, "conin$", "r+t", stdin);
		freopen_s(&file, "conout$", "w+t", stdout);
		freopen_s(&file, "conout$", "w+t", stderr);
	}
#endif
}


static HHOOK _messageHookHandle;

bool Pandora::AssemblyLoading::AssemblyLoader::HookMessageProc(IntPtr windowHandle, String ^ assemblyLocation, String ^ appDomainName, String ^ className, String ^ methodName, unsigned int message)
{
	LoadConsole();

	bool result = false;

	String^ params = assemblyLocation + "$";
	params += (appDomainName == nullptr ? "" : appDomainName + "$");
	params += (className == nullptr ? "" : className + "$");
	params += (methodName == nullptr ? "" : methodName);
	
	pin_ptr<const wchar_t> paramsPtr = PtrToStringChars(params);

	HINSTANCE hinstDLL;
	BOOL bModuleHandleResult = ::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)&MessageHookProc, &hinstDLL);
	cout << "GetModuleHandleEx\n" << " -> Result: " << (bModuleHandleResult ? "SUCCESS" : "FAIL") << " hinstDLL: " << hinstDLL << "\n\n";
	if (bModuleHandleResult) {

		DWORD processID = 0;
		DWORD threadID = ::GetWindowThreadProcessId((HWND)windowHandle.ToPointer(), &processID);
		cout << "GetWindowThreadProcessId\n" << " -> Result: " << (processID ? "SUCCESS" : "FAIL") << " processID: " << processID << " threadID: " << threadID << "\n\n";
		if (processID) {

			HANDLE hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
			cout << "OpenProcess\n" << " -> Result: " << (hProcess ? "SUCCESS" : "FAIL") << " hProcess: " << hProcess << "\n\n";
			if (hProcess) {

				int buffLen = (params->Length + 1) * sizeof(wchar_t);
				LPVOID allocatedMemory = ::VirtualAllocEx(hProcess, NULL, buffLen, MEM_COMMIT, PAGE_READWRITE);
				cout << "VirtualAllocEx\n" << " -> Result: " << (allocatedMemory ? "SUCCESS" : "FAIL") << " allocatedMemory: " << allocatedMemory << "\n\n";
				if (allocatedMemory) {

					BOOL bWriteMemory = ::WriteProcessMemory(hProcess, allocatedMemory, paramsPtr, buffLen, NULL);
					cout << "WriteProcessMemory\n" << " -> Result: " << (bWriteMemory ? "SUCCESS" : "FAIL") << " Wrote: " << paramsPtr << " to " << allocatedMemory << "\n\n";
					if (bWriteMemory) {

						_messageHookHandle = ::SetWindowsHookEx(WH_CALLWNDPROC, &MessageHookProc, hinstDLL, threadID);
						cout << "SetWindowsHookEx\n" << " -> Result: " << (_messageHookHandle ? "SUCCESS" : "FAIL") << "\n\n";
						if (_messageHookHandle) {

							::SendMessage((HWND)windowHandle.ToPointer(), message, (WPARAM)allocatedMemory, 0);
							::UnhookWindowsHookEx(_messageHookHandle);

							result = true;
						}
					}

					::VirtualFreeEx(hProcess, allocatedMemory, 0, MEM_RELEASE);
				}

				::CloseHandle(hProcess);
			}
		}
		::FreeLibrary(hinstDLL);
	}

	return result;
}

bool Pandora::AssemblyLoading::AssemblyLoader::ExecuteAssembly(IntPtr windowHandle, String ^ assemblyLocation, String ^ appDomainName)
{
	return HookMessageProc(windowHandle, assemblyLocation, appDomainName, nullptr, nullptr, CREATEAPPDOMAIN_EXEASM);
}

bool Pandora::AssemblyLoading::AssemblyLoader::InvokeMethodInAssembly(IntPtr windowHandle, String ^ assemblyLocation, String ^ className, String ^ methodName)
{
	return HookMessageProc(windowHandle, assemblyLocation, nullptr, className, methodName, LOADASM_METHODINVOKE);
}


public ref class AppDomainAssembly
{
private:
	String ^ AppDomainName;
	String ^ AssemblyLocation;

public:
	AppDomainAssembly(String^ assemblyLocation, String^ appDomainName) {
		AssemblyLocation = assemblyLocation;
		AppDomainName = appDomainName;
	}

	void ThreadProc() {

		try
		{
			Console::WriteLine("Creating AppDomain\n" + " -> " + AppDomainName + "\n\n");
			System::AppDomain^ newAppDomain = System::AppDomain::CreateDomain(AppDomainName);

			Console::WriteLine("Executing assembly\n" + " -> " + AssemblyLocation + "\n\n");
			newAppDomain->ExecuteAssembly(AssemblyLocation);
		}
		catch (Exception^) {

		}
	}
};


__declspec(dllexport)
LRESULT __stdcall MessageHookProc(int nCode, WPARAM wparam, LPARAM lparam)
{
	LoadConsole();

	if (nCode == HC_ACTION) {

		CWPSTRUCT* msg = (CWPSTRUCT*)lparam;

		if (msg != NULL) {

			//cout << "Received message" << endl
			//	<< " -> Message: " << msg->message << endl
			//	<< " -> wParam: " << msg->wParam << endl
			//	<< " -> lParam: " << msg->lParam << endl << endl;

			switch (msg->message) {

				case CREATEAPPDOMAIN_EXEASM: {

					array<String^>^ params = (gcnew String((wchar_t*)msg->wParam))->Split('$');

					String^ assemblyLocation = params[0];
					String^ newAppDomainName = params[1];

					AppDomainAssembly^ appDomainAssembly =
						gcnew AppDomainAssembly(assemblyLocation, newAppDomainName);

					System::Threading::Thread^ thread = gcnew System::Threading::Thread(
						gcnew System::Threading::ThreadStart(appDomainAssembly, &AppDomainAssembly::ThreadProc));

					thread->SetApartmentState(System::Threading::ApartmentState::STA);

					thread->Start();
				} break;

				case LOADASM_METHODINVOKE: {

					using System::Reflection::BindingFlags;
					using System::Reflection::MethodInfo;
					using System::Reflection::Assembly;
					using System::Type;

					String^ acmLocal = gcnew String((wchar_t*)msg->wParam);
					cli::array<String^>^ acmSplit = acmLocal->Split('$');

					Assembly^ assembly = Assembly::LoadFile(acmSplit[0]);
					//cout << "Loaded Assembly: " << MarshalString(acmSplit[0]) << endl << " -> Result: " << (assembly == nullptr ? "FAILED" : "SUCCESS") << endl << endl;
					if (assembly != nullptr) {

						Type^ type = assembly->GetType(acmSplit[1]);
						//cout << "Loaded Type: " << MarshalString(acmSplit[1]) << endl << " -> Result: " << (type == nullptr ? "FAILED" : "SUCCESS") << endl << endl;
						if (type != nullptr) {

							MethodInfo^ methodInfo = type->GetMethod(acmSplit[2], BindingFlags::Static | BindingFlags::Public);
							//cout << "Loaded Method: " << MarshalString(acmSplit[2]) << endl << " -> Result: " << (methodInfo == nullptr ? "FAILED" : "SUCCESS") << endl << endl;
							if (methodInfo != nullptr) {

								Object^ returnValue = methodInfo->Invoke(nullptr, nullptr);
								//cout << "Invoked Method: " << MarshalString(methodInfo->Name) << endl << endl;
							}
						}
					}
				} break;
			}
		}
	}
	return CallNextHookEx(_messageHookHandle, nCode, wparam, lparam);
}