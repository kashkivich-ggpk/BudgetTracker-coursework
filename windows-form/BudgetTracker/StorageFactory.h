#pragma once

#include "SupabaseStorage.h"
#include "ClientIdentityService.h"

namespace BudgetTracker {

    using namespace System;
    using namespace System::Collections::Generic;
    using namespace System::IO;
    using namespace System::Windows::Forms;

    public ref class StorageFactory abstract sealed {
    private:
        static Dictionary<String^, String^>^ ReadConfiguration(String^ path) {
            // Простой формат конфигурации: одна строка "Ключ=Значение".
            Dictionary<String^, String^>^ values =
                gcnew Dictionary<String^, String^>(StringComparer::OrdinalIgnoreCase);
            if (!File::Exists(path)) return values;

            for each (String^ raw in File::ReadAllLines(path)) {
                String^ line = raw->Trim();
                if (String::IsNullOrEmpty(line) || line->StartsWith("#")) continue;
                int separator = line->IndexOf('=');
                if (separator <= 0) continue;
                values[line->Substring(0, separator)->Trim()] =
                    line->Substring(separator + 1)->Trim();
            }
            return values;
        }

    public:
        static IDataStorage^ Create() {
            // Конфигурация лежит рядом с exe, поэтому установщик должен
            // обязательно копировать storage.config в каталог программы.
            String^ basePath = Application::StartupPath;
            Dictionary<String^, String^>^ config = ReadConfiguration(
                Path::Combine(basePath, "storage.config"));
            String^ projectUrl = config->ContainsKey("SupabaseUrl")
                ? config["SupabaseUrl"]
                : String::Empty;
            String^ publishableKey = config->ContainsKey("SupabasePublishableKey")
                ? config["SupabasePublishableKey"]
                : String::Empty;

            if (String::IsNullOrWhiteSpace(projectUrl) ||
                String::IsNullOrWhiteSpace(publishableKey))
                throw gcnew InvalidOperationException(
                    L"Supabase Data API не настроен.");

            // Форма получает общий интерфейс IDataStorage и не знает
            // технических деталей подключения к Supabase.
            return gcnew SupabaseStorage(
                projectUrl,
                publishableKey,
                ClientIdentityService::GetOrCreateClientId());
        }
    };
}
