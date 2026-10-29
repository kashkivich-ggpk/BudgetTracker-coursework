#pragma once

namespace BudgetTracker {

    using namespace System;
    using namespace System::IO;

    // Один идентификатор соответствует одной установке приложения.
    // Он отделяет данные этого пользователя от данных других установок.
    public ref class ClientIdentityService abstract sealed {
    public:
        static String^ GetOrCreateClientId() {
            String^ directory = Path::Combine(
                Environment::GetFolderPath(
                    Environment::SpecialFolder::LocalApplicationData),
                "FinancialAssistant");
            String^ filePath = Path::Combine(directory, "client_id.txt");

            if (File::Exists(filePath)) {
                // Используем прежний идентификатор, если файл корректен.
                String^ existingId = File::ReadAllText(filePath)->Trim();
                Guid parsedId;
                if (Guid::TryParse(existingId, parsedId))
                    return parsedId.ToString("D");
            }

            // Если файла нет или он повреждён, создаём новый идентификатор.
            Directory::CreateDirectory(directory);
            String^ newId = Guid::NewGuid().ToString("D");
            File::WriteAllText(filePath, newId);
            return newId;
        }
    };
}
