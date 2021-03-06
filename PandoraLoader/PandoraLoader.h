#pragma once

__declspec(dllexport)
LRESULT __stdcall MessageHookProc(int nCode, WPARAM wparam, LPARAM lparam);

using namespace System;

namespace Pandora {

	namespace AssemblyLoading {

		public ref class AssemblyLoader
		{
		private:
			static bool HookMessageProc(IntPtr windowHandle, String^ assemblyLocation, String^ appDomainName, String^ className, String^ methodName, unsigned int message);

		public:
			static bool ExecuteAssembly(IntPtr windowHandle, String^ assemblyLocation, String^ appDomainName);

			static bool InvokeMethodInAssembly(IntPtr windowHandle, String^ assemblyLocation, String^ className, String^ methodName);
		};
	}
}
