#include "MyForm.h"

using namespace System;
using namespace System::Windows::Forms;

// Точка входа приложения.
// STAThread обязателен для корректной работы Windows Forms и системных диалогов.
[STAThreadAttribute]
int main(array<String^>^ args) {
    // Включаем современный стиль стандартных элементов Windows.
    Application::EnableVisualStyles();
    Application::SetCompatibleTextRenderingDefault(false);
    BudgetTracker::MyForm form;
    Application::Run(% form);
    return 0;
}
